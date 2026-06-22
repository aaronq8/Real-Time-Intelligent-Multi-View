#pragma once

#include "AudioChunk.h"
#include "AudioScore.h"

#include <deque>

class AudioExcitementScorer {
public:
  explicit AudioExcitementScorer(int stream_id);

  AudioScore process_chunk(const AudioChunk &chunk);

private:
  int stream_id_ = -1;

  std::deque<float> rms_window_;
  double rms_sum_ = 0.0;
  double rms_sq_sum_ = 0.0;

  float ema_score_ = 0.0f;
  bool has_ema_ = false;

  // ~2.5s rolling baseline for AAC 48kHz, 1024 samples/frame.
  static constexpr int kRollingWindowChunks = 120;

  // ~0.85s warmup.
  static constexpr int kMinWarmupChunks = 40;

  // Slower than 0.25, so 1-frame spikes don't dominate switching.
  static constexpr float kEmaAlpha = 0.12f;

  // Ignore very quiet chunks as excitement candidates.
  static constexpr float kMinRmsForExcitement = 0.002f;

  static float compute_short_time_energy(const AudioChunk &chunk);
  static float compute_rms_from_energy(float energy);

  void push_rms(float rms);

  float rolling_mean() const;
  float rolling_stddev() const;

  static float clamp01(float x);
};