#ifndef FRAME_DECODER_H
#define FRAME_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#ifdef __cplusplus
}
#endif

class FrameDecoder {
  public:
    FrameDecoder();
    ~FrameDecoder();

    int initializeDecoder(AVFormatContext* formatContext);
    AVFrame* decodePacket(AVPacket* packet);
    void flushDecoder();
    void closeDecoder();
    AVCodecContext* getCodecContext() const;
    AVCodecParameters* getCodecParameters() const;
    AVCodecID getCodecId() const;  // Added getCodecId method

  private:
    AVCodecContext* codecContext_;
    AVCodecParameters* codecParameters_;
    const AVCodec* codec_;
};

#endif  // FRAME_DECODER_H