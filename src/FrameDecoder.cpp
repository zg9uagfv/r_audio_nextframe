#include "FrameDecoder.h"

#include <iostream>

FrameDecoder::FrameDecoder() : codecContext_(nullptr), codecParameters_(nullptr), codec_(nullptr) {}

FrameDecoder::~FrameDecoder() { closeDecoder(); }

int FrameDecoder::initializeDecoder(AVFormatContext* formatContext) {
    // Find audio stream
    int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec_, 0);
    if (streamIndex < 0) {
        std::cerr << "Could not find audio stream" << std::endl;
        return -1;
    }

    // Allocate codec context
    codecContext_ = avcodec_alloc_context3(codec_);
    if (!codecContext_) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return -1;
    }

    // Save codec parameters
    codecParameters_ = formatContext->streams[streamIndex]->codecpar;

    // Copy codec parameters to codec context
    if (avcodec_parameters_to_context(codecContext_, codecParameters_) < 0) {
        std::cerr << "Failed to copy codec parameters to codec context" << std::endl;
        return -1;
    }

    // Initialize codec context
    if (avcodec_open2(codecContext_, codec_, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return -1;
    }

    return 0;
}

AVFrame* FrameDecoder::decodePacket(AVPacket* packet) {
    if (!codecContext_ || !packet) {
        return nullptr;
    }

    int ret = avcodec_send_packet(codecContext_, packet);
    if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "Error sending packet for decoding: " << errBuf << std::endl;
        return nullptr;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Could not allocate frame" << std::endl;
        return nullptr;
    }

    ret = avcodec_receive_frame(codecContext_, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&frame);
        return nullptr;
    } else if (ret < 0) {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errBuf, sizeof(errBuf));
        std::cerr << "Error during decoding: " << errBuf << std::endl;
        av_frame_free(&frame);
        return nullptr;
    }

    return frame;
}

void FrameDecoder::flushDecoder() {
    if (!codecContext_) {
        return;
    }

    avcodec_send_packet(codecContext_, nullptr);
}

void FrameDecoder::closeDecoder() {
    if (codecContext_) {
        avcodec_free_context(&codecContext_);
        codecContext_ = nullptr;
    }
}

AVCodecContext* FrameDecoder::getCodecContext() const { return codecContext_; }

AVCodecParameters* FrameDecoder::getCodecParameters() const { return codecParameters_; }

AVCodecID FrameDecoder::getCodecId() const {
    if (codecParameters_) {
        return codecParameters_->codec_id;
    }
    return AV_CODEC_ID_NONE;
}