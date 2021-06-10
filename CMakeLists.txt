# Copyright(c) 2019 spdlog authors Distributed under the MIT License (http://opensource.org/licenses/MIT)

cmake_minimum_required(VERSION 3.10)
project(srt_live CXX)

# Use threads library
find_package(Threads)

# Use SLSCore
include_directories(slscore)
link_directories(slscore)
add_subdirectory(slscore)

add_executable(srt_server srt-live-server.cpp)
target_include_directories(srt_server PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/slscore>)
target_link_libraries(srt_server
    slscore
    -lspdlog
    -lsrt
    ${CMAKE_THREAD_LIBS_INIT}
)

add_executable(srt_client srt-live-client.cpp)
target_include_directories(srt_client PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/slscore>)
target_link_libraries(srt_client
    slscore
    -lspdlog
    -lsrt
    ${CMAKE_THREAD_LIBS_INIT}
)