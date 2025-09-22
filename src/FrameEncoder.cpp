#include "FrameEncoder.h"

#include <iostream>

// FFmpeg includes - configured in CMakeLists.txt
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

FrameEncoder::FrameEncoder()
    : codecContext_(nullptr),
      codec_(nullptr),
      formatContext_(nullptr),
      frameCount_(0),
      bufferFrame_(nullptr),
      bufferedSamples_(0) {}

FrameEncoder::~FrameEncoder() {

    // Clean up buffer
    if (bufferFrame_) {
        av_frame_free(&bufferFrame_);
    }

    closeEncoder();
}

int FrameEncoder::initializeEncoder(int sampleRate, int channels, AVCodecID codecId) {
    // Find encoder for specified codec
    codec_ = avcodec_find_encoder(codecId);
    if (!codec_) {
        std::cerr << "Codec not found" << std::endl;
        return -1;
    }

    // Allocate codec context
    codecContext_ = avcodec_alloc_context3(codec_);
    if (!codecContext_) {
        std::cerr << "Could not allocate audio codec context" << std::endl;
        return -1;
    }

    // Set codec parameters for MP3
    codecContext_->bit_rate = 320000;  // 320K bitrate
    codecContext_->sample_rate = sampleRate;

    AVChannelLayout chLayout;
    av_channel_layout_default(&chLayout, channels);
    codecContext_->ch_layout = chLayout;

    // Set sample format based on codec
    if (codecId == AV_CODEC_ID_AAC) {
        codecContext_->sample_fmt = AV_SAMPLE_FMT_FLTP;  // AAC requires fltp
    } else if (codecId == AV_CODEC_ID_PCM_S16LE) {
        codecContext_->sample_fmt = AV_SAMPLE_FMT_S16;  // WAV requires s16
    } else {
        codecContext_->sample_fmt = AV_SAMPLE_FMT_S16P;  // For MP3 encoding
    }

    // Open codec
    if (avcodec_open2(codecContext_, codec_, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return -1;
    }

    return 0;
}

int FrameEncoder::encodeFrame(AVFrame* frame, AVPacket** outputPacket) {
    if (!codecContext_) {
        return -1;
    }

    // If frame is null, just flush the encoder
    if (!frame) {
        int ret = avcodec_send_frame(codecContext_, nullptr);
        if (ret < 0) {
            std::cerr << "Error sending flush frame" << std::endl;
            return -1;
        }

        // Receive encoded packets
        AVPacket* pkt = av_packet_alloc();
        while (ret >= 0) {
            ret = avcodec_receive_packet(codecContext_, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                std::cerr << "Error during encoding" << std::endl;
                av_packet_free(&pkt);
                return -1;
            }

            // Write packet to file
            if (formatContext_) {
                WRITE_PACKET(formatContext_, pkt);
            }

            // Add packet to queue
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                packetQueue_.push(av_packet_clone(pkt));
            }
            queueCondition_.notify_one();

            // If outputPacket is provided, return the packet
            if (outputPacket) {
                *outputPacket = av_packet_clone(pkt);
            }

            av_packet_unref(pkt);
            frameCount_++;
        }

        av_packet_free(&pkt);
        return 0;
    }

    // For MP3 encoding, we need to handle frame sizes properly
    int frameSize = codecContext_->frame_size;
    if (frameSize > 0) {
        // Initialize buffer frame if needed
        if (!bufferFrame_) {
            bufferFrame_ = av_frame_alloc();
            bufferFrame_->format = frame->format;
            bufferFrame_->sample_rate = frame->sample_rate;
            bufferFrame_->ch_layout = frame->ch_layout;
            bufferFrame_->nb_samples = frameSize;

            if (av_frame_get_buffer(bufferFrame_, 0) < 0) {
                std::cerr << "Could not allocate buffer frame" << std::endl;
                return -1;
            }

            bufferedSamples_ = 0;
        }

        // Process input frame samples
        int samplesProcessed = 0;
        while (samplesProcessed < frame->nb_samples) {
            // Calculate how many samples we can copy to buffer
            int samplesToCopy =
                FFMIN(frameSize - bufferedSamples_, frame->nb_samples - samplesProcessed);

            // Copy samples to buffer
            av_samples_copy(bufferFrame_->data, frame->data, bufferedSamples_, samplesProcessed,
                            samplesToCopy, frame->ch_layout.nb_channels,
                            (AVSampleFormat)frame->format);

            bufferedSamples_ += samplesToCopy;
            samplesProcessed += samplesToCopy;

            // If buffer is full, encode it
            if (bufferedSamples_ == frameSize) {
                // Set PTS for the buffer frame
                bufferFrame_->pts = frame->pts + av_rescale_q(samplesProcessed - bufferedSamples_,
                                                              (AVRational){1, frame->sample_rate},
                                                              codecContext_->time_base);

                // Encode the buffer frame
                int ret = avcodec_send_frame(codecContext_, bufferFrame_);
                if (ret < 0) {
                    std::cerr << "Error sending frame for encoding" << std::endl;
                    return -1;
                }

                // Receive encoded packets
                AVPacket* pkt = av_packet_alloc();
                while (ret >= 0) {
                    ret = avcodec_receive_packet(codecContext_, pkt);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        std::cerr << "Error during encoding" << std::endl;
                        av_packet_free(&pkt);
                        return -1;
                    }

                    // Write packet to file
                    if (formatContext_) {
                        WRITE_PACKET(formatContext_, pkt);
                    }

                    // Add packet to queue
                    {
                        std::lock_guard<std::mutex> lock(queueMutex_);
                        packetQueue_.push(av_packet_clone(pkt));
                    }
                    queueCondition_.notify_one();

                    // If outputPacket is provided, return the packet
                    if (outputPacket) {
                        *outputPacket = av_packet_clone(pkt);
                    }

                    av_packet_unref(pkt);
                    frameCount_++;
                }
                av_packet_free(&pkt);
                bufferedSamples_ = 0;
            }
        }
    } else {
        // If codec doesn't have a fixed frame size, send frame directly
        int ret = avcodec_send_frame(codecContext_, frame);
        if (ret < 0) {
            std::cerr << "Error sending frame for encoding" << std::endl;
            return -1;
        }

        // Receive encoded packets
        AVPacket* pkt = av_packet_alloc();
        while (ret >= 0) {
            ret = avcodec_receive_packet(codecContext_, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                std::cerr << "Error during encoding" << std::endl;
                av_packet_free(&pkt);
                return -1;
            }

            // Write packet to file
            if (formatContext_) {
                WRITE_PACKET(formatContext_, pkt);
            }

            // Add packet to queue
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                packetQueue_.push(av_packet_clone(pkt));
            }
            queueCondition_.notify_one();

            // If outputPacket is provided, return the packet
            if (outputPacket) {
                *outputPacket = av_packet_clone(pkt);
            }

            av_packet_unref(pkt);
            frameCount_++;
        }

        av_packet_free(&pkt);
    }

    return 0;
}

void FrameEncoder::flushEncoder(AVPacket** outputPacket) {
    if (!codecContext_) {
        return;
    }

    // Handle any remaining buffered samples
    static AVFrame* bufferFrame = nullptr;
    static int bufferedSamples = 0;

    if (bufferFrame && bufferedSamples > 0) {
        // Set the actual number of samples in the buffer frame
        bufferFrame->nb_samples = bufferedSamples;

        // Encode the partial frame
        int ret = avcodec_send_frame(codecContext_, bufferFrame);
        if (ret < 0) {
            std::cerr << "Error sending final frame for encoding" << std::endl;
        }

        // Receive encoded packets
        AVPacket* pkt = av_packet_alloc();
        while (ret >= 0) {
            ret = avcodec_receive_packet(codecContext_, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                std::cerr << "Error during encoding" << std::endl;
                av_packet_free(&pkt);
                break;
            }

            // Write packet to file
            if (formatContext_) {
                WRITE_PACKET(formatContext_, pkt);
            }

            // Add packet to queue
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                packetQueue_.push(av_packet_clone(pkt));
            }
            queueCondition_.notify_one();

            // If outputPacket is provided, return the packet
            if (outputPacket) {
                *outputPacket = av_packet_clone(pkt);
            }

            av_packet_unref(pkt);
            frameCount_++;
        }

        // Clean up buffer
        av_frame_free(&bufferFrame);
        bufferFrame = nullptr;
        bufferedSamples = 0;
    }

    // Flush encoder
    encodeFrame(nullptr, outputPacket);

    // Write trailer
    if (formatContext_) {
        WRITE_TRAILER(formatContext_);
    }
}

void FrameEncoder::closeEncoder() {
    if (codecContext_) {
        avcodec_free_context(&codecContext_);
    }

    if (formatContext_) {
        avformat_free_context(formatContext_);
    }

    // Clear packet queue
    clearPacketQueue();
}

AVPacket* FrameEncoder::getNextEncodedPacket() {
    std::unique_lock<std::mutex> lock(queueMutex_);

    // Wait for packets to be available
    queueCondition_.wait(lock, [this] { return !packetQueue_.empty(); });

    // Get the front packet
    AVPacket* packet = packetQueue_.front();
    packetQueue_.pop();

    return packet;
}

bool FrameEncoder::hasEncodedPackets() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return !packetQueue_.empty();
}

void FrameEncoder::clearPacketQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);

    // Free all packets in the queue
    while (!packetQueue_.empty()) {
        AVPacket* packet = packetQueue_.front();
        av_packet_free(&packet);
        packetQueue_.pop();
    }
}

AVCodecContext* FrameEncoder::getCodecContext() const { return codecContext_; }

void FrameEncoder::setOutputFile(const std::string& outputFile) {
    outputFilePath_ = outputFile;

    // Initialize format context for output
    avformat_alloc_output_context2(&formatContext_, nullptr, nullptr, outputFile.c_str());
    if (!formatContext_) {
        std::cerr << "Could not create output context" << std::endl;
        return;
    }

    // Create output stream
    AVStream* outStream = avformat_new_stream(formatContext_, codec_);
    if (!outStream) {
        std::cerr << "Failed allocating output stream" << std::endl;
        return;
    }

    // Copy codec parameters to output stream
    avcodec_parameters_from_context(outStream->codecpar, codecContext_);

    // Open output file
    if (!(formatContext_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&formatContext_->pb, outputFile.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output file" << outputFile << std::endl;
            return;
        }
    }

    // Write header
    if (WRITE_HEADER(formatContext_, nullptr) < 0) {
        std::cerr << "Error occurred when opening output file" << std::endl;
        return;
    }
}