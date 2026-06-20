#include "MediaInput.h"

#include "FfmpegUtil.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include <iostream>
#include <stdexcept>
#include <utility>

MediaInput::MediaInput(std::string path) : path_(std::move(path)) {}

MediaInput::~MediaInput() { close(); }

void MediaInput::close() {
  if (format_ctx_) {
    avformat_close_input(&format_ctx_);
    format_ctx_ = nullptr;
  }

  video_stream_index_ = -1;
  audio_stream_index_ = -1;
}

void MediaInput::open() {
  close();

  int ret = avformat_open_input(&format_ctx_, path_.c_str(), nullptr, nullptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to open input file '" + path_ +
                             "': " + ffmpeg_error_string(ret));
  }

  ret = avformat_find_stream_info(format_ctx_, nullptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to find stream info for '" + path_ +
                             "': " + ffmpeg_error_string(ret));
  }

  ret =
      av_find_best_stream(format_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

  if (ret < 0) {
    throw std::runtime_error("Failed to find video stream in '" + path_ +
                             "': " + ffmpeg_error_string(ret));
  }

  video_stream_index_ = ret;

  ret =
      av_find_best_stream(format_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

  if (ret >= 0) {
    audio_stream_index_ = ret;
  } else {
    audio_stream_index_ = -1;
  }
}

void MediaInput::print_info(int input_id) const {
  std::cout << "\nInput " << input_id << ": " << path_ << "\n";

  if (!format_ctx_) {
    std::cout << "  Not opened\n";
    return;
  }

  {
    AVStream *video_stream = format_ctx_->streams[video_stream_index_];
    AVCodecParameters *par = video_stream->codecpar;

    const AVCodecDescriptor *desc = avcodec_descriptor_get(par->codec_id);

    std::cout << "  Video stream index: " << video_stream_index_ << "\n";
    std::cout << "  Video codec: " << (desc ? desc->name : "unknown") << "\n";
    std::cout << "  Resolution: " << par->width << "x" << par->height << "\n";
    std::cout << "  Video time_base: " << video_stream->time_base.num << "/"
              << video_stream->time_base.den << "\n";

    if (video_stream->avg_frame_rate.den != 0) {
      std::cout << "  Avg FPS: " << av_q2d(video_stream->avg_frame_rate)
                << "\n";
    }
  }

  if (audio_stream_index_ >= 0) {
    AVStream *audio_stream = format_ctx_->streams[audio_stream_index_];
    AVCodecParameters *par = audio_stream->codecpar;

    const AVCodecDescriptor *desc = avcodec_descriptor_get(par->codec_id);

    std::cout << "  Audio stream index: " << audio_stream_index_ << "\n";
    std::cout << "  Audio codec: " << (desc ? desc->name : "unknown") << "\n";
    std::cout << "  Sample rate: " << par->sample_rate << "\n";
    std::cout << "  Channels: " << par->ch_layout.nb_channels << "\n";
    std::cout << "  Audio time_base: " << audio_stream->time_base.num << "/"
              << audio_stream->time_base.den << "\n";
  } else {
    std::cout << "  Audio stream: none found\n";
  }
}