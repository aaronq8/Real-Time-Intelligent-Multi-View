#pragma once

#include <array>
#include <string>

struct AppConfig {
  std::array<std::string, 3> input_paths;
  int window_width = 1280;
  int window_height = 720;
};