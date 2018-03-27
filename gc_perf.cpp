#include <iostream>
#include <cstdio>
#include "gc.h"
#include "tools.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include <libpmemobj++/make_persistent_array.hpp>

using namespace std;


struct ListNode {
  pmem::obj::persistent_ptr<ListNode> next;
  pmem::obj::persistent_ptr<ListNode> prev;
  int data;
};

struct ListNodeSp {
  std::shared_ptr<ListNodeSp> next;
  std::shared_ptr<ListNodeSp> prev;
  int data;
};

template<typename Ptr>
void append(Ptr head, Ptr newNode) {
  auto last2 = head->prev;

  newNode->prev = last2;
  newNode->next = head;

  head->prev = newNode;
  last2->next = newNode;
}

template<typename Ptr>
void unattachNode(Ptr node) {
  auto next = node->next;
  auto prev = node->prev;

  next->prev = prev;
  prev->next = next;

  node->next = nullptr;
  node->prev = nullptr;
}

uint64_t offsets[] = {(uint64_t)&((ListNode*)0)->next, (uint64_t)&((ListNode*)0)->prev};
pmem::obj::persistent_ptr<uint64_t[]> pOffsets(nullptr);

pmem::obj::persistent_ptr<ListNode> newNodeWithGc(Algc &gc) {
  auto block = gc.allocate(sizeof(ListNode), pOffsets, 2);
  return pmem::obj::persistent_ptr<ListNode>(block->data.raw());
}
std::shared_ptr<ListNodeSp> newNode() {
  return std::make_shared<ListNodeSp>();
}

int64_t testGc(Algc &gc, int totalObjects, int gcThreshold) {
  pmem::obj::transaction::exec_tx(gc.getPool(), [&] {
    pOffsets = pmem::obj::make_persistent<uint64_t[]>(2);
    pOffsets[0] = offsets[0];
    pOffsets[1] = offsets[1];
  });

  pmem::obj::persistent_ptr<AlgcBlock> block = gc.allocate(sizeof(ListNode), pOffsets, 2);
  auto root = pmem::obj::persistent_ptr<ListNode>(block->data.raw());

  pmem::obj::transaction::exec_tx(gc.getPool(), [&] {
    root->next = root;
    root->prev = root;
    root->data = -1;
  });

//  gc.roots.push_back(block);
  gc.appendRootGc(block);

  return stopWatch([&gc, &root, totalObjects]() -> void {
    for (int i = 0; i < totalObjects; ++i) {
      auto p1 = newNodeWithGc(gc);
      p1->data = i;
      append(root, p1);
    }
//    gc.clearRootGc();

    gc.doGc();
  });
}
int64_t testSp(int totalObjects) {
  auto root2 = std::make_shared<ListNodeSp>();
  root2->next = root2;
  root2->prev = root2;

  return stopWatch([&root2, totalObjects]() -> void {
    for (int i = 0; i < totalObjects; ++i) {
      auto p1 = newNode();
      append(root2, p1);
    }
    while (root2->next != root2) {
      unattachNode(root2->next);
    }
  });
}

int main() {

  Algc gc("testgc1", "testGc1", 1048576*100, Algc::TriggerOptions::OnAllocation, 100000);
  gc.sweepCallback = [](pmem::obj::persistent_ptr<void> node) -> void {
    pmem::obj::persistent_ptr<ListNode> n(node.raw());
    cout << "sweep for " << n->data << endl;
  };
  gc.markCallback = [](pmem::obj::persistent_ptr<void> node) -> void {
    pmem::obj::persistent_ptr<ListNode> n(node.raw());
    cout << "mark for " << n->data << endl;
  };

  std::pair<int64_t, int64_t> data[32];
  int pts = 20;
  for (int i = 0; i <= pts; ++i) {
    auto totalObjecst = 2 << i;
    auto gcThreshold = std::max({1024, (2 << i) >> 2});
    auto t1 = testGc(gc, totalObjecst, gcThreshold);
    auto t2 = testSp(totalObjecst);
    data[i] = {t1, t2};
  }
  for (int i = 0; i <= pts; ++i) {
    printf("%02d, % 8ld, % 8ld\n", i, data[i].first, data[i].second);
  }
}
