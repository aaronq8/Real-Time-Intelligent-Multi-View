#pragma once

struct VisualFrameFeature {
  int stream_id = -1;
  double pts_sec = 0.0;

  // Fraction of pixels that look like grass/field.
  float green_ratio = 0.0f;
  float center_green_ratio = 0.0f;
  float green_tile_ratio = 0.0f;
  float center_non_green_ratio = 0.0f;
  float field_visibility_score = 0.0f;

  // Mean absolute difference from previous sampled frame, grayscale 0..255.
  float scene_diff = 0.0f;

  // Approx edge density / texture amount.
  float edge_density = 0.0f;
};