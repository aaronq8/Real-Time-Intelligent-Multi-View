#pragma once

class BigStreamSelector {
public:
  explicit BigStreamSelector(int initial_stream = 0);

  void notify_confirmed_goal(int stream_id, double playback_time_sec);

  int update(double playback_time_sec);

  int current_big_stream() const { return current_big_stream_; }

private:
  bool in_confirmed_goal_hold(double playback_time_sec) const;

  int current_big_stream_ = 0;

  int confirmed_goal_stream_ = -1;
  double confirmed_goal_hold_until_sec_ = -1.0;

  static constexpr double kConfirmedGoalHoldSec = 12.0;
};