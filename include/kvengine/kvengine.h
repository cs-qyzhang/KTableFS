#ifndef KTABLEFS_KVENGINE_H_
#define KTABLEFS_KVENGINE_H_

#include <stddef.h>

struct key;
struct value;

struct kv_request {
  union {
    struct key* key;
    struct key* min_key;
  };
  struct key* max_key;
  struct value* value;
  size_t sequence;
  enum req_type {
    PUT,
    GET,
    UPDATE,
    DELETE,
    SCAN
  } type;
};

struct kv_event {
  struct value* value;
  size_t sequence;
  int return_code;
};

struct kv_options {
  char* slab_dir;
  int thread_nr;
};

void kv_init(struct kv_options* options);
struct kv_event* kv_getevent(int sequence);
int kv_submit(struct kv_request* request);
struct key* keydup(struct key* key);
struct value* valuedup(struct value* value);

// implemention dependent
size_t kv_to_item(struct key* key, struct value* value, void** item);
void item_to_kv(void* item, struct key** key, struct value** value);
int get_thread_index(struct key* key, int thread_nr);
int key_comparator(void* a, void* b);

#endif // KTABLEFS_KVENGINE_H_