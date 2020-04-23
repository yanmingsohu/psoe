#include <stdio.h>
#include "cpu.h"

namespace ps1e {


void printSR(Cop0SR sr) {
  printf("COP0\tIE =%x EXL=%x ERL=%x KSU=%x UX =%x SX=%x KX=%x \
            \n\tIM7=%x NMI=%x SR =%x TS =%x BEV=%x PX=%x \
            \n\tMX =%x RE =%x FR =%x RP =%x CU =%x\n",\
                  sr.ie, sr.exl, sr.erl, sr.ksu, sr.ux, sr.sx, sr.kx,
                  sr.im7, sr.nmi, sr.sr, sr.ts, sr.bev, sr.px,
                  sr.mx, sr.re, sr.fr, sr.rp, sr.cu);
}


}