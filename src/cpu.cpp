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


void printSR(Cop0SR sr) {
  printf("COP0\tIE =%x KUc=%x IEp=%x KUp=%x IEo=%x IEp=%x SX=%x KX=%x \
            \n\tIM =%x NMI=%x SR =%x TS =%x BEV=%x PX=%x \
            \n\tMX =%x RE =%x FR =%x RP =%x CU =%x\n",\
                  sr.ie, sr.KUc, sr.IEp, sr.KUp, sr.IEo, sr.IEp, sr.sx, sr.kx,
                  sr.im, sr.nmi, sr.sr, sr.ts, sr.bev, sr.px,
                  sr.mx, sr.re, sr.fr, sr.rp, sr.cu);
}


}