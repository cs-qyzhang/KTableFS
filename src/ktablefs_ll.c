#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
#include <fuse3/fuse_lowlevel.h>
#include <fuse3/fuse.h>
#include <fuse3/fuse_opt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "util/hash.h"
#include "file.h"
#include "dcache.h"
#include "options.h"
#include "kv_impl.h"
#include "io.h"
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"
#include "debug.h"

struct options options;

pthread_mutex_t file_ino_lock = PTHREAD_MUTEX_INITIALIZER;
ino_t max_file_ino;

#define OPTION(t, p)  { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
  OPTION("--datadir=%s", datadir),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

#ifdef DEBUG
char log_path[256];
int log_fd;

static void print_info(const char* func, const char* path) {
  char buf[256];
  int len = sprintf(buf, "%s %s pid: %d, tid: %d\n", func, path, getpid(), gettid());
  lseek(log_fd, 0, SEEK_END);
  write(log_fd, buf, len);
  printf("%s", buf);
}
#else // DEBUG
static void print_info(const char* func, const char* path) {
}
#endif

struct kfs_data {
  struct kfs_file_handle root;
  struct stat root_stat;
  int timeout;
};

struct stat* root_stat;

static inline struct kfs_data* kfs_data(fuse_req_t req) {
  return (struct kfs_data*)fuse_req_userdata(req);
}

static inline struct kfs_file_handle* kfs_file_handle(fuse_req_t req, fuse_ino_t ino) {
  return (ino == FUSE_ROOT_ID) ? &kfs_data(req)->root : (struct kfs_file_handle*)(uintptr_t)ino;
}

static inline ino_t dir_ino(fuse_req_t req, fuse_ino_t ino) {
  return kfs_file_handle(req, ino)->file->stat.st_ino;
}

void kfs_lo_init(void* userdata, struct fuse_conn_info* conn) {
#ifdef DEBUG
  log_fd = open(log_path, O_WRONLY);
  char buf[256];
  int len = sprintf(buf, "init pid: %d, tid: %d\n", getpid(), gettid());
  lseek(log_fd, 0, SEEK_END);
  write(log_fd, buf, len);
#endif

  (void)conn;

  struct kfs_data* data = (struct kfs_data*)userdata;

  struct kv_options* kv_options = malloc(sizeof(*kv_options));
  kv_options->slab_dir = strdup(options.datadir);
  kv_options->thread_nr = KVENGINE_THREAD_NR;
  kv_init(kv_options);
  io_init();
  max_file_ino = FUSE_ROOT_ID;

  data->root.file = malloc(sizeof(*data->root.file));
  memset(&data->root_stat, 0, sizeof(data->root_stat));
  root_stat = &data->root_stat;

  struct stat statbuf;
  stat(options.datadir, &statbuf);
  data->root_stat.st_blksize = statbuf.st_blksize;
  data->root_stat.st_blocks = statbuf.st_blocks;
  data->root_stat.st_dev = statbuf.st_dev;
  data->root_stat.st_rdev = statbuf.st_rdev;
  data->root_stat.st_ino = FUSE_ROOT_ID;
  data->root_stat.st_uid = getuid();
  data->root_stat.st_gid = getgid();
  data->root_stat.st_mode = S_IFDIR | 0755;
  // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
  data->root_stat.st_nlink = 2;
  data->root_stat.st_atime = time(NULL);
  data->root_stat.st_ctime = time(NULL);
  data->root_stat.st_mtime = time(NULL);

  data->root.file->stat.atime = data->root_stat.st_atime;
  data->root.file->stat.mtime = data->root_stat.st_mtime;
  data->root.file->stat.ctime = data->root_stat.st_ctime;
  data->root.file->stat.st_uid = data->root_stat.st_uid;
  data->root.file->stat.st_gid = data->root_stat.st_gid;
  data->root.file->stat.st_ino = data->root_stat.st_ino;
  data->root.file->stat.st_mode = data->root_stat.st_mode;
  data->root.file->stat.st_nlink = data->root_stat.st_nlink;
  data->root.file->stat.st_size = data->root_stat.st_size;
}

void kfs_lo_destroy(void* userdata) {
  print_info("destroy", "");
}

// callback
void lookup_callback(void** userdata, struct kv_respond* respond) {
  fuse_req_t req = (fuse_req_t)userdata;
  struct fuse_entry_param e;
  memset(&e, 0, sizeof(e));
  if (respond->res < 0)
    fuse_reply_err(req, -respond->res);
  else {
    file_fill_stat(&e.attr, file_stat(&respond->value->handle));
    e.ino = (uintptr_t)&respond->value->handle;
    fuse_reply_entry(req, &e);
  }
}

/**
 * Look up a directory entry by name and get its attributes.
 *
 * Valid replies:
 *   fuse_reply_entry
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name the name to look up
 */
void kfs_lo_lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
  print_info("lookup", "");

  struct kv_request kv_req;
  struct key key;
  memset(&kv_req, 0, sizeof(kv_req));
  key.dir_ino = dir_ino(req, parent);
  key.length = strlen(name);
  key.hash = file_name_hash(name, key.length);
  key.data = (char*)name;
  kv_req.key = &key;
  kv_req.type = GET;
  kv_req.callback = lookup_callback;
  kv_req.userdata = (void*)req;

  struct kv_request* req_ptr = &kv_req;
  kv_submit(&req_ptr, 1);
}

/**
 * Get file attributes.
 *
 * If writeback caching is enabled, the kernel may have a
 * better idea of a file's length than the FUSE file system
 * (eg if there has been a write that extended the file size,
 * but that has not yet been passed to the filesystem.n
 *
 * In this case, the st_size value provided by the file system
 * will be ignored.
 *
 * Valid replies:
 *   fuse_reply_attr
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi for future use, currently always NULL
 */
void kfs_lo_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
  print_info("getattr", "");

  struct stat statbuf;
  file_fill_stat(&statbuf, &kfs_file_handle(req, ino)->file->stat);
  fuse_reply_attr(req, &statbuf, kfs_data(req)->timeout);
}

// callback
void create_callback(void** userdata, struct kv_respond* respond) {
  fuse_req_t req = (fuse_req_t)userdata[0];
  struct fuse_file_info* fi = userdata[1];
  struct fuse_entry_param e;
  memset(&e, 0, sizeof(e));

  if (respond->res < 0)
    fuse_reply_err(req, -respond->res);
  else {
    memset(&e, 0, sizeof(e));
    file_fill_stat(&e.attr, file_stat(&respond->value->handle));
    e.ino = (uintptr_t)&respond->value->handle;
    fi->fh = (uintptr_t)&respond->value->handle;
    fuse_reply_create(req, &e, fi);
  }
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * See the description of the open handler for more
 * information.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * If this request is answered with an error code of ENOSYS, the handler
 * is treated as not implemented (i.e., for this and future requests the
 * mknod() and open() handlers will be called instead).
 *
 * Valid replies:
 *   fuse_reply_create
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name to create
 * @param mode file type and mode with which to create the new file
 * @param fi file information
 */
void kfs_lo_create(fuse_req_t req, fuse_ino_t parent, const char* name,
                   mode_t mode, struct fuse_file_info* fi) {
  print_info("create", "");

  struct kfs_file_handle handle;
  struct kfs_file file;
  handle.big_file_fd = 0;
  handle.file = &file;

  memset(&file, 0, sizeof(file));
  file.type = KFS_REG;
  file.stat.st_uid = root_stat->st_uid;
  file.stat.st_gid = root_stat->st_gid;
  file.stat.st_nlink = 1;
  file.stat.st_mode = mode;
  update_atime(&handle);
  update_mtime(&handle);
  update_ctime(&handle);
  file_set_size(&handle, 0);

  pthread_mutex_lock(&file_ino_lock);
  file_set_ino(&handle, ++max_file_ino);
  pthread_mutex_unlock(&file_ino_lock);

  if (!(mode & S_IFDIR)) {
    allocate_aggregation_slab(&file.slab_file_no,
                              &file.slab_file_idx);
  }

  // PUT file info
  struct kv_request kv_req[2];
  struct kv_request* req_ptr[2];
  struct key key;
  char buf[512];

  key.dir_ino = parent;
  key.length = strlen(name);
  key.data = buf;
  memcpy(key.data, name, key.length);
  key.data[key.length] = '\0';
  key.hash = file_name_hash(name, key.length);

  memset(&kv_req[0], 0, sizeof(kv_req[0]));
  kv_req[0].type = PUT;
  kv_req[0].key = &key;
  kv_req[0].value = (struct value*)&handle;

  memset(&kv_req[1], 0, sizeof(kv_req[1]));
  kv_req[1].type = GET;
  kv_req[1].key = &key;
  kv_req[1].callback = create_callback;
  void** userdata = malloc(sizeof(void*) * 2);
  userdata[0] = (void*)req;
  userdata[1] = fi;
  kv_req[1].userdata = userdata;

  req_ptr[0] = &kv_req[0];
  req_ptr[1] = &kv_req[1];

  kv_submit(req_ptr, 2);
}

/**
 * Valid replies:
 *   fuse_reply_open
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi file information
 */
void kfs_lo_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
  print_info("open", "");

  fi->fh = ino;
  fuse_reply_open(req, fi);
}

/**
 * Read data
 *
 * Read should send exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the file
 * has been opened in 'direct_io' mode, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * fi->fh will contain the value set by the open method, or will
 * be undefined if the open method didn't set any value.
 *
 * Valid replies:
 *   fuse_reply_buf
 *   fuse_reply_iov
 *   fuse_reply_data
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param size number of bytes to read
 * @param off offset to read from
 * @param fi file information
 */
void kfs_lo_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                 struct fuse_file_info* fi) {
  print_info("read", "");

  read_file(req, (struct kfs_file_handle*)(uintptr_t)fi->fh, size, off);
}

/**
 * Write data
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the file has
 * been opened in 'direct_io' mode, in which case the return value
 * of the write system call will reflect the return value of this
 * operation.
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
 * expected to reset the setuid and setgid bits.
 *
 * fi->fh will contain the value set by the open method, or will
 * be undefined if the open method didn't set any value.
 *
 * Valid replies:
 *   fuse_reply_write
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param buf data to write
 * @param size number of bytes to write
 * @param off offset to write to
 * @param fi file information
 */
void kfs_lo_write(fuse_req_t req, fuse_ino_t ino, const char* buf,
                  size_t size, off_t off, struct fuse_file_info* fi) {
  print_info("write", "");

  write_file(req, (struct kfs_file_handle*)(uintptr_t)fi->fh, buf, size, off);
}

static const struct fuse_lowlevel_ops lo_oper = {
  .init       = kfs_lo_init,
  .destroy    = kfs_lo_destroy,

  .lookup     = kfs_lo_lookup,
  .getattr    = kfs_lo_getattr,
  .setattr    = NULL,
  .rename     = NULL,

  .mknod      = NULL,
  .create     = kfs_lo_create,
  .open       = kfs_lo_open,
  .read       = kfs_lo_read,
  .write      = kfs_lo_write,
  .release    = NULL,

  .link       = NULL,
  .unlink     = NULL,
  .symlink    = NULL,
  .readlink   = NULL,

  .mkdir      = NULL,
  .rmdir      = NULL,
  .opendir    = NULL,
  .readdir    = NULL,
  .releasedir = NULL,

  .flush      = NULL,
  .fsync      = NULL,
  .statfs     = NULL,
  .fallocate  = NULL,
#ifdef HAVE_COPY_FILE_RANGE
  .copy_file_range = NULL,
#endif
};

int main(int argc, char **argv) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_session *se;
  struct fuse_cmdline_opts opts;
  struct kfs_data userdata;
  int ret = -1;

  /* Don't mask creation mode, kernel already did that */
  umask(0);

  if (fuse_parse_cmdline(&args, &opts) != 0)
    return 1;
  if (opts.show_help) {
    printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
    fuse_cmdline_help();
    fuse_lowlevel_help();
    ret = 0;
    goto err_out1;
  } else if (opts.show_version) {
    printf("FUSE library version %s\n", fuse_pkgversion());
    fuse_lowlevel_version();
    ret = 0;
    goto err_out1;
  }

  if(opts.mountpoint == NULL) {
    printf("usage: %s [options] <mountpoint>\n", argv[0]);
    printf("       %s --help\n", argv[0]);
    ret = 1;
    goto err_out1;
  }

  if (fuse_opt_parse(&args, &options, option_spec, NULL)== -1)
    return 1;

  se = fuse_session_new(&args, &lo_oper, sizeof(lo_oper), &userdata);
  if (se == NULL)
      goto err_out1;

  if (fuse_set_signal_handlers(se) != 0)
      goto err_out2;

  if (fuse_session_mount(se, opts.mountpoint) != 0)
      goto err_out3;

  fuse_daemonize(opts.foreground);

  /* Block until ctrl+c or fusermount -u */
  if (opts.singlethread)
    ret = fuse_session_loop(se);
  else
    ret = fuse_session_loop_mt(se, opts.clone_fd);

  fuse_session_unmount(se);
err_out3:
  fuse_remove_signal_handlers(se);
err_out2:
  fuse_session_destroy(se);
err_out1:
  free(opts.mountpoint);
  fuse_opt_free_args(&args);

  return ret ? 1 : 0;
}