#include "AudioProcessor.h"

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <input_audio_file> [udp_server_ip] [udp_server_port]"
                  << std::endl;
        std::cerr << "Supported formats: MP3, WAV, AAC, FLAC, OGG" << std::endl;
        std::cerr << "Default UDP server: 127.0.0.1:8080" << std::endl;
        return -1;
    }

    std::string inputFilePath = argv[1];
    std::string udpServerIp = "127.0.0.1";
    int udpServerPort = 8080;

    if (argc >= 3) {
        udpServerIp = argv[2];
    }

    if (argc >= 4) {
        try {
            udpServerPort = std::stoi(argv[3]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[3] << std::endl;
            return -1;
        }
    }

    // Initialize FFmpeg
    // avformat_network_init();

    // Create audio processor
    AudioProcessor processor;

    // Process audio file
    int result = processor.processAudio(inputFilePath, udpServerIp, udpServerPort);

    // Clean up FFmpeg
    // avformat_network_deinit();

    return result;
}