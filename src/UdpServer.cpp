#include "UdpServer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#ifdef __cplusplus
}
#endif

UdpServer::UdpServer() : running_(false), socket_fd_(-1) {}

UdpServer::~UdpServer() { stop(); }

int UdpServer::start(int port) {
    while (true) {  // Loop to restart server after each connection
        // Create UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return -1;
        }

        // Set up server address
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        // Bind socket
        if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind socket" << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return -1;
        }

        running_ = true;
        std::cout << "UDP server started on port " << port << std::endl;

        // Buffer for receiving data
        const size_t BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];

        // Flag to indicate if we received end marker
        bool endMarkerReceived = false;

        // Flag to track if we've received any data
        bool dataReceived = false;

        // FFmpeg format context
        AVFormatContext* formatContext = nullptr;
        std::string outputFileName;

        // Receive data
        while (running_ && !endMarkerReceived) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);

            // Set timeout for recvfrom to detect end of transmission
            struct timeval timeout;
            timeout.tv_sec = 5;  // 5 seconds timeout
            timeout.tv_usec = 0;
            setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            ssize_t bytes_received = recvfrom(socket_fd_, buffer, BUFFER_SIZE, 0,
                                              (struct sockaddr*)&client_addr, &client_addr_len);

            if (bytes_received < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (dataReceived) {
                        std::cout << "Timeout reached, assuming end of transmission" << std::endl;
                    }
                    endMarkerReceived = true;
                } else {
                    std::cerr << "Error receiving data" << std::endl;
                    break;
                }
            } else if (bytes_received == 0) {
                // No data received (end marker)
                if (dataReceived) {
                    std::cout << "End marker received, closing connection" << std::endl;
                }
                endMarkerReceived = true;
            } else {
                // Create output file only when we receive data for the first time
                if (!dataReceived) {
                    dataReceived = true;
                    outputFileName = generateOutputFileName();

                    // Initialize format context for output
                    avformat_alloc_output_context2(&formatContext, nullptr, "mp3",
                                                   outputFileName.c_str());
                    if (!formatContext) {
                        std::cerr << "Could not create output context" << std::endl;
                        break;
                    }

                    // Create output stream
                    AVStream* outStream = avformat_new_stream(formatContext, nullptr);
                    if (!outStream) {
                        std::cerr << "Failed allocating output stream" << std::endl;
                        break;
                    }

                    // Set codec parameters for MP3
                    AVCodecParameters* codecpar = outStream->codecpar;
                    codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
                    codecpar->codec_id = AV_CODEC_ID_MP3;
                    codecpar->bit_rate = 320000;
                    codecpar->sample_rate = 48000;

                    AVChannelLayout chLayout;
                    av_channel_layout_default(&chLayout, 2);
                    av_channel_layout_copy(&codecpar->ch_layout, &chLayout);

                    codecpar->format = AV_SAMPLE_FMT_S16P;

                    // Open output file
                    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
                        if (avio_open(&formatContext->pb, outputFileName.c_str(), AVIO_FLAG_WRITE) <
                            0) {
                            std::cerr << "Could not open output file" << outputFileName
                                      << std::endl;
                            break;
                        }
                    }

                    // Write header
                    if (avformat_write_header(formatContext, nullptr) < 0) {
                        std::cerr << "Error occurred when opening output file" << std::endl;
                        break;
                    }

                    std::cout << "Writing received data to: " << outputFileName << std::endl;
                }

                // Create packet for received data
                AVPacket* pkt = av_packet_alloc();
                av_new_packet(pkt, bytes_received);
                memcpy(pkt->data, buffer, bytes_received);

                // Write packet to file
                if (av_write_frame(formatContext, pkt) < 0) {
                    std::cerr << "Error writing frame" << std::endl;
                }

                av_packet_unref(pkt);
                av_packet_free(&pkt);

                std::cout << "Received " << bytes_received << " bytes from "
                          << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port)
                          << std::endl;
            }
        }

        // Close file if we opened it
        if (formatContext) {
            // Write trailer
            av_write_trailer(formatContext);

            // Close output file
            if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&formatContext->pb);
            }

            // Free format context
            avformat_free_context(formatContext);

            std::cout << "File saved successfully" << std::endl;
        } else if (dataReceived) {
            std::cout << "No data received, not creating file" << std::endl;
        }

        // Close socket
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }

        std::cout << "Connection closed, waiting for new connection..." << std::endl;
    }

    std::cout << "UDP server stopped" << std::endl;
    return 0;
}

void UdpServer::stop() {
    running_ = false;
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

std::string UdpServer::generateOutputFileName() const {
    // Get current time
    time_t now = time(0);
    struct tm* timeinfo = localtime(&now);

    // Format time string
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", timeinfo);

    // Create file name
    std::string fileName = std::string(buffer) + "_recv.mp3";
    return fileName;
}