cmake_minimum_required(VERSION 3.18)
project(srt_live_server_project CXX)

# Use threads library
find_package(Threads)

find_package(spdlog CONFIG REQUIRED)

# Use SLSCore
include_directories(slscore)
link_directories(slscore)
add_subdirectory(slscore)

add_executable(srt_server srt-live-server.cpp)
target_include_directories(srt_server PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/slscore>)
target_link_libraries(srt_server
	slscore
	spdlog
	-lsrt
	${CMAKE_THREAD_LIBS_INIT}
)

add_executable(srt_client srt-live-client.cpp)
target_include_directories(srt_client PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/slscore>)
target_link_libraries(srt_client
	slscore
	spdlog
	-lsrt
	${CMAKE_THREAD_LIBS_INIT}
)
