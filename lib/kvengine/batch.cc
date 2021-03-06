#include "kvengine/batch.h"
#include "kvengine/db.h"

namespace kvengine {

Batch::Batch(const Batch& other)
  : requests_(other.requests_), callback_(other.callback_),
    cur_worker_(other.cur_worker_), db_(other.db_)
{
}

void Batch::FinishRequest(Slice* val, int res) {
  if (requests_.empty()) {
    if (callback_) {
      Respond* respond = new Respond(val, res);
      callback_(respond);
      delete respond;
    }
    delete this;
  } else {
    db_->Submit(this, false);
  }
}

void Batch::Clear() {
  while (!requests_.empty())
    requests_.pop_front();
  callback_ = nullptr;
}

void Batch::Get(const Slice& key) {
  Request* req(new Request);
  req->type = Request::ReqType::GET;
  req->key = new Slice(key, true);
  requests_.push_back(req);
}

void Batch::Get(Slice* key) {
  Request* req(new Request);
  req->type = Request::ReqType::GET;
  req->key = key;
  requests_.push_back(req);
}

void Batch::Put(const Slice& key, const Slice& value) {
  Request* req(new Request);
  req->type = Request::ReqType::PUT;
  req->key = new Slice(key, true);
  req->value = new Slice(value, true);
  requests_.push_back(req);
}

void Batch::Put(Slice* key, Slice* value) {
  Request* req(new Request);
  req->type = Request::ReqType::PUT;
  req->key = key;
  req->value = value;
  requests_.push_back(req);
}

void Batch::Update(const Slice& key, const Slice& value) {
  Request* req(new Request);
  req->type = Request::ReqType::UPDATE;
  req->key = new Slice(key, true);
  req->value = new Slice(value, true);
  requests_.push_back(req);
}

void Batch::Update(Slice* key, Slice* value) {
  Request* req(new Request);
  req->type = Request::ReqType::UPDATE;
  req->key = key;
  req->value = value;
  requests_.push_back(req);
}

void Batch::Delete(const Slice& key) {
  Request* req(new Request);
  req->type = Request::ReqType::DELETE;
  req->key = new Slice(key, true);
  requests_.push_back(req);
}

void Batch::Delete(Slice* key) {
  Request* req(new Request);
  req->type = Request::ReqType::DELETE;
  req->key = key;
  requests_.push_back(req);
}

void Batch::Scan(const Slice& min_key, const Slice& max_key,
                 std::function<bool(Slice*,Slice*)> scan_callback) {
  Request* req(new Request);
  req->type = Request::ReqType::SCAN;
  req->min_key = new Slice(min_key, true);
  req->max_key = new Slice(max_key, true);
  req->scan_callback = std::move(scan_callback);
  requests_.push_back(req);
}

void Batch::Scan(Slice* min_key, Slice* max_key,
                 std::function<bool(Slice*,Slice*)> scan_callback) {
  Request* req(new Request);
  req->type = Request::ReqType::SCAN;
  req->min_key = min_key;
  req->max_key = max_key;
  req->scan_callback = scan_callback;
  requests_.push_back(req);
}

void Batch::AddCallback(std::function<void(Respond*)> callback) {
  callback_ = std::move(callback);
}

std::ostream &operator<<(std::ostream &os, const Batch& b) {
  for (auto& req : b.requests_) {
    if (req->type != Batch::Request::ReqType::SCAN)
      os << static_cast<int>(req->type) << " " << req->key->ToString() << std::endl;
    else
      os << static_cast<int>(req->type) << " " << req->min_key->ToString()
         << " " << req->max_key->ToString() << std::endl;
  }
  return os;
}

} // namespace kvengine