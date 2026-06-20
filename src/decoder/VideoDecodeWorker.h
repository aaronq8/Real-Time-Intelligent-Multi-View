#pragma once

#include "MediaInput.h"
#include "SpscQueue.h"
#include "VideoFrame.h"

#include <atomic>
#include <exception>
#include <memory>
#include <thread>

class VideoDecodeWorker {
public:
  VideoDecodeWorker(MediaInput &input, int stream_id,
                    SpscQueue<std::shared_ptr<VideoFrame>> &output_queue);

  ~VideoDecodeWorker();

  VideoDecodeWorker(const VideoDecodeWorker &) = delete;
  VideoDecodeWorker &operator=(const VideoDecodeWorker &) = delete;

  void start();
  void join();

  bool finished() const { return finished_.load(); }

  std::exception_ptr exception() const { return exception_; }

private:
  MediaInput &input_;
  int stream_id_ = -1;
  SpscQueue<std::shared_ptr<VideoFrame>> &output_queue_;

  std::thread thread_;
  std::atomic<bool> finished_{false};
  std::exception_ptr exception_ = nullptr;

  void run();
};