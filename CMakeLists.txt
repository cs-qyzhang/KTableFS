cmake_minimum_required(VERSION 2.8)

project(KTableFS)
set(KTableFS_VERSION_MAJOR 1)
set(KTableFS_VERSION_MINOR 0)

# KTableFS configurations
set(PAGECACHE_NR_PAGE "1024 * 256")
set(AIO_MAX_EVENTS 64)
set(KVENGINE_THREAD_NR 4)
set(FILEDATA_SLAB_SIZE_KB 8)

# build type: Release/Debug
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")

if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Debug build.")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
  message(STATUS "Release build.")
else()
  message(STATUS "Some other build type.")
endif()

set(KVENGINE_SRC "${PROJECT_SOURCE_DIR}/lib/kvengine")
set(TEST_SRC     "${PROJECT_SOURCE_DIR}/test")
set(UTIL_SRC     "${PROJECT_SOURCE_DIR}/util")
set(FS_SRC       "${PROJECT_SOURCE_DIR}/src")

configure_file(
  "${KVENGINE_SRC}/kvengine_config.h.in"
  "${KVENGINE_SRC}/kvengine_config.h"
)

configure_file(
  "${FS_SRC}/ktablefs_config.h.in"
  "${FS_SRC}/ktablefs_config.h"
)

include(CheckIncludeFiles)
check_include_files("linux/aio_abi.h" HAVE_AIO)
if(NOT HAVE_AIO)
  message(FATAL_ERROR "linux/aio_abi.h is not found!")
endif()

enable_testing()

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