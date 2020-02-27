#ifndef KTABLEFS_KV_IMPL_H_
#define KTABLEFS_KV_IMPL_H_

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include "file.h"

struct key {
  union {
    struct {
      uint64_t hash    : 32;
      uint64_t length  : 8;
      uint64_t dir_ino : 24;
    };
    int64_t val;
  };
  char* data;
};

struct value {
  struct kfs_file_handle file;
};

struct item_head {
  // -1 means valid, 0 means deleted
  int8_t valid;
  union {
    struct {
      uint64_t hash    : 32;  // LSB
      uint64_t length  : 8;
      uint64_t dir_ino : 24;  // MSB
    };
    int64_t val;
  };
};

#endif // KTABLEFS_KV_IMPL_H_