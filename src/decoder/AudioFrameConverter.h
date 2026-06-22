#pragma once

#include "AudioChunk.h"
#include "AudioFrame.h"

#include <optional>

std::optional<AudioChunk>
convert_audio_frame_to_mono_float(const AudioFrame &audio_frame);