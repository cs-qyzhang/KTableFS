#ifndef KTABLEFS_KVENGINE_PAGECACHE_H_
#define KTABLEFS_KVENGINE_PAGECACHE_H_

#include <map>
#include "aio.h"

namespace kvengine {

class PageCache {
 public:
  class Entry;

  // write callback class
  class WriteCallback {
   private:
    PageCache* pagecache_;
    PageCache::Entry* ent_;
    const Slice* data_;
    off_t off_;

   public:
    WriteCallback(PageCache* cache, PageCache::Entry* ent, const Slice* data, off_t off)
      : pagecache_(cache), ent_(ent), data_(data), off_(off)
    {}

    void operator()(AIO*);
  };

  // read callback class
  class ReadCallback {
   private:
    PageCache* pagecache_;
    PageCache::Entry* ent_;

   public:
    ReadCallback(PageCache* cache, PageCache::Entry* ent)
      : pagecache_(cache), ent_(ent)
    {}

    void operator()(AIO*);
  };

  // pagecache lru entry
  struct Entry {
    char* page;
    uint64_t hash;
    // wait for data, can't be replaced
    bool wait;

   private:
    Entry* prev;
    Entry* next;
    bool valid;

    friend class PageCache;
    friend class PageCache::WriteCallback;
    friend class PageCache::ReadCallback;
  };

 private:
  Entry* newest_;
  Entry* oldest_;
  char* data_;
  std::map<uint64_t, Entry*> index_;
  size_t used_page_;
  size_t max_page_;

  void Update_(Entry* ent);

 public:
  static const int PAGE_SIZE = 4096;

  explicit PageCache(int pages);
  PageCache(const PageCache&) = delete;
  PageCache(const PageCache&&) = delete;
  ~PageCache();

  // method
  Entry* Lookup(uint64_t hash);
  Entry* NewEntry(uint64_t hash);
  WriteCallback MakeWriteCallback(Entry* ent, const Slice* data, off_t off);
  ReadCallback MakeReadCallback(Entry* ent);
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_PAGECACHE_H_