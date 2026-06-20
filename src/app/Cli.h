#pragma once

#include "AppConfig.h"

enum class ParseResult { Ok, Help, Error };

ParseResult parse_args(int argc, char **argv, AppConfig &config);