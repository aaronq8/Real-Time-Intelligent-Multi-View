cmake_minimum_required(VERSION 3.10.0)
project(RT-IMV VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS program_options)
find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
)

pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(RT-IMV 
    src/main.cpp
    src/app/Cli.cpp
    src/media/FfmpegUtil.cpp
    src/media/MediaInput.cpp
    src/decoder/VideoDecoder.cpp
    src/decoder/AudioDecoder.cpp
    src/decoder/VideoDecodeWorker.cpp
    src/render/SdlRenderer.cpp
    src/decoder/AudioExcitementScorer.cpp
    src/app/BigStreamSelector.cpp
    src/decoder/AudioFrameConverter.cpp
    src/decoder/AudioDecodeWorker.cpp
    src/vision/VisualFeatureExtractor.cpp)

target_include_directories(RT-IMV PRIVATE 
    src/app
    src/concurrency
    src/decoder
    src/media
    src/render
    src/sync
    src/vision)



target_link_libraries(RT-IMV
    PRIVATE
        Boost::program_options
        PkgConfig::FFMPEG
        PkgConfig::SDL2
)