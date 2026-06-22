#pragma once

extern "C" {
#include <libavutil/frame.h>
}

#include <memory>
#include <stdexcept>
struct AVFrameDeleter {
  void operator()(AVFrame *frame) const {
    if (frame) {
      av_frame_free(&frame);
    }
  }
};

using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

inline AVFramePtr make_av_frame() {
  AVFrame *raw = av_frame_alloc();

  if (!raw) {
    throw std::runtime_error("Failed to allocate AVFrame");
  }

  return AVFramePtr(raw);
}