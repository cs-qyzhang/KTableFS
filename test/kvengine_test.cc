#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>
#include <algorithm>
#include "kvengine/slice.h"
#include "kvengine/db.h"
#include "kvengine/batch.h"
#include "dbg.h"

using kvengine::Batch;
using kvengine::Respond;
using kvengine::Slice;
using kvengine::DB;

using namespace std;

DB* db;

class TestThread {
 private:
  thread t_;

  mutex get_mutex;
  condition_variable get_cv;
  Slice* get_res;
  bool get_ready;

  mutex put_mutex;
  condition_variable put_cv;
  int put_res;
  bool put_ready;

  mutex update_mutex;
  condition_variable update_cv;
  int update_res;
  bool update_ready;

  mutex delete_mutex;
  condition_variable delete_cv;
  int delete_res;
  bool delete_ready;

  class GetCallback {
   private:
    TestThread* t_;
   public:
    GetCallback(TestThread* t) : t_(t) {}
    void operator()(Respond* respond) {
      lock_guard<mutex> lock(t_->get_mutex);
      if (respond->res == 0)
        t_->get_res = respond->data;
      else
        t_->get_res = nullptr;
      t_->get_ready = true;
      t_->get_cv.notify_one();
    }
  };

  Slice* Get(const Slice& s) {
    Batch batch;
    batch.Get(s);
    batch.AddCallback(GetCallback(this));
    get_ready = false;
    db->Submit(&batch);
    unique_lock<mutex> lock(get_mutex);
    get_cv.wait(lock, [&]{ return get_ready; });
    return get_res;
  }

  class PutCallback {
   private:
    TestThread* t_;
   public:
    PutCallback(TestThread* t) : t_(t) {}
    void operator()(Respond* respond) {
      lock_guard<mutex> lock(t_->put_mutex);
      t_->put_res = respond->res;
      t_->put_ready = true;
      t_->put_cv.notify_one();
    }
  };

  int Put(const Slice& key, const Slice& data) {
    Batch batch;
    batch.Put(key, data);
    batch.AddCallback(PutCallback(this));
    put_ready = false;
    db->Submit(&batch);
    unique_lock<mutex> lock(put_mutex);
    put_cv.wait(lock, [&]{ return put_ready; });
    return put_res;
  }

  class UpdateCallback {
   private:
    TestThread* t_;
   public:
    UpdateCallback(TestThread* t) : t_(t) {}
    void operator()(Respond* respond) {
      lock_guard<mutex> lock(t_->update_mutex);
      t_->update_res = respond->res;
      t_->update_ready = true;
      t_->update_cv.notify_one();
    }
  };

  int Update(const Slice& key, const Slice& data) {
    Batch batch;
    batch.Update(key, data);
    batch.AddCallback(UpdateCallback(this));
    update_ready = false;
    db->Submit(&batch);
    unique_lock<mutex> lock(update_mutex);
    update_cv.wait(lock, [&]{ return update_ready; });
    return update_res;
  }

  class DeleteCallback {
   private:
    TestThread* t_;
   public:
    DeleteCallback(TestThread* t) : t_(t) {}
    void operator()(Respond* respond) {
      lock_guard<mutex> lock(t_->delete_mutex);
      t_->delete_res = respond->res;
      t_->delete_ready = true;
      t_->delete_cv.notify_one();
    }
  };

  int Delete(const Slice& key) {
    Batch batch;
    batch.Delete(key);
    batch.AddCallback(DeleteCallback(this));
    delete_ready = false;
    db->Submit(&batch);
    unique_lock<mutex> lock(delete_mutex);
    delete_cv.wait(lock, [&]{ return delete_ready; });
    return delete_res;
  }

  void Main(int begin, int end) {
    for (int i = begin; i < end; ++i) {
      Slice* res = Get(to_string(i));
      assert(res == nullptr);
    }

    for (int i = begin; i < end; ++i) {
      int res = Put(to_string(i), to_string(i + 1));
      assert(res == 0);
    }

    for (int i = begin; i < end; ++i) {
      Slice* res = Get(to_string(i));
      assert(res && res->ToString() == to_string(i + 1));
    }

    for (int i = begin; i < end; ++i) {
      int res = Update(to_string(i), to_string(i - 1));
      assert(res == 0);
    }

    for (int i = begin; i < end; ++i) {
      Slice* res = Get(to_string(i));
      assert(res && res->ToString() == to_string(i - 1));
    }

    for (int i = begin; i < end; i = i + 2) {
      int res = Delete(to_string(i));
      assert(res == 0);
    }

    for (int i = begin; i < end; i = i + 2) {
      Slice* res = Get(to_string(i));
      assert(res == nullptr);
    }

    for (int i = begin + 1; i < end; i = i + 2) {
      Slice* res = Get(to_string(i));
      assert(res && res->ToString() == to_string(i - 1));
    }
  }

 public:
  TestThread(int begin, int end) : t_(&TestThread::Main, this, begin, end) {}
  ~TestThread() {
    t_.join();
  }
};

int main(void) {
  int thread_num = 8;
  int total = 160000;
  int per_thread = total / thread_num;

  db = new DB();
  assert(db->Open("/home/qyzhang/Projects/GraduationProject/code/KTableFS/build/metadir", true, thread_num));

  vector<TestThread*> t;
  for (int i = 0; i < thread_num - 1; ++i)
    t.emplace_back(new TestThread(per_thread * i, per_thread * (i + 1)));
  t.emplace_back(new TestThread(per_thread * (thread_num - 1), total));

  for (auto x : t)
    delete x;

  delete db;

  return 0;
}