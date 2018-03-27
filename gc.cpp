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
    const uint64_t pointerOffsets[],
    int64_t nOffsets) {

  if (this->options == TriggerOptions::OnAllocation) {
    if (*this->poolRoot->blockCount >= this->gcBlockCountThreshold) {
      this->doGc();
    }
  }

  pmem::obj::persistent_ptr<AlgcBlock> listNode(nullptr);

  auto totalSize = size + sizeof(PMEMoid);

  pmem::obj::transaction::exec_tx(this->pool, [&] {
    auto data = pmem::obj::make_persistent<char[]>(totalSize);
    memset(data.get(), 0, size);

    auto persistentPointerOffsets = pmem::obj::make_persistent<uint64_t[]>((uint64_t)nOffsets);
    memcpy(persistentPointerOffsets.get(), pointerOffsets, sizeof(uint64_t) * nOffsets);

    listNode = this->poolRoot->rootAllObjs->append(
        totalSize,
        persistentPointerOffsets,
        nOffsets,
        data
    );
    *reinterpret_cast<PMEMoid*>((char*)data.get()+size) = listNode.raw();

    (*poolRoot->blockCount)++;
  });

  return listNode;
}

void Algc::doGc() {
  // mark stage
  this->doMark();

  // sweep stage
  int i = 0;
  auto it = this->poolRoot->rootAllObjs->begin(), itEnd = this->poolRoot->rootAllObjs->end();
  while (it != itEnd) {
    if (!(*it)->mark) {
      pmem::obj::transaction::exec_tx(pool, [&] {
        auto block = *it;
        if (sweepCallback) {
          sweepCallback(block->data);
        }
        it = it.detach();
        pmem::obj::delete_persistent<char[]>(block->data, block->dataSize);
        pmem::obj::delete_persistent<AlgcBlock>(block);
        (*this->poolRoot->blockCount)--;
      });
    } else {
      it++;
    }
  }
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
  pmem::obj::transaction::exec_tx(pool, [&] {
    auto last2 = poolRoot->rootGc->prev;
    last2->next = anotherRoot;
    poolRoot->rootGc->prev = anotherRoot;

    anotherRoot->next = poolRoot->rootGc;
    anotherRoot->prev = last2;
  });
}

void Algc::clearRootGc() {
  this->poolRoot->rootGc = nullptr;
}

void Algc::doMark() {
  // mark all nodes as `false`
  for (auto item : *this->poolRoot->rootAllObjs) {
    item->mark = false;
    cout << "mark false " << item->id << endl;
  }

  // mark stage
  for (auto item : *this->poolRoot->rootGc) {
    item->doMark(this->markCallback);
  }

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

pmem::obj::persistent_ptr<AlgcBlock> AlgcBlock::detach() {
  auto ret = this->next;
  this->next->prev = this->prev;
  this->prev->next = this->next;
  this->next = nullptr;
  this->prev = nullptr;
  return next;
}

pmem::obj::persistent_ptr<AlgcBlock> AlgcBlock::createFromDataPtr(void *p, uint64_t size) {
  auto oid = *(PMEMoid*)((char*)p + size);
  return pmem::obj::persistent_ptr<AlgcBlock>(oid);
}
