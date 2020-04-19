#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "kvengine/db.h"
#include "ktablefs.h"
#include "file.h"
#include "file_data.h"

namespace ktablefs {

using kvengine::DB;

uid_t KTableFS::st_uid;
gid_t KTableFS::st_gid;
dev_t KTableFS::st_dev;
dev_t KTableFS::st_rdev;
blksize_t KTableFS::st_blksize;
double KTableFS::timeout;

KTableFS* KTableFS::fs;

KTableFS::KTableFS(std::string data_dir)
  : data_dir_(data_dir), cur_ino_(FUSE_ROOT_ID)
{
  db_ = new DB();
  db_->Open(data_dir_ + "/DB", true, 1);

  struct stat stat_buf;
  int res = stat(data_dir_.c_str(), &stat_buf);
  assert(res == 0);
  st_uid = stat_buf.st_uid;
  st_gid = stat_buf.st_gid;
  st_dev = stat_buf.st_dev;
  st_rdev = stat_buf.st_rdev;
  st_blksize = stat_buf.st_blksize;
  timeout = 1.0;

  file_data_ = new FileData(data_dir_);
  root_ = new FileHandle();
  root_->file_.st_ino = FUSE_ROOT_ID;
  root_->file_.st_mode = stat_buf.st_mode;
  root_->file_.type = File::REGULAR;
  root_->UpdateATime();
  root_->UpdateMTime();
}

void KTableFS::Init(void *userdata, struct fuse_conn_info *conn) {
  fs = new KTableFS(option.datadir);
}

void KTableFS::Destroy(void *userdata) {
  delete fs;
}

} // namespace ktablefs