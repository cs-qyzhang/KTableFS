#ifndef KTABLEFS_QUEUE_H_
#define KTABLEFS_QUEUE_H_

#include <sys/types.h>

struct queue;

struct queue* queue_new(size_t max_size) __attribute__ ((warn_unused_result));
void queue_destroy(struct queue* que);
int queue_empty(struct queue* que) __attribute__ ((warn_unused_result));
int queue_full(struct queue* que) __attribute__ ((warn_unused_result));
void* dequeue(struct queue* que);
int enqueue(struct queue* que, void* data);
size_t queue_size(struct queue* que) __attribute__ ((warn_unused_result));

#endif // KTABLEFS_QUEUE_H_