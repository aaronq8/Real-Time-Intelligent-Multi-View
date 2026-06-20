#include "Cli.h"

#include <boost/program_options.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;
namespace fs = std::filesystem;

ParseResult parse_args(int argc, char **argv, AppConfig &config) {
  po::options_description desc("Allowed options");

  desc.add_options()("help", "Show help")(
      "input,i",
      po::value<std::vector<std::string>>()->multitoken()->required(),
      "Three input video files")(
      "width,w", po::value<int>(&config.window_width)->default_value(1280),
      "Output window width")(
      "height,h", po::value<int>(&config.window_height)->default_value(720),
      "Output window height");

  po::positional_options_description positional;
  positional.add("input", 3);

  po::variables_map vm;

  try {
    po::store(po::command_line_parser(argc, argv)
                  .options(desc)
                  .positional(positional)
                  .run(),
              vm);

    if (vm.count("help")) {
      std::cout << "Usage:\n"
                << "  multiview input0.mp4 input1.mp4 input2.mp4\n\n"
                << desc << "\n";
      return ParseResult::Help;
    }

    po::notify(vm);
    if (config.window_width <= 0 || config.window_height <= 0) {
      std::cerr << "Error: window width and height must be positive\n";
      return ParseResult::Error;
    }

    const auto inputs = vm["input"].as<std::vector<std::string>>();

    if (inputs.size() != 3) {
      std::cerr << "Error: expected exactly 3 input files, got "
                << inputs.size() << "\n";
      std::cerr << "Usage: multiview input0.mp4 input1.mp4 input2.mp4\n";
      return ParseResult::Error;
    }

    for (size_t i = 0; i < 3; ++i) {
      if (!fs::exists(inputs[i])) {
        std::cerr << "Error: input file does not exist: " << inputs[i] << "\n";
        return ParseResult::Error;
      }

      config.input_paths[i] = inputs[i];
    }

    return ParseResult::Ok;
  } catch (const po::error &e) {
    std::cerr << "Argument error: " << e.what() << "\n\n";
    std::cerr << "Usage:\n"
              << "  multiview input0.mp4 input1.mp4 input2.mp4\n\n"
              << desc << "\n";
    return ParseResult::Error;
  }
}