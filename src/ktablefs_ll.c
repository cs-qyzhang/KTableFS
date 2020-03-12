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
#include "util/hash.h"
#include "file.h"
#include "dcache.h"
#include "options.h"
#include "kv_impl.h"
#include "io.h"
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"
#include "debug.h"

struct kfs_data {
  struct kfs_handle root;
};


static inline struct kfs_data* kfs_data(fuse_req_t req) {
  return (struct kfd_data*)fuse_req_userdata(req);
}

static struct kfs_file_handle* kfs_file_handle(fuse_req_t req, fuse_ino_t ino) {
  if (ino == FUSE_ROOT_ID)
    return &kfs_data(req)->root;
  else
    return (struct kfs_file_handle*)(uintptr_t)ino;
}

void kfs_lo_init(void *userdata, struct fuse_conn_info *conn) {

}

void kfs_lo_destroy(void *userdata) {

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
void kfs_lo_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {

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
void kfs_lo_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {

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
void kfs_lo_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {

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
                 struct fuse_file_info *fi) {

}

static const struct fuse_lowlevel_ops lo_oper = {
  .init		= lo_init,
  .lookup		= lo_lookup,
  .mkdir		= lo_mkdir,
  .mknod		= lo_mknod,
  .symlink	= lo_symlink,
  .link		= lo_link,
  .unlink		= lo_unlink,
  .rmdir		= lo_rmdir,
  .rename		= lo_rename,
  .forget		= lo_forget,
  .forget_multi	= lo_forget_multi,
  .getattr	= lo_getattr,
  .setattr	= lo_setattr,
  .readlink	= lo_readlink,
  .opendir	= lo_opendir,
  .readdir	= lo_readdir,
  .readdirplus	= lo_readdirplus,
  .releasedir	= lo_releasedir,
  .fsyncdir	= lo_fsyncdir,
  .create		= lo_create,
  .open		= lo_open,
  .release	= lo_release,
  .flush		= lo_flush,
  .fsync		= lo_fsync,
  .read		= lo_read,
  .write_buf      = lo_write_buf,
  .statfs		= lo_statfs,
  .fallocate	= lo_fallocate,
  .flock		= lo_flock,
  .getxattr	= lo_getxattr,
  .listxattr	= lo_listxattr,
  .setxattr	= lo_setxattr,
  .removexattr	= lo_removexattr,
#ifdef HAVE_COPY_FILE_RANGE
  .copy_file_range = lo_copy_file_range,
#endif
  .lseek		= lo_lseek,
};

int main(int argc, char **argv) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_session *se;
  struct fuse_cmdline_opts opts;
  struct lo_data lo = { .debug = 1,
                        .writeback = 0 };
  int ret = -1;

  /* Don't mask creation mode, kernel already did that */
  umask(0);

  pthread_mutex_init(&lo.mutex, NULL);
  lo.root.next = lo.root.prev = &lo.root;
  lo.root.fd = -1;
  lo.cache = CACHE_NORMAL;

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

  if (fuse_opt_parse(&args, &lo, lo_opts, NULL)== -1)
    return 1;

  lo.debug = opts.debug;
  lo.root.refcount = 2;
  if (lo.source) {
    struct stat stat;
    int res;

    res = lstat(lo.source, &stat);
    if (res == -1) {
      fuse_log(FUSE_LOG_ERR, "failed to stat source (\"%s\"): %m\n",
         lo.source);
      exit(1);
    }
    if (!S_ISDIR(stat.st_mode)) {
      fuse_log(FUSE_LOG_ERR, "source is not a directory\n");
      exit(1);
    }

  } else {
    lo.source = "/";
  }
  lo.root.is_symlink = false;
  if (!lo.timeout_set) {
    switch (lo.cache) {
    case CACHE_NEVER:
      lo.timeout = 0.0;
      break;

    case CACHE_NORMAL:
      lo.timeout = 1.0;
      break;

    case CACHE_ALWAYS:
      lo.timeout = 86400.0;
      break;
    }
  } else if (lo.timeout < 0) {
    fuse_log(FUSE_LOG_ERR, "timeout is negative (%lf)\n",
       lo.timeout);
    exit(1);
  }

  lo.root.fd = open(lo.source, O_PATH);
  if (lo.root.fd == -1) {
    fuse_log(FUSE_LOG_ERR, "open(\"%s\", O_PATH): %m\n",
       lo.source);
    exit(1);
  }

  se = fuse_session_new(&args, &lo_oper, sizeof(lo_oper), &lo);
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

  if (lo.root.fd >= 0)
    close(lo.root.fd);

  return ret ? 1 : 0;
}