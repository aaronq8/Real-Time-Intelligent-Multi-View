#pragma once

#include "VisualFrameFeature.h"

#include <deque>
#include <vector>

class VisualFeatureBuffer {
public:
  void push(const VisualFrameFeature &feature) {
    features_.push_back(feature);

    constexpr double kKeepSec = 12.0;

    while (!features_.empty() &&
           features_.front().pts_sec < feature.pts_sec - kKeepSec) {
      features_.pop_front();
    }
  }

  std::vector<VisualFrameFeature> range(double start_sec,
                                        double end_sec) const {
    std::vector<VisualFrameFeature> out;

    for (const auto &f : features_) {
      if (f.pts_sec < start_sec) {
        continue;
      }

      if (f.pts_sec > end_sec) {
        break;
      }

      out.push_back(f);
    }

    return out;
  }

  bool empty() const { return features_.empty(); }

  const VisualFrameFeature *latest() const {
    if (features_.empty()) {
      return nullptr;
    }

    return &features_.back();
  }

  size_t size() const { return features_.size(); }

private:
  std::deque<VisualFrameFeature> features_;
};