#pragma once

#include "AVFramePtr.h"

extern "C" {
#include <libavutil/samplefmt.h>
}

struct AudioFrame {
  int stream_id = -1;

  double pts_sec = -1.0;

  int sample_rate = 0;
  int channels = 0;
  int nb_samples = 0;

  AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;

  AVFramePtr frame;
};