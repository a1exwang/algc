#include <iostream>
#include <cstdio>
#include "gc.h"
#include "tools.h"
#include <cstring>
#include <memory>
#include <algorithm>

using namespace std;

struct ListNode {
  ListNode *next;
  ListNode *prev;
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

ListNode *newNodeWithGc(Algc &gc) {
  auto block = gc.allocate(sizeof(ListNode), {(uint64_t)&((ListNode*)0)->next});
  return (ListNode*)block->data;
}
std::shared_ptr<ListNodeSp> newNode() {
  return std::make_shared<ListNodeSp>();
}

int64_t testGc(int totalObjects, int gcThreshold) {
  Algc gc(Algc::TriggerOptions::OnAllocation, gcThreshold);

  auto block = gc.allocate(sizeof(ListNode), {(uint64_t)&((ListNode*)0)->next});
  auto root = (ListNode*) block->data;
  root->next = root;
  root->prev = root;

  gc.setGetRoots([&block]() -> vector<AlgcBlock*> {
    return {block};
  });

  return stopWatch([&gc, &root, totalObjects]() -> void {
    for (int i = 0; i < totalObjects; ++i) {
      auto p1 = newNodeWithGc(gc);
      append(root, p1);
    }
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
  std::pair<int64_t, int64_t> data[32];
  int pts = 15;
  for (int i = 0; i <= pts; ++i) {
    auto totalObjecst = 2 << i;
    auto gcThreshold = std::max({1024, (2 << i) >> 2});
    auto t1 = testGc(totalObjecst, gcThreshold);
    auto t2 = testSp(totalObjecst);
    data[i] = {t1, t2};
  }
  for (int i = 0; i <= pts; ++i) {
    printf("%02d: % 8ld % 8ld\n", i, data[i].first, data[i].second);
  }
}
