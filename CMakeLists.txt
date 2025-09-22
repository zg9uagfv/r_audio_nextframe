cmake_minimum_required(VERSION 3.10)
project(r_audio_nextframe)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set local FFmpeg paths
set(FFMPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/ffmpeg-n7.1)
set(FFMPEG_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Create executables
add_executable(r_audio_nextframe
    src/main.cpp
    src/AudioProcessor.cpp
    src/FrameReader.cpp
    src/FrameDecoder.cpp
    src/Resampler.cpp
    src/FrameEncoder.cpp
)

add_executable(udp_server
    src/udp_server_main.cpp
    src/UdpServer.cpp
)

target_include_directories(r_audio_nextframe PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    src
)

target_link_libraries(r_audio_nextframe
    ${FFMPEG_LIB_DIR}/libavutil.so
    ${FFMPEG_LIB_DIR}/libavcodec.so
    ${FFMPEG_LIB_DIR}/libavformat.so
    ${FFMPEG_LIB_DIR}/libavfilter.so
    ${FFMPEG_LIB_DIR}/libavdevice.so
    ${FFMPEG_LIB_DIR}/libswresample.so
)

target_include_directories(udp_server PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    src
)

target_link_libraries(udp_server
    ${FFMPEG_LIB_DIR}/libavutil.so
    ${FFMPEG_LIB_DIR}/libavcodec.so
    ${FFMPEG_LIB_DIR}/libavformat.so
)
install(DIRECTORY DESTINATION ${CMAKE_SOURCE_DIR}/installed/bin)
install(TARGETS r_audio_nextframe udp_server DESTINATION ${CMAKE_SOURCE_DIR}/installed/bin)
