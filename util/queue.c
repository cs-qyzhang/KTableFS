#include <stdlib.h>
#include <assert.h>
#include "util/queue.h"

// TODO: make them static inline?

// TODO
#define die(x)  _exit(-1)

struct queue {
  size_t nmemb_;
  size_t max_size_;
  volatile int head_;
  volatile int tail_;
  void** que_;
};

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

int queue_empty(struct queue* que) {
  return !(que->head_ - que->tail_);
}

int queue_full(struct queue* que) {
  return !((que->head_ + 1 - que->tail_) % que->max_size_);
}

void* dequeue(struct queue* que) {
  if (queue_empty(que)) {
    return NULL;
  }
  void* ret = que->que_[que->tail_];
  que->tail_ = (que->tail_ + 1) % que->max_size_;
  return ret;
}

/* 
 * @data pointer to actual data, must be different
 */
int enqueue(struct queue* que, void* data) {
  if (queue_full(que)) {
    // die("queue full!");
    return -1;
  }
  que->que_[que->head_] = data;
  que->head_ = (que->head_ + 1) % que->max_size_;
  return 0;
}

size_t queue_size(struct queue* que) {
  return (que->head_ + que->max_size_ - que->tail_) % que->max_size_;
}