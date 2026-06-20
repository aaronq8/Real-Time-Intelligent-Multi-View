#pragma once

#include "MediaInput.h"
#include "VideoFrame.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <optional>

class VideoDecoder {
public:
  VideoDecoder(MediaInput &input, int stream_id);
  ~VideoDecoder();

  void open();

  // Returns next decoded video frame.
  // Returns std::nullopt when the stream reaches EOF.
  std::optional<VideoFrame> decode_next_frame();

  double frame_pts_seconds(const AVFrame *frame) const;

private:
  MediaInput &input_;
  int stream_id_ = -1;

  const AVCodec *codec_ = nullptr;
  AVCodecContext *codec_ctx_ = nullptr;
  AVPacket *packet_ = nullptr;

  bool reached_input_eof_ = false;
  bool decoder_flushed_ = false;

  void close();

  AVFramePtr receive_frame();
  void send_null_packet_to_flush_decoder();

  VideoFrame make_video_frame(AVFramePtr frame) const;
};