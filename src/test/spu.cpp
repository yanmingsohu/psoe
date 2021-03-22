#include "test.h"
#include "../spu.inl"
#include <conio.h>

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


// 该测试永不返回
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

  const bool use_io = 1;
  const bool remove_loop_flag = 0;

  SoundProcessing spu(b);
  b.write16(0x1F80'1DA6, 0x200); // address
  b.write16(0x1F80'1DAC, 2 << 1); // mode 2
  b.write16(0x1F80'1DAA, 0); //stop
  const u16 write_mode = 1<<4;
  const u16 busy = 1<<10;
  u8* const spu_mem = spu.get_spu_mem() + 0x1000;
  u32 wait_count = 0;

  u32 font[26] = {0x1000};
  u32 fp = 1;

  if (use_io) {
    u16 *buf2 = (u16*) buf;
    for (u32 i = 0; i<(size >> 1); i+=32) {
      for (u32 x=0; x<32; ++x) {
        if (((x & 0b111) == 0)) {
          if ((buf2[i] & 0xff00) == 0x300) {
            printf("Repeat point %x\n", i + x*2);
            //print_hex("\nspu mem", spu_mem, 256);
            if (fp < 26) {
              font[fp++] = i + x*2 + 0x1000;
            }
            if (remove_loop_flag) {
              buf2[i+x] = buf2[i+x] & 0x00ff;
            }
          }
        }
        b.write16(0x1F80'1DA8, buf2[i+x]);
      }
      b.write16(0x1F80'1DAA, write_mode);
   
      // wait write
      while ((b.read16(0x1F80'1DAE) & write_mode) == 0)
        printf("\rwait write %u %u", i, ++wait_count);

      // wait not busy
      //sleep(1); // 必须硬等待到 busy = 1, 然后等待 busy=0
      while (b.read16(0x1F80'1DAE) & busy)
        printf("\rwait not busy %u %u", i, ++wait_count);

      printf("\r\t\t\tWrite %d", i);
    }
  } 
  else {
    for (u32 i = 0; i<size; i++) {
      // 删除循环位
      if (remove_loop_flag && ((i & 0x0F) == 1)) {
        spu_mem[i] = 0;
        if (buf[i] == 3) {
          printf("Repeat point %x\n", i);
        }
      } else {
        spu_mem[i] = buf[i];
      }
    }
  }
  
  print_hex("\nspu mem", spu_mem, 256);
  for (u32 i=0; i<1024; ++i) {
    if (spu_mem[i] != buf[i]) {
      if (remove_loop_flag && ((i & 0x0F) == 1)) {
        continue;
      }
      error("Write SPU memory bad %x\n", i);
    }
  }

  ADSRReg a;
  a.v = 0;
  a.at_md = 1;
  a.at_sh = 5;
  a.at_st = 4;
  a.de_sh = 10;

  a.su_lv = 0;
  a.su_di = 1;
  a.su_md = 1;
  a.su_sh = 0x1f;
  a.su_st = 3;
  
  b.write32(0x1F80'1C08, a.v); // adsr
  b.write32(0x1F80'1C18, a.v); // adsr
  b.write32(0x1F80'1C28, a.v); // adsr
  b.write32(0x1F80'1C38, a.v); // adsr

  // 必须移动到正确的位置才能发出正确音色
  b.write16(0x1F80'1C06, 0x200);
  b.write16(0x1F80'1D88, 0xFFFF'ffff); // Kon
  //b.write16(0x1F80'1C0E, 0x78C); // 重复地址
  puts("Wait play sound");

  while (!use_io) {
    ps1e::sleep(1 * 1000);
  }

  printf("Press 'a' ~ 'z': ");
  for (;;) {
    int ch = _getch();
    _putch(ch);
    if (ch == 0x1b) {// ESC
      printf("exit sound test");
      return;
    }

    int font_i = 'z' - ch;
    printf(" [%x]\n", (font[font_i] + 0x10));
    if (font_i >= 0 && font_i < fp) {
      b.write16(0x1F80'1C06 + (0x10 * (font_i & 0xF)), (font[font_i]) >> 3);
      b.write16(0x1F80'1D88, 1 << font_i); // Kon
    }
  }
}


static void test_adsr() {
  SpuAdsr a;
  a.reset(0, 0, 0x10, 3);
  s32 level = 0;
  u32 cycle = 0;

  for (int i=0; i<100; ++i) {
    a.next(level, cycle);
    printf("+ %d %f %d\n", i, level, cycle);
  }

  level = 0x7fff;
  a.reset(1, 1, 0x10, 3);

  for (int i=0; i<100; ++i) {
    a.next(level, cycle);
    printf("+ %d %f %d\n", i, level, cycle);
  }

  panic("stop");
}


void test_spu() {
  test_spu_reg();
  //test_adsr();
  spu_play_sound();
}

}