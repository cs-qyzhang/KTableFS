cmake_minimum_required(VERSION 2.8)

project(KTableFS)

# program configurations
set(KTableFS_VERSION_MAJOR 0)
set(KTableFS_VERSION_MINOR 2)
set(PAGE_SIZE 4096)
set(PAGECACHE_NR_PAGE 262144)
set(ARENA_NB_BLOCKS 1024)
set(ARENA_BLOCK_SIZE PAGE_SIZE)
set(AIO_MAX_EVENTS 64)
set(BTREE_ORDER 4)
set(KVENGINE_THREAD_NR 1)
set(AGGREGATION_SLAB_SIZE 8)     # unit: KB
set(AGGREGATION_SLAB_NR 8192)
set(AGGREGATION_HEADER_SIZE 4096) # (4+((AGGREGATION_SLAB_NR+7)/8)+BLOCK_SIZE-1)/BLOCK_SIZE*BLOCK_SIZE
set(INDEX_TYPE rbtree)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")

if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Debug build.")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Release build.")
else()
  message(STATUS "Some other build type.")
endif()

configure_file(
  "${PROJECT_SOURCE_DIR}/ktablefs_config.h.in"
  "${PROJECT_SOURCE_DIR}/include/ktablefs_config.h"
)

include(CheckIncludeFiles)
check_include_files("linux/aio_abi.h" HAVE_AIO)
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

# test_unit(arena util/arena)
# test_unit(queue util/queue)
# target_link_libraries(queue_test pthread)
# test_unit(btree util/index/btree/btree util/arena)
# test_unit(rbtree util/index/rbtree/rbtree util/arena util/freelist util/memslab util/hash_table)

# add_executable(aio_test "${PROJECT_SOURCE_DIR}/test/kvengine/aio_test.c")
# add_test(aio_test aio_test)

set(KVENGINE_SRC "${PROJECT_SOURCE_DIR}/lib/kvengine")
set(TEST_SRC "${PROJECT_SOURCE_DIR}/test")
set(UTIL_SRC "${PROJECT_SOURCE_DIR}/util")
set(FS_SRC "${PROJECT_SOURCE_DIR}/src")

include_directories("${KVENGINE_SRC}/include")
include_directories("${UTIL_SRC}/include")

add_library(kvengine STATIC
  "${KVENGINE_SRC}/aio.cc"
  "${KVENGINE_SRC}/batch.cc"
  "${KVENGINE_SRC}/db.cc"
  "${KVENGINE_SRC}/fileslab.cc"
  "${KVENGINE_SRC}/local_fileset.cc"
  "${KVENGINE_SRC}/pagecache.cc"
  "${KVENGINE_SRC}/worker.cc"
  "${UTIL_SRC}/hash.cc"
)
target_link_libraries(kvengine pthread)

add_executable(ktablefs
  "${FS_SRC}/main.cc"
  "${FS_SRC}/ktablefs.cc"
  "${FS_SRC}/file.cc"
  "${FS_SRC}/file_data.cc"
  "${FS_SRC}/file_key.cc"

  "${FS_SRC}/lookup.cc"
  "${FS_SRC}/mknod.cc"
  "${FS_SRC}/read_write.cc"
  "${FS_SRC}/open.cc"
  "${FS_SRC}/attr.cc"
  "${FS_SRC}/dir.cc"
)
target_link_libraries(ktablefs kvengine fuse3)

add_executable(kvengine_test "${TEST_SRC}/kvengine_test.cc")
target_link_libraries(kvengine_test kvengine pthread)
add_test(kvengine_test kvengine_test)

# add_executable(ktablefs_ll "${PROJECT_SOURCE_DIR}/src/ktablefs_ll.c")
# target_sources(ktablefs_ll
#   PRIVATE
#     "${PROJECT_SOURCE_DIR}/src/io.c"
#     "${PROJECT_SOURCE_DIR}/src/kv_impl.c"
#     "${PROJECT_SOURCE_DIR}/src/kvengine/kvengine.c"
#     "${PROJECT_SOURCE_DIR}/src/kvengine/pagecache.c"
#     "${PROJECT_SOURCE_DIR}/src/kvengine/slab.c"
#     "${PROJECT_SOURCE_DIR}/util/hash.c"
#     "${PROJECT_SOURCE_DIR}/util/arena.c"
#     "${PROJECT_SOURCE_DIR}/util/freelist.c"
#     "${PROJECT_SOURCE_DIR}/util/queue.c"
#     "${PROJECT_SOURCE_DIR}/util/memslab.c"
#     "${PROJECT_SOURCE_DIR}/util/hash_table.c"
# )
# if (INDEX_TYPE MATCHES rbtree)
#   target_sources(ktablefs_ll
#     PRIVATE
#       "${PROJECT_SOURCE_DIR}/src/kvengine/rbtree_index.c"
#       "${PROJECT_SOURCE_DIR}/util/index/rbtree/rbtree.c"
#   )
# elseif (INDEX_TYPE MATCHES btree)
#   target_sources(ktablefs_ll
#     PRIVATE
#       "${PROJECT_SOURCE_DIR}/src/kvengine/btree_index.c"
#       "${PROJECT_SOURCE_DIR}/util/index/btree/btree.c"
#   )
# else()
#   message(ERROR "Wrong INDEX TYPE!")
# endif()
# target_link_libraries(ktablefs_ll pthread fuse3)

# add_executable(ext4-fuse "${PROJECT_SOURCE_DIR}/src/ext4_fuse.c")
# target_link_libraries(ext4-fuse pthread fuse3)