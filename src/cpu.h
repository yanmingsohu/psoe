#pragma once
#include "util.h"

namespace ps1e {

typedef union {
  u32 r[32];

  struct {
    u32 zero; // r00
    u32 at;   // r01
    u32 v0;   // r02   
    u32 v1;   // r03
    u32 a0;   // r04 - r07
    u32 a1, a2, a3;
    u32 t0;   // r08 - r15
    u32 t1, t2, t3, t4, t5, t6;
    u32 s0;   // r16 - r23
    u32 s1, s2, s3, s4, s5, s6, s7; 
    u32 t8;   // r24
    u32 t9;   // r25
    u32 k0;   // r26
    u32 k1;   // r27
    u32 gp;   // r28
    u32 sp;   // r29
    u32 fp;   // r30
    u32 ra;   // r31
  };
} MipsReg;


typedef union {
  u32 v;
  struct {
    u32 ie  : 1; // bit 0
    u32 exl : 1;
    u32 erl : 1;
    u32 ksu : 2;
    u32 ux  : 1;
    u32 sx  : 1;
    u32 kx  : 1;
    u32 im7 : 8; // bit 8
    u32 z   : 3; // bit 16
    u32 nmi : 1;
    u32 sr  : 1;
    u32 ts  : 1;
    u32 bev : 1;
    u32 px  : 1;
    u32 mx  : 1; // bit 24
    u32 re  : 1;
    u32 fr  : 1;
    u32 rp  : 1;
    u32 cu  : 4;
  };
} Cop0SR;


typedef union {
  u32 r[16];

  struct {
    u32 index; // r00 not use
    u32 rand;  // r01 not use
    u32 tlbl;  // r02 not use
    u32 bpc;   // r03
    u32 ctxt;  // r04
    u32 bda;   // r05
    u32 pidm;  // r06
    u32 dcic;  // r07
    u32 badv;  // r08
    u32 bdam;  // r09
    u32 tlbh;  // r10 not use
    u32 bpcm;  // r11
    Cop0SR sr; // r12
    u32 cause; // r13
    u32 epc;   // r14
    u32 prid;  // r15 
  };
} Cop0Reg;


void printSR(Cop0SR sr);


}