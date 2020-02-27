#include <stdlib.h>
#include <string.h>
#include "kv_impl.h"
#include "kvengine/kvengine.h"

size_t kv_to_item(struct key* key, struct value* value, void** item) {
  struct item_head head;
  head.val = key->val;
  head.valid = -1;
  size_t item_size;
  item_size = sizeof(head) + key->length + sizeof(*value);
  *item = malloc(item_size);
  memcpy(*item, &head, sizeof(head));
  memcpy(*item + sizeof(head), key->data, key->length);
  memcpy(*item + sizeof(head) + key->length, &(value->file.file), sizeof(value->file.file));
  return item_size;
}

void item_to_kv(void* item, struct key* key, struct value* value) {
  if (key) {
    key->val = ((struct item_head*)item)->val;
    key->data = malloc(key->length);
    memcpy(key->data, (char*)item + sizeof(struct item_head), key->length);
  }
  if (value) {
    memcpy(value, (char*)item + sizeof(struct item_head) + ((struct item_head*)item)->length, sizeof(value->file.file));
    value->file.aggregation_file_fd = 0;
    value->file.big_file_fd = 0;
  }
}

int get_thread_index(struct key* key, int thread_nr) {
  return key->dir_ino % thread_nr;
}

int key_comparator(void* a, void* b) {
  struct key* key1 = a;
  struct key* key2 = b;
  if (key1->val == key2->val) {
    return strcmp(key1->data, key2->data);
  } else {
    return (key1->val - key2->val);
  }
}

size_t key_size() {
  return sizeof(struct key);
}

size_t value_size() {
  return sizeof(struct value);
}