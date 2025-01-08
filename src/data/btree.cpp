#include "btree.h"

#include "common.h"
#include "small_vec.h"
#include <algorithm>
#include <cassert>
#include <compare>
#include <cstring>
#include <span>
#include <type_traits>

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
  void skip(usize size) {
    data += size;
  }
};

struct WCursor {
  char* data;

  WCursor& write_u16(u16 x) {
    *reinterpret_cast<u16*>(data)  = x;
    data                          += sizeof(x);
    return *this;
  }

  template <class T>
  WCursor& write_span(std::span<const T> t) {
    std::span<T> s{reinterpret_cast<T*>(data), t.size()};
    std::ranges::copy(t, s.begin());
    data += s.size_bytes();
    return *this;
  }
  WCursor& skip(usize size) {
    data += size;
    return *this;
  }

  WCursor clone() const {
    return *this;
  }
};

struct KV {
  std::span<const char> key;
  std::span<const char> val;

  usize size() const {
    return key.size_bytes() + val.size_bytes();
  }
};

struct KVArrayView {
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
    // Note: this is only true because the KV Array is a view into a node
    return std::span{reinterpret_cast<const char*>(offsets.data()), &*get(offsets.back()).val.end()};
  }

  // Number of kvs
  u16 size() const {
    return static_cast<u16>(offsets.size());
  }

  // Size taken if written in memory
  usize size_bytes() const {
    return as_bytes().size_bytes();
  }
};

struct KVArrayList {
  KVArrayView cur;
  KVArrayList* next;
};

struct NodeView {
  BTreeNodeType type;
  u16 nkeys;
  KVArrayView kvs;

  usize size() const {
    return sizeof(type) + sizeof(nkeys) + kvs.size_bytes();
  }
};

NodeView btree_decode_node(RCursor& c) {
  NodeView node{
      BTreeNodeType{c.read_u16()},
      c.read_u16(),
  };
  switch (node.type) {
  case BTreeNodeType::InternalNode:
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

struct NodeWriter {
  WCursor mem;

  BTreeNodeType type;
  u16 nkeys;

  u16 cur_idx;
  u16 cur_offset;

  NodeWriter(WCursor mem, BTreeNodeType type)
      : mem(mem)
      , type(type) {
    WCursor{mem}.write_u16(static_cast<u16>(type));
  }

  static NodeWriter with_header_from(WCursor mem, const NodeView& node) {
    NodeWriter n(mem, node.type);

    switch (node.type) {
    case BTreeNodeType::InternalNode:
      break;
    case BTreeNodeType::Leaf:
      break;
    }

    return n;
  }

  // This invalidate both offsets and kvs
  NodeWriter& set_nkeys(u16 count) {
    WCursor{mem}.skip(sizeof(u16)).write_u16(count);

    cur_idx    = 0;
    cur_offset = header_size() + count * sizeof(u16); // header + offsets
    return *this;
  }

  u16 header_size() const {
    return 2 * sizeof(u16);
  }

  NodeWriter& push_KV(KV kv) {
    // offset
    mem.clone().skip(header_size() + cur_idx * sizeof(u16)).write_u16(cur_offset);

    // kv
    {
      WCursor c{mem};
      c.skip(cur_offset);

      c.write_span(kv.key);
      c.write_span(kv.val);
    }

    cur_offset += kv.key.size() + kv.val.size();
    cur_idx    += 1;
    return *this;
  }

  NodeWriter& push_KVs(const KVArrayView& kvs, u16 start_idx, u16 end_idx) {
    for (u16 i = start_idx; i < end_idx; i++) {
      push_KV(kvs[i]);
    }
    return *this;
  }

  NodeView as_node_view() {
    RCursor c{mem.data};
    return btree_decode_node(c);
  };
};

NodeView btree_leaf_insert(WCursor mem, NodeView& node, u16 idx, KV kv) {
  return NodeWriter::with_header_from(mem, node)
      .set_nkeys(node.nkeys + 1)
      .push_KVs(node.kvs, 0, idx)
      .push_KV(kv)
      .push_KVs(node.kvs, idx, node.nkeys)
      .as_node_view();
}

// replace a kid with N kids
// precondition: child_pointers is a sorted span of kv of childs pointers
// + Ordering must be kept after the replace step
NodeView btree_node_replace_kids(WCursor mem, NodeView& node, u16 idx, std::span<KV> child_pointers) {
  auto nw = NodeWriter::with_header_from(mem, node);
  nw.set_nkeys(node.nkeys + static_cast<u16>(child_pointers.size()) - 1);

  nw.push_KVs(node.kvs, 0, idx);
  for (auto kv : child_pointers) {
    nw.push_KV(kv);
  }
  nw.push_KVs(node.kvs, idx + 1, (u16)node.kvs.size());

  return nw.as_node_view();
}

// Can split into 2. Node: this does not assure that the second node returned is of size lower than BTREE_PAGE_SIZE
// BUG: this does allocation and does not disallocate
small_vec<NodeView, 2> btree_split_node2(BTree& b, const NodeView& old) {
  u16 cutoff = 0;
  usize size = sizeof(u16) * 2;
  for (; cutoff < old.nkeys; cutoff++) {
    usize additional = sizeof(u16) +           // offset
                       old.kvs[cutoff].size(); // kv

    if (size + additional > BTREE_PAGE_SIZE) {
      break;
    }
    size += additional;
  }

  small_vec<NodeView, 2> ret;
  if (cutoff == old.nkeys) {
    /*split into 1*/
    ret.push_back(old);
  } else {
    {
      auto mem = b.allocate_page();
      auto w   = NodeWriter::with_header_from(WCursor{mem}, old);

      w.set_nkeys(cutoff);
      w.push_KVs(old.kvs, 0, cutoff);
      ret.push_back(w.as_node_view());
    }

    {
      auto mem = b.allocate_page();
      auto w   = NodeWriter::with_header_from(WCursor{mem}, old);

      w.set_nkeys(old.nkeys - cutoff);
      w.push_KVs(old.kvs, cutoff, old.nkeys - cutoff);
      ret.push_back(w.as_node_view());
    }
  }
  return ret;
}

// Can split into 3 if the layout is a B c where B is a big chunk of data while a and c are small
// BUG: this does allocation and does not disallocate
small_vec<NodeView, 3> btree_split_node3(BTree& b, const NodeView& old) {
  small_vec<NodeView, 3> ret;

  if (old.size() <= BTREE_PAGE_SIZE) {
    ret.push_back(old);
    return ret;
  }

  auto ret2 = btree_split_node2(b, old);
  assert(ret2.size() == 2);

  ret.push_back(ret2[0]);
  if (ret2[1].size() <= BTREE_PAGE_SIZE) {
    ret.push_back(ret2[1]);
  } else {
    for (auto y : btree_split_node2(b, ret2[1])) {
      ret.push_back(y);
    }
  }

  return ret;
}
