#ifndef KTABLEFS_KVENGINE_KV_REQUEST_H_
#define KTABLEFS_KVENGINE_kV_REQUEST_H_

#include <sys/types.h>
#include "kvengine/key.h"

struct kv_request {
  union {
    struct key* key;
    struct key* min_key;
  };
  struct key* max_key;
  void* value;
  size_t value_size;
  size_t sequence;
  enum req_type {
    ADD,
    GET,
    UPDATE,
    DELETE,
    SCAN
  } type;
};

#endif // KTABLEFS_KVENGINE_KV_REQUEST_H_