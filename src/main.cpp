#include "AppConfig.h"
#include "Cli.h"
#include "MediaInput.h"
#include "PlaybackClock.h"
#include "SdlRenderer.h"
#include "SpscQueue.h"
#include "VideoDecodeWorker.h"
#include "VideoFrame.h"
#include "VideoFrameBuffer.h"

#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

int main(int argc, char **argv) {
  AppConfig config;

  const ParseResult parse_result = parse_args(argc, argv, config);

  if (parse_result == ParseResult::Help) {
    return 0;
  }

  if (parse_result == ParseResult::Error) {
    return 1;
  }

  try {
    std::array<MediaInput, 3> inputs;

    for (size_t i = 0; i < config.input_paths.size(); ++i) {
      inputs[i] = MediaInput(config.input_paths[i]);
      inputs[i].open();
      inputs[i].print_info(static_cast<int>(i));
    }

    std::cout << "\nAll inputs opened successfully.\n";

    constexpr size_t queue_capacity = 64;

    std::array<std::unique_ptr<SpscQueue<std::shared_ptr<VideoFrame>>>, 3>
        video_queues;
    std::array<std::unique_ptr<VideoDecodeWorker>, 3> workers;

    for (size_t i = 0; i < 3; ++i) {
      video_queues[i] =
          std::make_unique<SpscQueue<std::shared_ptr<VideoFrame>>>(
              queue_capacity);

      workers[i] = std::make_unique<VideoDecodeWorker>(
          inputs[i], static_cast<int>(i), *video_queues[i]);
    }

    for (auto &worker : workers) {
      worker->start();
    }

    SdlRenderer renderer(config.window_width, config.window_height);

    PlaybackClock clock;
    clock.start(0.0);

    std::array<VideoFrameBuffer, 3> frame_buffers;

    // Frames currently selected for display by PTS.
    std::array<std::shared_ptr<VideoFrame>, 3> display_frames;

    // Last frames uploaded to SDL texture.
    // Avoid re-uploading the same frame every render loop.
    std::array<std::shared_ptr<VideoFrame>, 3> uploaded_frames;

    std::array<int, 3> received_count = {0, 0, 0};

    auto last_stats_time = std::chrono::steady_clock::now();
    uint64_t render_count = 0;

    int big_stream_id = 0;

    while (true) {
      if (renderer.poll_quit()) {
        break;
      }

      constexpr double max_buffer_lead_sec = 0.5;

      const double t_now = clock.now_sec();

      for (size_t i = 0; i < 3; ++i) {
        while (frame_buffers[i].needs_more_frames(t_now, max_buffer_lead_sec)) {
          std::optional<std::shared_ptr<VideoFrame>> frame =
              video_queues[i]->try_pop();

          if (!frame) {
            break;
          }

          if (*frame) {
            frame_buffers[i].push(*frame);
            ++received_count[i];
          }
        }
      }

      // 2. Select frame based on playback clock.
      const double t = clock.now_sec();

      for (size_t i = 0; i < 3; ++i) {
        std::shared_ptr<VideoFrame> selected =
            frame_buffers[i].frame_for_time(t);

        if (selected && selected != display_frames[i]) {
          display_frames[i] = selected;
        }

        // Upload only if selected frame changed.
        if (display_frames[i] && display_frames[i] != uploaded_frames[i]) {
          renderer.update_texture(*display_frames[i]);
          uploaded_frames[i] = display_frames[i];
        }
      }

      // 3. Render current selected frames.
      renderer.render(display_frames, big_stream_id);
      ++render_count;
    }

    for (auto &queue : video_queues) {
      queue->close();
    }

    for (auto &worker : workers) {
      worker->join();
    }

    for (size_t i = 0; i < 3; ++i) {
      if (workers[i]->exception()) {
        std::rethrow_exception(workers[i]->exception());
      }
    }

    std::cout << "\nReceived frame counts:\n";
    for (size_t i = 0; i < 3; ++i) {
      std::cout << "  stream " << i << ": " << received_count[i] << " frames\n";
    }

  } catch (const std::exception &e) {
    std::cerr << "\nFatal error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}