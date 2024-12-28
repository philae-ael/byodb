#include "btree.h"

#include "common.h"
#include <algorithm>
#include <compare>
#include <cstring>
#include <iterator>
#include <span>
#include <type_traits>
#include <vector>

const usize BTREE_PAGE_SIZE = KB(4);

// TODO: Little endian vs big endian
struct RCursor {
  const char* data;

  u16 read_u16() {
    auto x  = *reinterpret_cast<const u16*>(data);
    data   += sizeof(x);
    return x;
  }

  template <class T, class U = const std::remove_cv_t<T>>
  std::span<U> read_span(usize len) {
    std::span<U> s{reinterpret_cast<U*>(data), len};

    data += len * sizeof(T);
    return s;
  }
};

struct WCursor {
  char* data;

  void write_u16(u16 x) {
    *reinterpret_cast<u16*>(data)  = x;
    data                          += sizeof(x);
  }

  template <class T>
  void write_span(std::span<const T> t) {
    std::span<T> s{reinterpret_cast<T*>(data), t.size()};
    std::ranges::copy(t, s.begin());
    data += s.size_bytes();
  }
};

struct BTree {
  void* userdata;
  BTreeVTable vtable;

  char* get(u64 node_index) {
    return vtable.get(userdata, node_index);
  }
};

struct KV {
  std::span<const char> key;
  std::span<const char> val;
};

struct KVArray {
  std::span<const u16> offsets;
  const char* data;

  KV get(u16 idx) const {
    assert(idx < offsets.size());
    RCursor c{data + offsets[idx]};

    u16 klen = c.read_u16();
    u16 vlen = c.read_u16();

    return {
        c.read_span<char>(klen),
        c.read_span<char>(vlen),
    };
  }

  KV operator[](u16 idx) const {
    return get(idx);
  }

  std::span<const char> as_bytes() const {
    return std::span{reinterpret_cast<const char*>(offsets.data()), &*get(offsets.back()).val.end()};
  }

  usize size() {
    return offsets.size();
  }
};

struct KVArrayList {
  KVArray cur;
  KVArrayList* next;
};

struct NodeView {
  BTreeNodeType type;
  u16 nkeys;

  union {
    struct {
    } leaf;
    struct {
      std::span<const u64> pointers;
    } internal_node;
  };

  KVArray kvs;
};

NodeView btree_decode_node(RCursor& c) {
  NodeView node{
      BTreeNodeType{c.read_u16()},
      c.read_u16(),
  };
  switch (node.type) {
  case BTreeNodeType::InternalNode:
    node.internal_node = {
        c.read_span<u64>(node.nkeys),
    };
    break;
  case BTreeNodeType::Leaf:
    break;
  }
  node.kvs = {c.read_span<u16>(node.nkeys), c.data};
  return node;
}

std::strong_ordering compare_keys(std::span<const char> s, std::span<const char> t) {
  return std::lexicographical_compare_three_way(s.begin(), s.end(), t.begin(), t.end());
}

u16 btree_lookup_le(BTree* b, NodeView& node, std::span<char> key) {
  // precondition: node.nkeys > 0 && key(node.kid(0)) >= key
  u16 found = 0;

  // invariant: key(node.kid(i)) >= key
  for (u16 i = 0; i < node.nkeys; i++) {
    auto cmp = compare_keys(node.kvs[i].key, key);
    if (cmp == std::strong_ordering::less || cmp == std::strong_ordering::equal) {
      found = i;
    }

    if (cmp == std::strong_ordering::greater || cmp == std::strong_ordering::equal) {
      break;
    }
  }
  return found;
}

NodeView btree_leaf_insert(
    BTree* b,
    const NodeView& view,
    WCursor& output,
    u16 idx,
    std::span<const char> key,
    std::span<const char> val
) {
  // TODO
}

struct NodeWriter {
  BTreeNodeType type;

  union {
    struct {
    } leaf;
    struct {
      std::span<const u64> pointers;
    } internal_node;
  };

  KVArrayList kvs;

  NodeView commit(WCursor& mem) {
    RCursor start{mem.data};

    mem.write_u16(static_cast<u16>(type));

    WCursor nkeys{mem.data};
    mem.write_u16(0);

    switch (type) {
    case BTreeNodeType::InternalNode:
      mem.write_span(internal_node.pointers);
      break;
    case BTreeNodeType::Leaf:
      break;
    }

    KVArrayList* kvs_entry = &kvs;
    u16 nkeys_count        = 0;

    while (kvs_entry != nullptr) {
      mem.write_span(kvs_entry->cur.offsets);
      mem.write_span(kvs_entry->cur.as_bytes());
      nkeys_count += kvs_entry->cur.size();
      kvs_entry    = kvs_entry->next;
    }
    nkeys.write_u16(nkeys_count);

    return btree_decode_node(start);
  };
};
