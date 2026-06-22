#include "AudioFrameConverter.h"

#include <stdexcept>

std::optional<AudioChunk>
convert_audio_frame_to_mono_float(const AudioFrame &audio_frame) {
  if (!audio_frame.frame) {
    return std::nullopt;
  }

  if (audio_frame.sample_rate <= 0 || audio_frame.nb_samples <= 0) {
    return std::nullopt;
  }

  if (audio_frame.pts_sec < 0.0) {
    return std::nullopt;
  }

  if (audio_frame.channels <= 0) {
    return std::nullopt;
  }

  if (audio_frame.sample_fmt != AV_SAMPLE_FMT_FLTP) {
    throw std::runtime_error("convert_audio_frame_to_mono_float currently only "
                             "supports AV_SAMPLE_FMT_FLTP");
  }

  const AVFrame *frame = audio_frame.frame.get();

  AudioChunk chunk;
  chunk.stream_id = audio_frame.stream_id;
  chunk.start_sec = audio_frame.pts_sec;
  chunk.sample_rate = audio_frame.sample_rate;
  chunk.duration_sec = static_cast<double>(audio_frame.nb_samples) /
                       static_cast<double>(audio_frame.sample_rate);

  chunk.mono_samples.resize(audio_frame.nb_samples);

  if (audio_frame.channels == 1) {
    const float *mono = reinterpret_cast<const float *>(frame->data[0]);

    for (int i = 0; i < audio_frame.nb_samples; ++i) {
      chunk.mono_samples[i] = mono[i];
    }

    return chunk;
  }

  const float *left = reinterpret_cast<const float *>(frame->data[0]);
  const float *right = reinterpret_cast<const float *>(frame->data[1]);

  for (int i = 0; i < audio_frame.nb_samples; ++i) {
    chunk.mono_samples[i] = 0.5f * (left[i] + right[i]);
  }

  return chunk;
}