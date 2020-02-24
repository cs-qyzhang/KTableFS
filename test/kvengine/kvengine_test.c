#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kvengine/kvengine.h"

void io_worker_init(struct option* option);
void io_worker_destroy();

int main(void) {
  struct option* option = malloc(sizeof(*option));
  option->slab_dir = malloc(255);
  option->thread_nr = 1;
  system("mkdir -p /tmp/kv");
  sprintf(option->slab_dir, "%s", "/tmp/kv");

  io_worker_init(option);

  struct kv_request* request = malloc(sizeof(*request));
  memset(&request->value, 0, sizeof(request->value));
  request->value.stat.st_uid = 14;
  request->key.data = malloc(2);
  request->key.data[0] = 'a';
  request->key.data[1] = '\0';

  int sequence;
  struct kv_event* event;

  for (int i = 0; i < 100000; ++i) {
    request->type = ADD;
    request->value.stat.st_uid = i + 10;
    request->key.val = i;
    sequence = kv_submit(request);
    // printf("add submit sequence: %d\n", sequence);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
    // printf("add return: %d, sequence: %ld, st_uid: %d\n", event->return_code, event->sequence, event->value.stat.st_uid);
  }

  for (int i = 0; i < 100000; ++i) {
    request->type = GET;
    request->key.val = i;
    sequence = kv_submit(request);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
    assert(event->value.stat.st_uid == i + 10);
  }

  for (int i = 0; i < 100000; ++i) {
    request->type = GET;
    request->key.val = i;
    sequence = kv_submit(request);
    event = kv_getevent(sequence);
    assert(event->return_code == 0);
    assert(event->sequence == sequence);
    assert(event->value.stat.st_uid == i + 10);
  }

  io_worker_destroy();
  return 0;
}