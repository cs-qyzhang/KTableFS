#include <iostream>
#include "fuse_header.h"
#include "ktablefs.h"

using namespace std;
using ktablefs::KTableFS;

KTableFS::options KTableFS::option;

#define OPTION(t, p)  { t, offsetof(KTableFS::options, p), 1 }
static const struct fuse_opt option_spec[] = {
  OPTION("--datadir=%s", datadir),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

static const struct fuse_lowlevel_ops lo_oper = {
  .init            = KTableFS::Init,
  .destroy         = KTableFS::Destroy,
  .lookup          = KTableFS::Lookup,
  .forget          = nullptr,
  .getattr         = KTableFS::GetAttr,
  .setattr         = nullptr,
  .readlink        = nullptr,
  .mknod           = KTableFS::Mknod,
  .mkdir           = KTableFS::Mkdir,
  .unlink          = nullptr,
  .rmdir           = nullptr,
  .symlink         = nullptr,
  .rename          = nullptr,
  .link            = nullptr,
  .open            = KTableFS::Open,
  .read            = KTableFS::Read,
  .write           = KTableFS::Write,
  .flush           = nullptr,
  .release         = nullptr,
  .fsync           = nullptr,
  .opendir         = KTableFS::OpenDir,
  .readdir         = KTableFS::ReadDir,
  .releasedir      = nullptr,
  .statfs          = nullptr,
  .write_buf       = nullptr,
  .fallocate       = nullptr,
  .readdirplus     = nullptr,
  .copy_file_range = nullptr,
};

int main(int argc, char **argv) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_session *se;
  struct fuse_cmdline_opts opts;
  int ret = -1;

  /* Don't mask creation mode, kernel already did that */
  umask(0);

  if (fuse_parse_cmdline(&args, &opts) != 0)
    return 1;
  if (opts.show_help) {
    cout << "usage: " << argv[0] << " [options] <mountpoint>" << endl << endl;
    fuse_cmdline_help();
    fuse_lowlevel_help();
    ret = 0;
    goto err_out1;
  } else if (opts.show_version) {
    cout << "FUSE library version " << fuse_pkgversion() << endl;
    fuse_lowlevel_version();
    ret = 0;
    goto err_out1;
  }

  if(opts.mountpoint == nullptr) {
    cout << "usage: " << argv[0] << " [options] <mountpoint>" << endl;
    cout << "       " << argv[0] << " --help" << endl;
    ret = 1;
    goto err_out1;
  }

  if (fuse_opt_parse(&args, &KTableFS::option, option_spec, nullptr)== -1)
    return 1;

  se = fuse_session_new(&args, &lo_oper, sizeof(lo_oper), nullptr);
  if (se == nullptr)
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