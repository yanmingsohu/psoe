#include "test.h"
#include "../spu.inl"

namespace ps1e_t {
using namespace ps1e;


class EmuSpu {
public:
  void testreg1(Bus& b) {
    SpuIO<EmuSpu, SpuReg, DeviceIOMapper::spu_main_vol> i(*this, b);
    i.write(u32(0x1122'3344));
    eq(i.r.v, u32(0x1122'3344), "SpuIO reg val 1");
    i.write2(u16(0xeeaa));
    eq(i.r.v, u32(0xeeaa'3344), "SpuIO reg val 2");
    i.write3(u8(0xcc)); // 打印警告
    eq(i.r.v, u32(0xeeaa'3344), "SpuIO reg val 3 block");
    i.write2(u8(0xcc));
    eq(i.r.v, u32(0x00cc'3344), "SpuIO reg val 4");
  }

  void testreg2(Bus& b) {
    SpuBit24IO<EmuSpu, DeviceIOMapper::spu_reverb_vol> i(*this, b);
    i.write(u32(0xffff'ffff));
    eq(i.read(), u32((1<<24)-1), "SpuBit24IO 1");
    i.write(u8(0xA5));
    eq(i.read(), u32(0x00ff'ffa5), "SpuBit24IO 2");
  }
};


static void test_spu_reg(Bus& b) {
  SpuCntReg cnt;
  cnt.v = 0x8000;
  eq(cnt.spu_enb, u32(1), "cnt spu enb");
  eq(cnt.mute, u32(0), "cnt spu mute");

  EmuSpu es;
  es.testreg1(b);
  es.testreg2(b);
}


void test_spu() {
  MemJit mj;
  MMU mmu(mj);
  Bus b(mmu);
  test_spu_reg(b);
}

}