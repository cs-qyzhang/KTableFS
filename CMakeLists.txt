cmake_minimum_required(VERSION 2.8)

project(KTableFS)
set(KTableFS_VERSION_MAJOR 0)
set(KTableFS_VERSION_MINOR 1)
set(CMAKE_C_STANDARD 11)

include_directories(${PROJECT_SOURCE_DIR}/include)
configure_file(
  "${PROJECT_SOURCE_DIR}/ktablefs_config.h.in"
  "${PROJECT_SOURCE_DIR}/include/ktablefs_config.h"
)

add_definitions(-DDEBUG)
add_executable(arena_test ${PROJECT_SOURCE_DIR}/test/arena_test.c ${PROJECT_SOURCE_DIR}/src/arena.c)
enable_testing()
add_test(arena_test arena_test)