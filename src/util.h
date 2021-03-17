#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <list>
#include <unordered_set>

namespace ps1e_t {
  extern int ext_stop;
}

namespace ps1e {

#if defined(_WIN32) || defined(_WIN64) || \
    defined(__WIN32__) || defined(__TOS_WIN__) || \
    defined(__WINDOWS__)
  #define WIN_NT
#elif defined(linux) || defined(__linux) || \
      defined(__linux__) || defined(__gnu_linux__) 
  #define LINUX
#elif defined(macintosh) || defined(Macintosh) || \
      (defined(__APPLE__) && defined(__MACH__))
  #define MACOS
#endif 

// Check bound when read/write memory; 
// Remove it when debug over.
#define SAFE_MEM 
#define NOT(x)        (!(x))
#define RED(s)        "\x1b[31m" s "\033[0m"
#define GREEN(s)      "\x1b[32m" s "\033[0m"
#define YELLOW(s)     "\x1b[33m" s "\033[0m"
#define BLUE(s)       "\x1b[34m" s "\033[0m"
#define MAGENTA(s)    "\x1b[35m" s "\033[0m"
#define CYAN(s)       "\x1b[36m" s "\033[0m"
#define GRAY(s)       "\x1b[37m" s "\033[0m"
#define SIGNEL_MASK32 0b1000'0000'0000'0000'0000'0000'0000'0000
#define SIGNEL_MASK16 0b1000'0000'0000'0000
#define SIGNEL_MASK8  0b1000'0000

// tar(nochange=~mask, change=mask) = mask & src
// tar 中 maks 为 0 的部分不变; 为 1 的部分从 src 复制.
#define SET_BIT(tar, src_mask, src)  (((tar) & (~src_mask)) | ((src) & (src_mask)))

#define CASE_MEM_MIRROR(d1) \
    case ((d1 & 0x0FFF'FFFF) | 0x1000'0000): \
    case ((d1 & 0x0FFF'FFFF) | 0x9000'0000): \
    case ((d1 & 0x0FFF'FFFF) | 0xB000'0000)


using s8  = int8_t;
using u8  = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;

typedef u32 psmem;

enum class LogLevel {
  all,
  debug,
  info,
  warn,
  error,
};
extern LogLevel log_level;

//
// 固定内存块分配器, 任何操作失败都可以抛出 std::exception 异常 
// 适合于小/固定内存分配
//
class MemBlock {
public:
  typedef std::unordered_set<void*> AddrSet; 

private:
  AddrSet addr_set;
  u8* addr;
  u8* used;
  size_t size;

  void operator=(MemBlock&);
  MemBlock(MemBlock&);

public:
  MemBlock(void* near);
  MemBlock(void* near, size_t);
  // 如果已经分配的内存块没有归还, 将抛出异常
  ~MemBlock();

  // 尝试分配 s 字节的内存块, 失败抛出异常
  void* get(size_t s);
  // 归还内存块, 该方法并不会释放内存, 不保证使失败的 get 调用变成成功
  void free(void* addr);
  // 返回该内存块分配出去内存的(引用)数量
  int ref_count();
  // 该内存块可用内存(字节)
  size_t free_size();
  // 已分配内存地址迭代器
  const AddrSet& allocated(); 
};


//
// 动态内存分配器, 对块内存的包装, 总是尽可能用尽一个块
// 并在块用尽后分配新的块, 老块在完全释放后被丢弃(返还给系统)
// 内存将尽可能的在 4GB 内靠近; 线程不安全
// 任何操作失败都可以抛出 std::exception 异常 
//
class MemJit {
private:
  typedef std::list<MemBlock*> AddrList;
  typedef std::unordered_map<void*, AddrList::iterator> AddrMap;

  size_t min_block;
  void* base;
  AddrList::iterator last;
  AddrList addr_list;
  AddrMap addr_map;

  void operator=(MemJit&);
  MemJit(MemJit&);

public:
  MemJit();
  ~MemJit();

  // 分配内存
  void* get(size_t);
  // 释放内存
  void free(void*);
};


template<class T> class Overflow {
private:
  T s, neq;
  T mask;
public:
  Overflow(T x, T y) {
    mask = (1 << (sizeof(T)*8 - 1));
    s = (x & y) & mask;
    neq = (x ^ y) & mask;
    // printf("O mask %x s %x\n", mask, neq);
  }
  bool check(T result) {
    if (neq) return false;
    return s ^ (result & mask);
  }
};


template<class F> class FuncLocal {
private:
  F release;
public:
  FuncLocal(F f) : release(f) {}
  virtual ~FuncLocal() {
    release();
  }
};


class NonCopy {
private:
  NonCopy(const NonCopy &) = delete;
  NonCopy &operator=(const NonCopy&) = delete;
public:
  NonCopy() {}
  virtual ~NonCopy() {}
};


template<class F> FuncLocal<F> createFuncLocal(F f) {
  return FuncLocal<F> (f);
}


// 返回 reserve 和 set 逐位运算的结果.
// 该运算使 set 中的位复制到 reserve 中, 如果对应 reserveMask 位是 1,
// 否则 reserve 中的位不变.
template<class T> inline T setbit_with_mask(T reserve, T set, T reserveMask) {
  return (reserveMask & ~reserve) | (reserveMask & set);
}


bool    check_little_endian();
void*   melloc_exec(size_t size, void* near = 0);
bool    free_exec(void* p, size_t size = 0);
size_t  get_page_size();
void    print_code(const char* src);
void    print_hex(const char* title, u8* data, u32 size);
size_t  readFile(void *buf, size_t bufsize, char const* filename);
size_t  this_thread_id();
u32     add_us(u32, s32);

void debug(const char* format, ...);
void info(const char* format, ...);
void warn(const char* format, ...);
void error(const char* format, ...);

}