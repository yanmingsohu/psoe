#include "util.h" 
#include <stdio.h>
#include <exception>
#include <new>
#include <stdexcept>
#include <string.h>

#if defined(LINUX) || defined(MACOS)
#include <sys/mman.h>
#include <unistd.h>
#elif defined(WIN_NT)
#include <windows.h>
#include <memoryapi.h>
#include <stdlib.h>
#endif

namespace ps1e { 

LogLevel log_level = LogLevel::all;


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


void* melloc_exec(size_t size, void* near_addr) {
#if defined(LINUX) || defined(MACOS)
  return mmap(near_addr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(WIN_NT)
  /*size_t pps = get_page_size();
  void* a = VirtualAlloc(near_addr, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if (!a) {
    while (GetLastError() == ERROR_INVALID_ADDRESS) {
      near_addr = (u8*)near_addr + pps;
      a = VirtualAlloc(near_addr, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
      printf("-- %x %x %x\n", near_addr, a, GetLastError());
    }
  }
  printf("%x %x %d\n", near_addr, a, GetLastError());
  return a;*/
  void * p = malloc(size);
  DWORD old;
  VirtualProtect(p, size, PAGE_EXECUTE_READWRITE, &old);
  return p;
#else
  #error "Cannot support this OS"
#endif
}


bool free_exec(void* p, size_t size) {
#if defined(LINUX) || defined(MACOS)
  return 0 == munmap(p, size);
#elif defined(WIN_NT)
  //return VirtualFree(p, 0, MEM_RELEASE);
  free(p);
  return true;
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


MemBlock::MemBlock(void* near_addr) : used(0) {
  size = get_page_size();
  used = addr = (u8*) melloc_exec(size, near_addr);
  if (!addr) {
    throw std::bad_alloc();
  }
}


MemBlock::MemBlock(void* near_addr, size_t s) : used(0), size(s) {
  if (!s) {
    throw std::invalid_argument("Size must bigger than zero");
  }
  used = addr = (u8*) melloc_exec(size, near_addr);
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


void print_code(const char* src) {
  char buf[255];
  int i = 0;
  int s = 0;
  int line = 1;

  for (char ch = src[i]; ch != '\0'; ch = src[++i]) {
    if (ch == '\n' || ch == '\r') {
      if (i > s) {
        int len = min(i-s, sizeof(buf));
        memcpy(buf, &src[s], len);
        buf[len] = '\0';
        printf(MAGENTA("%4d |")" %s\n", line, buf);
      } else {
        printf(MAGENTA("%4d |")" \n", line);
      }
      ++line;
      s = i+1;
    }
  }
  if (s < i) {
    printf(MAGENTA("%4d |")" %s\n", line, &src[s]);
  } else {
    printf(MAGENTA("%4d |")" \n", line);
  }
}


#define warp_printf(fmt, color_prifix) \
  char f[1000] = ""; \
  sprintf(f, "%s%s\033[0m", color_prifix, fmt); \
  va_list __args; \
  va_start(__args, fmt); \
  vprintf(f, __args); \
  va_end(__args) \


void debug(const char* format, ...) {
  if (log_level > LogLevel::debug) return;
  warp_printf(format, "\x1b[1;30m");
}


void info(const char* format, ...) {
  if (log_level > LogLevel::info) return;
  warp_printf(format, "\x1b[32m");
}


void warn(const char* format, ...) {
  if (log_level > LogLevel::warn) return;
  warp_printf(format, "\x1b[33m");
}


void error(const char* format, ...) {
  if (log_level > LogLevel::error) return;
  warp_printf(format, "\x1b[31m");
}

#undef warp_printf

}