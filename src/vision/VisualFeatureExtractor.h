#pragma once

#include "VideoFrame.h"
#include "VisualFrameFeature.h"

#include <optional>
#include <vector>

class VisualFeatureExtractor {
public:
  std::optional<VisualFrameFeature> extract(const VideoFrame &frame);

private:
  std::vector<unsigned char> previous_gray_small_;
  bool has_previous_gray_small_ = false;

  static constexpr int kSmallWidth = 64;
  static constexpr int kSmallHeight = 36;

  static unsigned char clamp_u8(int x);

  static void yuv420_to_rgb_pixel(const VideoFrame &frame, int x, int y, int &r,
                                  int &g, int &b);
};