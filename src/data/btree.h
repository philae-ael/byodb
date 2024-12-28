#pragma once

#include "common.h"

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