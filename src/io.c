#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include "util/hash.h"
#include "file.h"
#include "options.h"
#include "kv_impl.h"
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"
#include "debug.h"

extern struct stat root_stat;
size_t datadir_path_len;

pthread_mutex_t aggregation_file_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t aggregation_file_no;
uint32_t aggregation_file_max_idx;
int* aggregation_fd;
int max_aggregation_fd;
pthread_mutex_t big_file_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t max_big_file_no;
pthread_mutex_t file_ino_lock = PTHREAD_MUTEX_INITIALIZER;
ino_t max_file_ino;

void io_init() {
  aggregation_file_no = 0;
  aggregation_file_max_idx = 0;
  aggregation_fd = malloc(1024 * sizeof(*aggregation_fd));
  max_aggregation_fd = 1024;
  max_big_file_no = 0;
  max_file_ino = 1;
  datadir_path_len = strlen(options.datadir);
}

void io_destroy() {
  for (int i = 0; i <= aggregation_file_no; ++i)
    close(aggregation_fd[i]);
  free(aggregation_fd);
}

static char* get_aggregation_file_path(int file_no) {
  char* path = malloc(datadir_path_len + 20);
  sprintf(path, "%s/aggregation-%06d", options.datadir, file_no);
  return path;
}

static char* get_big_file_path(int file_no) {
  char* path = malloc(datadir_path_len + 12);
  sprintf(path, "%s/big-%06d", options.datadir, file_no);
  return path;
}

static int get_aggregation_file_fd(struct kfs_file_handle* handle) {
  if (handle->file->aggregation_file_no < max_aggregation_fd) {
    if (!aggregation_fd[handle->file->aggregation_file_no]) {
      char* path = get_aggregation_file_path(handle->file->aggregation_file_no);
      aggregation_fd[handle->file->aggregation_file_no] = open(path, O_RDWR);
      free(path);
      Assert(aggregation_fd[handle->file->aggregation_file_no]);
    }
  } else {
    pthread_mutex_lock(&aggregation_file_lock);
    if (handle->file->aggregation_file_no < max_aggregation_fd) {
      if (!aggregation_fd[handle->file->aggregation_file_no]) {
        char* path = get_aggregation_file_path(handle->file->aggregation_file_no);
        aggregation_fd[handle->file->aggregation_file_no] = open(path, O_RDWR);
        free(path);
        Assert(aggregation_fd[handle->file->aggregation_file_no]);
      }
    } else {
      max_aggregation_fd += 1024;
      aggregation_fd = realloc(aggregation_fd, max_aggregation_fd * sizeof(*aggregation_fd));

      char* path = get_aggregation_file_path(handle->file->aggregation_file_no);
      aggregation_fd[handle->file->aggregation_file_no] = open(path, O_RDWR);
      free(path);
      Assert(aggregation_fd[handle->file->aggregation_file_no]);
    }
    pthread_mutex_unlock(&aggregation_file_lock);
  }
  return aggregation_fd[handle->file->aggregation_file_no];
}

static int get_big_file_fd(struct kfs_file_handle* handle) {
  if (handle->big_file_fd == 0) {
    char* path = get_big_file_path(handle->file->big_file_no);
    handle->big_file_fd = open(path, O_RDWR | O_CREAT, 0666);
    free(path);
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
        handle->file->aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
    size -= FILE_AGGREGATION_SIZE - offset;
    int big_file_fd = get_big_file_fd(handle);
    ret += pwrite(big_file_fd, data + (FILE_AGGREGATION_SIZE - offset), size, 0);
  } else {
    int fd = get_aggregation_file_fd(handle);
    ret += pwrite(fd, data, size, handle->file->aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
  }

  if (offset + size > file_size(handle))
    file_set_size(handle, offset + size);

  update_mtime(handle);
  update_ctime(handle);

  return ret;
}

int read_file(struct kfs_file_handle* handle, void* data, size_t size, off_t offset) {
  int ret = 0;
  if (offset >= file_size(handle)) {
    return -EOF;
  }
  if (offset + size >= file_size(handle)) {
    size = file_size(handle) - offset;
  }
  if (offset >= FILE_AGGREGATION_SIZE) {
    int big_file_fd = get_big_file_fd(handle);
    ret += pread(big_file_fd, data, size, offset - FILE_AGGREGATION_SIZE);
  } else if (offset < FILE_AGGREGATION_SIZE && offset + size > FILE_AGGREGATION_SIZE) {
    int aggregation_file_fd = get_aggregation_file_fd(handle);
    ret += pread(aggregation_file_fd, data, FILE_AGGREGATION_SIZE - offset,
        handle->file->aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
    size -= FILE_AGGREGATION_SIZE - offset;
    int big_file_fd = get_big_file_fd(handle);
    ret += pread(big_file_fd, data + (FILE_AGGREGATION_SIZE - offset), size, 0);
  } else {
    int fd = get_aggregation_file_fd(handle);
    ret += pread(fd, data, size, handle->file->aggregation_file_idx * FILE_AGGREGATION_SIZE + offset);
  }
  update_mtime(handle);
  update_ctime(handle);
  return ret;
}

struct kv_event* create_file(ino_t parent_ino, const char* file_name, mode_t mode, size_t len) {
  struct kfs_file_handle* handle = malloc(sizeof(*handle));
  handle->file = malloc(sizeof(*handle->file));
  handle->big_file_fd = 0;
  handle->offset = 0;

  pthread_mutex_lock(&aggregation_file_lock);
  if (aggregation_file_max_idx < AGGREGATION_FILE_SIZE / FILE_AGGREGATION_SIZE) {
    handle->file->aggregation_file_no = aggregation_file_no;
    handle->file->aggregation_file_idx = aggregation_file_max_idx++;
  } else {
    handle->file->aggregation_file_no = ++aggregation_file_no;
    aggregation_file_max_idx = 0;
    handle->file->aggregation_file_idx = aggregation_file_max_idx++;
  }
  pthread_mutex_unlock(&aggregation_file_lock);

  // if need new aggregation file, create it.
  if (handle->file->aggregation_file_idx == 0) {
    char* path = get_aggregation_file_path(handle->file->aggregation_file_no);
    int fd = creat(path, 0644);
    Assert(fd != -1);
    close(fd);
  }

  handle->file->big_file_no = 0;

  handle->file->stat.st_uid = root_stat.st_uid;
  handle->file->stat.st_gid = root_stat.st_gid;
  handle->file->stat.st_dev = root_stat.st_dev;
  handle->file->stat.st_rdev = root_stat.st_rdev;
  handle->file->stat.st_blksize = root_stat.st_blksize;
  handle->file->stat.st_nlink = 1;
  handle->file->stat.st_mode = mode;
  update_atime(handle);
  update_mtime(handle);
  update_ctime(handle);
  file_set_size(handle, 0);

  pthread_mutex_lock(&file_ino_lock);
  file_set_ino(handle, max_file_ino++);
  pthread_mutex_unlock(&file_ino_lock);

  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->key = malloc(sizeof(*req->key));
  req->key->dir_ino = parent_ino;
  req->key->length = len;
  req->key->data = malloc(req->key->length + 1);
  memcpy(req->key->data, file_name, req->key->length);
  req->key->data[req->key->length] = '\0';
  req->key->hash = file_name_hash(file_name, req->key->length);
  req->value = (struct value*)handle;
  req->type = PUT;

  int sequence = kv_submit(req);
  struct kv_event* event = kv_getevent(sequence);
  Assert(event->return_code == 0);
  event->value = (struct value*)handle;

  return event;
}