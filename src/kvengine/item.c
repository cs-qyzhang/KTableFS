#include <stdlib.h>
#include <string.h>
#include "kvengine/kvengine.h"
#include "kvengine/item.h"

size_t kv_to_item(struct key* key, struct value* value, void** item) {
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

void item_to_kv(void* item, struct key* key, struct value* value) {
  if (key) {
    key->val = ((struct item_head*)item)->val;
    key->data = malloc(key->length);
    memcpy(key->data, (char*)item + sizeof(struct item_head), key->length);
  }
  if (value) {
    memcpy(value, (char*)item + sizeof(struct item_head) + ((struct item_head*)item)->length, sizeof(*value));
  }
}