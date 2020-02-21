#ifndef KTABLEFS_KVENGINE_KV_EVENT_H_
#define KTABLEFS_KVENGINE_KV_EVENT_H_

struct kv_event {
  void* value;
  size_t value_size;
  size_t sequence;
  int return_code;
};

#endif // KTABLEFS_KVENGINE_KV_EVENT_H_