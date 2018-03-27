#include <iostream>
#include <vector>
#include <cstring>
#include <iostream>


#include "gc.h"

using namespace std;


AlgcBlock *Algc::allocate(uint64_t size, const uint64_t* pointerOffsets, int64_t nOffsets) {
  if (this->options == TriggerOptions::OnAllocation) {
    if (this->blocks.size() >= this->gcBlockCountThreshold) {
      this->doGc();
    }
  }
  uint64_t totalSize = size + sizeof(AlgcBlock);
  auto ret = (AlgcBlock*)new char[totalSize];
  memset(ret, 0, totalSize);
  ret->dataSize = size;
//  ret->pointerOffsets = move(pointerOffsets);
  ret->pointerOffsets = pointerOffsets;
  ret->nOffsets = nOffsets;
  this->blocks.push_back(ret);
  return ret;
}

void Algc::doGc() {
  for (auto block: this->blocks) {
    block->mark = false;
  }
//  for (auto root : this->getRoots()) {
//    root->doMark();
//  }
  for (auto root : this->roots) {
    root->doMark();
  }

  int i = 0;
  for (auto it = this->blocks.begin(); it != this->blocks.end(); it++) {
    if (!(*it)->mark) {
      delete *it;
      i++;
      it = this->blocks.erase(it);
    }
  }

  cout << "gc " << i << " objects" << endl;
}

void AlgcBlock::doMark() {
  // prevent loop pointers
  if (this->mark)
    return;

  this->mark = true;
  for (int64_t i = 0; i < this->nOffsets; i++) {
    auto childDataPtr = *reinterpret_cast<char**>(this->data + this->pointerOffsets[i]);
    auto dataOffset = (uint64_t)((AlgcBlock*)(0))->data;
    auto childBlock = reinterpret_cast<AlgcBlock*>(childDataPtr - dataOffset);
    childBlock->doMark();
  }
}
