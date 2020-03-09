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
#include "util/freelist.h"
#include "file.h"
#include "options.h"
#include "kv_impl.h"
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"
#include "debug.h"

extern struct stat* root_stat;
size_t datadir_path_len;

pthread_mutex_t aggregation_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t slab_file_no;
uint32_t slab_file_max_idx;
int* slab_file_fd;
int slab_file_nr;
struct freelist* aggregation_freelist;

pthread_mutex_t big_file_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t big_file_nr;

pthread_mutex_t file_ino_lock = PTHREAD_MUTEX_INITIALIZER;
ino_t max_file_ino;

void io_init() {
  slab_file_no = -1;
  slab_file_max_idx = AGGREGATION_SLAB_NR;
  slab_file_fd = malloc(1024 * sizeof(*slab_file_fd));
  memset(slab_file_fd, 0, 1024 * sizeof(int));
  slab_file_nr = 1024;
  big_file_nr = 0;
  max_file_ino = 1;
  datadir_path_len = strlen(options.datadir);
  aggregation_freelist = freelist_new(NULL);
}

void io_destroy() {
  for (int i = 0; i <= slab_file_no; ++i)
    close(slab_file_fd[i]);
  free(slab_file_fd);
}

static char* get_slab_file_path(uint32_t file_no) {
  char* path = malloc(datadir_path_len + 20);
  sprintf(path, "%s/aggregation-%06d", options.datadir, file_no);
  return path;
}

static char* get_big_file_path(uint32_t file_no) {
  char* path = malloc(datadir_path_len + 12);
  sprintf(path, "%s/big-%06d", options.datadir, file_no);
  return path;
}

// we will open all the slab file when fs is mounting
static inline int get_slab_file_fd(struct kfs_file_handle* handle) {
  return slab_file_fd[handle->file->slab_file_no];
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

// create a new slab file and open it
void create_new_slab_file(uint32_t file_no) {
  printf("create new slab file\n");
  char* path = get_slab_file_path(file_no);
  int fd = open(path, O_RDWR | O_CREAT,0644);
  free(path);
  assert(fd > 0);
  ftruncate(fd, AGGREGATION_HEADER_SIZE);

  if (file_no >= slab_file_nr) {
    slab_file_nr += 1024;
    slab_file_fd = realloc(slab_file_fd, slab_file_nr * sizeof(*slab_file_fd));
  }
  slab_file_fd[file_no] = fd;
}

void allocate_aggregation_slab(uint32_t* file_no, uint32_t* file_idx) {
  pthread_mutex_lock(&aggregation_lock);
  // get free slab
  int* slab = freelist_get(aggregation_freelist);
  if (slab == NULL) {
    if (slab_file_max_idx < AGGREGATION_SLAB_NR) {
      *file_no = slab_file_no;
      *file_idx = slab_file_max_idx++;
    } else {
      *file_no = ++slab_file_no;
      slab_file_max_idx = 0;
      *file_idx = slab_file_max_idx++;
      create_new_slab_file(*file_no);
    }
  } else {
    *file_no = slab[0];
    *file_idx = slab[1];
  }

  char* buf = malloc(4);
  // update slab nr
  pread(slab_file_fd[*file_no], buf, 4, 0);
  *((uint32_t*)buf) += 1;
  pwrite(slab_file_fd[*file_no], buf, 4, 0);

  // update slab bitmap
  int bitmap_pos = 4 + *file_idx / 8;
  pread(slab_file_fd[*file_no], buf, 1, bitmap_pos);
  buf[0] = buf[0] | (1U << (7 - (*file_idx % 8)));
  pwrite(slab_file_fd[*file_no], buf, 1, bitmap_pos);

  pthread_mutex_unlock(&aggregation_lock);
}

struct kv_event* create_file(ino_t parent_ino, const char* file_name, mode_t mode, size_t len) {
  struct kfs_file_handle* handle = malloc(sizeof(*handle));
  handle->file = malloc(sizeof(*handle->file));
  memset(handle->file, 0, sizeof(*handle->file));
  handle->big_file_fd = 0;
  handle->offset = 0;

  allocate_aggregation_slab(&handle->file->slab_file_no,
                            &handle->file->slab_file_idx);

  handle->file->big_file_no = 0;

  handle->file->stat.st_uid = root_stat->st_uid;
  handle->file->stat.st_gid = root_stat->st_gid;
  handle->file->stat.st_dev = root_stat->st_dev;
  handle->file->stat.st_rdev = root_stat->st_rdev;
  handle->file->stat.st_blksize = root_stat->st_blksize;
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

  req->value = NULL;
  req->type = GET;
  sequence = kv_submit(req);
  event = kv_getevent(sequence);
  Assert(event->return_code == 0);

  return event;
}

int write_file(struct kfs_file_handle* handle, void* data, size_t size, off_t offset) {
  int ret = 0;

  if (offset + size > file_size(handle))
    file_set_size(handle, offset + size);

  if (offset >= AGGREGATION_SLAB_SIZE) {
    int big_file_fd = get_big_file_fd(handle);
    ret += pwrite(big_file_fd, data, size, offset - AGGREGATION_SLAB_SIZE);
  } else if (offset < AGGREGATION_SLAB_SIZE && offset + size > AGGREGATION_SLAB_SIZE) {
    int slab_file_fd = get_slab_file_fd(handle);
    ret += pwrite(slab_file_fd, data, AGGREGATION_SLAB_SIZE - offset,
        AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset);
    size -= AGGREGATION_SLAB_SIZE - offset;
    int big_file_fd = get_big_file_fd(handle);
    ret += pwrite(big_file_fd, data + (AGGREGATION_SLAB_SIZE - offset), size, 0);
  } else {
    int slab_file_fd = get_slab_file_fd(handle);
    ret += pwrite(slab_file_fd, data, size,
        AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset);
  }

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
  if (offset >= AGGREGATION_SLAB_SIZE) {
    int big_file_fd = get_big_file_fd(handle);
    ret += pread(big_file_fd, data, size, offset - AGGREGATION_SLAB_SIZE);
  } else if (offset < AGGREGATION_SLAB_SIZE && offset + size > AGGREGATION_SLAB_SIZE) {
    int slab_file_fd = get_slab_file_fd(handle);
    ret += pread(slab_file_fd, data, AGGREGATION_SLAB_SIZE - offset,
        AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset);
    size -= AGGREGATION_SLAB_SIZE - offset;
    int big_file_fd = get_big_file_fd(handle);
    ret += pread(big_file_fd, data + (AGGREGATION_SLAB_SIZE - offset), size, 0);
  } else {
    int slab_file_fd = get_slab_file_fd(handle);
    ret += pread(slab_file_fd, data, size,
        AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset);
  }

  if (ret == -1) {
    perror(strerror(errno));
  }

  update_mtime(handle);
  update_ctime(handle);

  return ret;
}