#pragma once

#include "../common.h"
#include <utility>

template <class T, usize static_storage_length>
struct small_vec {
  usize _size     = 0;
  usize _capacity = static_storage_length;
  /*Humph.. is this reinterpret cast sound? Well idk but it should work*/
  T* _data        = reinterpret_cast<T*>(static_storage);

  struct alignas(T) MaybeUninitT {
    char t[sizeof(T)];
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
