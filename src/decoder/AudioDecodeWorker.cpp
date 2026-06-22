#include "AudioDecodeWorker.h"

#include <exception>
#include <iostream>
#include <optional>

AudioDecodeWorker::AudioDecodeWorker(std::string input_path, int stream_id,
                                     SpscQueue<AudioScore> &output_queue)
    : input_path_(std::move(input_path)), stream_id_(stream_id),
      output_queue_(output_queue) {}

AudioDecodeWorker::~AudioDecodeWorker() { join(); }

void AudioDecodeWorker::start() {
  thread_ = std::thread(&AudioDecodeWorker::run, this);
}

void AudioDecodeWorker::join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void AudioDecodeWorker::run() {
  try {
    MediaInput input(input_path_);
    input.open();

    if (input.audio_stream_index() < 0) {
      std::cout << "[audio-worker] stream=" << stream_id_
                << " no audio stream\n";
      output_queue_.close();
      return;
    }

    AudioDecoder decoder(input, stream_id_);
    decoder.open();

    AudioExcitementScorer scorer(stream_id_);

    while (true) {
      std::optional<AudioFrame> frame = decoder.decode_next_frame();

      if (!frame) {
        break;
      }

      decoded_frame_count_.fetch_add(1, std::memory_order_relaxed);

      std::optional<AudioChunk> chunk =
          convert_audio_frame_to_mono_float(*frame);

      if (!chunk) {
        continue;
      }

      AudioScore score = scorer.process_chunk(*chunk);

      if (!output_queue_.push_blocking(score)) {
        break;
      }

      scored_chunk_count_.fetch_add(1, std::memory_order_relaxed);
    }

    std::cout << "[audio-worker] stream=" << stream_id_
              << " decoded_frames=" << decoded_frame_count()
              << " scored_chunks=" << scored_chunk_count() << "\n";

    output_queue_.close();
  } catch (const std::exception &e) {
    std::cerr << "[audio-worker] stream=" << stream_id_
              << " error: " << e.what() << "\n";
    output_queue_.close();
  }
}