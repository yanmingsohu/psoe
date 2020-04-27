#include "util.h"
#include <exception>
#include <new>
#include <stdexcept>

#if defined(LINUX) || defined(MACOS)
#include <sys/mman.h>
#include <unistd.h>
#elif defined(WIN_NT)
#include <windows.h>
#endif

namespace ps1e {


bool check_little_endian() {
  union {
    u16 a;
    struct {
      u8  l;
      u8  h;
    };
  } test;
  test.a = 0x0100;
  return test.h;
}


void* melloc_exec(size_t size, void* near) {
#if defined(LINUX) || defined(MACOS)
  return mmap(near, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(WIN_NT)
  return VirtualAlloc(neer, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
  #error "Cannot support this OS"
#endif
}


bool free_exec(void* p, size_t size) {
#if defined(LINUX) || defined(MACOS)
  return 0 == munmap(p, size);
#elif defined(WIN_NT)
  return VirtualFree(p, 0, MEM_RELEASE);
#else
  #error "Cannot support this OS"
#endif
}


size_t get_page_size() {
#if defined(LINUX) || defined(MACOS)
  return getpagesize();
#elif defined(WIN_NT)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#else
  #error "Cannot support this OS"
#endif
}


MemBlock::MemBlock(void* near) : used(0) {
  size = get_page_size();
  used = addr = (u8*) melloc_exec(size, near);
  if (!addr) {
    throw std::bad_alloc();
  }
}


MemBlock::MemBlock(void* near, size_t s) : used(0), size(s) {
  if (!s) {
    throw std::invalid_argument("Size must bigger than zero");
  }
  used = addr = (u8*) melloc_exec(size, near);
  if (!addr) {
    throw std::bad_alloc();
  }
}


MemBlock::~MemBlock() {
  if (addr_set.size()) {
    throw std::logic_error("Some pointers are not released");
  }
  if (!free_exec(addr, size)) {
    throw std::runtime_error("Release mem fail");
  }
  addr = used = 0;
}


void* MemBlock::get(size_t s) {
  if (!s) {
    throw std::invalid_argument("Size must bigger than zero");
  }
  if (s > free_size()) {
    throw std::out_of_range("Not enough space");
  }
  void* m = used;
  used += s;
  addr_set.insert(m);
  return m;
}


void MemBlock::free(void* p) {
  const size_t count = addr_set.erase(p);
  if (!count) {
    throw std::out_of_range("Not belong(memblock.free)");
  }
  if (addr_set.size() == 0) {
    used = addr;
  }
}


int MemBlock::ref_count() {
  return addr_set.size();
}


size_t MemBlock::free_size() {
  return size - (used - addr);
}


const MemBlock::AddrSet& MemBlock::allocated() {
  return addr_set;
}


MemJit::MemJit() : base(0) {
  last = addr_list.end();
  min_block = get_page_size();
}


MemJit::~MemJit() {
  for (auto& n : addr_list) {
    delete n;
  }
  addr_list.clear();
  addr_map.clear();
  base = 0;
  last = addr_list.end();
}


void* MemJit::get(size_t size) {
  MemBlock *bl = 0;
  do {
    if (last != addr_list.end()) {
      bl = *last;
      if (bl->free_size() > size) {
        break;
      }
    } 
    bl = new MemBlock(base, size < min_block ? min_block : size);
    last = addr_list.insert(addr_list.begin(), bl);
  } while(0);

  void* mem = bl->get(size);
  addr_map.insert({mem, last});
  if (!base) base = mem;
  return mem;
}


void MemJit::free(void* p) {
  auto it = addr_map.find(p);
  if (it == addr_map.end()) {
    throw std::out_of_range("Not belong (memjit.free)");
  }

  MemBlock *bl = *(it->second);
  bl->free(p);
  addr_map.erase(p);

  if (bl->ref_count() == 0) {
    if (last == it->second) {
      last = addr_list.end();
    }
    addr_list.erase(it->second);
    delete bl;
  }
}

}