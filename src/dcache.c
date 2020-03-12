/*
 * dcache.c
 *
 * in-memory dentry cache
 *
 */
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "util/arena.h"
#include "util/index.h"
#include "util/hash.h"
#include "kv_impl.h"
#include "dcache.h"
#include "file.h"
#include "kvengine/kvengine.h"
#include "debug.h"

struct dentrycache {
  struct index* index;
};

struct dcache_key {
  union {
    struct {
      uint32_t hash;
      uint16_t length;
      uint16_t level;
    };
    uint64_t val;
  };
  char* path;
};

static struct dcache_key* path_to_key(const char* path, size_t len) {
  struct dcache_key* key = malloc(sizeof(*key));
  key->path = malloc(len + 1);
  memcpy(key->path, path, len);
  key->path[len] = '\0';
  key->hash = dir_name_hash(path, len);
  key->length = len;
  key->level = 0;
  char* end = (char*)path + (len - 1);
  while (end != path) {
    end--;
    if (*end == '/')
      key->level++;
  }
  return key;
}

int dcache_key_comparator(void* a, void* b) {
  struct dcache_key* key1 = a;
  struct dcache_key* key2 = b;
  if (key1->val == key2->val) {
    return memcmp(key1->path, key2->path, key1->length);
  } else {
    return (key1->val - key2->val);
  }
}

struct dentrycache* dcache_new() {
  struct dentrycache* dcache = malloc(sizeof(*dcache));
  dcache->index = index_new(dcache_key_comparator);
  return dcache;
}

void dcache_destroy(struct dentrycache* dcache) {
  index_destroy(dcache->index);
  free(dcache);
}

ino_t dcache_lookup(struct dentrycache* dcache, const char* dir, size_t len) {
  if (len == 1 && *dir == '/') {
    return 0;
  }

  char* end = (char*)dir + (len - 1);
  Assert(*end == '/');

  struct dcache_key* key = path_to_key(dir, len);
  void* val = index_lookup(dcache->index, key);
  if (val) {
    return (ino_t)val - 1;
  } else {
    char* pos = end - 1;
    while (*pos != '/')
      pos--;

    ino_t parent_dir = dcache_lookup(dcache, dir, (pos - dir) + 1);
    if (parent_dir == -1)
      return -1;

    struct kv_request* req = malloc(sizeof(*req));
    memset(req, 0, sizeof(*req));
    req->key = malloc(sizeof(*req->key));
    req->key->dir_ino = parent_dir;
    req->key->length = len - (pos - dir + 1) - 1;
    req->key->data = malloc(req->key->length + 1);
    memcpy(req->key->data, pos + 1, req->key->length);
    req->key->data[req->key->length] = '\0';
    req->key->hash = file_name_hash(pos + 1, req->key->length);
    req->type = GET;

    int sequence = kv_submit(req);
    free(req->key->data);
    free(req->key);
    free(req);
    struct kv_event* event = kv_getevent(sequence);
    Assert(event && event->return_code == 0);

    ino_t ino = file_ino(&event->value->handle);
    free(event);
    index_insert(dcache->index, key, (void*)(ino + 1));
    return ino;
  }
}