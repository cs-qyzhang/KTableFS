#ifndef KTABLEFS_KVENGINE_DB_H_
#define KTABLEFS_KVENGINE_DB_H_

#include <vector>
#include <functional>

namespace kvengine {

class Slice;
class Batch;
class Worker;

class DB {
 private:
  std::vector<Worker*> workers_;
  std::function<int(const Slice*,int)> split_func_;

 public:
  DB();
  bool Open(std::string work_dir, bool create = false, int thread_num = 4);
  void SetSplitFunction(std::function<int(const Slice*,int)> func);
  void Submit(Batch*, bool cp = true);
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_DB_H_