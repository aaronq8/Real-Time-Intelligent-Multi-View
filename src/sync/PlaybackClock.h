#pragma once

#include <chrono>

class PlaybackClock {
public:
  void start(double start_media_time_sec = 0.0) {
    start_media_time_sec_ = start_media_time_sec;
    start_wall_time_ = std::chrono::steady_clock::now();
    started_ = true;
  }

  double now_sec() const {
    if (!started_) {
      return start_media_time_sec_;
    }

    const auto now = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double>(now - start_wall_time_).count();

    return start_media_time_sec_ + elapsed;
  }

  bool started() const { return started_; }

private:
  bool started_ = false;
  double start_media_time_sec_ = 0.0;
  std::chrono::steady_clock::time_point start_wall_time_{};
};