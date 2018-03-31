#include <iostream>
#include <cstdio>
#include "gc.h"
#include "misc/tools.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include <libpmemobj++/make_persistent_array.hpp>

using namespace std;


#pragma pack(push, 1)
struct ListNode {
  ListNode() { }
  ListNode(
       pmem::obj::persistent_ptr<AlgcPmemObj<ListNode>> prev,
       pmem::obj::persistent_ptr<AlgcPmemObj<ListNode>> next,
       int data
  ) :prev(prev), next(next), data(data) { }
  pmem::obj::persistent_ptr<AlgcPmemObj<ListNode>> prev;
  pmem::obj::persistent_ptr<AlgcPmemObj<ListNode>> next;
  int data;
};
#pragma pack(pop)

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

static uint64_t myOffsets[] = {0, 16};

std::shared_ptr<ListNodeSp> newNode() {
  return std::make_shared<ListNodeSp>();
}

pmem::obj::persistent_ptr<AlgcBlock> gcRoots[100];

int64_t testGc(Algc &gc, int totalObjects, int gcThreshold) {
  pmem::obj::persistent_ptr<AlgcPmemObj<ListNode>> root(nullptr);
  pmem::obj::transaction::exec_tx(gc.getPool(), [&] {
    root = gc.allocate<ListNode>(myOffsets, 2);
    root->data.next = root;
    root->data.prev = root;
    root->data.data = 123;
  });

  gcRoots[0] = root->block();
  gc.gcRootsCallback = [](uint64_t & n) -> pmem::obj::persistent_ptr<AlgcBlock>* {
    n = 1;
    return gcRoots;
  };

  return stopWatch([&gc, &root, totalObjects]() -> void {
    for (int i = 0; i < totalObjects; ++i) {
      auto last2 = root->data.prev;
      pmem::obj::transaction::exec_tx(gc.getPool(), [&] {
//        cout << "root " << root->oid.off << endl;
        auto node = gc.allocate<ListNode>(myOffsets, 2);
        node->data.next = root;
        node->data.prev = last2;

        root->data.prev = node;
//        cout << last2->next.raw().off << endl;
        last2->data.next = node;
//        cout << last2->next.raw().off << endl;
      });
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

  Algc gc("test_file", "testGc1", 1048576*100, Algc::TriggerOptions::OnAllocation, 100000);
  gc.sweepCallback = [](pmem::obj::persistent_ptr<void> node) -> void {
    pmem::obj::persistent_ptr<ListNode> n(node.raw());
//    cout << "sweep for " << n->data << endl;
  };
  gc.markCallback = [](pmem::obj::persistent_ptr<void> node) -> void {
    pmem::obj::persistent_ptr<ListNode> n(node.raw());
//    cout << "mark for " << n->data << endl;
  };

  std::pair<int64_t, int64_t> data[32];
  int pts = 6;
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
