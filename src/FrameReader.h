#ifndef FRAME_READER_H
#define FRAME_READER_H

#include <string>

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

class FrameReader {
  public:
    FrameReader();
    ~FrameReader();

    int openInputFile(const std::string& filePath);
    AVPacket* readFrame();
    void closeInput();
    AVFormatContext* getFormatContext() const;

  private:
    AVFormatContext* formatContext_;
};

#endif  // FRAME_READER_H