#include "FrameReader.h"

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

FrameReader::FrameReader() : formatContext_(nullptr) {}

FrameReader::~FrameReader() { closeInput(); }

int FrameReader::openInputFile(const std::string& filePath) {
    // Open input file
    if (avformat_open_input(&formatContext_, filePath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Could not open source file " << filePath << std::endl;
        return -1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(formatContext_, nullptr) < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        return -1;
    }

    return 0;
}

AVPacket* FrameReader::readFrame() {
    if (!formatContext_) {
        return nullptr;
    }

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        return nullptr;
    }

    int ret = av_read_frame(formatContext_, packet);
    if (ret < 0) {
        av_packet_free(&packet);
        return nullptr;
    }

    return packet;
}

void FrameReader::closeInput() {
    if (formatContext_) {
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
    }
}

AVFormatContext* FrameReader::getFormatContext() const { return formatContext_; }