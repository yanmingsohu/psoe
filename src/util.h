#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <list>
#include <unordered_set>

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

// Check bound when read/write memory 
#define SAFE_MEM 
#define RED(s) "\x1b[31m" s "\033[0m"

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;

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


bool check_little_endian();
void* melloc_exec(size_t size, void* near = 0);
bool free_exec(void* p, size_t size = 0);

}