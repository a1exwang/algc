#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <list>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

// This is a fixed size meta data struct for a memory block


struct AlgcBlock {
  struct Iterator {
    pmem::obj::persistent_ptr<AlgcBlock> obj;
    Iterator operator++(int) {
      Iterator ret({obj});
      obj = obj->next;
      return ret;
    }
    Iterator operator++() {
      obj = obj->next;
      return Iterator({obj});
    }
    pmem::obj::persistent_ptr<AlgcBlock> operator*() {
      return obj;
    }
    bool operator!=(const Iterator &rhs) {
      return this->obj != rhs.obj;
    }
  };

  AlgcBlock(
      pmem::obj::persistent_ptr<AlgcBlock> prev,
      pmem::obj::persistent_ptr<AlgcBlock> next,
      uint64_t dataSize,
      pmem::obj::persistent_ptr<const uint64_t[]> pointerOffsets,
      int64_t nOffsets,
      pmem::obj::persistent_ptr<char[]> data
  ) :next(next), prev(prev), mark(false), dataSize(dataSize),
     pointerOffsets(pointerOffsets), nOffsets(nOffsets),
     data(data) { }

  static pmem::obj::persistent_ptr<AlgcBlock> createHead();

  Iterator begin() {
    return Iterator({this->next});
  }
  Iterator end() {
    return Iterator({this});
  }

  pmem::obj::persistent_ptr<AlgcBlock> append(
      uint64_t dataSize,
      pmem::obj::persistent_ptr<const uint64_t[]> pointerOffsets,
      int64_t nOffsets,
      pmem::obj::persistent_ptr<char[]> data
  );

  void detach();

  pmem::obj::persistent_ptr<AlgcBlock> next;
  pmem::obj::persistent_ptr<AlgcBlock> prev;

  bool mark;
  uint64_t dataSize;

  pmem::obj::persistent_ptr<const uint64_t[]> pointerOffsets;
  int64_t nOffsets;

  pmem::obj::persistent_ptr<char[]> data;

  void doMark(std::function<void (pmem::obj::persistent_ptr<void>)> markCallback);

  static pmem::obj::persistent_ptr<AlgcBlock> getBlockByDataPtr(const char *data) {
    return pmem::obj::persistent_ptr<AlgcBlock>(*reinterpret_cast<const PMEMoid*>(data));
  }
  int id;
};


struct AlgcPmemRoot {
  pmem::obj::persistent_ptr<AlgcBlock> rootAllObjs;
  pmem::obj::persistent_ptr<AlgcBlock> rootGc;
  pmem::obj::persistent_ptr<int64_t> blockCount;
};


class Algc {
public:
  enum TriggerOptions {
    Manual = 1,
    OnAllocation = 2,
  };

  Algc(
      const std::string &poolName,
      const std::string &layoutName,
      uint64_t maxSize,
      TriggerOptions options,
      int gcBlockCountThreshold
  );
  ~Algc();

  pmem::obj::persistent_ptr<AlgcBlock> allocate(
      uint64_t size,
      pmem::obj::persistent_ptr<const uint64_t[]> pointerOffsets,
      int64_t nOffsets
  );

  void doGc();
  void appendRootGc(pmem::obj::persistent_ptr<AlgcBlock> anotherRoot);
  void clearRootGc();
  pmem::obj::pool<AlgcPmemRoot> &getPool() { return pool; }

  std::function<void (pmem::obj::persistent_ptr<void>)> sweepCallback;
  std::function<void (pmem::obj::persistent_ptr<void>)> markCallback;
private:
  std::string poolName;
  std::string layoutName;
  uint64_t maxSize;

  TriggerOptions options;
  int gcBlockCountThreshold;

  pmem::obj::persistent_ptr<AlgcPmemRoot> poolRoot;
  pmem::obj::pool<AlgcPmemRoot> pool;
};
