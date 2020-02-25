#ifndef KTABLEFS_KVENGINE_H_
#define KTABLEFS_KVENGINE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

struct key {
  union {
    struct {
      uint64_t hash   : 32;
      uint64_t length : 8;
      uint64_t dir_fd : 24;
    };
    int64_t val;
  };
  char* data;
};

struct value {
  union {
    struct stat stat;
  };
};

struct kv_request {
  union {
    struct key key;
    struct key min_key;
  };
  struct key max_key;
  struct value value;
  size_t sequence;
  enum req_type {
    ADD,
    GET,
    UPDATE,
    DELETE,
    SCAN
  } type;
};

struct kv_event {
  struct value value;
  size_t sequence;
  int return_code;
};

struct option {
  char* slab_dir;
  int thread_nr;
};

struct kv_event* kv_getevent(int sequence);
int kv_submit(struct kv_request* request);

#endif // KTABLEFS_KVENGINE_H_