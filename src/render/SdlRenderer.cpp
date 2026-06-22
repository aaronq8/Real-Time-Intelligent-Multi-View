#include "SdlRenderer.h"

#include <SDL2/SDL.h>

#include <iostream>
#include <stdexcept>
void SdlRenderer::compute_layout_rects() {
  const int big_w = (window_width_ * 2) / 3;
  const int small_w = window_width_ - big_w;

  const int top_h = window_height_ / 2;
  const int bottom_h = window_height_ - top_h;

  big_rect_ = SDL_Rect{
      .x = 0,
      .y = 0,
      .w = big_w,
      .h = window_height_,
  };

  small_rects_[0] = SDL_Rect{
      .x = big_w,
      .y = 0,
      .w = small_w,
      .h = top_h,
  };

  small_rects_[1] = SDL_Rect{
      .x = big_w,
      .y = top_h,
      .w = small_w,
      .h = bottom_h,
  };
  std::cout << "[layout]\n"
            << "  big: x=" << big_rect_.x << " y=" << big_rect_.y
            << " w=" << big_rect_.w << " h=" << big_rect_.h << "\n"
            << "  small0: x=" << small_rects_[0].x << " y=" << small_rects_[0].y
            << " w=" << small_rects_[0].w << " h=" << small_rects_[0].h << "\n"
            << "  small1: x=" << small_rects_[1].x << " y=" << small_rects_[1].y
            << " w=" << small_rects_[1].w << " h=" << small_rects_[1].h << "\n";
}

SdlRenderer::SdlRenderer(int window_width, int window_height)
    : window_width_(window_width), window_height_(window_height) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
  }

  window_ =
      SDL_CreateWindow("RT-IMV", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       window_width_, window_height_, SDL_WINDOW_SHOWN);

  if (!window_) {
    throw std::runtime_error(std::string("SDL_CreateWindow failed: ") +
                             SDL_GetError());
  }

  renderer_ = SDL_CreateRenderer(
      window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (!renderer_) {
    throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") +
                             SDL_GetError());
  }

  compute_layout_rects();
}

SdlRenderer::~SdlRenderer() {
  destroy_textures();

  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }

  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }

  SDL_Quit();
}

void SdlRenderer::destroy_textures() {
  for (SDL_Texture *&texture : textures_) {
    if (texture) {
      SDL_DestroyTexture(texture);
      texture = nullptr;
    }
  }
}

bool SdlRenderer::poll_quit() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return true;
    }

    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
      return true;
    }
  }

  return false;
}

void SdlRenderer::ensure_texture_for_frame(const VideoFrame &frame) {
  const int stream_id = frame.stream_id;

  if (stream_id < 0 || stream_id >= 3) {
    return;
  }

  if (frame.pix_fmt != AV_PIX_FMT_YUV420P) {
    throw std::runtime_error(
        "SdlRenderer currently only supports AV_PIX_FMT_YUV420P");
  }

  if (textures_[stream_id] && texture_width_[stream_id] == frame.width &&
      texture_height_[stream_id] == frame.height) {
    return;
  }

  if (textures_[stream_id]) {
    SDL_DestroyTexture(textures_[stream_id]);
    textures_[stream_id] = nullptr;
  }

  textures_[stream_id] =
      SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV,
                        SDL_TEXTUREACCESS_STREAMING, frame.width, frame.height);

  if (!textures_[stream_id]) {
    throw std::runtime_error(std::string("SDL_CreateTexture failed: ") +
                             SDL_GetError());
  }

  texture_width_[stream_id] = frame.width;
  texture_height_[stream_id] = frame.height;
}

void SdlRenderer::update_texture(const VideoFrame &frame) {
  const int stream_id = frame.stream_id;

  if (stream_id < 0 || stream_id >= 3) {
    return;
  }

  ensure_texture_for_frame(frame);

  AVFrame *avframe = frame.frame.get();

  const int ret = SDL_UpdateYUVTexture(textures_[stream_id], nullptr,
                                       avframe->data[0], avframe->linesize[0],
                                       avframe->data[1], avframe->linesize[1],
                                       avframe->data[2], avframe->linesize[2]);

  if (ret != 0) {
    throw std::runtime_error(std::string("SDL_UpdateYUVTexture failed: ") +
                             SDL_GetError());
  }
}

void SdlRenderer::render(
    const std::array<std::shared_ptr<VideoFrame>, 3> &latest_frames,
    int big_stream_id) {
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  SDL_RenderClear(renderer_);

  if (big_stream_id < 0 || big_stream_id >= 3) {
    big_stream_id = 0;
  }

  int small_index = 0;

  for (int stream_id = 0; stream_id < 3; ++stream_id) {
    if (!latest_frames[stream_id]) {
      continue;
    }

    if (!textures_[stream_id]) {
      continue;
    }

    if (stream_id == big_stream_id) {
      SDL_RenderCopy(renderer_, textures_[stream_id], nullptr, &big_rect_);
    } else {
      if (small_index < 2) {
        SDL_RenderCopy(renderer_, textures_[stream_id], nullptr,
                       &small_rects_[small_index]);
        ++small_index;
      }
    }
  }

  SDL_RenderPresent(renderer_);
}