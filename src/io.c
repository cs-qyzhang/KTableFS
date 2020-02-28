#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include "file.h"
#include "kv_impl.h"
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"
#include "debug.h"

pthread_mutex_lock aggregation_file_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t aggregation_file_no;
uint32_t aggregation_file_max_idx;
pthread_mutex_lock big_file_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t big_file_no;
uint32_t big_file_max_idx;
pthread_mutex_lock file_ino_lock = PTHREAD_MUTEX_INITIALIZER;
ino_t file_ino;

int get_aggregation_file_fd(struct kfs_file_handle* handle) {
  if (handle->aggregation_file_fd == 0) {
    handle->aggregation_file_fd = open(get_aggregation_file_path(handle->file.aggregation_file_no), O_RDWR);
    assert(handle->aggregation_file_fd > 0);
  }
  return handle->aggregation_file_fd;
}

int get_big_fd(struct kfs_file_handle* handle) {
  if (handle->big_file_fd == 0) {
    handle->big_file_fd = open(get_big_file_path(handle->file.big_file_no), O_RDWR | O_CREAT, 0666);
    assert(handle->big_file_fd > 0);
  }
  return handle->big_file_fd;
}

int write_file(struct kfs_file_handle* handle, void* data, size_t size, off_t offset) {
  int ret = 0;
  if (offset >= FILE_AGGREGATION_SIZE) {
    int big_file_fd = get_big_file_fd(handle);
    ret += pwrite(big_file_fd, data, size, offset - FILE_AGGREGATION_SIZE);
  } else if (offset < FILE_AGGREGATION_SIZE && offset + size > FILE_AGGREGATION_SIZE) {
    int aggregation_file_fd = get_aggregation_file_fd(handle);
    ret += pwrite(aggregation_file_fd, data, FILE_AGGREGATION_SIZE - offset,
        handle->file.aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
    size -= FILE_AGGREGATION_SIZE - offset;
    int big_file_fd = get_big_file_fd(handle);
    ret += pwrite(big_file_fd, data + (FILE_AGGREGATION_SIZE - offset), size, 0);
  } else {
    int fd = get_aggregation_file_fd(handle);
    ret += pwrite(fd, data, size, handle->file.aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
  }
  return ret;
}

int read_file(struct kfs_file_handle* handle, void* data, size_t size, off_t offset) {
  int ret = 0;
  if (offset >= FILE_AGGREGATION_SIZE) {
    int big_file_fd = get_big_file_fd(handle);
    ret += pread(big_file_fd, data, size, offset - FILE_AGGREGATION_SIZE);
  } else if (offset < FILE_AGGREGATION_SIZE && offset + size > FILE_AGGREGATION_SIZE) {
    int aggregation_file_fd = get_aggregation_file_fd(handle);
    ret += pread(aggregation_file_fd, data, FILE_AGGREGATION_SIZE - offset,
        handle->file.aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
    size -= FILE_AGGREGATION_SIZE - offset;
    int big_file_fd = get_big_file_fd(handle);
    ret += pread(big_file_fd, data + (FILE_AGGREGATION_SIZE - offset), size, 0);
  } else {
    int fd = get_aggregation_file_fd(handle);
    ret += pread(fd, data, size, handle->file.aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
  }
  return ret;
}

int create_file(ino_t parent_ino, const char* file_name, size_t len) {
  struct kfs_file_handle* handle = malloc(sizeof(*handle));
  memset(handle, 0, sizeof(*handle));

  pthread_mutex_lock(&aggregation_file_lock);
  if (aggregation_file_max_idx < AGGREGATION_FILE_SIZE / FILE_AGGREGATION_SIZE) {
    handle->file.aggregation_file_no = aggregation_file_no;
    handle->file.aggregation_file_idx = aggregation_file_max_idx++;
  } else {
    handle->file.aggregation_file_no = ++aggregation_file_no;
    aggregation_file_max_idx = 0;
    handle->file.aggregation_file_idx = aggregation_file_max_idx++;
  }
  pthread_mutex_unlock(&aggregation_file_lock);

  // if need new aggregation file, create it.
  if (handle->file.aggregation_file_idx == 0) {
    int ret = creat(get_aggregation_file_path(handle->file.aggregation_file_no), 0666);
    Assert(ret == 0);
  }

  handle->file.big_file_no = 0;

  pthread_mutex_lock(&file_ino_lock);
  handle->file.stat.st_ino = file_ino++;
  pthread_mutex_unlock(&file_ino_lock);

  struct kv_request* req = malloc(sizeof(*req));
  req->key->dir_ino = parent_ino;
  req->key->length = len;
  req->key->data = malloc(req->key->length + 1);
  memcpy(req->key->data, file_name, req->key->length);
  req->key->data[req->key->length] = '\0';
  req->key->hash = file_name_hash(file_name, req->key->length);
  req->value = file;
  req->type = PUT;

  int sequence = kv_submit(req);
  struct kv_event* event = kv_getevent(sequence);
  Assert(event->return_code == 0);

  return 0;
}