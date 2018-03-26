#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <list>

struct AlgcBlock {
  bool mark;
  uint64_t dataSize;
  std::vector<uint64_t> pointerOffsets;
  char data[0];

  void doMark();
};


class Algc {
public:
  enum TriggerOptions {
    Manual = 1,
    OnAllocation = 2,
  };
  Algc(TriggerOptions options, int gcBlockCountThreshold)
      :options(options), gcBlockCountThreshold(gcBlockCountThreshold) { }

  AlgcBlock *allocate(
      uint64_t size,
      std::vector<uint64_t> pointerOffsets
  );

  void doGc();
  void setGetRoots(std::function<std::vector<AlgcBlock*>()> getRoots) { this->getRoots = getRoots; }
private:
  std::list<AlgcBlock*> blocks;
  TriggerOptions options;
  std::function<std::vector<AlgcBlock*>()> getRoots;
  int gcBlockCountThreshold;
};
