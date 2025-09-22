#include "AudioProcessor.h"

#include <unistd.h>

#include "FrameDecoder.h"
#include "FrameEncoder.h"
#include "FrameReader.h"
#include "Resampler.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>

// FFmpeg includes - configured in CMakeLists.txt
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include <fstream>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

//#define WRITE_PCM_DEBUG
#ifdef WRITE_PCM_DEBUG
// 将 AVFrame(S16P) 写入 PCM 文件
// out_file: 已打开的 std::ofstream
// frame: 输入 AVFrame，必须是 S16P
// channels: 声道数（frame->ch_layout.nb_channels 或固定值）
// 返回值: 写入字节数
std::ofstream outfile("output.pcm", std::ios::binary);
#endif

size_t write_s16p_frame_to_pcm(std::ofstream& out_file, AVFrame* frame) {
    if (!frame || frame->format != AV_SAMPLE_FMT_S16P) return 0;

    int channels = 2;//av_frame_get_channels(frame); // 获取声道数
    int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    int nb_samples = frame->nb_samples;
    size_t total_bytes = 0;

    // 手动 interleave
    for (int i = 0; i < nb_samples; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            out_file.write((char*)(frame->data[ch] + i * bytes_per_sample), bytes_per_sample);
            total_bytes += bytes_per_sample;
        }
    }

    return total_bytes;
}

AudioProcessor::AudioProcessor()
    : frameReader_(new FrameReader()),
      frameDecoder_(new FrameDecoder()),
      resampler_(new Resampler()),
      frameEncoder_(new FrameEncoder()),
      localFormatContext_(nullptr),
      localOutputFilePath_("local_output.mp3"),
      udpSocket_(-1) {}

AudioProcessor::~AudioProcessor() { closeUdpClient(); }

int AudioProcessor::processAudio(const std::string& inputFilePath, const std::string& udpServerIp,
                                 int udpServerPort) {
    // Initialize FFmpeg
    avformat_network_init();

    // Initialize UDP client (sending to specified server)
    if (initUdpClient(udpServerIp, udpServerPort) < 0) {
        std::cerr << "Warning: Failed to initialize UDP client" << std::endl;
    }

    // Open input file
    if (frameReader_->openInputFile(inputFilePath) < 0) {
        std::cerr << "Failed to open input file" << std::endl;
        closeUdpClient();
        return -1;
    }

    // Initialize decoder
    if (frameDecoder_->initializeDecoder(frameReader_->getFormatContext()) < 0) {
        std::cerr << "Failed to initialize decoder" << std::endl;
        frameReader_->closeInput();
        closeUdpClient();
        return -1;
    }

    // Initialize resampler
    AVCodecParameters* codecParams = frameDecoder_->getCodecParameters();
    AVCodecID codecId = frameDecoder_->getCodecId();
    // Determine target sample format based on output codec
    AVSampleFormat targetSampleFormat;
    if (codecId == AV_CODEC_ID_AAC) {
        targetSampleFormat = AV_SAMPLE_FMT_FLTP;  // AAC requires FLTP
    } else if (codecId == AV_CODEC_ID_MP2 || codecId == AV_CODEC_ID_MP1) {
        targetSampleFormat = AV_SAMPLE_FMT_S16P; 
		/* todo 对几个mp2/mp1文件测试后发现原格式是AV_SAMPLE_FMT_FLTP，则需把重采样的格式设为AV_SAMPLE_FMT_S16P*/
		if (codecParams->format != AV_SAMPLE_FMT_FLTP)
			codecParams->format = AV_SAMPLE_FMT_S16P;
    } else if (codecId == AV_CODEC_ID_MP3) {
        targetSampleFormat = AV_SAMPLE_FMT_S16P;  // MP3 works better with S16P
    } else {
        targetSampleFormat = AV_SAMPLE_FMT_FLTP;  // Default to FLTP
    }

    if (resampler_->initializeResampler(codecParams, targetSampleFormat) < 0) {
        std::cerr << "Failed to initialize resampler" << std::endl;
        frameEncoder_->closeEncoder();
        frameDecoder_->closeDecoder();
        frameReader_->closeInput();
        closeUdpClient();
        return -1;
    }

	    // Initialize encoder with target format (48000 Hz, stereo) and appropriate codec
    // For now, we always encode to MP3 format regardless of input format
    codecId = AV_CODEC_ID_MP3;
    if (frameEncoder_->initializeEncoder(48000, 2, AV_CODEC_ID_MP3) < 0) {
        std::cerr << "Failed to initialize encoder" << std::endl;
        frameDecoder_->closeDecoder();
        frameReader_->closeInput();
        closeUdpClient();
        return -1;
    }

    // Set output file name
    std::string outputFileName = generateOutputFileName(inputFilePath, codecId);
    frameEncoder_->setOutputFile(outputFileName);

    // Initialize local output file
    if (initializeLocalOutputFile(frameEncoder_->getCodecContext()) < 0) {
        std::cerr << "Failed to initialize local output file" << std::endl;
        frameDecoder_->closeDecoder();
        frameReader_->closeInput();
        closeUdpClient();
        return -1;
    }

    // Process frames
    AVPacket* packet;
    bool udpError = false;
    int frameCount = 0;
    while ((packet = frameReader_->readFrame()) != nullptr) {
        frameCount++;
        // Decode frame
        AVFrame* decodedFrame = frameDecoder_->decodePacket(packet);
        if (!decodedFrame) {
            std::cerr << "Failed to decode frame " << frameCount << std::endl;
            av_packet_unref(packet);
            continue;
        }

        // Resample frame
        AVFrame* resampledFrame = resampler_->resampleFrame(decodedFrame);
        if (!resampledFrame) {
            std::cerr << "Failed to resample frame " << frameCount << std::endl;
            av_frame_unref(decodedFrame);
            av_packet_unref(packet);
            continue;
        }
#ifdef WRITE_PCM_DEBUG
		write_s16p_frame_to_pcm(outfile, resampledFrame);
#endif
        // Encode frame (packet will be added to queue)
        if (frameEncoder_->encodeFrame(resampledFrame) < 0) {
            std::cerr << "Failed to encode frame " << frameCount << std::endl;
            av_frame_unref(decodedFrame);
            av_frame_unref(resampledFrame);
            av_packet_unref(packet);
            break;
        }

        // Get encoded packets from queue and process them
        while (frameEncoder_->hasEncodedPackets()) {
            AVPacket* encodedPacket = frameEncoder_->getNextEncodedPacket();
            if (encodedPacket) {
                // Write the encoded packet to a local MP3 file for comparison with UDP server
                // output
                if (writeLocalOutputPacket(encodedPacket) < 0) {
                    std::cerr << "Failed to write local output packet" << std::endl;
                }

                // Send the encoded packet over UDP
                if (udpSocket_ >= 0 && !udpError) {
                    if (sendUdpData(encodedPacket->data, encodedPacket->size) < 0) {
                        std::cerr << "Failed to send UDP data, stopping UDP transmission"
                                  << std::endl;
                        udpError = true;
                    }
                }

                // Clean up
                av_packet_unref(encodedPacket);
            }
        }

        // Clean up
        av_frame_unref(decodedFrame);
        av_frame_unref(resampledFrame);
        av_packet_unref(packet);
    }

    std::cout << "Processed " << frameCount << " frames" << std::endl;

    // Flush decoder
    frameDecoder_->flushDecoder();

    // Flush resampler
    AVFrame* flushedFrame = resampler_->flushResampler();
    if (flushedFrame) {
        frameEncoder_->encodeFrame(flushedFrame);
        // Get encoded packets from queue and process them
        while (frameEncoder_->hasEncodedPackets()) {
            AVPacket* encodedPacket = frameEncoder_->getNextEncodedPacket();
            if (encodedPacket) {
                // Write the encoded packet to a local MP3 file for comparison with UDP server
                // output
                if (writeLocalOutputPacket(encodedPacket) < 0) {
                    std::cerr << "Failed to write local output packet (flush)" << std::endl;
                }

                // Send the encoded packet over UDP if no error occurred
                if (udpSocket_ >= 0 && !udpError) {
                    if (sendUdpData(encodedPacket->data, encodedPacket->size) < 0) {
                        std::cerr << "Failed to send UDP data, stopping UDP transmission"
                                  << std::endl;
                        udpError = true;
                    }
                }
                // Clean up
                av_packet_unref(encodedPacket);
            }
        }
        av_frame_unref(flushedFrame);
    }

    // Flush encoder
    AVPacket* flushedPacket = nullptr;
    frameEncoder_->flushEncoder(&flushedPacket);
    if (flushedPacket) {
        // Write the flushed packet to a local MP3 file for comparison with UDP server output
        if (writeLocalOutputPacket(flushedPacket) < 0) {
            std::cerr << "Failed to write local output packet (flush encoder)" << std::endl;
        }

        // Send the flushed packet over UDP if no error occurred
        if (udpSocket_ >= 0 && !udpError) {
            if (sendUdpData(flushedPacket->data, flushedPacket->size) < 0) {
                std::cerr << "Failed to send UDP data, stopping UDP transmission" << std::endl;
                udpError = true;
            }
        }
        // Clean up
        av_packet_unref(flushedPacket);
    }

    // Send end marker to UDP server if no error occurred
    if (udpSocket_ >= 0 && !udpError) {
        // Send a special end marker (0 bytes)
        if (sendUdpData(nullptr, 0) >= 0) {
            std::cout << "End marker sent to UDP server" << std::endl;
        } else {
            std::cerr << "Failed to send end marker to UDP server" << std::endl;
        }
    }

    // Clean up
    frameReader_->closeInput();
    frameDecoder_->closeDecoder();
    resampler_->closeResampler();
    frameEncoder_->closeEncoder();
    closeLocalOutputFile();
    closeUdpClient();

    if (udpError) {
        std::cout << "Audio processing completed with UDP transmission errors" << std::endl;
        return -1;
    } else {
        std::cout << "Audio processing completed successfully" << std::endl;
        return 0;
    }
}

// Helper function to convert string to lowercase
std::string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lowerStr;
}

// Helper function to get file extension
std::string getFileExtension(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        return toLower(filePath.substr(dotPos + 1));
    }
    return "";
}

// Helper function to get codec ID based on file extension
AVCodecID getCodecIdFromExtension(const std::string& filePath) {
    std::string ext = getFileExtension(filePath);

    if (ext == "mp3") {
        return AV_CODEC_ID_MP3;
    } else if (ext == "wav") {
        return AV_CODEC_ID_PCM_S16LE;
    } else if (ext == "aac") {
        return AV_CODEC_ID_AAC;
    } else if (ext == "flac") {
        return AV_CODEC_ID_FLAC;
    } else if (ext == "ogg" || ext == "oga") {
        return AV_CODEC_ID_VORBIS;
    } else if (ext == "m4a" || ext == "mp4" || ext == "aac") {
        return AV_CODEC_ID_AAC;
    } else {
        // Default to MP3
        return AV_CODEC_ID_MP3;
    }
}

// Helper function to generate output file name based on input file name and codec
std::string generateOutputFileName(const std::string& inputFilePath, AVCodecID codecId) {
    // Extract just the file name without directory path
    size_t slashPos = inputFilePath.find_last_of("/\\");
    std::string fileName =
        (slashPos != std::string::npos) ? inputFilePath.substr(slashPos + 1) : inputFilePath;

    // Extract base name without extension
    size_t dotPos = fileName.find_last_of('.');
    std::string baseName = (dotPos != std::string::npos) ? fileName.substr(0, dotPos) : fileName;

    switch (codecId) {
        case AV_CODEC_ID_PCM_S16LE:
            return baseName + "_48000.wav";
        case AV_CODEC_ID_AAC:
            return baseName + "_48000.aac";
        case AV_CODEC_ID_FLAC:
            return baseName + "_48000.flac";
        case AV_CODEC_ID_VORBIS:
            return baseName + "_48000.ogg";
        case AV_CODEC_ID_MP3:
        default:
            return baseName + "_48000.mp3";
    }
}

int AudioProcessor::initUdpClient(const std::string& serverIp, int serverPort) {
    // Create UDP socket
    udpSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket_ < 0) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        return -1;
    }

    // Set up server address
    memset(&udpServerAddr_, 0, sizeof(udpServerAddr_));
    udpServerAddr_.sin_family = AF_INET;
    udpServerAddr_.sin_port = htons(serverPort);
    udpServerAddr_.sin_addr.s_addr = inet_addr(serverIp.c_str());

    std::cout << "UDP client initialized to send to " << serverIp << ":" << serverPort << std::endl;
    return 0;
}

void AudioProcessor::closeUdpClient() {
    if (udpSocket_ >= 0) {
        close(udpSocket_);
        udpSocket_ = -1;
    }
}

int AudioProcessor::sendUdpData(const uint8_t* data, size_t length) {
    if (udpSocket_ < 0) {
        return -1;
    }

    ssize_t bytesSent;
    if (data == nullptr && length == 0) {
        // Send end marker (0 bytes)
        bytesSent =
            sendto(udpSocket_, "", 0, 0, (struct sockaddr*)&udpServerAddr_, sizeof(udpServerAddr_));
    } else {
        bytesSent = sendto(udpSocket_, data, length, 0, (struct sockaddr*)&udpServerAddr_,
                           sizeof(udpServerAddr_));
    }

    if (bytesSent < 0) {
        std::cerr << "Failed to send UDP data" << std::endl;
        return -1;
    }

    if (static_cast<size_t>(bytesSent) != length) {
        std::cerr << "Warning: Incomplete UDP data sent. Sent " << bytesSent << " out of " << length
                  << " bytes" << std::endl;
    }

    return static_cast<int>(bytesSent);
}

int AudioProcessor::initializeLocalOutputFile(AVCodecContext* codecContext) {
    // Allocate format context for output
    avformat_alloc_output_context2(&localFormatContext_, nullptr, nullptr,
                                   localOutputFilePath_.c_str());
    if (!localFormatContext_) {
        std::cerr << "Could not create output context for local file" << std::endl;
        return -1;
    }

    // Create output stream
    AVStream* outStream = avformat_new_stream(localFormatContext_, nullptr);
    if (!outStream) {
        std::cerr << "Failed allocating output stream for local file" << std::endl;
        return -1;
    }

    // Copy codec parameters to output stream
    avcodec_parameters_from_context(outStream->codecpar, codecContext);

    // Open output file
    if (!(localFormatContext_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&localFormatContext_->pb, localOutputFilePath_.c_str(), AVIO_FLAG_WRITE) <
            0) {
            std::cerr << "Could not open output file " << localOutputFilePath_ << std::endl;
            return -1;
        }
    }

    // Write header
    if (avformat_write_header(localFormatContext_, nullptr) < 0) {
        std::cerr << "Error occurred when opening output file " << localOutputFilePath_
                  << std::endl;
        return -1;
    }

    return 0;
}

int AudioProcessor::writeLocalOutputPacket(AVPacket* packet) {
    if (!localFormatContext_ || !packet) {
        return -1;
    }

    // Write packet to file
    if (av_write_frame(localFormatContext_, packet) < 0) {
        std::cerr << "Error writing packet to local output file" << std::endl;
        return -1;
    }

    return 0;
}

void AudioProcessor::closeLocalOutputFile() {
    if (localFormatContext_) {
        // Write trailer
        av_write_trailer(localFormatContext_);

        // Close output file
        if (!(localFormatContext_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&localFormatContext_->pb);
        }

        // Free format context
        avformat_free_context(localFormatContext_);
        localFormatContext_ = nullptr;
    }
}