#ifndef KTABLEFS_KVENGINE_THREADSAFE_QUEUE_H_
#define KTABLEFS_KVENGINE_THREADSAFE_QUEUE_H_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace kvengine {

template<typename T>
class ThreadsafeQueue {
 private:
  std::queue<T> queue_;
  mutable std::mutex m_;
  std::condition_variable data_cond;

 public:
  ThreadsafeQueue() {}
  ThreadsafeQueue(const ThreadsafeQueue& other) {
    std::lock_guard<std::mutex> lock(other.m_);
    queue_ = other.queue_;
  }

  void Push(T data) {
    std::lock_guard<std::mutex> lock(m_);
    queue_.push(data);
    data_cond.notify_one();
  }

  std::shared_ptr<T> WaitAndPop() {
    std::unique_lock<std::mutex> lock(m_);
    data_cond.wait(lock, [&]{ return !queue_.empty(); });
    std::shared_ptr<T> const res(std::make_shared<T>(queue_.front()));
    queue_.pop();
    return res;
  }

  void WaitAndPop(T& data) {
    std::unique_lock<std::mutex> lock(m_);
    data_cond.wait(lock, [&]{ return !queue_.empty(); });
    data = queue_.front();
    queue_.pop();
  }

  void WaitData() {
    std::unique_lock<std::mutex> lock(m_);
    data_cond.wait(lock, [&]{ return !queue_.empty(); });
  }

  std::shared_ptr<T> TryPop() {
    std::lock_guard<std::mutex> lock(m_);
    if (queue_.empty())
      return std::shared_ptr<T>();
    std::shared_ptr<T> const res(std::make_shared<T>(queue_.front()));
    queue_.pop();
    return res;
  }

  bool TryPop(T& data) {
    std::lock_guard<std::mutex> lock(m_);
    if (queue_.empty())
      return false;
    data = queue_.front();
    queue_.pop();
    return true;
  }

  bool Empty() const {
    std::lock_guard<std::mutex> lock(m_);
    return queue_.empty();
  }
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_THREADSAFE_QUEUE_H_