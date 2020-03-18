#ifndef KTABLEFS_UTIL_QUEUE_H_
#define KTABLEFS_UTIL_QUEUE_H_

#include <sys/types.h>

struct queue {
  size_t nmemb_;
  size_t max_size_;
  volatile int head_;
  volatile int tail_;
  void** que_;
};

struct queue* queue_new(size_t max_size) __attribute__ ((warn_unused_result));
void queue_destroy(struct queue* que);
void* dequeue(struct queue* que);
int enqueue(struct queue* que, void* data);

static __always_inline int queue_empty(struct queue* que) {
  return (que->head_ == que->tail_);
}

static __always_inline int queue_full(struct queue* que) {
  return !((que->head_ + 1 - que->tail_) % que->max_size_);
}

static __always_inline size_t queue_size(struct queue* que) {
  return (que->head_ + que->max_size_ - que->tail_) % que->max_size_;
}

#endif // KTABLEFS_UTIL_QUEUE_H_