#ifndef KTABLEFS_KVENGINE_KEY_H_
#define KTABLEFS_KVENGINE_KEY_H_

#include <sys/types.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

struct key {
  union {
    struct {
      u_int64_t dir_fd : 24;
      u_int64_t length : 8;
      u_int64_t hash   : 32;
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

struct item_head {
  int8_t valid;
  union {
    struct {
      u_int64_t dir_fd : 24;
      u_int64_t length : 8;
      u_int64_t hash   : 32;
    };
    int64_t val;
  };
};

typedef int (*key_cmp_t)(struct key*, struct key*);

static inline int key_comparator(struct key* a, struct key* b) {
  if (a->val == b->val) {
    return strcmp(a->data, b->data);
  } else {
    return (a->val - b->val);
  }
}

static inline size_t kv_to_item(struct key* key, struct value* value, void** item) {
  struct item_head head;
  head.val = key->val;
  head.valid = -1;
  size_t item_size;
  item_size = sizeof(head) + key->length + sizeof(*value);
  *item = malloc(item_size);
  memcpy(*item, &head, sizeof(head));
  memcpy(*item + sizeof(head), key->data, key->length);
  memcpy(*item + sizeof(head) + key->length, value, sizeof(*value));
  return item_size;
}

static inline void item_to_kv(void* item, struct key* key, struct value* value) {
  if (key) {
    key->val = ((struct item_head*)item)->val;
    key->data = malloc(key->length);
    memcpy(key->data, (char*)item + sizeof(struct item_head), key->length);
  }
  if (value) {
    memcpy(value, (char*)item + sizeof(struct item_head) + ((struct item_head*)item)->length, sizeof(*value));
  }
}

#endif // KTABLEFS_KVENGINE_KEY_H_