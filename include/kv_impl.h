#ifndef KTABLEFS_KV_IMPL_H_
#define KTABLEFS_KV_IMPL_H_

#include <stdint.h>
#include "file.h"

struct key {
  union {
    struct {
      uint64_t hash    : 32;
      uint64_t length  : 8;
      uint64_t dir_ino : 24;
    };
    uint64_t val;
  };
  char* data;
};

struct value {
  struct kfs_file_handle handle;
};

struct item_head {
  // -1 means valid, 0 means deleted
  int64_t valid;
  union {
    struct {
      uint64_t hash    : 32;  // LSB
      uint64_t length  : 8;
      uint64_t dir_ino : 24;  // MSB
    };
    uint64_t val;
  };
};

#endif // KTABLEFS_KV_IMPL_H_