cmake_minimum_required(VERSION 3.0.0)
project(srt-live-server VERSION 1.5.1)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-Wall -Wno-invalid-offsetof")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -s")

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_compile_definitions( SLS_VERSION="${PROJECT_VERSION}" )

# Use spdlog
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/spdlog)
find_package(spdlog)

# Use JSON library
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/json)

# Use threads library
find_package(Threads)

# Use SLS Core
add_subdirectory(${PROJECT_SOURCE_DIR}/src)
