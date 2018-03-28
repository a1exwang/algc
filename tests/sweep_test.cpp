#include "single_list.h"
#include "../gc.h"
#include "../misc/log.h"

#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <iostream>

using namespace std;


AlLogger::Logger logger(cerr);

int main() {
  Algc gc("sweep_test", "sweep_test", 1048576*100, Algc::TriggerOptions::OnAllocation, 100000);
  gc.markCallback = [&](pmem::obj::persistent_ptr<void> _node) -> void {
    pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> node(_node.raw());
    logger.i() << "mark id=" << node->data.data << "\n";
  };
  gc.sweepCallback = [&](pmem::obj::persistent_ptr<void> _node) -> void {
    pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> node(_node.raw());
    logger.i() << "sweep id=" << node->data.data << "\n";
  };

  uint64_t offsets[] = {(uint64_t)offsetOf(&SingleListNode::next)};

  /** User code */
  pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> head = gc.allocate<SingleListNode>(offsets, 1);
  auto node = head;
  head->block()->id = -1;
  head->data.data = -1;
  head->data.next = nullptr;

  vector<pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>>> ptrs;
  for (int i = 0; i < 10; ++i) {
    auto newNode = gc.allocate<SingleListNode>(offsets, 1);
    auto nodeBlock = newNode->block();
    nodeBlock->id = i;

    pmem::obj::transaction::exec_tx(gc.getPool(), [&] {
      newNode->data.next = nullptr;
      newNode->data.data = i;
      node->data.next = newNode;
      node = newNode;
    });
    ptrs.push_back(newNode);
  }

  for (auto item : *gc.poolRoot->rootAllObjs) {
    logger.i() << "all objs " << item->id << "\n";
  }

  pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> n = head;
  while(n != nullptr) {
    logger.i() << "SingleListNode: data=" << n->data.data
               << ", blk_oid=(" << std::hex << n->oid.pool_uuid_lo << ", "
               << std::hex << n->oid.off << "), user_oid=("
               << std::hex << n->block()->data.raw().pool_uuid_lo << ", "
               << std::hex << n->block()->data.raw().off << ")\n";
    n = n->data.next;
  }

  auto headBlock = head->block();
//  gc.appendRootGc(headBlock);

  auto roots1 = std::make_unique<pmem::obj::persistent_ptr<AlgcBlock>[]>(1);
  roots1[0] = headBlock;
  gc.gcRootsCallback = [&](uint64_t &n) -> pmem::obj::persistent_ptr<AlgcBlock>* {
    n = 1;
    return roots1.get();
  };
  ptrs[5]->data.next = nullptr;

  gc.doGc();
}
