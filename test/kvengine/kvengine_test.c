#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "kv_impl.h"
#include "file.h"
#include "kvengine/kvengine.h"

void scan_func(void* key, void* value, void* scan_arg) {
  struct key* k = key;
  assert(k->val >= 100 && k->val <= 10000);
  *(int*)scan_arg += 1;
}

int main(void) {
  printf("%ld %ld %ld\n", sizeof(struct item_head), sizeof(struct stat), sizeof(struct value));
  struct kv_options* option = malloc(sizeof(*option));
  option->slab_dir = malloc(255);
  option->thread_nr = 1;
  system("mkdir -p /tmp/kv");
  sprintf(option->slab_dir, "%s", "/tmp/kv");

  kv_init(option);

  struct kv_request* request = malloc(sizeof(*request));
  request->key = malloc(sizeof(*request->key));
  request->value = malloc(sizeof(*request->value));
  memset(request->value, 0, sizeof(struct value));
  request->value->handle.file = malloc(sizeof(struct kfs_file));
  memset(request->value->handle.file, 0, sizeof(struct kfs_file));
  request->value->handle.file->stat.st_uid = 14;
  request->key->data = malloc(2);
  request->key->data[0] = 'a';
  request->key->data[1] = '\0';

  int sequence;
  struct kv_event* event;

  // PUT test
  for (int i = 0; i < 100000; ++i) {
    request->type = PUT;
    request->value->handle.file->stat.st_uid = i + 10;
    request->key->val = i;
    sequence = kv_submit(request);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
  }

  // SCAN test
  struct kv_request* scan_req = malloc(sizeof(*scan_req));
  memset(scan_req, 0, sizeof(*scan_req));
  scan_req->min_key = malloc(sizeof(*scan_req->min_key));
  scan_req->min_key->val = 100;
  scan_req->min_key->data = malloc(2);
  scan_req->min_key->data[0] = 'a';
  scan_req->min_key->data[1] = '\0';
  scan_req->max_key = malloc(sizeof(*scan_req->max_key));
  scan_req->max_key->val = 10000;
  scan_req->max_key->data = malloc(2);
  scan_req->max_key->data[0] = 'a';
  scan_req->max_key->data[1] = '\0';
  scan_req->scan = scan_func;
  scan_req->scan_arg = malloc(sizeof(int));
  scan_req->type = SCAN;
  *(int*)(scan_req->scan_arg) = 0;
  sequence = kv_submit(scan_req);
  event = kv_getevent(sequence);
  assert(event->return_code == (10000 - 100 + 1));
  assert(*(int*)(scan_req->scan_arg) == (10000 - 100 + 1));

  // UPDATE test
  for (int i = 0; i < 100000; i += 2) {
    request->type = UPDATE;
    request->value->handle.file->stat.st_uid = i + 20;
    request->key->val = i;
    sequence = kv_submit(request);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
  }

  // DELETE test
  for (int i = 1; i < 1000; i += 2) {
    request->type = DELETE;
    request->key->val = i;
    sequence = kv_submit(request);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
  }

  // GET test
  for (int i = 0; i < 100000; ++i) {
    request->type = GET;
    request->key->val = i;
    sequence = kv_submit(request);
    event = kv_getevent(sequence);
    if (i % 2 && i < 1000) {
      assert(event->return_code == -1);
    } else {
      assert(event->return_code == 0);
    }
    assert(event->sequence == sequence);
    if (i % 2) {
      if (i > 1000) {
        assert(event->value->handle.file->stat.st_uid == i + 10);
      }
    } else {
      assert(event->value->handle.file->stat.st_uid == i + 20);
    }
  }

  kv_destroy();
  return 0;
}