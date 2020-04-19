#ifndef KTABLEFS_KVENGINE_WORKER_H_
#define KTABLEFS_KVENGINE_WORKER_H_

#include <deque>
#include <map>
#include <thread>
#include <fstream>
#include "kvengine/slice.h"
#include "kvengine/batch.h"
#include "threadsafe_queue.h"
#include "fileslab.h"

namespace kvengine {

class AIO;

class Worker {
 private:
  int id_;
  std::string work_dir_;
  kvengine::ThreadsafeQueue<Batch*> batch_queue_;
  std::deque<AIO*> io_queue_;
  std::map<Slice, slab_t> index_;
  FileSlab slab_;
  aio_context_t aio_ctx_;
  std::thread* thread_;
#ifdef DEBUG
  std::ofstream debug_;
#endif

  void ProcessRequest_(Batch* batch);
  void Main_();

 public:
  Worker(int id, std::string work_dir, bool create = false);
  Worker(const Worker&) = delete;
  Worker(Worker&&) = delete;
  ~Worker();

  void Submit(Batch*);
  void BatchEnqueue(Batch*);
  void IOEnqueue(AIO*);
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_WORKER_H_