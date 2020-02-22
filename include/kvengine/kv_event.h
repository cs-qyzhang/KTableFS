#ifndef KTABLEFS_KVENGINE_KV_EVENT_H_
#define KTABLEFS_KVENGINE_KV_EVENT_H_

#include "kvengine/key.h"

struct kv_event {
  struct value value;
  size_t sequence;
  int return_code;
};

#endif // KTABLEFS_KVENGINE_KV_EVENT_H_