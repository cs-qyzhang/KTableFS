cmake_minimum_required(VERSION 2.8)

project(KTableFS)

# program configurations
set(KTableFS_VERSION_MAJOR 0)
set(KTableFS_VERSION_MINOR 1)
set(PAGE_SIZE 4096)
set(ARENA_NB_BLOCKS 256)
set(ARENA_BLOCK_SIZE PAGE_SIZE)
set(AIO_MAX_EVENTS 64)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Debug build.")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Release build.")
else()
  message(STATUS "Some other build type.")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
configure_file(
  "${PROJECT_SOURCE_DIR}/ktablefs_config.h.in"
  "${PROJECT_SOURCE_DIR}/include/ktablefs_config.h"
)

include(CheckIncludeFiles)
check_include_files(linux/aio_abi.h HAVE_AIO)
if(NOT HAVE_AIO)
  message(FATAL_ERROR "linux/aio_abi.h is not found!")
endif()

enable_testing()

function(test_unit unit file)
  add_executable("${unit}_test" "${PROJECT_SOURCE_DIR}/test/${file}_test.c" "${PROJECT_SOURCE_DIR}/src/${file}.c")
  foreach(src IN LISTS ARGN)
    target_sources("${unit}_test" PRIVATE "${PROJECT_SOURCE_DIR}/src/${src}.c")
  endforeach()
  add_test("${unit}_test" "${unit}_test")
endfunction(test_unit)

test_unit(arena arena)
test_unit(queue kvengine/queue)
target_link_libraries(queue_test pthread)
test_unit(btree kvengine/index/btree/btree arena)
test_unit(btree_index kvengine/index/btree/btree_index arena kvengine/index/btree/btree)