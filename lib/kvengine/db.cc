#include <cassert>
#include <filesystem>
#include <fstream>
#include "kvengine/db.h"
#include "kvengine/slice.h"
#include "worker.h"

namespace kvengine {

int DefaultSplitFunc(const Slice* key, int thread_num) {
  return key->hash() % thread_num;
}

DB::DB()
  : split_func_(DefaultSplitFunc)
{}

void DB::SetSplitFunction(std::function<int(const Slice*,int)> func) {
  split_func_ = std::move(func);
}

bool DB::Open(std::string work_dir, bool create, int thread_num) {
  if (work_dir.size() > 1 && work_dir.back() == '/')
    work_dir.pop_back();
  if (create) {
    if (!std::filesystem::exists(work_dir))
      std::filesystem::create_directory(work_dir);
    std::ofstream manifest(work_dir + "/MANIFEST");
    assert(manifest.good());
    manifest << thread_num;
    manifest.close();
    for (int i = 0; i < thread_num; ++i) {
      Worker* worker = new Worker(i, work_dir, true);
      workers_.push_back(worker);
    }
  } else {
    if (!std::filesystem::exists(work_dir))
      return false;
    std::ifstream manifest(work_dir + "/MANIFEST");
    if (!manifest.good())
      return false;
    manifest >> thread_num;
    for (int i = 0; i < thread_num; ++i) {
      Worker* worker = new Worker(i, work_dir);
      workers_.push_back(worker);
    }
  }
  return true;
}

void DB::Submit(Batch* batch, bool cp) {
  if (cp) {
    Batch* old = batch;
    batch = new Batch(*old);
    old->Clear();
  }
  batch->db_ = this;
  auto req = batch->requests_.front();
  if (req->type == Batch::Request::ReqType::SCAN) {
    int thread_idx = reinterpret_cast<uintptr_t>(req->key);
    if (thread_idx == 0) {
      for (unsigned int i = 1; i < workers_.size(); ++i) {
        batch->Scan(req->min_key, req->max_key, req->scan_callback);
        batch->requests_.back()->key = reinterpret_cast<Slice*>(i);
      }
    }
    workers_[thread_idx]->Submit(batch);
  } else {
    Slice* key = req->key;
    int thread_idx = split_func_(key, workers_.size());
    workers_[thread_idx]->Submit(batch);
  }
}

} // namespace kvengine