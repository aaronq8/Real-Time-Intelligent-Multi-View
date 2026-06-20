#pragma once

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class MediaInput {
public:
  MediaInput() = default;
  explicit MediaInput(std::string path);

  ~MediaInput();

  void open();
  void print_info(int input_id) const;

  const std::string &path() const { return path_; }

  AVFormatContext *format_context() const { return format_ctx_; }

  int video_stream_index() const { return video_stream_index_; }

  int audio_stream_index() const { return audio_stream_index_; }

private:
  std::string path_;
  AVFormatContext *format_ctx_ = nullptr;

  int video_stream_index_ = -1;
  int audio_stream_index_ = -1;

  void close();
};