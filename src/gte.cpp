#include "gte.h"

namespace ps1e {


void GteIrgb::write(u32 d) {
  v = d;
  r.ir1.v = (0x1f & v) << 3;
  r.ir2.v = (0x1f & (v >> 5)) << 3;
  r.ir3.v = (0x1f & (v >> 10)) << 3;
}


u32 GteOrgb::read() {
  return i.v;
}


void GteFlag::write(u32 d) {
  v = GteReg63WriteMask & d;
}


void GteFlag::set(GteReg63Error f) {
  v |= static_cast<u32>(f);
  if (v & GteFlagLogSum) {
    v |= (1 << 31);
  } else {
    v &= (1 << 31)-1;
  }
}


void GteVectorXY::write(u32 d) {
  fx = (s32) (d & 0xffff);
  fy = (s32) ((d>>16) & 0xffff);
}


u32 GteVectorXY::read() {
  s16 x = fx;
  s16 y = fy;
  return x & (((s32) y) << 16);
}


}