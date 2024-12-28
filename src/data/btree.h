#pragma once

#include "common.h"

const usize BTREE_PAGE_SIZE = KB(4);

enum class BTreeNodeType : u16 {
  InternalNode = 1,
  Leaf,
};

struct BTree;

struct BTreeVTable {
  char* (*get)(void*, u64 node_index);
  char* (*allocate)(usize);
  void (*deallocate)(void*);
};
