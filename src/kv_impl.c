#include <stdlib.h>
#include <string.h>
#include "kv_impl.h"
#include "kvengine/kvengine.h"

size_t kv_to_item(struct key* key, struct value* value, void** item) {
  struct item_head head;
  head.val = key->val;
  head.valid = -1;
  size_t item_size;
  item_size = sizeof(head) + key->length + sizeof(*(value->handle.file));
  *item = malloc(item_size);
  memcpy(*item, &head, sizeof(head));
  memcpy(*item + sizeof(head), key->data, key->length);
  memcpy(*item + sizeof(head) + key->length, value->handle.file, sizeof(*(value->handle.file)));
  return item_size;
}

void item_to_kv(void* item, struct key** key, struct value** value) {
  if (key) {
    *key = malloc(sizeof(**key));
    (*key)->val = ((struct item_head*)item)->val;
    (*key)->data = malloc((*key)->length + 1);
    memcpy((*key)->data, (char*)item + sizeof(struct item_head), (*key)->length);
    (*key)->data[(*key)->length] = '\0';
  }
  if (value) {
    *value = malloc(sizeof(**value));
    (*value)->handle.file = (struct kfs_file*)((char*)item +
        (sizeof(struct item_head) + ((struct item_head*)item)->length));
    (*value)->handle.big_file_fd = 0;
  }
}

struct key* keydup(struct key* key) {
  struct key* new = malloc(sizeof(*new));
  new->data = malloc(key->length + 1);
  memcpy(new->data, key->data, key->length);
  new->data[key->length] = '\0';
  new->val = key->val;
  return new;
}

struct value* valuedup(struct value* value) {
  struct value* new = malloc(sizeof(*new));
  new->handle.file = malloc(sizeof(*(new->handle.file)));
  memcpy(new->handle.file, value->handle.file, sizeof(*(new->handle.file)));
  new->handle.big_file_fd = value->handle.big_file_fd;
  return new;
}

int get_thread_index(struct key* key, int thread_nr) {
  return key->dir_ino % thread_nr;
}

int key_comparator(void* a, void* b) {
  struct key* key1 = a;
  struct key* key2 = b;
  if (key1->val == key2->val) {
    return memcmp(key1->data, key2->data, key1->length);
  } else {
    return (key1->val < key2->val) ? -1 : 1;
  }
}