#ifndef KTABLEFS_KVENGINE_KV_
#define KTABLEFS_KVENGINE_KV_

#include <vector>
#include "kvengine/aio-wrapper.h"

namespace ktablefs {

class KVThread;
class KVBatch;

/**
 * kvengine
 */
class KV {
 private:
  std::vector<KVThread> threads_;

 public:
  void Submit(const KVBatch&);
};

} // namespace ktablefs

#endif // KTABLEFS_KVENGINE_KV_