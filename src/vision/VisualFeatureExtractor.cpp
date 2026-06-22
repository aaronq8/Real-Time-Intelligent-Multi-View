#include "VisualFeatureExtractor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

unsigned char VisualFeatureExtractor::clamp_u8(int x) {
  if (x < 0) {
    return 0;
  }

  if (x > 255) {
    return 255;
  }

  return static_cast<unsigned char>(x);
}

void VisualFeatureExtractor::yuv420_to_rgb_pixel(const VideoFrame &frame, int x,
                                                 int y, int &r, int &g,
                                                 int &b) {
  const int width = frame.width;
  const int height = frame.height;

  x = std::clamp(x, 0, width - 1);
  y = std::clamp(y, 0, height - 1);

  const uint8_t *y_plane = frame.frame->data[0];
  const uint8_t *u_plane = frame.frame->data[1];
  const uint8_t *v_plane = frame.frame->data[2];

  const int y_stride = frame.frame->linesize[0];
  const int u_stride = frame.frame->linesize[1];
  const int v_stride = frame.frame->linesize[2];

  const int Y = static_cast<int>(y_plane[y * y_stride + x]);
  const int U = static_cast<int>(u_plane[(y / 2) * u_stride + (x / 2)]) - 128;
  const int V = static_cast<int>(v_plane[(y / 2) * v_stride + (x / 2)]) - 128;

  // BT.601-ish integer conversion. Good enough for heuristics.
  int C = Y - 16;
  if (C < 0) {
    C = 0;
  }

  r = clamp_u8((298 * C + 409 * V + 128) >> 8);
  g = clamp_u8((298 * C - 100 * U - 208 * V + 128) >> 8);
  b = clamp_u8((298 * C + 516 * U + 128) >> 8);
}

std::optional<VisualFrameFeature>
VisualFeatureExtractor::extract(const VideoFrame &frame) {
  if (!frame.frame) {
    return std::nullopt;
  }

  // For now assume YUV420P/NV12-like planar frame is available as data[0..2].
  // If your decoder outputs another pixel format, we can add sws_scale later.
  if (!frame.frame->data[0] || !frame.frame->data[1] || !frame.frame->data[2]) {
    return std::nullopt;
  }

  VisualFrameFeature out;
  out.stream_id = frame.stream_id;
  out.pts_sec = frame.pts_sec;

  const int width = frame.width;
  const int height = frame.height;

  if (width <= 0 || height <= 0) {
    return std::nullopt;
  }

  // --------------------------------------------------------------------------
  // 1. Green ratio.
  // Sample on a coarse grid to keep it cheap.
  // --------------------------------------------------------------------------
  int green_count = 0;
  int sample_count = 0;

  int center_green_count = 0;
  int center_sample_count = 0;

  constexpr int kStep = 8;

  // Center crop: where a zoomed player usually appears.
  // Tune if needed.
  const int center_x0 = width / 4;
  const int center_x1 = 3 * width / 4;
  const int center_y0 = height / 5;
  const int center_y1 = 4 * height / 5;

  for (int y = 0; y < height; y += kStep) {
    for (int x = 0; x < width; x += kStep) {
      int r = 0;
      int g = 0;
      int b = 0;

      yuv420_to_rgb_pixel(frame, x, y, r, g, b);

      const bool green_pixel =
          g > 60 && g > static_cast<int>(1.15f * static_cast<float>(r)) &&
          g > static_cast<int>(1.15f * static_cast<float>(b));

      if (green_pixel) {
        ++green_count;
      }

      ++sample_count;

      const bool in_center =
          x >= center_x0 && x < center_x1 && y >= center_y0 && y < center_y1;

      if (in_center) {
        if (green_pixel) {
          ++center_green_count;
        }

        ++center_sample_count;
      }
    }
  }

  if (sample_count > 0) {
    out.green_ratio =
        static_cast<float>(green_count) / static_cast<float>(sample_count);
  }

  if (center_sample_count > 0) {
    out.center_green_ratio = static_cast<float>(center_green_count) /
                             static_cast<float>(center_sample_count);

    out.center_non_green_ratio = 1.0f - out.center_green_ratio;
  }

  // --------------------------------------------------------------------------
  // 1b. Green tile ratio.
  // Divide frame into tiles. A tile is "field-like" only if most sampled pixels
  // inside that tile are green.
  //
  // This is better than global green ratio for player close-ups on grass.
  // --------------------------------------------------------------------------
  constexpr int kTileCols = 8;
  constexpr int kTileRows = 6;
  constexpr float kGreenDominantTileThreshold = 0.55f;

  int green_dominant_tiles = 0;
  int total_tiles = 0;

  for (int ty = 0; ty < kTileRows; ++ty) {
    for (int tx = 0; tx < kTileCols; ++tx) {
      const int x0 = tx * width / kTileCols;
      const int x1 = (tx + 1) * width / kTileCols;
      const int y0 = ty * height / kTileRows;
      const int y1 = (ty + 1) * height / kTileRows;

      int tile_green = 0;
      int tile_samples = 0;

      for (int y = y0; y < y1; y += kStep) {
        for (int x = x0; x < x1; x += kStep) {
          int r = 0;
          int g = 0;
          int b = 0;

          yuv420_to_rgb_pixel(frame, x, y, r, g, b);

          const bool green_pixel =
              g > 60 && g > static_cast<int>(1.15f * static_cast<float>(r)) &&
              g > static_cast<int>(1.15f * static_cast<float>(b));

          if (green_pixel) {
            ++tile_green;
          }

          ++tile_samples;
        }
      }

      if (tile_samples > 0) {
        const float tile_green_ratio =
            static_cast<float>(tile_green) / static_cast<float>(tile_samples);

        if (tile_green_ratio >= kGreenDominantTileThreshold) {
          ++green_dominant_tiles;
        }

        ++total_tiles;
      }
    }
  }

  if (total_tiles > 0) {
    out.green_tile_ratio = static_cast<float>(green_dominant_tiles) /
                           static_cast<float>(total_tiles);
  }

  out.field_visibility_score = 0.35f * out.green_ratio +
                               0.35f * out.green_tile_ratio +
                               0.30f * out.center_green_ratio;

  // --------------------------------------------------------------------------
  // 2. Build small grayscale frame.
  // --------------------------------------------------------------------------
  std::vector<unsigned char> gray_small(kSmallWidth * kSmallHeight);

  for (int sy = 0; sy < kSmallHeight; ++sy) {
    for (int sx = 0; sx < kSmallWidth; ++sx) {
      const int x = sx * width / kSmallWidth;
      const int y = sy * height / kSmallHeight;

      const uint8_t *y_plane = frame.frame->data[0];
      const int y_stride = frame.frame->linesize[0];

      gray_small[sy * kSmallWidth + sx] = y_plane[y * y_stride + x];
    }
  }

  // --------------------------------------------------------------------------
  // 3. Scene diff from previous sampled frame.
  // --------------------------------------------------------------------------
  if (has_previous_gray_small_ &&
      previous_gray_small_.size() == gray_small.size()) {
    double sum_abs_diff = 0.0;

    for (size_t i = 0; i < gray_small.size(); ++i) {
      sum_abs_diff += std::abs(static_cast<int>(gray_small[i]) -
                               static_cast<int>(previous_gray_small_[i]));
    }

    out.scene_diff = static_cast<float>(sum_abs_diff /
                                        static_cast<double>(gray_small.size()));
  } else {
    out.scene_diff = 0.0f;
  }

  previous_gray_small_ = gray_small;
  has_previous_gray_small_ = true;

  // --------------------------------------------------------------------------
  // 4. Edge density using simple Sobel-ish gradient on small grayscale.
  // --------------------------------------------------------------------------
  int edge_count = 0;
  int edge_total = 0;

  constexpr int kEdgeThreshold = 35;

  for (int y = 1; y + 1 < kSmallHeight; ++y) {
    for (int x = 1; x + 1 < kSmallWidth; ++x) {
      const int left = gray_small[y * kSmallWidth + (x - 1)];
      const int right = gray_small[y * kSmallWidth + (x + 1)];
      const int up = gray_small[(y - 1) * kSmallWidth + x];
      const int down = gray_small[(y + 1) * kSmallWidth + x];

      const int gx = right - left;
      const int gy = down - up;

      const int mag = std::abs(gx) + std::abs(gy);

      if (mag > kEdgeThreshold) {
        ++edge_count;
      }

      ++edge_total;
    }
  }

  if (edge_total > 0) {
    out.edge_density =
        static_cast<float>(edge_count) / static_cast<float>(edge_total);
  }

  return out;
}