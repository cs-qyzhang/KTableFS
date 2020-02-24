#ifndef KTABLEFS_KVENGINE_KEY_H_
#define KTABLEFS_KVENGINE_KEY_H_

#include <sys/types.h>

/*
 * key-value pair stored in disk using following format:
 *   [item_head key-string value]
 */
struct item_head {
  // -1 means valid, 0 means deleted
  int8_t valid;
  union {
    struct {
      u_int64_t hash   : 32;  // LSB
      u_int64_t length : 8;
      u_int64_t dir_fd : 24;  // MSB
    };
    int64_t val;
  };
};

struct key;
struct value;

size_t kv_to_item(struct key* key, struct value* value, void** item);
void item_to_kv(void* item, struct key* key, struct value* value);

#endif // KTABLEFS_KVENGINE_KEY_H_