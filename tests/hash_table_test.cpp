#include "../gc.h"
#include "../hash_table.h"
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <iostream>
#include <libpmemobj++/pool.hpp>
#include <sstream>


using namespace std;


int main() {

  auto poolName = "test_file";
  auto layoutName = "test_file";
  uint64_t maxSize = 1048576 * 1280;


  pmem::obj::pool<HashTable> pool;
  pmem::obj::persistent_ptr<HashTable> hashTable(nullptr);
  stringstream ss;
  try {
    pool = pmem::obj::pool<HashTable>::create(poolName, layoutName, maxSize);
    hashTable = pool.get_root();
    pmem::obj::transaction::exec_tx(pool, [&] {
      memset(hashTable.get(), 0, sizeof(HashTable));
    });
  } catch(pmem::pool_error &pe) {
    pool = pmem::obj::pool<HashTable>::open(poolName, layoutName);
    hashTable = pool.get_root();
  }

  int n = 200;

  // PutKey test
  for (int i = 0; i < n; ++i) {
    pmem::obj::transaction::exec_tx(pool, [&]() -> void {
      ss.str(""); ss.clear();
      ss << "hello_" << i;
      auto key = ss.str();
      ss.str(""); ss.clear();
      ss << "world_" << i;
      auto value = ss.str();

      auto pValue = pmem::obj::make_persistent<char[]>(value.length());
      memcpy(pValue.get(), value.c_str(), value.length() + 1);

      auto bPutKey = hashTable->putKey(key.c_str(), pValue.raw());
      AlLogger::Logger::getStdErrLogger().i()
          << "loadFactor=" << hashTable->loadFactor
          << ", key=" << key
          << ", value=" << value << "\n";
      assert(bPutKey);
    });
  }


  // Iteration test
  for (auto &item : *hashTable) {
    AlLogger::Logger::getStdErrLogger().i()
        << "loadFactor=" << hashTable->loadFactor
        << ", offset=" << (((uint64_t)&item - (uint64_t)hashTable.get()) / sizeof(HashTableItem))
        << ", key='" << item.key
        << "', value=("  << item.oid.pool_uuid_lo << ", " << item.oid.off << ")\n";
  }

  // GetKey test
  string key("hello_1");
  PMEMoid oid;
  assert(hashTable->getKey(key, oid));
  AlLogger::Logger::getStdErrLogger().i()
      << "HashTable::getKey(), key=" << "hello_1, value=" << oid.pool_uuid_lo << ", " << oid.off << ")\n";
}