#include <stdlib.h>
#include <assert.h>
#include "util/queue.h"

// TODO
#define die(x)  _exit(-1)

struct queue* queue_new(size_t max_size) {
  assert(max_size > 1);
  struct queue* que = malloc(sizeof(struct queue));
  que->max_size_ = max_size;
  que->que_ = malloc(sizeof(void*) * max_size);
  que->head_ = 0;
  que->tail_ = 0;
  return que;
}

void queue_destroy(struct queue* que) {
  free(que->que_);
  free(que);
}

void* dequeue(struct queue* que) {
  if (queue_empty(que)) {
    return NULL;
  }
  void* ret = que->que_[que->tail_];
  que->tail_ = (que->tail_ + 1) % que->max_size_;
  return ret;
}

static inline void nop() {
  for (volatile int i = 0; i < 1000; ++i) ;
}

/*
 * @data pointer to actual data, must be different
 */
int enqueue(struct queue* que, void* data) {
  while (queue_full(que)) {
    nop();
  }
  que->que_[que->head_] = data;
  que->head_ = (que->head_ + 1) % que->max_size_;
  return 0;
}