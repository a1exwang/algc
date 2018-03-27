#include <iostream>
#include <vector>
#include <cstring>
#include <iostream>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/detail/pexceptions.hpp>


#include "gc.h"

using namespace std;


pmem::obj::persistent_ptr<AlgcBlock>
Algc::allocate(
    uint64_t size,
    pmem::obj::persistent_ptr<const uint64_t[]> pointerOffsets,
    int64_t nOffsets) {

  if (this->options == TriggerOptions::OnAllocation) {
    if (*this->poolRoot->blockCount >= this->gcBlockCountThreshold) {
      this->doGc();
    }
  }

  pmem::obj::persistent_ptr<AlgcBlock> listNode(nullptr);
  if (poolRoot->rootAllObjs == nullptr) {
    pmem::obj::transaction::exec_tx(pool, [&] {
      // `data` is user data
      auto totalSize = size + sizeof(PMEMoid);
      auto data = pmem::obj::make_persistent<char[]>(totalSize);
      memset(data.get(), 0, size);
      *reinterpret_cast<PMEMoid*>((char*)data.get()+size) = listNode.raw();

      listNode = pmem::obj::make_persistent<AlgcBlock>(
          nullptr,
          nullptr,
          totalSize,
          pointerOffsets,
          nOffsets,
          data
      );
      assert(data != nullptr);
      listNode->next = listNode;
      listNode->prev = listNode;
      (*poolRoot->blockCount)++;
      this->poolRoot->rootAllObjs = listNode;
    });
  }
  else {
    pmem::obj::transaction::exec_tx(pool, [&] {
      auto last2 = poolRoot->rootAllObjs->prev;

      // `data` is user data
      auto totalSize = size + sizeof(PMEMoid);
      auto data = pmem::obj::make_persistent<char[]>(totalSize);
      *reinterpret_cast<PMEMoid*>(data.get()) = listNode.raw();
      memset(data.get(), 0, totalSize);

      listNode = pmem::obj::make_persistent<AlgcBlock>(
          last2,
          poolRoot->rootAllObjs,
          totalSize,
          pointerOffsets,
          nOffsets,
          data
      );

      last2->next = listNode;
      poolRoot->rootAllObjs->prev = listNode;
      (*poolRoot->blockCount)++;
    });

  }

  return listNode;
}

void Algc::doGc() {

  // mark all nodes as `false`
  auto node = this->poolRoot->rootAllObjs;
  do {
    node->mark = false;
    node = node->next;
  } while (node != this->poolRoot->rootAllObjs);

  // mark stage
  node = this->poolRoot->rootGc;
  if (node != nullptr) {
    do {
      node->doMark(this->markCallback);
      node = node->next;
    } while (node != this->poolRoot->rootGc);
  }

  // sweep stage
  int i = 0;
  node = this->poolRoot->rootAllObjs;
  do {
    if (!node->mark) {
      // cleanup node
      i++;
      pmem::obj::transaction::exec_tx(pool, [&] {
        node->prev->next = node->next;
        node->next->prev = node->prev;

        if (sweepCallback) {
          sweepCallback(node->data);
        }
        pmem::obj::delete_persistent<char[]>(node->data, node->dataSize);
        pmem::obj::delete_persistent<AlgcBlock>(node);
        (*this->poolRoot->blockCount)--;
      });
    } else {
      node = node->next;
    }
  } while (node != this->poolRoot->rootAllObjs);
  cout << "gc " << i << " objects" << endl;
}

Algc::Algc(
    const std::string &poolName,
    const std::string &layoutName,
    uint64_t maxSize,
    Algc::TriggerOptions options,
    int gcBlockCountThreshold) : poolName(poolName),
                                 layoutName(layoutName),
                                 maxSize(maxSize),
                                 options(options),
                                 gcBlockCountThreshold(gcBlockCountThreshold) {

  try {
    this->pool = pmem::obj::pool<AlgcPmemRoot>::create(poolName, layoutName, maxSize);
  } catch(pmem::pool_error &pe) {
    this->pool = pmem::obj::pool<AlgcPmemRoot>::open(poolName, layoutName);
  }

  this->poolRoot = this->pool.get_root();
  if (this->poolRoot->blockCount == nullptr) {
    pmem::obj::transaction::exec_tx(pool, [&]() -> void {
      this->poolRoot->blockCount = pmem::obj::make_persistent<int64_t>(0);
    });
  }
  if (this->poolRoot->rootAllObjs == nullptr) {
    pmem::obj::transaction::exec_tx(pool, [&]() -> void {
      this->poolRoot->rootAllObjs = AlgcBlock::createHead();
    });
  }
  if (this->poolRoot->rootGc == nullptr) {
    pmem::obj::transaction::exec_tx(pool, [&]() -> void {
      this->poolRoot->rootGc = AlgcBlock::createHead();
    });
  }
}

Algc::~Algc() {
  this->pool.close();
}


void Algc::appendRootGc(pmem::obj::persistent_ptr<AlgcBlock> anotherRoot) {
  if (this->poolRoot->rootGc  == nullptr) {
    pmem::obj::transaction::exec_tx(pool, [&] {
      this->poolRoot->rootGc = anotherRoot;
      anotherRoot->next = anotherRoot;
      anotherRoot->prev = anotherRoot;
    });
  } else {
    pmem::obj::transaction::exec_tx(pool, [&] {
      auto last2 = poolRoot->rootGc->prev;
      last2->next = anotherRoot;
      poolRoot->rootGc->prev = anotherRoot;
    });
  }
}

void Algc::clearRootGc() {
  this->poolRoot->rootGc = nullptr;
}

void AlgcBlock::doMark(std::function<void (pmem::obj::persistent_ptr<void>)> markCallback) {
  // prevent loop pointers
  if (this->mark)
    return;

  this->mark = true;
  if (markCallback)
    markCallback(this->data);
  for (int64_t i = 0; i < this->nOffsets; i++) {
    auto persistPtrAddr = this->data.get() + this->pointerOffsets[i];
    auto childDataPtr = *reinterpret_cast<PMEMoid*>(persistPtrAddr);
    pmem::obj::persistent_ptr<AlgcBlock> ptr(childDataPtr);
    ptr->doMark(markCallback);
  }
}

pmem::obj::persistent_ptr<AlgcBlock>
AlgcBlock::append(uint64_t dataSize, pmem::obj::persistent_ptr<const uint64_t[]> pointerOffsets, int64_t nOffsets,
    pmem::obj::persistent_ptr<char[]> data) {

  auto last2 = this->prev;
  pmem::obj::persistent_ptr<AlgcBlock> ret = pmem::obj::make_persistent<AlgcBlock>(
      last2,
      this,
      dataSize,
      pointerOffsets,
      nOffsets,
      data
  );

  last2->next = ret;
  this->prev = ret;
  return ret;
}

pmem::obj::persistent_ptr<AlgcBlock> AlgcBlock::createHead() {
  auto ret = pmem::obj::make_persistent<AlgcBlock>(nullptr, nullptr, 0, nullptr, 0, nullptr);
  ret->next = ret;
  ret->prev = ret;
  return ret;
}

void AlgcBlock::detach() {
  this->next->prev = this->prev;
  this->prev->next = this->next;
  this->next = nullptr;
  this->prev = nullptr;
}
