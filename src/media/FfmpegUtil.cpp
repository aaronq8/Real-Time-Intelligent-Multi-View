#include "FfmpegUtil.h"

extern "C" {
#include <libavutil/error.h>
}

std::string ffmpeg_error_string(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {};
  av_strerror(errnum, errbuf, sizeof(errbuf));
  return std::string(errbuf);
}