#pragma once

#include "../cpu.h"
#include "../asm.h"
#include "../mips.h"
#include "../gpu.h"
#include "../inter.h"

namespace ps1e_t {

void panic(char const* msg);
void tsize(int s, int t, char const* msg);
extern int ext_stop;

template<class T>
void eq(T a, T b, char const* errmsg) {
  if (a != b) {
    char tmsg[255];
    sprintf(tmsg, "Not EQ %s, %d[%Xh] != %d[%Xh]", errmsg, a, a, b, b);
    panic(tmsg);
  }
}

void test_disassembly();
void test_cpu();
void test_jit();
void test_util();
void test_gpu(ps1e::GPU& gpu, ps1e::Bus& bus);
void test_dma();
void test_cd();

// 专门用于调试 cpu, 可在任何条件下调用
void debug_system(ps1e::R3000A& cpu, ps1e::Bus& bus, ps1e::MMU&);

}