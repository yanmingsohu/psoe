#include <stdio.h> 
#include "cpu.h"

namespace ps1e {

const char* MipsRegName[] = {
  "zr", "at", "v0", "v1",
  "a0", "a1", "a2", "a3",
  "t0", "t1", "t2", "t3",
  "t4", "t5", "t6", "t7",
  "s0", "s1", "s2", "s3",
  "s4", "s5", "s6", "s7",
  "t8", "t9", "k0", "k1",
  "gp", "sp", "fp", "ra",
};


const char* MipsCauseStr[] = {
  /*  0 */ "External Interrupt",
  /*  1 */ "TLB modification exception",
  /*  2 */ "TLB load exception",
  /*  3 */ "TLB store exception",
  /*  4 */ "Address error, Data load or Instruction fetch",
  /*  5 */ "Address error, Data store",
  /*  6 */ "Bus error on Instruction fetch",
  /*  7 */ "Bus error on Data load/store",
  /*  8 */ "Syscall",
  /*  9 */ "Breakpoint",
  /* 10 */ "Reserved instruction",
  /* 11 */ "Coprocessor unusable",
  /* 12 */ "Arithmetic overflow",
};


void printSR(Cop0SR sr) {
  printf("COP0\tIE =%x KUc=%x IEp=%x KUp=%x IEo=%x IEp=%x SX=%x KX=%x \
            \n\tIM =%x NMI=%x SR =%x TS =%x BEV=%x PX=%x \
            \n\tMX =%x RE =%x FR =%x RP =%x CU =%x\n",\
                  sr.ie, sr.KUc, sr.IEp, sr.KUp, sr.IEo, sr.IEp, sr.sx, sr.kx,
                  sr.im, sr.nmi, sr.sr, sr.ts, sr.bev, sr.px,
                  sr.mx, sr.re, sr.fr, sr.rp, sr.cu);
}


void printMipsReg(MipsReg& r) {
  for (int i=0; i<MIPS_REG_COUNT; ++i) {
    printf("$%2d  $%s  %08x [ %12d_s %12u_u ]\n",
           i, MipsRegName[i], r.u[i], r.s[i], r.u[i]);
  }
}


}