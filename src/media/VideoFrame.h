#pragma once

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include "AVFramePtr.h"

struct VideoFrame {
  int stream_id = -1;

  double pts_sec = -1.0;

  int width = 0;
  int height = 0;

  AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

  AVFramePtr frame;
};