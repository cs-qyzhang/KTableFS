/*
 * dcache.c
 *
 * in-memory dentry cache
 *
 */
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "util/arena.h"
#include "util/index.h"
#include "kvengine/kvengine.h"
#include "dcache.h"

struct dentrycache {
  struct index* index;
};

ino_t dcache_lookup(struct dentrycache* dcache, const char* dir, size_t len) {
  if (len == 1 && *dir == '/') {
    return 0;
  }

  char* end = dir + len - 1;
  Assert(*end == '/');

  void* val = index_lookup(dcache->index, dir_hash);
  if (val) {
    return (ino_t)val;
  } else {
    char* pos = end;
    while (*pos != '/') {
      pos--;
    }

    ino_t parent_ino = dcache_lookup(dcache, dir, pos - dir + 1);

    struct kv_request* req = malloc(sizeof(*req));
    req->key.dir_ino = parent_ino;
    req->key.length = len - (pos - dir + 1) - 1;
    req->key.data = malloc(req->key.length + 1);
    memcpy(req->key.data, pos + 1, req->key.length);
    req->key.data[req->key.length - 1] = '\0';
    req->key.hash = file_name_hash(pos + 1, req->key.length);
    req->type = GET;

    int sequence = kv_submit(req);
    struct kv_event* event = kv_getevent(sequence);
    Assert(event && event->return_code == 0);
    ino_t ino = event->value.stat.st_ino;
    index_insert(dcache->index, dir_hash, (void*)ino);
    return ino;
  }
}