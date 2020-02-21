#ifndef KTABLEFS_KVENGINE_KV_REQUEST_H_
#define KTABLEFS_KVENGINE_kV_REQUEST_H_

#include <sys/types.h>

struct kv_request {
  void* key;
  void* value;
  size_t key_size;
  size_t value_size;
  size_t sequence;
  enum req_type {
    ADD,
    UPDATE,
    DELETE
  } type;
};

#endif // KTABLEFS_KVENGINE_KV_REQUEST_H_