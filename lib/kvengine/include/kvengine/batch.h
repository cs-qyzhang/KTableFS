#ifndef KTABLEFS_KVENGINE_BATCH_H_
#define KTABLEFS_KVENGINE_BATCH_H_

#include <deque>
#include <functional>
#include "kvengine/slice.h"

namespace kvengine {

class Worker;
class LocalFileSet;
class AIO;
class DB;

struct Respond {
  int res;
  Slice* data;

  Respond(Slice* data, int res) : res(res), data(data) {}
};

class Batch {
 private:
  struct Request {
    enum class ReqType { GET, PUT, UPDATE, DELETE, SCAN } type;
    Slice* key;
    Slice* min_key;
    Slice* max_key;
    Slice* value;
    std::function<bool(Slice*,Slice*)> scan_callback;
    Request()
      : key(nullptr), min_key(nullptr), max_key(nullptr), value(nullptr)
    {}
    ~Request() {
      if (key) delete key;
      if (min_key) delete min_key;
      if (max_key) delete max_key;
      if (value) delete value;
    }
  };

  std::deque<Request*> requests_;
  std::function<void(Respond*)> callback_;
  Worker* cur_worker_;
  DB* db_;

  void FinishRequest(Slice* val, int res);

  friend class AIO;
  friend class DB;
  friend class Worker;
  friend std::ostream &operator<<(std::ostream &os, const Batch& b);

 public:
  Batch() {}
  Batch(const Batch& batch);

  void Clear();
  void Get(const Slice& key);
  void Get(Slice* key);
  void Put(const Slice& key, const Slice& value);
  void Put(Slice* key, Slice* value);
  void Update(const Slice& key, const Slice& value);
  void Update(Slice* key, Slice* value);
  void Delete(const Slice& key);
  void Delete(Slice* key);
  void Scan(const Slice& min_key, const Slice& max_key,
            std::function<bool(Slice*,Slice*)> scan_callback);
  void Scan(Slice* min_key, Slice* max_key,
            std::function<bool(Slice*,Slice*)> scan_callback);
  void AddCallback(std::function<void(Respond*)> callback);
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_BATCH_H_