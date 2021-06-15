cmake_minimum_required(VERSION 3.0.0)
project(srt-live-server VERSION 1.5.0)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-Wall -Wno-invalid-offsetof")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_compile_definitions( SLS_VERSION="${PROJECT_VERSION}" )

# Use spdlog
add_subdirectory(lib/spdlog)
find_package(spdlog)

# Use threads library
find_package(Threads)

# Use SLSCore
include_directories(slscore)
add_subdirectory(slscore)

add_executable(srt_server srt-live-server.cpp)
target_link_libraries(srt_server
        slscore
        spdlog
        -lsrt
        ${CMAKE_THREAD_LIBS_INIT}
)

add_executable(srt_client srt-live-client.cpp)
target_link_libraries(srt_client
        slscore
        spdlog
        -lsrt
        ${CMAKE_THREAD_LIBS_INIT}
)
