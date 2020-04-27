#pragma once

#include "../cpu.h"
#include "../asm.h"
#include "../mips.h"
#include "../gpu.h"

namespace ps1e_t {

void panic(char const* msg);
void tsize(int s, int t, char const* msg);

template<class T>
void eq(T a, T b, char const* errmsg) {
  if (a != b) {
    char tmsg[255];
    sprintf(tmsg, "Not EQ %s, %d != %d", errmsg, a, b);
    panic(tmsg);
  }
}

void test_disassembly();
void test_cpu();
void test_jit();
void test_util();
void test_gpu();
void test_dma();

void test();

}