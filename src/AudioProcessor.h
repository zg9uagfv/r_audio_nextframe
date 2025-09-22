#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <memory>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

// Forward declarations for helper functions
std::string getFileExtension(const std::string& filePath);
AVCodecID getCodecIdFromExtension(const std::string& filePath);
std::string generateOutputFileName(const std::string& inputFilePath, AVCodecID codecId);

#include "FrameDecoder.h"
#include "FrameEncoder.h"
#include "FrameReader.h"
#include "Resampler.h"

class AudioProcessor {
  public:
    AudioProcessor();
    ~AudioProcessor();

    int processAudio(const std::string& inputFilePath, const std::string& serverIp = "127.0.0.1",
                     int serverPort = 8080);

  private:
    std::unique_ptr<FrameReader> frameReader_;
    std::unique_ptr<FrameDecoder> frameDecoder_;
    std::unique_ptr<Resampler> resampler_;
    std::unique_ptr<FrameEncoder> frameEncoder_;

    // Local output file members
    AVFormatContext* localFormatContext_;
    std::string localOutputFilePath_;
    int initializeLocalOutputFile(AVCodecContext* codecContext);
    int writeLocalOutputPacket(AVPacket* packet);
    void closeLocalOutputFile();

    // UDP client members
    int udpSocket_;
    struct sockaddr_in udpServerAddr_;
    int initUdpClient(const std::string& serverIp, int serverPort);
    void closeUdpClient();
    int sendUdpData(const uint8_t* data, size_t length);
};

#endif  // AUDIO_PROCESSOR_H