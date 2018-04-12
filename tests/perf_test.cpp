#include "../gc.h"
#include "../misc/log.h"
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <iostream>
#include "single_list.h"
#include "../misc/tools.h"

using namespace std;


AlLogger::Logger logger(cerr);

int main() {
  Algc gc("perf_test", "perf_test", 1048576*100, Algc::TriggerOptions::OnAllocation, 100000);
  gc.markCallback = [&](pmem::obj::persistent_ptr<void> _node) -> void {
    pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> node(_node.raw());
    logger.i() << "mark id=" << node->data.data << "\n";
  };
  uint64_t offsets[] = {(uint64_t)offsetOf(&SingleListNode::next)};

  /** User code */
  pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> head(nullptr);

  pmem::obj::transaction::exec_tx(gc.getPool(), [&] {
    head = gc.allocate<SingleListNode>(offsets, 1, nullptr, -1);
    head->block()->id = -1;
  });

  auto node = head;
  for (int x = 0; x < 10; ++x) {
    stopWatch([&] {
      for (int i = 0; i < 10; ++i) {
        pmem::obj::persistent_ptr<AlgcPmemObj<SingleListNode>> newNode(nullptr);
        pmem::obj::transaction::exec_tx(gc.getPool(), [&] {
          newNode = gc.allocate<SingleListNode>(offsets, 1, nullptr, i);
//          newNode->block()->id = i;
//
//          node->data.next = newNode;
//          node = newNode;
        });
//

      }
    });
  }

  auto headBlock = head->block();
  auto roots1 = std::make_unique<pmem::obj::persistent_ptr<AlgcBlock>[]>(1);
  roots1[0] = headBlock;
  gc.gcRootsCallback = [&](uint64_t &n) -> pmem::obj::persistent_ptr<AlgcBlock>* {
    n = 1;
    return roots1.get();
  };

//  gc.doMark();
}
