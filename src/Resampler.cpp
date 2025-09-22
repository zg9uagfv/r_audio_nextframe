#include "Resampler.h"

#include <iostream>

Resampler::Resampler() : swrContext_(nullptr), resampledFrame_(nullptr) {}

Resampler::~Resampler() { closeResampler(); }

int Resampler::initializeResampler(AVCodecParameters* inputCodecParameters,
                                   AVSampleFormat targetSampleFormat) {
    if (!inputCodecParameters) {
        return -1;
    }

    // Save target sample format
    this->targetSampleFormat_ = targetSampleFormat;

    // Allocate resample context
    swrContext_ = swr_alloc();
    if (!swrContext_) {
        std::cerr << "Could not allocate resample context" << std::endl;
        return -1;
    }

    // Set options for resampling to 48000 Hz, stereo
    av_opt_set_chlayout(swrContext_, "in_chlayout", &inputCodecParameters->ch_layout, 0);
    av_opt_set_int(swrContext_, "in_sample_rate", inputCodecParameters->sample_rate, 0);
    av_opt_set_sample_fmt(swrContext_, "in_sample_fmt",
                          (AVSampleFormat)inputCodecParameters->format, 0);

    AVChannelLayout outChLayout;
    av_channel_layout_default(&outChLayout, 2);
    av_opt_set_chlayout(swrContext_, "out_chlayout", &outChLayout, 0);
    av_opt_set_int(swrContext_, "out_sample_rate", 48000, 0);
    // Use appropriate sample format based on codec requirements
    av_opt_set_sample_fmt(swrContext_, "out_sample_fmt", targetSampleFormat, 0);

    // Initialize resample context
    if (swr_init(swrContext_) < 0) {
        std::cerr << "Failed to initialize resample context" << std::endl;
        return -1;
    }

    // Allocate resampled frame
    resampledFrame_ = av_frame_alloc();
    if (!resampledFrame_) {
        std::cerr << "Could not allocate resampled frame" << std::endl;
        return -1;
    }

    resampledFrame_->sample_rate = 48000;
    resampledFrame_->nb_samples = 1024;  // Default value, will be adjusted as needed
    resampledFrame_->format = targetSampleFormat_;

    AVChannelLayout chLayout;
    av_channel_layout_default(&chLayout, 2);
    resampledFrame_->ch_layout = chLayout;

    if (av_frame_get_buffer(resampledFrame_, 0) < 0) {
        std::cerr << "Could not allocate resampled frame samples" << std::endl;
        return -1;
    }

    return 0;
}

AVFrame* Resampler::resampleFrame(AVFrame* inputFrame) {
    if (!swrContext_ || !inputFrame) {
        return nullptr;
    }

    // Calculate destination samples
    int dst_nb_samples =
        av_rescale_rnd(swr_get_delay(swrContext_, inputFrame->sample_rate) + inputFrame->nb_samples,
                       48000, inputFrame->sample_rate, AV_ROUND_UP);

    if (dst_nb_samples > resampledFrame_->nb_samples) {
        av_frame_free(&resampledFrame_);
        resampledFrame_ = av_frame_alloc();
        resampledFrame_->sample_rate = 48000;
        resampledFrame_->nb_samples = dst_nb_samples;
        resampledFrame_->format = this->targetSampleFormat_;

        AVChannelLayout chLayout;
        av_channel_layout_default(&chLayout, 2);
        resampledFrame_->ch_layout = chLayout;

        if (av_frame_get_buffer(resampledFrame_, 0) < 0) {
            std::cerr << "Could not allocate resampled frame samples" << std::endl;
            return nullptr;
        }
    }

    // Perform resampling
    int ret = swr_convert(swrContext_, resampledFrame_->data, dst_nb_samples,
                          (const uint8_t**)inputFrame->data, inputFrame->nb_samples);

    if (ret < 0) {
        std::cerr << "Error while converting" << std::endl;
        return nullptr;
    }

    resampledFrame_->nb_samples = ret;
    resampledFrame_->pts = av_rescale_q(swr_next_pts(swrContext_, INT64_MIN),
                                        (AVRational){1, 48000}, (AVRational){1, 48000});

    return resampledFrame_;
}

AVFrame* Resampler::flushResampler() {
    if (!swrContext_) {
        return nullptr;
    }

    // Flush the resampler
    int ret =
        swr_convert(swrContext_, resampledFrame_->data, resampledFrame_->nb_samples, nullptr, 0);
    if (ret <= 0) {
        return nullptr;
    }

    resampledFrame_->nb_samples = ret;
    return resampledFrame_;
}

void Resampler::closeResampler() {
    if (swrContext_) {
        swr_free(&swrContext_);
    }

    if (resampledFrame_) {
        av_frame_free(&resampledFrame_);
    }
}