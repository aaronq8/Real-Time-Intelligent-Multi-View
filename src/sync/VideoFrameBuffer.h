#pragma once

#include "VideoFrame.h"

#include <cstddef>
#include <deque>
#include <memory>

class VideoFrameBuffer {
public:
  void push(std::shared_ptr<VideoFrame> frame) {
    if (!frame) {
      return;
    }

    frames_.push_back(std::move(frame));
  }

  std::shared_ptr<VideoFrame> frame_for_time(double t_sec) {
    while (frames_.size() >= 2 && frames_[1]->pts_sec <= t_sec) {
      // our render thread was slow slower than wall clock so we need to drop
      // this frame
      frames_.pop_front();
    }

    if (frames_.empty()) {
      return nullptr;
    }

    if (frames_.front()->pts_sec > t_sec) {
      // don't display frame yet
      return nullptr;
    }
    // pts_sec <= t_sec
    return frames_.front();
  }

  bool needs_more_frames(double playback_time_sec, double max_lead_sec) const {
    if (frames_.empty()) {
      return true;
    }

    return frames_.back()->pts_sec < playback_time_sec + max_lead_sec;
  }

  double buffered_until_sec() const {
    if (frames_.empty()) {
      return -1.0;
    }

    return frames_.back()->pts_sec;
  }

  size_t size() const { return frames_.size(); }

  bool empty() const { return frames_.empty(); }

private:
  std::deque<std::shared_ptr<VideoFrame>> frames_;
};