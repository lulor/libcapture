cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

# find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

# include_directories(${X11_INCLUDE_DIR})

# link_directories(${X11_LIBRARIES})

add_executable(screen-recorder
    src/main.cpp
    src/screen_recorder.cpp
    include/screen_recorder.h
)

target_link_libraries(screen-recorder
    PkgConfig::LIBAV
    # ${X11_LIBRARIES}
)