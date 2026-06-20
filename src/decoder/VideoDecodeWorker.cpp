#include "VideoDecodeWorker.h"
#include "VideoDecoder.h"

#include <iostream>

VideoDecodeWorker::VideoDecodeWorker(
    MediaInput &input, int stream_id,
    SpscQueue<std::shared_ptr<VideoFrame>> &output_queue)
    : input_(input), stream_id_(stream_id), output_queue_(output_queue) {}

VideoDecodeWorker::~VideoDecodeWorker() { join(); }

void VideoDecodeWorker::start() {
  thread_ = std::thread(&VideoDecodeWorker::run, this);
}

void VideoDecodeWorker::join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void VideoDecodeWorker::run() {
  try {
    VideoDecoder decoder(input_, stream_id_);
    decoder.open();

    int count = 0;

    while (true) {
      std::optional<VideoFrame> frame = decoder.decode_next_frame();

      if (!frame) {
        break;
      }

      auto frame_ptr = std::make_shared<VideoFrame>(std::move(*frame));
      // If queue reached capacity, we will yeild and retry until consumer
      // consumes a frame...TODO maybe we can drop older frame ?
      if (!output_queue_.push_blocking(std::move(frame_ptr))) {
        break;
      }

      ++count;
    }

    std::cout << "Video worker " << stream_id_ << " decoded " << count
              << " frames\n";
  } catch (...) {
    exception_ = std::current_exception();
  }

  finished_.store(true);
  output_queue_.close();
}