#pragma once
#include <libpmemobj++/persistent_ptr.hpp>
#include "../gc.h"


#pragma pack(push, 1)

struct SingleListNode {
  SingleListNode() :next(nullptr), data(0) { }
  SingleListNode(pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> next, int data) :next(next), data(data) { }

  pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> next;
  int data;
};

#pragma pack(pop)
