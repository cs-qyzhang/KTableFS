#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
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
#include "util/hash.h"
#include "file.h"
#include "dcache.h"
#include "options.h"
#include "kv_impl.h"
#include "io.h"
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"
#include "debug.h"

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

struct options options;
struct dentrycache* dcache;
struct stat* root_stat;
struct kfs_file_handle root_handle;

#define OPTION(t, p)  { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
  OPTION("--datadir=%s", datadir),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

static inline int path_is_root(const char* path) {
  return (path[0] == '/' && path[1] == '\0');
}

static struct key* path_to_key(const char* path) {
  size_t len = strlen(path);
  char* end = (char*)path + (len - 1);
  while (*end != '/')
    end--;

  ino_t dir_ino = dcache_lookup(dcache, path, (end - path) + 1);
  if (dir_ino == -1)
    return NULL;

  struct key* key = malloc(sizeof(*key));
  key->dir_ino = dir_ino;
  key->length = len - (end - path) - 1;
  key->data = malloc(key->length + 1);
  memcpy(key->data, end + 1, key->length);
  key->data[key->length] = '\0';
  key->hash = file_name_hash(end + 1, key->length);

  return key;
}

static struct kv_event* get_file(const char* path) {
  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->key = path_to_key(path);
  req->type = GET;

  int sequence = kv_submit(req);
  struct kv_event* event = kv_getevent(sequence);
  if (event->return_code != 0)
    return event;

  if ((event->value->handle.file->type & KFS_REG) ||
      (event->value->handle.file->type & KFS_SYMLINK)) {

    event->value->handle.key = req->key;
    free(req);
    return event;
  }

  // KFS_HARDLINK
  req->key->data = NULL;
  req->key->val = 0;
  req->key->dir_ino = event->value->handle.file->stat.st_ino;
  req->type = GET;

  sequence = kv_submit(req);
  event = kv_getevent(sequence);
  event->value->handle.key = req->key;
  free(req);
  return event;
}

void* kfs_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
#ifdef DEBUG
  log_fd = open(log_path, O_WRONLY);
  char buf[256];
  int len = sprintf(buf, "init pid: %d, tid: %d\n", getpid(), gettid());
  lseek(log_fd, 0, SEEK_END);
  write(log_fd, buf, len);
#endif

  (void)conn;
  (void)cfg;

  struct kv_options* kv_options = malloc(sizeof(*kv_options));
  kv_options->slab_dir = strdup(options.datadir);
  kv_options->thread_nr = KVENGINE_THREAD_NR;
  kv_init(kv_options);
  io_init();
  dcache = dcache_new();

  root_handle.file = malloc(sizeof(*(root_handle.file)));
  root_stat = malloc(sizeof(*root_stat));

  memset(root_stat, 0, sizeof(*root_stat));
  struct stat statbuf;
  stat(options.datadir, &statbuf);
  root_stat->st_blksize = statbuf.st_blksize;
  root_stat->st_blocks = statbuf.st_blocks;
  root_stat->st_dev = statbuf.st_dev;
  root_stat->st_rdev = statbuf.st_rdev;
  root_stat->st_ino = 0;
  root_stat->st_uid = getuid();
  root_stat->st_gid = getgid();
  root_stat->st_mode = S_IFDIR | 0755;
  // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
  root_stat->st_nlink = 2;
  root_stat->st_atime = time(NULL);
  root_stat->st_ctime = time(NULL);
  root_stat->st_mtime = time(NULL);

  root_handle.file->stat.atime = root_stat->st_atime;
  root_handle.file->stat.mtime = root_stat->st_mtime;
  root_handle.file->stat.ctime = root_stat->st_ctime;
  root_handle.file->stat.st_uid = root_stat->st_uid;
  root_handle.file->stat.st_gid = root_stat->st_gid;
  root_handle.file->stat.st_ino = root_stat->st_ino;
  root_handle.file->stat.st_mode = root_stat->st_mode;
  root_handle.file->stat.st_nlink = root_stat->st_nlink;
  root_handle.file->stat.st_size = root_stat->st_size;

  return NULL;
}

void kfs_destroy(void* private_data) {
  print_info("destroy", "");
}

int kfs_mknod(const char* path, mode_t mode, dev_t dev) {
  print_info("mkmod", path);

  size_t len = strlen(path);
  char* end = (char*)path + (len - 1);
  while (*end != '/')
    end--;

  ino_t dir_ino = dcache_lookup(dcache, path, (end - path) + 1);
  assert(dir_ino >= 0);
  struct kv_event* event = create_file(dir_ino, end + 1, mode, len - (end - path + 1), KFS_REG);
  return event->return_code;
}

/*
 * Set fi->fh to the file handle.
 */
int kfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
  print_info("create", path);

  size_t len = strlen(path);
  char* end = (char*)path + (len - 1);
  while (*end != '/')
    end--;

  ino_t dir_ino = dcache_lookup(dcache, path, (end - path) + 1);
  assert(dir_ino >= 0);
  struct kv_event* event = create_file(dir_ino, end + 1, mode, len - (end - path + 1), KFS_REG);
  if (event->return_code == 0)
    fi->fh = (uint64_t)&event->value->handle;

  return event->return_code;
}

/*
 * Set fi->fh to the file handle.
 * If file doesn't exist, return -ENOENT.
 * Don't check creation (O_CREAT, O_EXCL, O_NOCTTY) flags,
 * because they have been filtered out / handled by the kernel.
 */
int kfs_open(const char* path, struct fuse_file_info* fi) {
  print_info("open", path);

  struct kv_event* event = get_file(path);

  if (event == NULL || event->return_code != 0)
    return -ENOENT;

  fi->fh = (uint64_t)&event->value->handle;
  return 0;
}

/*
 * return read byte.
 * change atime.
 */
int kfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  print_info("read", path);

  struct kfs_file_handle* handle = (void*)fi->fh;
  return read_file(handle, (void*)buf, size, offset);
}

/*
 * return write byte.
 * change mtime and ctime.
 */
int kfs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  print_info("write", path);

  struct kfs_file_handle* handle = (void*)fi->fh;
  return write_file(handle, (void*)buf, size, offset);
}

int kfs_unlink(const char* path) {
  print_info("unlink", path);

  // get file handle
  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->key = path_to_key(path);
  req->type = GET;
  int sequence = kv_submit(req);
  struct kv_event* event =  kv_getevent(sequence);
  if (event->return_code != 0)
    return -ENOENT;

  // delete file
  req->type = DELETE;
  sequence = kv_submit(req);
  kv_getevent(sequence);

  // if file's type is KFS_HARDLINK, then dec inode's st_nlink,
  // if st_nlink drops to 0, remove inode.
  if (event->value->handle.file->type == KFS_HARDLINK) {
    free(req->key->data);
    req->key->data = NULL;
    req->key->val = 0;
    req->key->dir_ino = file_ino(&event->value->handle);
    req->type = GET;
    sequence = kv_submit(req);
    free(event);
    event = kv_getevent(sequence);
    Assert(event->return_code == 0);
    event->value->handle.file->stat.st_nlink -= 1;
    if (event->value->handle.file->stat.st_nlink == 0) {
      remove_file(&event->value->handle);
      req->type = DELETE;
      sequence = kv_submit(req);
      free(event);
      event = kv_getevent(sequence);
      Assert(event->return_code == 0);
    } else {
      req->value = event->value;
      req->type = UPDATE;
      sequence = kv_submit(req);
      free(event);
      event = kv_getevent(sequence);
      Assert(event->return_code == 0);
    }
  } else {
    // KFS_REG
    remove_file(&event->value->handle);
  }
  free(req->key);
  free(req);
  return event->return_code;
}

int kfs_getattr(const char* path, struct stat* st, struct fuse_file_info *fi) {
  print_info("getattr", path);

  // GNU's definitions of the attributes (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
  //    st_uid:   The user ID of the file's owner.
  //    st_gid:   The group ID of the file.
  //    st_atime: This is the last access time for the file.
  //    st_mtime: This is the time of the last modification to the contents of the file.
  //    st_mode:  Specifies the mode of the file. This includes file type information (see Testing File Type) and the file permission bits (see Permission Bits).
  //    st_nlink: The number of hard links to the file. This count keeps track of how many directories have entries for this file. If the count is ever decremented to zero, then the file itself is discarded as soon 
  //              as no process still holds it open. Symbolic links are not counted in the total.
  //    st_size:  This specifies the size of a regular file in bytes. For files that are really devices this field isn't usually meaningful. For symbolic links this specifies the length of the file name the link refers to.

  if (path_is_root(path)) {
    root_stat->st_atime = time(NULL);
    *st = *root_stat;
  } else {
    struct kv_event* event = get_file(path);
    if (event == NULL || event->return_code != 0)
      return -ENOENT;
    update_atime(&event->value->handle);
    *st = *root_stat;
    file_fill_stat(st, &event->value->handle.file->stat);
  }

  return 0;
}

int kfs_mkdir(const char* path, mode_t mode) {
  print_info("mkdir", path);

  size_t len = strlen(path);
  char* end = (char*)path + (len - 1);
  while (*end != '/')
    end--;

  ino_t dir_ino = dcache_lookup(dcache, path, (end - path) + 1);
  assert(dir_ino >= 0);
  struct kv_event* event = create_file(dir_ino, end + 1, mode | S_IFDIR,
      len - (end - path + 1), KFS_REG);
  return event->return_code;
}

int kfs_rmdir(const char* path) {
  print_info("rmdir", path);

  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->key = path_to_key(path);
  req->type = DELETE;

  int sequence = kv_submit(req);
  free(req->key);
  free(req);

  struct kv_event* event = kv_getevent(sequence);
  return event->return_code;
}

int kfs_opendir(const char* path, struct fuse_file_info* fi) {
  print_info("opendir", path);

  if (path_is_root(path)) {
    fi->fh = (uint64_t)&root_handle;
    return 0;
  }

  struct kv_event* event = get_file(path);
  if (event == NULL || event->return_code != 0)
    return -ENOENT;

  fi->fh = (uint64_t)&event->value->handle;
  update_atime(&event->value->handle);
  return 0;
}

void readdir_scan(void* key, void* value, void* scan_arg) {
  // KFS_HARDLINK
  if (((struct key*)key)->data == NULL)
    return;

  fuse_fill_dir_t filler = ((void**)scan_arg)[0];
  void* buf = ((void**)scan_arg)[1];
  off_t* offset = ((void**)scan_arg)[2];
  struct stat* stat = ((void**)scan_arg)[3];
  struct kfs_file_handle* handle = &(((struct value*)value)->handle);

  *offset += 1;
  file_fill_stat(stat, &handle->file->stat);
  filler(buf, ((struct key*)key)->data, stat, 0, 0);
}

int kfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
  print_info("readdir", path);

  struct kfs_file_handle* handle = (struct kfs_file_handle*)fi->fh;
  struct stat* stat_buf = malloc(sizeof(*stat_buf));
  file_fill_stat(stat_buf, &handle->file->stat);
  filler(buf, ".", stat_buf, 0, 0);
  filler(buf, "..", stat_buf, 0, 0);

  ino_t dir_ino = file_ino(handle);
  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->min_key = malloc(sizeof(*req->min_key));
  req->min_key->data = NULL;
  req->min_key->dir_ino = dir_ino;
  req->min_key->hash = 0;
  req->min_key->length = 0;
  req->max_key = malloc(sizeof(*req->max_key));
  req->max_key->data = NULL;
  req->max_key->dir_ino = dir_ino + 1;
  req->max_key->hash = 0;
  req->max_key->length = 0;
  req->scan = readdir_scan;
  void** scan_arg = malloc(sizeof(void*) * 4);
  scan_arg[0] = (void*)filler;
  scan_arg[1] = buf;
  scan_arg[2] = malloc(sizeof(off_t));
  scan_arg[3] = stat_buf;
  *(off_t*)(scan_arg[2]) = 0;
  req->scan_arg = (void*)scan_arg;
  req->type = SCAN;

  int sequence = kv_submit(req);
  free(req->min_key);
  free(req->max_key);
  free(req);
  struct kv_event* event = kv_getevent(sequence);
  Assert(event->return_code >= 0);
  free(scan_arg[2]);
  free(scan_arg[3]);
  free(scan_arg);
  free(event);

  return 0;
}

int kfs_releasedir(const char* path, struct fuse_file_info* fi) {
  print_info("releasedir", path);

  return 0;
}

int kfs_link(const char* src, const char* dst) {
  print_info("link dst", dst);
  print_info("link src", src);

  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->key = path_to_key(src);
  req->type = GET;

  int sequence = kv_submit(req);
  struct kv_event* event = kv_getevent(sequence);
  if (event->return_code != 0)
    return -ENOENT;
  
  struct kfs_file_handle* src_handle = &event->value->handle;
  if (src_handle->file->type == KFS_REG) {
    req->value = (struct value*)src_handle;
    src_handle->file->stat.st_nlink += 1;
    src_handle->file->type = KFS_HARDLINK;
    req->type = UPDATE;

    sequence = kv_submit(req);
    event = kv_getevent(sequence);
    Assert(event->return_code == 0);

    free(req->key->data);
    req->key->data = NULL;
    req->key->val = 0;
    req->key->dir_ino = src_handle->file->stat.st_ino;

    req->type = PUT;

    sequence = kv_submit(req);
    free(req->key);
    event = kv_getevent(sequence);
    Assert(event->return_code == 0);
  } else {
    free(req->key->data);
    free(req->key);
  }

  req->key = path_to_key(dst);
  req->value = (struct value*)src_handle;
  req->type = PUT;

  sequence = kv_submit(req);
  free(req->key->data);
  free(req->key);
  free(req);
  event = kv_getevent(sequence);
  Assert(event->return_code == 0);

  return event->return_code;
}

int kfs_symlink(const char* src, const char* dst) {
  print_info("symlink", src);
  print_info("symlink", dst);

  size_t len = strlen(dst);
  char* end = (char*)dst + (len - 1);
  while (*end != '/')
    end--;

  ino_t dir_ino = dcache_lookup(dcache, dst, (end - dst) + 1);
  assert(dir_ino >= 0);
  mode_t mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
  struct kv_event* event = create_file(dir_ino, end + 1, mode, len - (end - dst + 1), KFS_HARDLINK | KFS_SYMLINK);
  if (event->return_code != 0)
    return event->return_code;

  struct kfs_file_handle* handle = &event->value->handle;
  free(event);

  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->key = malloc(sizeof(*req->key));
  req->key->data = NULL;
  req->key->val = 0;
  req->key->dir_ino = handle->file->stat.st_ino;
  req->value = malloc(sizeof(*req->value));
  req->value->handle.file = malloc(sizeof(*req->value->handle.file));
  req->value->handle.file->type = KFS_SYMLINK;
  req->type = PUT;

  int src_len = strlen(src) + 1;
  int max_node_size = sizeof(struct kfs_stat);
  int seq = 0;
  while (src_len > 0) {
    req->key->hash = seq;
    if (src_len > max_node_size) {
      req->value->handle.file->next = 1;
      memcpy(req->value->handle.file->blob, src, max_node_size);
    } else {
      req->value->handle.file->next = 0;
      memcpy(req->value->handle.file->blob, src, src_len);
    }
    int sequence = kv_submit(req);
    event = kv_getevent(sequence);
    Assert(event->return_code == 0);
    if (event->return_code != 0)
      return event->return_code;
    free(event);
    src_len -= max_node_size;
    src += max_node_size;
    seq++;
  }
  free(req->key);
  free(req->value->handle.file);
  free(req->value);
  free(req);
  return 0;
}

int kfs_readlink(const char* path, char* buf, size_t size) {
  print_info("readlink", path);
  printf("readlink %lu\n", size);

  struct kv_request* req = malloc(sizeof(*req));
  memset(req, 0, sizeof(*req));
  req->key = path_to_key(path);
  req->type = GET;

  int sequence = kv_submit(req);
  struct kv_event* event = kv_getevent(sequence);
  if (event->return_code != 0)
    return -ENOENT;

  free(req->key->data);
  req->key->data = NULL;
  req->key->val = 0;
  req->key->dir_ino = event->value->handle.file->stat.st_ino;

  int seq = 0;
  int blob_size = sizeof(struct kfs_stat);
  while (1) {
    req->key->hash = seq;
    sequence = kv_submit(req);
    free(event);
    event = kv_getevent(sequence);
    Assert(event->return_code == 0);
    Assert(event->value->handle.file->type == KFS_SYMLINK);
    if (event->value->handle.file->next) {
      memcpy(buf, event->value->handle.file->blob, blob_size);
      buf += blob_size;
      Assert(size >= blob_size);
      size -= blob_size;
      seq++;
    } else {
      memcpy(buf, event->value->handle.file->blob, blob_size);
      buf += blob_size;
      Assert(size >= blob_size);
      size -= blob_size;
      break;
    }
  }

  free(event);
  free(req->key);
  free(req);
  return 0;
}

static struct fuse_operations kfs_operations = {
  .init       = kfs_init,
  .getattr    = kfs_getattr,
  .opendir    = kfs_opendir,
  .readdir    = kfs_readdir,
  .releasedir = NULL,
  .mkdir      = kfs_mkdir,
  .rmdir      = kfs_rmdir,
  .rename     = NULL,

  .link       = kfs_link,
  .symlink    = kfs_symlink,
  .readlink   = kfs_readlink,

  .open       = kfs_open,
  .read       = kfs_read,
  .write      = kfs_write,
  .create     = kfs_create,
  .mknod      = NULL,
  .unlink     = kfs_unlink,
  .release    = NULL,
  .chmod      = NULL,
  .chown      = NULL,

  .truncate   = NULL,
  .access     = NULL,
  .utimens    = NULL,
  .destroy    = kfs_destroy,
};

void show_help(const char* progname) {
  printf("usage: %s [options] --datadir=<datadir> mountdir\n", progname);
}

int main(int argc, char** argv) {
  options.mountdir = strdup(argv[argc - 1]);
  options.workdir = getcwd(NULL, 0);

  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Set defaults -- we have to use strdup so that
     fuse_opt_parse can free the defaults if other
     values are specified */
  // options.filename = strdup("hello");
  // options.contents = strdup("Hello World!\n");

  /* Parse options */
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;

#ifdef DEBUG
  printf("workdir: %s, datadir: %s, mountdir: %s\n", options.workdir,
      options.datadir, options.mountdir);

  sprintf(log_path, "%s/log", options.workdir);
  int fd = open(log_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  char buf[256];
  int len = sprintf(buf, "main pid: %d, tid: %d\n", getpid(), gettid());
  pwrite(fd, buf, len, 0);
  close(fd);
#endif

  /* When --help is specified, first print our own file-system
     specific help text, then signal fuse_main to show
     additional help (by adding `--help` to the options again)
     without usage: line (by setting argv[0] to the empty
     string) */
  if (options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }

  ret = fuse_main(args.argc, args.argv, &kfs_operations, NULL);
  fuse_opt_free_args(&args);
  return ret;
}