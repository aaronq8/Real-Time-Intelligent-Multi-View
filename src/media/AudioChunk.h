#pragma once

#include <vector>

struct AudioChunk {
  int stream_id = -1;

  // Start time of this audio chunk in media/playback time.
  double start_sec = -1.0;

  // Duration covered by mono_samples.
  double duration_sec = 0.0;

  int sample_rate = 0;

  // Mono float PCM samples in range roughly [-1.0, 1.0].
  std::vector<float> mono_samples;
};