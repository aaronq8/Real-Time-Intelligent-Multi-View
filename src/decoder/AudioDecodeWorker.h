#pragma once

#include "AudioDecoder.h"
#include "AudioExcitementScorer.h"
#include "AudioFrameConverter.h"
#include "AudioScore.h"
#include "MediaInput.h"
#include "SpscQueue.h"

#include <atomic>
#include <string>
#include <thread>

class AudioDecodeWorker {
public:
  AudioDecodeWorker(std::string input_path, int stream_id,
                    SpscQueue<AudioScore> &output_queue);

  ~AudioDecodeWorker();

  AudioDecodeWorker(const AudioDecodeWorker &) = delete;
  AudioDecodeWorker &operator=(const AudioDecodeWorker &) = delete;

  void start();
  void join();

  uint64_t decoded_frame_count() const { return decoded_frame_count_.load(); }

  uint64_t scored_chunk_count() const { return scored_chunk_count_.load(); }

private:
  void run();

  std::string input_path_;
  int stream_id_ = -1;

  SpscQueue<AudioScore> &output_queue_;

  std::thread thread_;

  std::atomic<uint64_t> decoded_frame_count_{0};
  std::atomic<uint64_t> scored_chunk_count_{0};
};