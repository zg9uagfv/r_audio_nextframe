#ifndef RESAMPLER_H
#define RESAMPLER_H

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
#include <libswresample/swresample.h>
#ifdef __cplusplus
}
#endif

class Resampler {
  public:
    Resampler();
    ~Resampler();

    int initializeResampler(AVCodecParameters* inputCodecParameters,
                            AVSampleFormat targetSampleFormat = AV_SAMPLE_FMT_FLTP);
    AVFrame* resampleFrame(AVFrame* inputFrame);
    AVFrame* flushResampler();
    void closeResampler();

  private:
    SwrContext* swrContext_;
    AVFrame* resampledFrame_;
    AVSampleFormat targetSampleFormat_;
};

#endif  // RESAMPLER_H