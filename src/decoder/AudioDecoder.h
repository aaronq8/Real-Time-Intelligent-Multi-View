#pragma once

#include "AudioFrame.h"
#include "MediaInput.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <optional>

class AudioDecoder {
public:
  AudioDecoder(MediaInput &input, int stream_id);
  ~AudioDecoder();

  AudioDecoder(const AudioDecoder &) = delete;
  AudioDecoder &operator=(const AudioDecoder &) = delete;

  AudioDecoder(AudioDecoder &&) = delete;
  AudioDecoder &operator=(AudioDecoder &&) = delete;

  void open();

  std::optional<AudioFrame> decode_next_frame();

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

  AudioFrame make_audio_frame(AVFramePtr frame) const;
};