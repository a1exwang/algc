#include <iostream>
#include <vector>
#include <cstring>
#include <iostream>


#include "gc.h"

using namespace std;


AlgcBlock *Algc::allocate(uint64_t size, std::vector<uint64_t> pointerOffsets) {
  if (this->options == TriggerOptions::OnAllocation) {
    if (this->blocks.size() >= this->gcBlockCountThreshold) {
      this->doGc();
    }
  }
  uint64_t totalSize = size + sizeof(AlgcBlock);
  auto ret = (AlgcBlock*)new char[totalSize];
  memset(ret, 0, totalSize);
  ret->dataSize = size;
  ret->pointerOffsets = pointerOffsets;
  this->blocks.push_back(ret);
  return ret;
}

void Algc::doGc() {
  for (auto block: this->blocks) {
    block->mark = false;
  }
  for (auto root : this->getRoots()) {
    root->doMark();
  }

  std::list<AlgcBlock*> newBlocks;
  std::list<AlgcBlock*> oldBlocks;
  for (auto block: this->blocks) {
    if (block->mark) {
      newBlocks.push_back(block);
    } else {
      oldBlocks.push_back(block);
    }
  }
  int i = 0;
  for (auto block : oldBlocks) {
    delete block;
    i++;
  }
  cout << "gc " << i << " objects" << endl;
  this->blocks = newBlocks;
}

void AlgcBlock::doMark() {
  // prevent loop pointers
  if (this->mark)
    return;

  this->mark = true;
  for (auto offset : this->pointerOffsets) {
    auto childDataPtr = *reinterpret_cast<char**>(this->data + offset);
    auto dataOffset = (uint64_t)((AlgcBlock*)(0))->data;
    auto childBlock = reinterpret_cast<AlgcBlock*>(childDataPtr - dataOffset);
    childBlock->doMark();
  }
}
