#include <stdlib.h>
#include <string.h>
#include "kv_impl.h"
#include "util/memslab.h"
#include "kvengine/kvengine.h"

size_t kv_to_item(struct key* key, struct value* value, void* item) {
  struct item_head head;
  head.val = key->val;
  head.valid = -1;
  size_t item_size;
  item_size = sizeof(head) + key->length + sizeof(*value->handle.file);
  memcpy(item, &head, sizeof(head));
  if (key->length)
    memcpy(item + sizeof(head), key->data, key->length);
  memcpy(item + sizeof(head) + key->length, value->handle.file, sizeof(*value->handle.file));
  return item_size;
}

void item_to_value(void* item, struct value** value) {
  *value = malloc(sizeof(**value));
  (*value)->handle.file = malloc(sizeof(*(*value)->handle.file));
  memcpy((*value)->handle.file, ((char*)item) + sizeof(struct item_head) + ((struct item_head*)item)->length,
      sizeof(*(*value)->handle.file));
  (*value)->handle.big_file_fd = 0;

  struct key* key;
  key = malloc(sizeof(*key));
  key->val = ((struct item_head*)item)->val;
  if (key->length) {
    key->data = malloc(key->length + 1);
    memcpy(key->data, (char*)item + sizeof(struct item_head), key->length);
    key->data[key->length] = '\0';
  } else {
    key->data = NULL;
  }
  (*value)->handle.key = key;
}

static __thread struct memslab key_memslab = {
  .arena     = NULL,
  .freelist  = NULL,
  .slab_size = sizeof(struct key)};
static __thread struct memslab value_memslab = {
  .arena     = NULL,
  .freelist  = NULL,
  .slab_size=sizeof(struct value)};

struct key* keydup(struct key* key) {
  struct key* new = memslab_alloc(&key_memslab);
  if (key->data) {
    new->data = malloc(key->length + 1);
    memcpy(new->data, key->data, key->length);
    new->data[key->length] = '\0';
  } else {
    new->data = NULL;
  }
  new->val = key->val;
  return new;
}

void keydup_free(struct key* key) {
  if (key->data)
    free(key->data);
  memslab_free(&key_memslab, key);
}

struct value* valuedup(struct value* value) {
  struct value* new = memslab_alloc(&value_memslab);
  new->handle.file = malloc(sizeof(*new->handle.file));
  memcpy(new->handle.file, value->handle.file, sizeof(*new->handle.file));
  new->handle.big_file_fd = value->handle.big_file_fd;
  return new;
}

void valuedup_free(struct value* value) {
  free(value->handle.file);
  memslab_free(&value_memslab, value);
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