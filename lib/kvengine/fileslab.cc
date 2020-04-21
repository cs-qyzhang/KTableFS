#include "fileslab.h"

namespace kvengine {

slab_t FileSlab::Create(const Slice* key, const Slice* value) {
  size_t item_size = key->size() + value->size() + 3 * sizeof(uint16_t);

  for (auto& slab: slabs_) {
    if (slab.slab_size >= item_size) {
      if (!slab.free_slab_.empty()) {
        slab_t res = slab.free_slab_.front();
        slab.free_slab_.pop_front();
        return res;
      } else if (slab.cur_slab == slab.slab_per_file) {
        std::string file_name = "/slab_" + std::to_string(slab.slab_size)
          + "_" + std::to_string(slab.files.size());
        slab.cur_file = file_set_.Create(file_name, 0644);
        slab.cur_slab = 0;
        slab.files.push_back(slab.cur_file);
      } else {
        slab.cur_slab++;
      }
      slab_t res;
      res.size = slab.slab_size;
      res.index = slab.cur_slab;
      res.file = slab.cur_file;
      return res;
    }
  }
  throw "ERROR in FileSlab::Create(): size to large!";
}

void FileSlabReadCallback(AIO* aio) {
  if (aio->Success()) {
    size_t read_off = aio->GetReadOff();
    char* data = aio->GetBuf() + read_off;
    uint16_t valid = *reinterpret_cast<uint16_t*>(data);
    assert(FileSlab::Valid(valid));
    data += sizeof(uint16_t);
    uint16_t key_size = *reinterpret_cast<uint16_t*>(data);
    data += sizeof(uint16_t);
    uint16_t value_size = *reinterpret_cast<uint16_t*>(data);
    aio->SetReadSizeAndOff(value_size, read_off + 3 * sizeof(uint16_t) + key_size);
  }
}

void FileSlab::Read(slab_t slab, AIO* aio) {
  aio->AddCallback(FileSlabReadCallback);
  file_set_.Read(slab.file, slab.size, slab.size * slab.index, aio);
}

void FileSlab::ReadSync(slab_t slab, Slice*& key, Slice*& value) {
  Slice* s = file_set_.ReadSync(slab.file, slab.size, slab.size * slab.index);
  char* data = const_cast<char*>(s->data());
  uint16_t valid = *reinterpret_cast<uint16_t*>(data);
  assert(FileSlab::Valid(valid));
  data += sizeof(uint16_t);
  uint16_t key_size = *reinterpret_cast<uint16_t*>(data);
  data += sizeof(uint16_t);
  uint16_t value_size = *reinterpret_cast<uint16_t*>(data);
  data += sizeof(uint16_t);
  key = new Slice(data, key_size);
  data += key_size;
  value = new Slice(data, value_size);
}

void FileSlab::Write(slab_t slab, const Slice* key, const Slice* value, AIO* aio) {
  uint16_t key_size = key->size();
  uint16_t value_size = value->size();
  size_t item_size = key_size + value_size + 3 * sizeof(uint16_t);
  char* buf = new char[item_size];
  char* p = buf;
  *reinterpret_cast<uint16_t*>(p) = FileSlab::VALID;
  p += sizeof(uint16_t);
  *reinterpret_cast<uint16_t*>(p) = key_size;
  p += sizeof(uint16_t);
  *reinterpret_cast<uint16_t*>(p) = value_size;
  p += sizeof(uint16_t);
  std::memcpy(p, key->data(), key_size);
  p += key_size;
  std::memcpy(p, value->data(), value_size);
  Slice* s = new Slice(buf, item_size);
  file_set_.Write(slab.file, s, slab.size * slab.index, aio);
}

void FileSlab::Delete(slab_t slab_idx, AIO* aio) {
  for (auto& slab: slabs_) {
    if (slab.slab_size == slab_idx.size) {
      slab.free_slab_.push_back(slab_idx);
      // write unvalid flag
      char* buf = new char[sizeof(size_t)];
      *reinterpret_cast<size_t*>(buf) = 0;
      Slice* s = new Slice(buf, sizeof(size_t));
      file_set_.Write(slab_idx.file, s, slab_idx.size * slab_idx.index, aio);
      return;
    }
  }
  throw "ERROR in FileSlab::Delete(): slab not exist!";
}

} // namespace kvengine