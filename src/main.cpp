#include "common.h"
#include "data/btree.h"
#include <cstdlib>
#include <vector>

struct Page {
  char* mem;
  usize uses;
};

struct BTreeTestImpl {
  std::vector<Page> pages;

  char* allocate_page() {
    auto mem = static_cast<char*>(malloc(BTREE_PAGE_SIZE));
    pages.push_back(Page{.mem = mem, .uses = 1});

    return mem;
  }

  void deallocate(void* ptr) {
    for (auto& page : pages) {
      if (ALIGN_DOWN(ptr, BTREE_PAGE_SIZE) == page.mem) {
        page.uses -= 1;
        if (page.uses == 0) {
          free(page.mem);

          std::swap(page, pages.back());
          pages.pop_back();
        }
      }
    }
  }
  void commit(void*) {}
  const char* get(u64 node_index) {
    todo();
  }

  BTreeVTable vtable() {
    return {
        .get           = [](void* self, u64 node_index) { return static_cast<decltype(this)>(self)->get(node_index); },
        .allocate_page = [](void* self) { return static_cast<decltype(this)>(self)->allocate_page(); },
        .commit        = [](void* self, void* ptr) { return static_cast<decltype(this)>(self)->commit(ptr); },
        .deallocate    = [](void* self, void* ptr) { return static_cast<decltype(this)>(self)->deallocate(ptr); },
    };
  }
};

int main(int argc, char* argv[]) {
  BTreeTestImpl t;
  BTree b{&t, t.vtable()};

  return 0;
}
