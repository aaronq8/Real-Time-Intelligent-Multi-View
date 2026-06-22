#include "AudioExcitementScorer.h"

#include <algorithm>
#include <cmath>

AudioExcitementScorer::AudioExcitementScorer(int stream_id)
    : stream_id_(stream_id) {}

float AudioExcitementScorer::compute_short_time_energy(
    const AudioChunk &chunk) {
  if (chunk.mono_samples.empty()) {
    return 0.0f;
  }

  double sum_sq = 0.0;

  for (float x : chunk.mono_samples) {
    sum_sq += static_cast<double>(x) * x;
  }

  return static_cast<float>(sum_sq /
                            static_cast<double>(chunk.mono_samples.size()));
}

float AudioExcitementScorer::compute_rms_from_energy(float energy) {
  return std::sqrt(std::max(0.0f, energy));
}

void AudioExcitementScorer::push_rms(float rms) {
  rms_window_.push_back(rms);

  rms_sum_ += rms;
  rms_sq_sum_ += static_cast<double>(rms) * rms;

  if (static_cast<int>(rms_window_.size()) > kRollingWindowChunks) {
    const float old = rms_window_.front();
    rms_window_.pop_front();

    rms_sum_ -= old;
    rms_sq_sum_ -= static_cast<double>(old) * old;
  }
}

float AudioExcitementScorer::rolling_mean() const {
  if (rms_window_.empty()) {
    return 0.0f;
  }

  return static_cast<float>(rms_sum_ / static_cast<double>(rms_window_.size()));
}

float AudioExcitementScorer::rolling_stddev() const {
  if (rms_window_.size() < 2) {
    return 1e-6f;
  }

  const double n = static_cast<double>(rms_window_.size());
  const double mean = rms_sum_ / n;
  const double mean_sq = rms_sq_sum_ / n;

  double variance = mean_sq - mean * mean;

  if (variance < 1e-12) {
    variance = 1e-12;
  }

  return static_cast<float>(std::sqrt(variance));
}

float AudioExcitementScorer::clamp01(float x) {
  return std::clamp(x, 0.0f, 1.0f);
}

AudioScore AudioExcitementScorer::process_chunk(const AudioChunk &chunk) {
  AudioScore out;

  out.stream_id = stream_id_;
  out.start_sec = chunk.start_sec;
  out.end_sec = chunk.start_sec + chunk.duration_sec;

  out.short_time_energy = compute_short_time_energy(chunk);
  out.rms = compute_rms_from_energy(out.short_time_energy);

  const float mean_before = rolling_mean();
  const float std_before = rolling_stddev();

  const bool warmed_up =
      static_cast<int>(rms_window_.size()) >= kMinWarmupChunks;

  // During warmup or near-silence, update the baseline but don't emit
  // excitement.
  if (!warmed_up || out.rms < kMinRmsForExcitement) {
    out.energy_delta = 0.0f;
    out.z_score = 0.0f;
    out.raw_score = 0.0f;

    if (!has_ema_) {
      ema_score_ = 0.0f;
      has_ema_ = true;
    } else {
      ema_score_ = kEmaAlpha * out.raw_score + (1.0f - kEmaAlpha) * ema_score_;
    }

    out.smoothed_score = ema_score_;

    push_rms(out.rms);
    return out;
  }

  out.energy_delta = out.rms - mean_before;
  out.z_score = (out.rms - mean_before) / (std_before + 1e-6f);

  const float z_component = clamp01(out.z_score / 3.0f);

  float delta_component = 0.0f;
  if (mean_before > 1e-6f) {
    delta_component =
        clamp01(std::max(0.0f, out.energy_delta) / (mean_before + 1e-6f));
  }

  out.raw_score = clamp01(0.70f * z_component + 0.30f * delta_component);

  ema_score_ = kEmaAlpha * out.raw_score + (1.0f - kEmaAlpha) * ema_score_;

  out.smoothed_score = ema_score_;

  push_rms(out.rms);

  return out;
}