#include "VideoDecoder.h"

#include "FfmpegUtil.h"

extern "C" {
#include <libavutil/pixdesc.h>
}

#include <iostream>
#include <stdexcept>

namespace {

AVFramePtr make_avframe() {
  AVFrame *raw = av_frame_alloc();

  if (!raw) {
    throw std::runtime_error("Failed to allocate AVFrame");
  }

  return AVFramePtr(raw);
}

} // namespace

VideoDecoder::VideoDecoder(MediaInput &input, int stream_id)
    : input_(input), stream_id_(stream_id) {}

VideoDecoder::~VideoDecoder() { close(); }

void VideoDecoder::close() {
  if (packet_) {
    av_packet_free(&packet_);
    packet_ = nullptr;
  }

  if (codec_ctx_) {
    avcodec_free_context(&codec_ctx_);
    codec_ctx_ = nullptr;
  }

  codec_ = nullptr;
  reached_input_eof_ = false;
  decoder_flushed_ = false;
}

void VideoDecoder::open() {
  close();

  AVFormatContext *format_ctx = input_.format_context();
  const int video_stream_index = input_.video_stream_index();

  if (!format_ctx || video_stream_index < 0) {
    throw std::runtime_error(
        "VideoDecoder::open called with invalid MediaInput");
  }

  AVStream *video_stream = format_ctx->streams[video_stream_index];
  AVCodecParameters *codecpar = video_stream->codecpar;

  codec_ = avcodec_find_decoder(codecpar->codec_id);
  if (!codec_) {
    throw std::runtime_error("Failed to find video decoder");
  }

  codec_ctx_ = avcodec_alloc_context3(codec_);
  if (!codec_ctx_) {
    throw std::runtime_error("Failed to allocate video codec context");
  }

  int ret = avcodec_parameters_to_context(codec_ctx_, codecpar);
  if (ret < 0) {
    throw std::runtime_error("Failed to copy video codec parameters: " +
                             ffmpeg_error_string(ret));
  }

  ret = avcodec_open2(codec_ctx_, codec_, nullptr);
  if (ret < 0) {
    throw std::runtime_error("Failed to open video decoder: " +
                             ffmpeg_error_string(ret));
  }

  packet_ = av_packet_alloc();
  if (!packet_) {
    throw std::runtime_error("Failed to allocate AVPacket");
  }

  reached_input_eof_ = false;
  decoder_flushed_ = false;

  std::cout << "Opened video decoder: " << codec_->name << "\n";
}

double VideoDecoder::frame_pts_seconds(const AVFrame *frame) const {
  AVFormatContext *format_ctx = input_.format_context();
  AVStream *video_stream = format_ctx->streams[input_.video_stream_index()];

  int64_t ts = frame->best_effort_timestamp;

  if (ts == AV_NOPTS_VALUE) {
    ts = frame->pts;
  }

  if (ts == AV_NOPTS_VALUE) {
    return -1.0;
  }

  return static_cast<double>(ts) * av_q2d(video_stream->time_base);
}

VideoFrame VideoDecoder::make_video_frame(AVFramePtr frame) const {
  VideoFrame out;

  out.stream_id = stream_id_;
  out.pts_sec = frame_pts_seconds(frame.get());
  out.width = frame->width;
  out.height = frame->height;
  out.pix_fmt = static_cast<AVPixelFormat>(frame->format);
  out.frame = std::move(frame);

  return out;
}

AVFramePtr VideoDecoder::receive_frame() {
  auto frame = make_avframe();

  const int ret = avcodec_receive_frame(codec_ctx_, frame.get());

  if (ret == 0) {
    return frame;
  }

  if (ret == AVERROR(EAGAIN)) {
    return nullptr;
  }

  if (ret == AVERROR_EOF) {
    decoder_flushed_ = true;
    return nullptr;
  }

  throw std::runtime_error("Error while receiving video frame: " +
                           ffmpeg_error_string(ret));
}

void VideoDecoder::send_null_packet_to_flush_decoder() {
  if (reached_input_eof_) {
    return;
  }

  const int ret = avcodec_send_packet(codec_ctx_, nullptr);

  if (ret < 0) {
    throw std::runtime_error("Failed to flush video decoder: " +
                             ffmpeg_error_string(ret));
  }

  reached_input_eof_ = true;
}

std::optional<VideoFrame> VideoDecoder::decode_next_frame() {
  if (!codec_ctx_) {
    throw std::runtime_error(
        "VideoDecoder::decode_next_frame called before open");
  }

  if (decoder_flushed_) {
    return std::nullopt;
  }

  if (auto frame = receive_frame()) {
    return make_video_frame(std::move(frame));
  }

  AVFormatContext *format_ctx = input_.format_context();
  const int video_stream_index = input_.video_stream_index();

  while (!decoder_flushed_) {
    if (reached_input_eof_) {
      if (auto frame = receive_frame()) {
        return make_video_frame(std::move(frame));
      }

      return std::nullopt;
    }

    const int ret = av_read_frame(format_ctx, packet_);

    if (ret == AVERROR_EOF) {
      send_null_packet_to_flush_decoder();
      continue;
    }

    if (ret < 0) {
      throw std::runtime_error("Error while reading packet: " +
                               ffmpeg_error_string(ret));
    }

    if (packet_->stream_index != video_stream_index) {
      av_packet_unref(packet_);
      continue;
    }

    const int send_ret = avcodec_send_packet(codec_ctx_, packet_);
    av_packet_unref(packet_);

    if (send_ret == AVERROR(EAGAIN)) {
      if (auto frame = receive_frame()) {
        return make_video_frame(std::move(frame));
      }

      continue;
    }

    if (send_ret < 0) {
      throw std::runtime_error("Error while sending video packet to decoder: " +
                               ffmpeg_error_string(send_ret));
    }

    if (auto frame = receive_frame()) {
      return make_video_frame(std::move(frame));
    }
  }

  return std::nullopt;
}