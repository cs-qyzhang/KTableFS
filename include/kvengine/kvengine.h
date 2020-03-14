#ifndef KTABLEFS_KVENGINE_H_
#define KTABLEFS_KVENGINE_H_

#include <stddef.h>
#include "util/index.h"

struct key;
struct value;

struct kv_respond {
  struct value* value;
  int res;
};

typedef void (*kv_callback_t)(void** userdata, struct kv_respond* respond);

struct kv_request {
  union {
    struct key* key;
    struct key* min_key;
  };
  struct key* max_key;
  struct value* value;
  kv_callback_t callback;
  void** userdata;
  scan_t scan;
  void** scan_arg;
  enum req_type {
    PUT,
    GET,
    UPDATE,
    DELETE,
    SCAN
  } type;
};

struct kv_options {
  char* slab_dir;
  int thread_nr;
};

void kv_init(struct kv_options* options);
void kv_destroy();
void kv_submit(struct kv_request* requests, int nr);

struct kv_batch;
void kv_finish(struct kv_batch* batch, struct kv_respond* respond);

// implemention dependent
size_t kv_to_item(struct key* key, struct value* value, void* item);
void item_to_value(void* item, struct value** value);
int get_thread_index(struct key* key, int thread_nr);
int key_comparator(void* a, void* b);
struct key* keydup(struct key* key);
struct value* valuedup(struct value* value);
void keydup_free(struct key* key);
void valuedup_free(struct value* value);

#endif // KTABLEFS_KVENGINE_H_