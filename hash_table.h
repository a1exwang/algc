#pragma once
#include <cstdint>
#include <libpmemobj++/persistent_ptr.hpp>
#include "misc/log.h"

constexpr uint64_t HashTableMaxKeyLen = 40;
constexpr uint64_t HashTableMaxItemsExp = 20; // 1Mi items, 64MiB in total
constexpr uint64_t HashTableMaxItems = 1 << HashTableMaxItemsExp; //
constexpr uint64_t FNV_prime = 14695981039346656037U;

#pragma pack(push, 1)
// Totally 64 bytes
struct HashTableItem {
  char key[HashTableMaxKeyLen]; // 40 bytes
  int64_t nextKeyOffset;       // 8 bytes
  PMEMoid oid;                  // 16 bytes
};
#pragma pack(pop)

struct HashTable {
  int64_t nItems;
  int64_t firstOffset;
  int64_t lastOffset;
  int64_t loadFactor;
  HashTableItem items[HashTableMaxItems];

  struct Iterator {
    Iterator operator++() {
      offset = hashTable->items[offset].nextKeyOffset;
      first = false;
    }
    bool operator!=(const Iterator &rhs) {
      return
          !(this->hashTable == rhs.hashTable
            && this->offset == rhs.offset
            && this->first == rhs.first);
    }
    HashTableItem &operator*() {
      return this->hashTable->items[this->offset];
    }

    HashTable *hashTable;
    int64_t offset;
    bool first;
  };

  Iterator begin() {
    return Iterator({this, firstOffset, (nItems > 0)});
  }
  Iterator end() {
    return Iterator({this, firstOffset, false});
  }
  static std::hash<std::string> h;

  static uint64_t hash(const std::string &key) {
    uint64_t ret = 0;
    ret = h(key);
    return ret % HashTableMaxItems;
  }

  /**
   * If found, it returns the item.
   * If not found, it returns the item it should be placed in.
   * @param key
   * @param found
   * @return
   */
  HashTableItem &getItem(const std::string &key, bool &found, int64_t &nTried) {
    if (key.length() >= HashTableMaxKeyLen || key.length() == 0) {
      AlLogger::Logger::getStdErrLogger().e() << "HashTable::getItem(), empty key or key too long '" << key << "'\n";
      abort();
    }
    auto offset = hash(key);
    // not empty string
    auto originOffset = offset;
    nTried = 0;
    do {
      nTried++;
      if (this->items[offset].key[0] == 0) {
        found = false;
        return this->items[offset];
      }
      if (strncmp(key.c_str(), this->items[offset].key, HashTableMaxKeyLen) == 0) {
        found = true;
        return this->items[offset];
      }
      offset++;
      if (offset >= HashTableMaxItems) {
        offset = 0;
      }
    } while (offset != originOffset);
  }

  bool getKey(const std::string &key, PMEMoid &oid) {
    bool found;
    int64_t _;
    oid = getItem(key, found, _).oid;
    return found;
  }
  bool hasKey(const std::string &key) {
    bool found;
    int64_t _;
    getItem(key, found, _);
    return found;
  }

  bool putKey(const std::string &key, const PMEMoid &oid) {
    bool found;
    int64_t nTried;
    auto &item = getItem(key, found, nTried);

    // Override value if found
    if (found) {
      item.oid = oid;
      return true;
    }

    // hashTable[key] = value
    strncpy(item.key, key.c_str(), HashTableMaxKeyLen);
    item.oid = oid;

    // append new item to the list
    int64_t offset = ((uint64_t)&item - (uint64_t)this) / sizeof(HashTableItem);
    if (this->nItems == 0) {
      this->firstOffset = offset;
      this->lastOffset = offset;
    }

    this->items[this->lastOffset].nextKeyOffset = offset;
    this->lastOffset = offset;
    item.nextKeyOffset = this->firstOffset;

    // Update statistics
    this->nItems++;
    if (nTried > this->loadFactor) {
      this->loadFactor = nTried;
    }
    return true;
  }
};