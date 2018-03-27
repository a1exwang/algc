#include "../gc.h"
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <iostream>

using namespace std;

int main() {

  Algc gc("testgc2", "testGc2", 1048576*100, Algc::TriggerOptions::OnAllocation, 100000);
  pmem::obj::persistent_ptr<AlgcBlock> head(nullptr);
  pmem::obj::transaction::exec_tx(gc.getPool(), [&]() -> void {
    head = AlgcBlock::createHead();
    head->id = -1;
  });

  // Test insertion
  std::vector<pmem::obj::persistent_ptr<AlgcBlock>> ptrs;
  for (int i = 0; i < 10; i++) {
    pmem::obj::transaction::exec_tx(gc.getPool(), [&]() -> void {
      auto a = head->append(8, {}, 0, nullptr);
      a->id = i;
      cout << "wtf " << i << endl;
      ptrs.push_back(a);
    });
  }

  // Test iteration
  for (auto &&node: *head) {
    cout << "out " << node->id << endl;
  }

  // Test removal
  for (int i = 0; i < 5; i++) {
    pmem::obj::transaction::exec_tx(gc.getPool(), [&]() -> void {
      ptrs[i]->detach();
      pmem::obj::delete_persistent<AlgcBlock>(ptrs[i]);
    });
  }

  for (auto &&node: *head) {
    cout << "out " << node->id << endl;
  }
}
