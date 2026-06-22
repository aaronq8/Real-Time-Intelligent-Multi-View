#pragma once

#include "VisualFeatureBuffer.h"

#include <algorithm>
#include <vector>

struct GoalVisualEvidence {
  double candidate_time_sec = 0.0;

  // These names are kept for compatibility, but they now mean
  // "field visibility" rather than raw green percentage.
  float green_before = 0.0f;

  float green_after = 0.0f;
  float green_drop = 0.0f;

  // Extra debug fields.
  float green_early_avg = 0.0f;
  float green_replay_avg = 0.0f;
  float green_replay_min = 0.0f;

  float early_green_drop = 0.0f;
  float replay_green_drop = 0.0f;

  double low_green_duration_sec = 0.0;

  float visual_score = 0.0f;
  bool confirmed = false;
};

class GoalVisualHeuristic {
public:
  GoalVisualEvidence evaluate(const VisualFeatureBuffer &visual_buffer,
                              double candidate_time_sec) const {
    GoalVisualEvidence out;
    out.candidate_time_sec = candidate_time_sec;

    const std::vector<VisualFrameFeature> before = visual_buffer.range(
        candidate_time_sec - kBeforeWindowSec, candidate_time_sec);

    // Immediate close-up / celebration window.
    const std::vector<VisualFrameFeature> early_after =
        visual_buffer.range(candidate_time_sec + kEarlyAfterDelaySec,
                            candidate_time_sec + kEarlyAfterWindowSec);

    // Delayed replay / celebration continuation window.
    const std::vector<VisualFrameFeature> replay_after =
        visual_buffer.range(candidate_time_sec + kReplayAfterStartSec,
                            candidate_time_sec + kReplayAfterEndSec);

    if (before.size() < kMinSamplesBefore ||
        early_after.size() < kMinSamplesAfter ||
        replay_after.size() < kMinSamplesAfter) {
      return out;
    }

    out.green_before = average_field_visibility(before);

    out.green_early_avg = average_field_visibility(early_after);
    out.green_replay_avg = average_field_visibility(replay_after);
    out.green_replay_min = min_field_visibility(replay_after);

    out.early_green_drop = out.green_before - out.green_early_avg;
    out.replay_green_drop = out.green_before - out.green_replay_avg;

    // Keep old fields mapped to the stronger replay-aware value.
    out.green_after = std::min(out.green_early_avg, out.green_replay_avg);
    out.green_drop = std::max(out.early_green_drop, out.replay_green_drop);

    out.low_green_duration_sec = estimate_low_field_duration(replay_after);

    const float early_drop_score =
        clamp01(out.early_green_drop / kStrongGreenDrop);

    const float replay_drop_score =
        clamp01(out.replay_green_drop / kStrongGreenDrop);

    const float replay_min_score = clamp01(
        (kReplayMinFieldGood - out.green_replay_min) / kReplayMinFieldGood);

    const float low_field_score = clamp01(static_cast<float>(
        out.low_green_duration_sec / kStrongLowGreenDurationSec));

    out.visual_score = 0.25f * early_drop_score + 0.35f * replay_drop_score +
                       0.20f * replay_min_score + 0.20f * low_field_score;

    const bool early_confirmed = out.early_green_drop >= kMinEarlyGreenDrop;

    const bool replay_confirmed =
        out.replay_green_drop >= kMinReplayGreenDrop &&
        (out.low_green_duration_sec >= kMinLowGreenDurationSec ||
         out.green_replay_min <= kVeryLowFieldVisibility);

    const bool fast_replay_confirmed =
        out.green_before >= kMinFieldBeforeForReplay &&
        out.green_replay_min <= kVeryLowFieldVisibility;

    out.confirmed =
        fast_replay_confirmed || (out.visual_score >= kMinVisualScore &&
                                  (early_confirmed || replay_confirmed));

    return out;
  }

private:
  // GoalVisualHeuristic.h
  static constexpr double kBeforeWindowSec = 3.0;

  static constexpr double kEarlyAfterDelaySec = 0.5;
  static constexpr double kEarlyAfterWindowSec = 3.25;

  static constexpr double kReplayAfterStartSec = 2.25;
  static constexpr double kReplayAfterEndSec = 3.75;

  static constexpr size_t kMinSamplesBefore = 4;
  static constexpr size_t kMinSamplesAfter = 3;

  static constexpr float kMinEarlyGreenDrop = 0.10f;
  static constexpr float kMinReplayGreenDrop = 0.15f;
  static constexpr float kStrongGreenDrop = 0.35f;

  static constexpr float kLowFieldVisibility = 0.25f;
  static constexpr float kVeryLowFieldVisibility = 0.18f;
  static constexpr float kReplayMinFieldGood = 0.35f;
  static constexpr float kMinFieldBeforeForReplay = 0.45f;

  static constexpr double kMinLowGreenDurationSec = 0.25;
  static constexpr double kStrongLowGreenDurationSec = 0.75;

  static constexpr float kMinVisualScore = 0.35f;

  static float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

  static float field_visibility_of(const VisualFrameFeature &f) {
    // Requires VisualFrameFeature to have field_visibility_score.
    // Fallback to green_ratio if you have not added the new field yet.
    return f.field_visibility_score;
  }

  static float
  average_field_visibility(const std::vector<VisualFrameFeature> &frames) {
    if (frames.empty()) {
      return 0.0f;
    }

    double sum = 0.0;
    for (const auto &f : frames) {
      sum += field_visibility_of(f);
    }

    return static_cast<float>(sum / static_cast<double>(frames.size()));
  }

  static float
  min_field_visibility(const std::vector<VisualFrameFeature> &frames) {
    if (frames.empty()) {
      return 0.0f;
    }

    float m = field_visibility_of(frames.front());
    for (const auto &f : frames) {
      m = std::min(m, field_visibility_of(f));
    }

    return m;
  }

  static double
  estimate_low_field_duration(const std::vector<VisualFrameFeature> &frames) {
    if (frames.size() < 2) {
      return 0.0;
    }

    double duration = 0.0;

    for (size_t i = 1; i < frames.size(); ++i) {
      const double dt = frames[i].pts_sec - frames[i - 1].pts_sec;

      if (field_visibility_of(frames[i - 1]) < kLowFieldVisibility &&
          dt > 0.0 && dt < 1.0) {
        duration += dt;
      }
    }

    return duration;
  }
};