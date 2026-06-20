#pragma once

#include "VideoFrame.h"
#include <SDL2/SDL_rect.h>
#include <array>
#include <memory>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class SdlRenderer {
public:
  SdlRenderer(int window_width, int window_height);
  ~SdlRenderer();

  SdlRenderer(const SdlRenderer &) = delete;
  SdlRenderer &operator=(const SdlRenderer &) = delete;

  bool poll_quit();

  void update_texture(const VideoFrame &frame);
  void render(const std::array<std::shared_ptr<VideoFrame>, 3> &latest_frames,
              int big_stream_id);

private:
  int window_width_ = 0;
  int window_height_ = 0;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;

  SDL_Rect big_rect_{};
  std::array<SDL_Rect, 2> small_rects_{};

  std::array<SDL_Texture *, 3> textures_ = {nullptr, nullptr, nullptr};
  std::array<int, 3> texture_width_ = {0, 0, 0};
  std::array<int, 3> texture_height_ = {0, 0, 0};

  void destroy_textures();
  void compute_layout_rects();
  void ensure_texture_for_frame(const VideoFrame &frame);
};