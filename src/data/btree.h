#pragma once

#include "common.h"

// NOTE: HAS TO BE A POWER OF 2 (for alignement purposes)
const usize BTREE_PAGE_SIZE = 512;

enum class BTreeNodeType : u16 {
  InternalNode = 1,
  Leaf,
};

struct BTreeVTable {
  // Load from disk or whatever
  // This should maybe be COW?
  const char* (*get)(void* userdata, u64 node_index);
  // Allocate BTREE_PAGE_SIZE of mem that may or not be backed by a file
  char* (*allocate_page)(void* userdata);
  // Commit memory from allocate into disk or whatever
  void (*commit)(void* userdata, void*);
  // Dealocate
  void (*deallocate)(void* userdata, void*);
};

struct BTree {
  void* userdata;
  BTreeVTable vtable;

  const char* get(u64 node_index) {
    return vtable.get(userdata, node_index);
  }

  char* allocate_page() {
    return vtable.allocate_page(userdata);
  }

  void commit(void* ptr) {
    return vtable.commit(userdata, ptr);
  }

  void deallocate(void* ptr) {
    return vtable.deallocate(userdata, ptr);
  }
};
