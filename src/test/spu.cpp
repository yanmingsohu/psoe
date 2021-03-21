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


static void test_spu_reg() {
  MemJit mj;
  MMU mmu(mj);
  Bus b(mmu);

  SpuCntReg cnt;
  cnt.v = 0x8000;
  eq(cnt.spu_enb, u32(1), "cnt spu enb");
  eq(cnt.mute, u32(0), "cnt spu mute");

  EmuSpu es;
  es.testreg1(b);
  es.testreg2(b);
}


// 该测试消耗很长时间
static void spu_play_sound() {
  const char *fname = "D:\\ps1e\\demo\\YarozeSDK\\PSX\\DATA\\SOUND\\STD0.VB";
  u32 bufsize = 1024*1024;
  u8 *buf = new u8[bufsize];
  std::shared_ptr<u8> ps(buf);
  u32 size = readFile(buf, bufsize, fname);
  if (!size) return;
  printf("Read %s, size %dB\n", fname, size);
  
  MemJit mj;
  MMU mmu(mj);
  Bus b(mmu);

  SoundProcessing spu(b);
  b.write16(0x1F80'1DA6, 0x200); // address
  b.write16(0x1F80'1DAC, 2 << 1); // mode 2
  b.write16(0x1F80'1DAA, 0); //stop
  const u16 write_mode = 1<<4;
  const u16 busy = 1<<10;
  const bool use_io = false;

  if (use_io) {
    u16 *buf2 = (u16*) buf;
    for (u32 i = 0; i<(size >> 1); i+=32) {
      for (u32 x=0; x<32; ++x) {
        b.write16(0x1F80'1DA8, buf2[i+x]);
      }
      b.write16(0x1F80'1DAA, write_mode);
   
      // wait write
      while ((b.read16(0x1F80'1DAE) & write_mode) == 0)
        printf("\rwait write %d", i);

      // wait not busy
      while (b.read16(0x1F80'1DAE) & busy)
        printf("\rwait not busy %d", i);

      b.write16(0x1F80'1DAA, 0); //stop
      printf("\r%d", i);
    }
  } else {
    u8* spu_mem = spu.get_spu_mem() + 0x1000;
    for (u32 i = 0; i<size; i++) {
      spu_mem[i] = buf[i];
    }
    print_hex("spu mem", spu_mem, 256);
  }

  // 必须移动到正确的位置才能发出正确音色
  b.write16(0x1F80'1C06, 0x200);
  puts("Wait play sound");
  sleep(60 * 1000);
}


void test_spu() {
  test_spu_reg();
  //spu_play_sound();
}

}