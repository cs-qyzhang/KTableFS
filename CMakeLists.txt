cmake_minimum_required(VERSION 2.8)

project(KTableFS)

# program configurations
set(KTableFS_VERSION_MAJOR 0)
set(KTableFS_VERSION_MINOR 1)
set(PAGE_SIZE 4096)
set(ARENA_NB_BLOCKS 256)
set(ARENA_BLOCK_SIZE PAGE_SIZE)
set(AIO_MAX_EVENTS 64)
set(BTREE_ORDER 16)
set(FILE_AGGREGATION_SIZE "(1024 * 1024)")
set(AGGREGATION_FILE_SIZE "(1024 * 1024 * 1024)")

set(CMAKE_BUILD_TYPE Debug)
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
  add_executable("${unit}_test" "${PROJECT_SOURCE_DIR}/test/${file}_test.c" "${PROJECT_SOURCE_DIR}/${file}.c")
  foreach(src IN LISTS ARGN)
    target_sources("${unit}_test" PRIVATE "${PROJECT_SOURCE_DIR}/${src}.c")
  endforeach()
  add_test("${unit}_test" "${unit}_test")
endfunction(test_unit)

test_unit(arena util/arena)
test_unit(queue util/queue)
target_link_libraries(queue_test pthread)
test_unit(btree util/index/btree/btree util/arena)

add_executable(kvengine_test)
target_sources(kvengine_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/test/kvengine/kvengine_test.c"
    "${PROJECT_SOURCE_DIR}/src/kvengine/io_worker.c"
    "${PROJECT_SOURCE_DIR}/src/kvengine/pagecache.c"
    "${PROJECT_SOURCE_DIR}/src/kvengine/slab.c"
    "${PROJECT_SOURCE_DIR}/src/kvengine/btree_index.c"
    "${PROJECT_SOURCE_DIR}/src/kv_impl.c"
    "${PROJECT_SOURCE_DIR}/util/hash.c"
    "${PROJECT_SOURCE_DIR}/util/freelist.c"
    "${PROJECT_SOURCE_DIR}/util/queue.c"
    "${PROJECT_SOURCE_DIR}/util/index/btree/btree.c"
    "${PROJECT_SOURCE_DIR}/util/arena.c"
)
target_link_libraries(kvengine_test pthread)
add_test(kvengine_test kvengine_test)

add_executable(aio_test "${PROJECT_SOURCE_DIR}/test/kvengine/aio_test.c")
add_test(aio_test aio_test)

add_executable(fuse_test "${PROJECT_SOURCE_DIR}/test/fuse_test.c")
target_link_libraries(fuse_test fuse3)
#add_test(fuse_test fuse_test)