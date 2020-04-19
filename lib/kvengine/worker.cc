#include <thread>
#include <filesystem>
#include <queue>
#include "kvengine/batch.h"
#include "worker.h"
#include "aio.h"

namespace kvengine {

Worker::Worker(int id, std::string work_dir, bool create)
  : id_(id), work_dir_(work_dir + "/" + std::to_string(id)),
    slab_(work_dir_, (1 << 16), {64, 128, 256, 512, 1024})
{
  AIO::Setup(&aio_ctx_);
  if (create && !std::filesystem::exists(work_dir_))
    std::filesystem::create_directory(work_dir_);
  thread_ = new std::thread(&Worker::Main_, this);
#ifdef DEBUG
  debug_.open(work_dir_ + "/debug");
  assert(debug_.good());
#endif
}

Worker::~Worker() {
  assert(thread_->joinable());
  delete thread_;
  AIO::Destroy(aio_ctx_);
}

class PutCallback {
 private:
  std::map<Slice, slab_t>* index_;
  Slice key_;
  slab_t slab_idx_;

 public:
  PutCallback(std::map<Slice, slab_t>* index, const Slice* key, slab_t slab)
    : index_(index), key_(key->data(), key->size()), slab_idx_(slab)
  {
  }

  void operator()(AIO* aio) {
    if (aio->Success())
      index_->insert({key_, slab_idx_});
  }
};

class DeleteCallback {
 private:
  std::map<Slice, slab_t>* index_;
  Slice key_;

 public:
  DeleteCallback(std::map<Slice, slab_t>* index, const Slice* key)
    : index_(index), key_(key->data(), key->size())
  {
  }

  void operator()(AIO* aio) {
    if (aio->Success())
      index_->erase(key_);
  }
};

void Worker::ProcessRequest_(Batch* batch) {
  assert(!batch->requests_.empty());
  if (batch->requests_.empty())
    return;
  Batch::Request* req = batch->requests_.front();
  batch->requests_.pop_front();

  std::map<Slice,slab_t>::iterator slab_iter;
  slab_t slab_idx;
  AIO* aio;
  switch (req->type) {
    case Batch::Request::ReqType::GET:
      slab_iter = index_.find(*req->key);
      if (slab_iter == index_.end()) {
        batch->FinishRequest(nullptr, -ENOENT);
        return;
      }
      aio = new AIO(batch);
      slab_.Read(slab_iter->second, aio);
      break;
    case Batch::Request::ReqType::PUT:
      slab_iter = index_.find(*req->key);
      if (slab_iter != index_.end()) {
        batch->FinishRequest(nullptr, -EEXIST);
        return;
      }
      aio = new AIO(batch);
      slab_idx = slab_.Create(req->key, req->value);
      aio->AddCallback(PutCallback(&index_, req->key, slab_idx));
      slab_.Write(slab_idx, req->key, req->value, aio);
      break;
    case Batch::Request::ReqType::UPDATE:
      slab_iter = index_.find(*req->key);
      if (slab_iter == index_.end()) {
        batch->FinishRequest(nullptr, -ENOENT);
        return;
      }
      aio = new AIO(batch);
      slab_.Write(slab_iter->second, req->key, req->value, aio);
      break;
    case Batch::Request::ReqType::DELETE:
      slab_iter = index_.find(*req->key);
      if (slab_iter == index_.end()) {
        batch->FinishRequest(nullptr, -ENOENT);
        return;
      }
      aio = new AIO(batch);
      aio->AddCallback(DeleteCallback(&index_, req->key));
      slab_.Delete(slab_iter->second, aio);
      break;
    case Batch::Request::ReqType::SCAN:
      break;
  }
}

void Worker::Main_() {
  std::queue<AIO*> fake_aio;
  while (true) {
    // wait and pop batch
    batch_queue_.WaitData();
    Batch* batch;
    int cnt = 0;
#ifdef DEBUG
    debug_ << "begin" << std::endl;
#endif
    while (batch_queue_.TryPop(batch)) {
#ifdef DEBUG
      debug_ << *batch;
#endif
      ProcessRequest_(batch);
      if (++cnt == AIO::MAX_EVENT)
        break;
    }
#ifdef DEBUG
    debug_ << "end" << std::endl;
#endif

    int io_nr = 0;
    static thread_local iocb* iocbs[AIO::MAX_EVENT];
    static thread_local io_event io_events[AIO::MAX_EVENT];

    for (auto& aio : io_queue_) {
      if (aio->submit_)
        iocbs[io_nr++] = aio->iocb_;
    }

    if (io_nr) {
      int res = AIO::Submit(aio_ctx_, io_nr, iocbs);
      if (res < 0)
        perror(strerror(errno));
      assert(res == io_nr);
      res = AIO::GetEvents(aio_ctx_, io_nr, io_events, NULL);
      assert(res == io_nr);
    }
    io_nr = 0;
    for (auto& aio : io_queue_) {
      if (aio->submit_)
        aio->SetEvent(&io_events[io_nr++]);
      aio->Finish();
      delete aio;
    }
    io_queue_.clear();
  }
}

void Worker::IOEnqueue(AIO* io) {
  io_queue_.push_back(io);
}

void Worker::BatchEnqueue(Batch* batch) {
  batch_queue_.Push(batch);
}

void Worker::Submit(Batch* batch) {
  batch->cur_worker_ = this;
  BatchEnqueue(batch);
}

} // namespace kvengine
