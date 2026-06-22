#pragma once

#include "AudioScore.h"

#include <algorithm>
#include <deque>

class AudioScoreBuffer {
public:
  void push(AudioScore score) { scores_.push_back(score); }

  float score_for_time(double t_sec) {
    constexpr double kLookbackSec = 0.5;
    constexpr double kKeepHistorySec = 1.5;

    while (scores_.size() >= 2 &&
           scores_[1].end_sec < t_sec - kKeepHistorySec) {
      scores_.pop_front();
    }

    float max_score = 0.0f;
    double area = 0.0;
    double total_dt = 0.0;

    const double window_start = t_sec - kLookbackSec;
    const double window_end = t_sec;

    for (const AudioScore &s : scores_) {
      if (s.end_sec < window_start) {
        continue;
      }

      if (s.start_sec > window_end) {
        break;
      }

      const float e = event_score(s);

      max_score = std::max(max_score, e);

      const double dt = s.end_sec - s.start_sec;
      area += static_cast<double>(e) * dt;
      total_dt += dt;
    }

    const float avg_score =
        total_dt > 0.0 ? static_cast<float>(area / total_dt) : 0.0f;

    return 0.35f * max_score + 0.65f * avg_score;
  }

  bool needs_more_scores(double playback_time_sec, double max_lead_sec) const {
    if (scores_.empty()) {
      return true;
    }

    return scores_.back().end_sec < playback_time_sec + max_lead_sec;
  }

  size_t size() const { return scores_.size(); }

  double buffered_until_sec() const {
    if (scores_.empty()) {
      return -1.0;
    }

    return scores_.back().end_sec;
  }

private:
  static float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

  static float startup_weight(double t_sec) {
    if (t_sec < 3.0) {
      return 0.25f;
    }

    if (t_sec < 6.0) {
      return 0.25f + 0.75f * static_cast<float>((t_sec - 3.0) / 3.0);
    }

    return 1.0f;
  }

  static float event_score(const AudioScore &s) {
    const float rel = std::max(s.smoothed_score, 0.65f * s.raw_score);

    constexpr float kQuietRms = 0.025f;
    constexpr float kLoudRms = 0.090f;

    const float loud = clamp01((s.rms - kQuietRms) / (kLoudRms - kQuietRms));

    const float combined = 0.40f * rel + 0.40f * loud + 0.20f * rel * loud;

    return clamp01(startup_weight(s.start_sec) * combined);
  }

  std::deque<AudioScore> scores_;
};