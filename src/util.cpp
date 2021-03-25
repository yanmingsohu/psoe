#include "util.h" 
#include <stdio.h>
#include <string.h>
#include <exception>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <chrono>
#include <thread>

#if defined(LINUX) || defined(MACOS)
#include <sys/mman.h>
#include <sys/types.h>
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


size_t readFile(void *buf, size_t bufsize, char const *filename) {
  FILE* f = fopen(filename, "rb");
  if (!f) {
    warn("cannot open file %s\n", filename);
    return 0;
  }
  auto closeFile = createFuncLocal([f] { 
    fclose(f);
  });
  size_t readsize = fread(buf, 1, bufsize, f);
  if (readsize < 0) {
    return 0;
  }
  return readsize;
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


void print_hex(const char* title, u8* data, u32 size, s32 addrOffset) {
  PrintfBuf buf;
  buf.printf("%s\n", title);
  buf.printf("|----------|-");
  for (u32 i=0; i<0x10; ++i) {
    buf.printf(" -%X", (s32(data + i) + addrOffset) & 0x0F);
  }
  buf.printf(" | ----------------");

  for (u8* i=data; i<data+size; i+=16) {
    buf.printf("\n 0x%08X| ", s32(i) + addrOffset);

    for (u8* j=i; j<i+16; ++j) {
      buf.printf(" %02X", *j);
    }
    
    buf.printf("   ");
    for (u8* j=i; j<i+16; ++j) {
      u8 c = *j;
      if (c >= 32) {
        buf.putchar(c);
      } else {
        buf.putchar('.');
      }
    }
  }
  buf.printf("\n|-over-----|- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- | ----------------\n");
}


void print_binary(u32 v) {
  PrintfBuf buf;
  buf.bit(v);
}


void sleep(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


size_t this_thread_id() {
#if defined(LINUX) || defined(MACOS)
  return gettid();
#elif defined(WIN_NT)
  return GetCurrentThreadId();
#endif
}


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


// 禁止调用
u32 add_us(u32 a, s32 b) {
  if (b < 0) {
    return a - u16(b);
  }
  return a + b;
}


// 写入的总长度不能超过 1000(内部缓冲区) 否则直接输出到 stdout
void PrintfBuf::printf(const char* fmt, ...) {
  va_list __args;
  __crt_va_start(__args, fmt);
  const int ls = sizeof(buf) - wc;
  int cu = vsnprintf(buf + wc, ls, fmt, __args);

  if (cu >= sizeof(buf)) {
    buf[wc] = '\0';
    flush();
    printf(fmt, __args);
  } else if (cu >= ls - 1) {
    buf[wc] = '\0';
    flush();
    vsnprintf(buf, sizeof(buf), fmt, __args);
  } if (cu < 0) {
    error("bad format: %s", fmt);
  } else {
    wc += cu;
  }
  __crt_va_end(__args);
}


void PrintfBuf::bit(u32 v, const char* prefix) {
  if (prefix) printf("%s", prefix);
  for (int i=31; i>=0; --i) {
    printf("%02d ", i);
  }
  putchar('\n');
  if (prefix) printf("%s", prefix);
  for (int i=31; i>=0; --i) {
    if (((1<<i) & v) == 0) {
      printf("__ ");
    } else {
      printf(".1 ");
    }
  }
  putchar('\n');
}


void PrintfBuf::flush() {
  fputs(buf + offset, stdout);
  reset();
}


void PrintfBuf::putchar(char c) {
  if (wc >= sizeof(buf) - 2) {
    flush();
  }
  buf[wc] = c;
  buf[++wc] = '\0';
}


void PrintfBuf::reset() {
  wc = 0;
  offset = 0;
  buf[0] = '\0';
}

}