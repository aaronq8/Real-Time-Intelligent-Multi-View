#include "BigStreamSelector.h"

#include <iostream>

BigStreamSelector::BigStreamSelector(int initial_stream)
    : current_big_stream_(initial_stream) {}

void BigStreamSelector::notify_confirmed_goal(int stream_id,
                                              double playback_time_sec) {
  if (stream_id < 0 || stream_id >= 3) {
    return;
  }

  current_big_stream_ = stream_id;
  confirmed_goal_stream_ = stream_id;
  confirmed_goal_hold_until_sec_ = playback_time_sec + kConfirmedGoalHoldSec;

  std::cout << "[goal-hold]" << " t=" << playback_time_sec
            << " stream=" << stream_id
            << " hold_until=" << confirmed_goal_hold_until_sec_ << "\n";
}

bool BigStreamSelector::in_confirmed_goal_hold(double playback_time_sec) const {
  return confirmed_goal_stream_ >= 0 &&
         playback_time_sec < confirmed_goal_hold_until_sec_;
}

int BigStreamSelector::update(double playback_time_sec) {
  if (in_confirmed_goal_hold(playback_time_sec)) {
    current_big_stream_ = confirmed_goal_stream_;
    return current_big_stream_;
  }

  if (confirmed_goal_stream_ >= 0 &&
      playback_time_sec >= confirmed_goal_hold_until_sec_) {
    confirmed_goal_stream_ = -1;
    confirmed_goal_hold_until_sec_ = -1.0;
  }

  return current_big_stream_;
}