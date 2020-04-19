#include <cassert>
#include <cstdlib>
#include "kvengine/slice.h"
#include "pagecache.h"
#include "aio.h"

namespace kvengine {

PageCache::PageCache(int pages)
  : newest_(nullptr), oldest_(nullptr),
    used_page_(0), max_page_(pages)
{
  data_ = reinterpret_cast<char*>(std::aligned_alloc(PAGE_SIZE, PAGE_SIZE * pages));
  assert(data_);
}

PageCache::~PageCache() {
  std::free(data_);
  while (newest_ != nullptr) {
    Entry* old = newest_;
    newest_ = newest_->next;
    delete old;
  }
}

void PageCache::Update_(Entry* ent) {
  if (newest_ == nullptr) {
    newest_ = ent;
    oldest_ = ent;
    ent->prev = nullptr;
    ent->next = nullptr;
  } else {
    ent->next = newest_;
    ent->prev = nullptr;
    newest_->prev = ent;
  }
}

PageCache::Entry* PageCache::NewEntry(uint64_t hash) {
  Entry* ent;
  if (used_page_ < max_page_) {
    ent = new Entry;
    ent->page = &data_[used_page_++ * PAGE_SIZE];
  } else {
    ent = oldest_;
    while (ent && ent->wait)
      ent = ent->prev;
    assert(ent);
    if (ent == oldest_) {
      oldest_ = ent->prev;
      oldest_->next = nullptr;
    } else {
      ent->prev->next = ent->next;
      ent->next->prev = ent->prev;
    }
    index_.erase(ent->hash);
  }
  ent->hash = hash;
  ent->valid = true;
  ent->wait = true;
  index_.emplace(hash, ent);
  Update_(ent);
  return ent;
}

PageCache::Entry* PageCache::Lookup(uint64_t hash) {
  Entry* p = newest_;
  for (int i = 0; i < 10; ++i) {
    if (p == nullptr)
      return nullptr;
    if (p->valid && p->hash == hash) {
      Update_(p);
      return p;
    }
    p = p->next;
  }

  auto iter = index_.find(hash);
  if (iter != index_.end() && iter->second->valid) {
    Update_(iter->second);
    return iter->second;
  } else {
    return nullptr;
  }
}

void PageCache::WriteCallback::operator()(AIO* aio) {
  if (aio->Success()) {
    std::memcpy(ent_->page + off_, data_->data(), data_->size());
  } else {
    assert(0);
  }
}

void PageCache::ReadCallback::operator()(AIO* aio) {
  if (aio->Success()) {
    ent_->valid = true;
    ent_->wait = false;
  }
}

PageCache::WriteCallback PageCache::MakeWriteCallback(Entry* ent, const Slice* data, off_t off) {
  return WriteCallback(this, ent, data, off);
}

PageCache::ReadCallback PageCache::MakeReadCallback(Entry* ent) {
  return ReadCallback(this, ent);
}

} // namespace kvengine