#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include "kvengine/queue.h"

struct queue* que;
pthread_t enqueue_thread, dequeue_thread;

struct data {
  int seq;
};

static inline void nop() {
  for (volatile int i = 0; i < 1000; ++i) ;
}

void* enqueue_test(void* arg) {
  assert(queue_empty(que));
  for (int i = 0; i < 1000; ++i) {
    struct data* d = malloc(sizeof(*d));
    d->seq = i;
    int try = 1;
    while (enqueue(que, d)) {
      printf("  Enqueue fail %d time! try again\n", try++);
      nop();
    }
  }
  return NULL;
}

void* dequeue_test(void* arg) {
  for (int i = 0; i < 1000; ++i) {
    struct data* d;
    d = dequeue(que);
    int try = 1;
    while (d == NULL) {
      printf("    Dequeue fail %d time! try again\n", try++);
      nop();
      d = dequeue(que);
    }
    printf("dequeue data: %d\n", d->seq);
    assert(d->seq == i);
  }
  assert(queue_empty(que));
  return NULL;
}

int main(void) {
  que = queue_new(20);
  pthread_create(&enqueue_thread, NULL, enqueue_test, NULL);
  pthread_create(&dequeue_thread, NULL, dequeue_test, NULL);
  pthread_join(enqueue_thread, NULL);
  pthread_join(dequeue_thread, NULL);
  queue_destroy(que);
  return 0;
}