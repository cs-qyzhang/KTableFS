#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
#include <fuse3/fuse_lowlevel.h>
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

extern struct options options;
size_t datadir_path_len;

pthread_mutex_t aggregation_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t slab_file_no;
uint32_t slab_file_max_idx;
int* slab_file_fd;
int slab_file_nr;
struct freelist* aggregation_freelist;
char slab_buf[4]; // used to read slab file's head

pthread_mutex_t big_file_lock = PTHREAD_MUTEX_INITIALIZER;
uint32_t big_file_nr;

void io_init() {
  slab_file_no = -1;
  slab_file_max_idx = AGGREGATION_SLAB_NR;
  slab_file_fd = malloc(1024 * sizeof(*slab_file_fd));
  memset(slab_file_fd, 0, 1024 * sizeof(int));
  slab_file_nr = 1024;
  big_file_nr = 0;
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
    if (handle->file->big_file_no == 0) {
      pthread_mutex_lock(&big_file_lock);
      handle->file->big_file_no = ++big_file_nr;
      pthread_mutex_unlock(&big_file_lock);
    }
    char* path = get_big_file_path(handle->file->big_file_no);
    handle->big_file_fd = open(path, O_RDWR | O_CREAT, 0666);
    free(path);
    assert(handle->big_file_fd > 0);
  }
  return handle->big_file_fd;
}

// create a new slab file and open it
static void create_new_slab_file(uint32_t file_no) {
  char* path = get_slab_file_path(file_no);
  int fd = open(path, O_RDWR | O_CREAT,0644);
  free(path);
  assert(fd > 0);
  int ret;
  ret = ftruncate(fd, AGGREGATION_HEADER_SIZE);
  if (ret) {
    perror(strerror(errno));
  }

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

  ssize_t ret;
  // update slab nr
  ret = pread(slab_file_fd[*file_no], slab_buf, 4, 0);
  assert(ret == 4);
  *((uint32_t*)slab_buf) += 1;
  ret = pwrite(slab_file_fd[*file_no], slab_buf, 4, 0);
  assert(ret == 4);

  // update slab bitmap
  int bitmap_pos = 4 + *file_idx / 8;
  ret = pread(slab_file_fd[*file_no], slab_buf, 1, bitmap_pos);
  assert(ret == 1);
  slab_buf[0] = slab_buf[0] | (1U << (7 - (*file_idx % 8)));
  ret = pwrite(slab_file_fd[*file_no], slab_buf, 1, bitmap_pos);
  assert(ret == 1);

  pthread_mutex_unlock(&aggregation_lock);
}

void write_callback(void** userdata, struct kv_respond* respond) {
  fuse_req_t req = (fuse_req_t)userdata[0];
  ssize_t count = (size_t)(uintptr_t)userdata[1];
  if (respond->res < 0)
    fuse_reply_err(req, -respond->res);
  else
    fuse_reply_write(req, count);
}

void write_file(fuse_req_t req, struct kfs_file_handle* handle, const char* data, size_t size, off_t offset) {
  ssize_t count = 0;
  ssize_t ret = 0;

  if (offset + size > file_size(handle))
    file_set_size(handle, offset + size);

  if (offset >= AGGREGATION_SLAB_SIZE) {
    int big_file_fd = get_big_file_fd(handle);
    ret = pwrite(big_file_fd, data, size, offset - AGGREGATION_SLAB_SIZE);
    if (ret < 0) {
      Assert(ret == 0);
      fuse_reply_err(req, errno);
      return;
    }
    count += ret;
  } else if (offset < AGGREGATION_SLAB_SIZE && offset + size > AGGREGATION_SLAB_SIZE) {
    int slab_file_fd = get_slab_file_fd(handle);
    ret = pwrite(slab_file_fd, data, AGGREGATION_SLAB_SIZE - offset,
        AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset);
    if (ret < 0) {
      Assert(ret == 0);
      fuse_reply_err(req, errno);
      return;
    }
    count += ret;
    size -= AGGREGATION_SLAB_SIZE - offset;
    int big_file_fd = get_big_file_fd(handle);
    ret = pwrite(big_file_fd, data + (AGGREGATION_SLAB_SIZE - offset), size, 0);
    if (ret < 0) {
      Assert(ret == 0);
      fuse_reply_err(req, errno);
      return;
    }
    count += ret;
  } else {
    int slab_file_fd = get_slab_file_fd(handle);
    ret = pwrite(slab_file_fd, data, size,
        AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset);
    if (ret < 0) {
      Assert(ret == 0);
      fuse_reply_err(req, errno);
      return;
    }
    count += ret;
  }

  update_mtime(handle);
  update_ctime(handle);

  struct kv_request kv_req;
  memset(&kv_req, 0, sizeof(kv_req));
  kv_req.key = handle->key;
  kv_req.value = (struct value*)handle;
  kv_req.type = UPDATE;
  kv_req.callback = write_callback;
  kv_req.userdata = malloc(sizeof(void*) * 2);
  kv_req.userdata[0] = (void*)req;
  kv_req.userdata[1] = (void*)(uintptr_t)count;

  kv_submit(&kv_req, 1);
}

void read_file(fuse_req_t req, struct kfs_file_handle* handle, size_t size, off_t offset) {
  if (offset >= file_size(handle)) {
    printf("EOF!\n");
    fuse_reply_err(req, EOF);
    return;
  }
  if (offset + size >= file_size(handle))
    size = file_size(handle) - offset;

  if (offset >= AGGREGATION_SLAB_SIZE) {
    // only in big file
    int big_file_fd = get_big_file_fd(handle);
    struct fuse_bufvec buf = FUSE_BUFVEC_INIT(size);
    buf.buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
    buf.buf[0].fd = big_file_fd;
    buf.buf[0].pos = offset - AGGREGATION_SLAB_SIZE;
    fuse_reply_data(req, &buf, FUSE_BUF_SPLICE_MOVE);
  } else if (offset < AGGREGATION_SLAB_SIZE && offset + size > AGGREGATION_SLAB_SIZE) {
    int slab_file_fd = get_slab_file_fd(handle);
    int big_file_fd = get_big_file_fd(handle);
    struct fuse_bufvec* bufvec;
    bufvec = malloc(sizeof(*bufvec) + sizeof(struct fuse_buf));
    bufvec->count = 2;
    bufvec->idx = 0;
    bufvec->off = 0;
    bufvec->buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
    bufvec->buf[0].fd = slab_file_fd;
    bufvec->buf[0].size = AGGREGATION_SLAB_SIZE - offset;
    bufvec->buf[0].pos = AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset;
    bufvec->buf[1].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
    bufvec->buf[1].fd = big_file_fd;
    bufvec->buf[1].size = size - (AGGREGATION_SLAB_SIZE - offset);
    bufvec->buf[1].pos = 0;
    fuse_reply_data(req, bufvec, FUSE_BUF_SPLICE_MOVE);
  } else {
    int slab_file_fd = get_slab_file_fd(handle);
    struct fuse_bufvec buf = FUSE_BUFVEC_INIT(size);
    buf.buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
    buf.buf[0].fd = slab_file_fd;
    buf.buf[0].pos = AGGREGATION_HEADER_SIZE + handle->file->slab_file_idx * AGGREGATION_SLAB_SIZE + offset;
    fuse_reply_data(req, &buf, FUSE_BUF_SPLICE_MOVE);
  }
  update_atime(handle);
}

void remove_file(struct kfs_file_handle* handle) {
  int* slab_entry = malloc(2 * sizeof(int));
  slab_entry[0] = handle->file->slab_file_no;
  slab_entry[1] = handle->file->slab_file_idx;

  pthread_mutex_lock(&aggregation_lock);

  // add free slab to freelist
  freelist_add(aggregation_freelist, slab_entry);

  ssize_t ret;
  // update slab nr
  ret = pread(slab_file_fd[handle->file->slab_file_no], slab_buf, 4, 0);
  assert(ret == 4);
  *((uint32_t*)slab_buf) -= 1;
  ret = pwrite(slab_file_fd[handle->file->slab_file_no], slab_buf, 4, 0);
  assert(ret == 4);

  // update slab bitmap
  int bitmap_pos = 4 + handle->file->slab_file_idx / 8;
  ret = pread(slab_file_fd[handle->file->slab_file_no], slab_buf, 1, bitmap_pos);
  assert(ret == 1);
  slab_buf[0] = slab_buf[0] ^ (1U << (7 - (handle->file->slab_file_idx % 8)));
  ret = pwrite(slab_file_fd[handle->file->slab_file_no], slab_buf, 1, bitmap_pos);
  assert(ret == 1);

  pthread_mutex_unlock(&aggregation_lock);
}