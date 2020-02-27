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

void io_worker_init(struct option* option);
void io_worker_destroy();

int main(void) {
  printf("%ld %ld %ld\n", sizeof(struct item_head), sizeof(struct stat), sizeof(struct value));
  struct option* option = malloc(sizeof(*option));
  option->slab_dir = malloc(255);
  option->thread_nr = 1;
  system("mkdir -p /tmp/kv");
  sprintf(option->slab_dir, "%s", "/tmp/kv");

  io_worker_init(option);

  struct kv_request* request = malloc(sizeof(*request));
  request->key = malloc(sizeof(*request->key));
  request->value = malloc(sizeof(*request->value));
  memset(request->value, 0, sizeof(*request->value));
  request->value->file.file.stat.st_uid = 14;
  request->key->data = malloc(2);
  request->key->data[0] = 'a';
  request->key->data[1] = '\0';

  int sequence;
  struct kv_event* event;

  for (int i = 0; i < 100000; ++i) {
    request->type = PUT;
    request->value->file.file.stat.st_uid = i + 10;
    request->key->val = i;
    sequence = kv_submit(request);
    // printf("add submit sequence: %d\n", sequence);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
    // printf("add return: %d, sequence: %ld, st_uid: %d\n", event->return_code, event->sequence, event->value.stat.st_uid);
  }

  for (int i = 0; i < 100000; i += 2) {
    request->type = UPDATE;
    request->value->file.file.stat.st_uid = i + 20;
    request->key->val = i;
    sequence = kv_submit(request);
    // printf("add submit sequence: %d\n", sequence);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
    // printf("add return: %d, sequence: %ld, st_uid: %d\n", event->return_code, event->sequence, event->value.stat.st_uid);
  }

  for (int i = 1; i < 1000; i += 2) {
    request->type = DELETE;
    request->key->val = i;
    sequence = kv_submit(request);
    // printf("add submit sequence: %d\n", sequence);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
  }

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
        assert(event->value->file.file.stat.st_uid == i + 10);
      }
    } else {
      assert(event->value->file.file.stat.st_uid == i + 20);
    }
  }

  io_worker_destroy();
  return 0;
}