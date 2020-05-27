#pragma once

#include "util.h"

namespace ps1e {

#define GteReg63WriteMask 0x7FFFF000
#define GteFlagLogSum     0x7F87E000


enum class GteReg63Error : u32 {
  Error = 1<<31, // (Bit30..23, and 18..13 ORed together) (Read only)
  Mac1p = 1<<30, // MAC1结果大于43位且为正
  Mac2p = 1<<29, // MAC2 Result larger than 43 bits and positive
  Mac3p = 1<<28,
  Mac1n = 1<<27, // MAC1结果大于43位且为负
  Mac2n = 1<<26, // MAC2 Result larger than 43 bits and negative
  Mac3n = 1<<25,
  Ir1   = 1<<24, // IR1 饱和至 +0000h .. +7FFFh（lm = 1）或 -8000h .. +7FFFh（lm = 0）
  Ir2   = 1<<23, // IR2 saturated to +0000h..+7FFFh (lm=1) or to -8000h..+7FFFh (lm=0)
  Ir3   = 1<<22, 
  R     = 1<<21, // Color-FIFO-R 饱和至 +00h .. +FFh
  G     = 1<<20, // Color-FIFO-G saturated to +00h..+FFh
  B     = 1<<19,
  Sz3   = 1<<18, // SZ3 or OTZ saturated to +0000h..+FFFFh
  Div   = 1<<17, // Divide overflow. RTPS/RTPT division result saturated to max=1FFFFh
  Mac0p = 1<<16, // MAC0 Result larger than 31 bits and positive
  Mac0n = 1<<15, // MAC0 Result larger than 31 bits and negative
  Sx2   = 1<<14, // SX2 saturated to -0400h..+03FFh
  Sy2   = 1<<13, // SY2 saturated to -0400h..+03FFh
  Ir0   = 1<<12, // IR0 saturated to +0000h..+1000h
};


typedef u32 GteDataRegisterType;
typedef u32 GteCtrlRegisterType;
class GteDataReg;


struct GteFlag {
  u32 v;

  u32 read() { return v; }
  void write(u32 d);
  void set(GteReg63Error f);
};


struct GteVectorXY {
  float fx, fy;

  void write(u32 xy);
  u32 read();
};


struct GteVectorZ {
  float fz;

  void write(u32 z) { fz = (s32) z; }
  u32 read() { return (s32) fz; }
};


struct GteRgb {
  u32 rgb;

  void write(u32 r) { rgb = r; }
  u32 read() { return rgb; }
};


template<int Lbit, GteReg63Error IRE> 
struct GteIR {
  GteDataReg& r;
  u32 v;

  GteIR(GteDataReg& _r) : r(_r) {}

  u32 read() { return v; }

  void write(u32 _v) {
    v = _v;
    u8 s;
    if (v & 0x8000) {
      s = 0;
      r.flag.set(IRE);
    } else if (v & 0x7000) {
      s = 0x1f;
      r.flag.set(IRE);
    } else {
      s = v >> 3;
    }
    u32 mask = 0x1f << Lbit;
    r.irgb.rgb = (r.irgb.rgb & mask) | (s << Lbit);
  }
};


struct GteIrgb {
  u32 v;
  GteDataReg& r;

  GteIrgb(GteDataReg& _r) : r(_r) {}
  u32 read() { return 0; }
  void write(u32 _v);
};


struct GteOrgb {
  GteIrgb &i;

  GteOrgb(GteIrgb& _i) : i(_i) {}

  void write(u32) {}
  u32 read();
};


struct GteFloat {
  float r;

  u32 read() { return (s32) r; }
  void write(u32 v) { r = (s32) v; }
};


class GteReg {
private:
  // r0-r1 Vector 0 (X,Y,Z)
  GteVectorXY vxy0;
  GteVectorZ  vz0; 
  // r2-r3 Vector 1 (X,Y,Z)
  GteVectorXY vxy1; 
  GteVectorZ  vz1; 
  // r4-r5 Vector 2 (X,Y,Z)
  GteVectorXY vxy2;
  GteVectorZ  vz2; 
  // r6 4xU8 Color/code value
  GteDataRegisterType rgbc;      
  // r7 Average Z value (for Ordering Table)
  GteDataRegisterType otz;       
  // r8 16bit Accumulator (Interpolate)
  GteFloat ir0;
  // r9-r11 16bit Accumulator (Vector)
  GteIR<0, GteReg63Error::Ir1> ir1; 
  GteIR<5, GteReg63Error::Ir2> ir2;
  GteIR<10, GteReg63Error::Ir3> ir3;
  // r12-r15 Screen XY-coordinate FIFO  (3 stages)
  GteDataRegisterType sxy0, sxy1, sxy2, sxyp;
  // r16-r19 Screen Z-coordinate FIFO   (4 stages)
  GteDataRegisterType sz0, sz1, sz2, sz3;
  // r20-r22 Color CRGB-code/color FIFO (3 stages)
  GteDataRegisterType rgb0, rgb1, rgb2;
  // Prohibited
  GteDataRegisterType res1;
  // r24 32bit Maths Accumulators (Value)
  GteDataRegisterType mac0;
  // r25-27 32bit Maths Accumulators (Vector)
  GteDataRegisterType mac1, mac2, mac3;
  // r28-r29 Convert RGB Color (48bit vs 15bit)
  GteDataRegisterType irgb, orgb;
  // r30-r31 Count Leading-Zeroes/Ones (sign bits)
  GteDataRegisterType lzcs, lzcr;

  // r63/cnt31 Returns any calculation errors
  GteFlag flag;

public:
  GteReg();

  void write_data(u32 index, u32 data);
  u32 read_data(u32 index);

  void write_ctrl(u32 index, u32 data);
  u32 read_ctrl(u32 index);

friend struct GteIrgb;
template<int, GteReg63Error> friend struct GteIR;
};


union GteCtrlReg {
  GteCtrlRegisterType c[32];
  struct {
  };
};

}