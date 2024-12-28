#include "btree.h"

#include "common.h"
#include <algorithm>
#include <cassert>
#include <compare>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

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

  u16 size() {
    return static_cast<u16>(offsets.size());
  }
};

struct KVArrayList {
  KVArray cur;
  KVArrayList* next;
};

struct NodeView {
  BTreeNodeType type;
  u16 nkeys;
  KVArray kvs;
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

  static NodeWriter with_header_from(WCursor mem, NodeView& node) {
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
    cur_offset = header_size();
    return *this;
  }

  u16 header_size() const {
    return 2 * sizeof(u16);
  }

  NodeWriter& push_KV(KV kv) {
    mem.clone().skip(header_size() + cur_idx * sizeof(u16)).write_u16(cur_offset);

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

  NodeWriter& push_KVs(const KVArray& kvs, u16 start_idx, u16 end_idx) {
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

template <class T, usize static_storage_length>
struct small_vec {
  usize _size     = 0;
  usize _capacity = static_storage_length;
  T* _data        = reinterpret_cast<T*>(static_storage);

  union MaybeUninitT {
    T t;
  };

  MaybeUninitT static_storage[static_storage_length];

  small_vec() {}

  template <usize N>
  small_vec(small_vec<T, N>&& other)
      : _size(std::exchange(other._size, 0))
      , _capacity(std::exchange(other._capacity, 0))
      , _data(std::exchange(other._data, nullptr)) {
    if (_capacity <= N) {
      // Move from other's static_storage to our own storage
      reserve_inner(_capacity);
    }
  }

  usize capacity() const {
    return _capacity;
  }
  usize size() const {
    return _size;
  }

  // Is in static_storage
  bool is_small() {
    return _capacity <= static_storage_length;
  }

  void reserve(usize size) {
    if (size <= this->size()) {
      return;
    }
    reserve_inner(size);
  }

  T* begin() {
    return _data;
  }
  T* end() {
    return _data + size();
  }

  void push_back(const T& t) {
    if (size() == capacity()) {
      reserve(capacity() * 2);
    }

    T* loc = &_data[size()];
    new (loc) T{t};
    _size += 1;
  }
  void pop() {
    assert(size() > 0);

    back().~T();
    _size--;

    if (2 * _size < _capacity) {
      reserve_inner(_size);
    }
  }

  auto& at(this auto& self, usize i) {
    assert(self.size() > i);
    return self._data[i];
  }
  auto& operator[](this auto& self, usize i) {
    return self.at(i);
  }
  auto& front(this auto& self) {
    assert(self.size() > 0);
    return self.at(0);
  }
  auto& back(this auto& self) {
    assert(self.size() > 0);
    return self.at(self.size() - 1);
  }

  ~small_vec() {
    for (auto& t : *this) {
      t.~T();
    }

    reserve_inner(0);
  }

private:
  void reserve_inner(usize size) {
    T* new_data        = reinterpret_cast<T*>(static_storage);
    usize new_capacity = static_storage_length;
    if (size > static_storage_length) {
      new_data     = new T[size]();
      new_capacity = size;
    }

    std::copy(begin(), end(), new_data);
    if (!is_small()) {
      delete[] (_data);
    }

    _data     = new_data;
    _capacity = new_capacity;
  }
};

template struct small_vec<int, 3>;

small_vec<NodeView, 3> btree_split_node(const NodeView& old) {}
