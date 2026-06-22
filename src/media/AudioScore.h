#pragma once

struct AudioScore {
  int stream_id = -1;

  double start_sec = -1.0;
  double end_sec = -1.0;

  float rms = 0.0f;
  float short_time_energy = 0.0f;

  float energy_delta = 0.0f;
  float z_score = 0.0f;

  float raw_score = 0.0f;
  float smoothed_score = 0.0f;
};