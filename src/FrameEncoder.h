#ifndef FRAME_ENCODER_H
#define FRAME_ENCODER_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

// 宏定义用于控制是否写入文件
#define ENABLE_FILE_WRITING 1

#if ENABLE_FILE_WRITING
#define WRITE_PACKET(ctx, pkt) av_write_frame(ctx, pkt)
#define WRITE_HEADER(ctx, options) avformat_write_header(ctx, options)
#define WRITE_TRAILER(ctx) av_write_trailer(ctx)
#else
#define WRITE_PACKET(ctx, pkt) (0)
#define WRITE_HEADER(ctx, options) (0)
#define WRITE_TRAILER(ctx) (0)
#endif

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

class FrameEncoder {
  public:
    FrameEncoder();
    ~FrameEncoder();

    int initializeEncoder(int sampleRate, int channels, AVCodecID codecId = AV_CODEC_ID_MP3);
    int encodeFrame(AVFrame* frame, AVPacket** outputPacket = nullptr);
    void flushEncoder(AVPacket** outputPacket = nullptr);
    void closeEncoder();
    void setOutputFile(const std::string& outputFile);

    // Queue methods for AudioProcessor to get encoded packets
    AVPacket* getNextEncodedPacket();
    bool hasEncodedPackets() const;
    void clearPacketQueue();

    // Get codec context for AudioProcessor
    AVCodecContext* getCodecContext() const;

  private:
    AVCodecContext* codecContext_;
    const AVCodec* codec_;
    AVFormatContext* formatContext_;
    std::string outputFilePath_;
    int frameCount_;

    // Buffer for MP3 encoding
    AVFrame* bufferFrame_;
    int bufferedSamples_;

    // Packet queue for encoded packets
    std::queue<AVPacket*> packetQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
};

#endif  // FRAME_ENCODER_H