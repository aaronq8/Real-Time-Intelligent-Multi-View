#pragma once

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include <memory>

struct AVFrameDeleter {
  void operator()(AVFrame *frame) const {
    if (frame) {
      av_frame_free(&frame);
    }
  }
};

using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

struct VideoFrame {
  int stream_id = -1;

  double pts_sec = -1.0;

  int width = 0;
  int height = 0;

  AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

  AVFramePtr frame;
};