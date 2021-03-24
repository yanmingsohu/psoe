#include "test.h"
#include "../spu.inl"
#include <conio.h>

namespace ps1e_t {
using namespace ps1e;

static const u16 toneMap[] = {
  /* a */ 11,
  /* b */ 24,
  /* c */ 22,
  /* d */ 13,
  /* e */ 3,
  /* f */ 14,
  /* g */ 15,
  /* h */ 16,
  /* i */ 8,
  /* j */ 17,
  /* k */ 18,
  /* l */ 19,
  /* m */ 26,
  /* n */ 25,
  /* o */ 9,
  /* p */ 10,
  /* q */ 1,
  /* r */ 4,
  /* s */ 12,
  /* t */ 5,
  /* u */ 7,
  /* v */ 23,
  /* w */ 2,
  /* x */ 21,
  /* y */ 6,
  /* z */ 20, 
};


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


static void print_adsr(u32 v) {
  static const char* md[] = {"线性", "指数"};
  static const char* di[] = {"增加", "减少"};
  ADSRReg a;
  a.v = v;
  printf("ADSR %08X A[%s,H%d,T%d] D[H%d] S[%s,%s,H%d,T%d,L%d] R[%s,H%d]\n", 
         v,
         md[a.at_md], a.at_sh, a.at_st, 
         a.de_sh, 
         di[a.su_di], md[a.su_md], a.su_sh, a.su_st, a.su_lv,
         md[a.re_md], a.re_sh);
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

  // 调试开关
  const bool remove_loop_flag = 0; // 移除重复标记
  const u32  use_channel_n    = 24; // 使用的通道数量, 从 1-24

  SoundProcessing spu(b);
  b.write16(0x1F80'1DA6, 0x200); // address
  b.write16(0x1F80'1DAC, 2 << 1); // mode 2
  b.write16(0x1F80'1DAA, 0); //stop
  const u16 write_mode = 1<<4;
  const u16 busy = 1<<10;
  u8* const spu_mem = spu.get_spu_mem() + 0x1000;
  u32 wait_count = 0;
  u32 channelIdx = 0;

  u32 font[0xff] = {0x1000};
  u32 fp = 1;
  u32 volume = 0;

  u16 *buf2 = (u16*) buf;
  for (u32 i = 0; i<(size >> 1); i+=32) {
    for (u32 x=0; x<32; ++x) {
      if (((x & 0b111) == 0)) {
        if ((buf2[i+x] & 0xff00) == 0x300) {
          printf("\tRepeat point %x ", (i + x)<<1);
          if (fp < (sizeof(font)/sizeof(u32))) {
            printf("save in %d", fp);
            font[fp++] = ((i + x)<<1) + 0x1000 + 0x10;
          }
          putchar('\n');
        }
        if (remove_loop_flag) {
          buf2[i+x] = buf2[i+x] & 0x00ff;
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

    //printf("\r\t\t\tWrite %d", i);
  }
  
  b.write32(0x1F80'1DAA, 0x0000'C083); // Unmute
  print_hex("\nspu mem", spu_mem, 256);
  for (u32 i=0; i<1024; ++i) {
    if (spu_mem[i] != buf[i]) {
      if (remove_loop_flag && ((i & 0x0F) == 1)) {
        continue;
      }
      error("Write SPU memory bad %x\n", i);
    }
  }

  u32 bios_adsr = 0xdfed'8c7a;
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

  printf("BIOS fail:");
  print_adsr(bios_adsr);
  printf("DIY succ:");
  print_adsr(a.v);
  
  for (u32 i=0; i<use_channel_n; ++i) {
    u32 chi = 0x10 * i;
    b.write32(0x1F80'1C08 +chi, bios_adsr); // adsr
    b.write16(0x1F80'1C06 +chi, 0x200); // 必须移动到正确的位置才能发出正确音色
    b.write16(0x1F80'1C04 +chi, 0x1000); // 设定频率 1000h == 44100
    b.write32(0x1F80'1C00 +chi, volume); // 音量
  }
  //if (!remove_loop_flag) b.write16(0x1F80'1D88, 0xFFFF'ffff); // Kon

  printf("Press a-z 0-9 [] ,. : ");
  int font_i = 0;
  u16 pitch = 0x100;
  u16 mpitch = 0;

  for (;;) {
    int ch = _getch();

    if (ch > '0' && ch <= '9') {
      pitch = 0x100 + (ch - '1') * 0x400;
    } 
    else if (ch >= 'a' && ch <= 'z') {
      mpitch = toneMap[ch - 'a'] * 0x90;
    }
    else switch (ch) {
    case '\b':
      channelIdx = 0;
      b.write32(0x1F80'1D8C, 0xffff'ffff); // Koff
      continue;
    case 0x1b: // ESC
      printf("exit sound test");
      return;
    case '=': case '+':
      pitch += 0x10;
      break;
    case '-': case '_':
      pitch -= 0x10;
      break;
    case '0':
      pitch = 0x1000;
      volume = 0x0000'0000;
      break;
    case '/':
      spu.use_low_pass = !spu.use_low_pass;
      printf("use low pass %d\n", spu.use_low_pass);
      continue;
    case '[':
      volume += 0x0010;
      b.write32(0x1F80'1C00, volume);
      break;
    case ']':
      volume += 0x0010'0000;
      break;
    case '}':
      volume -= 0x0010'0000;
      break;
    case '{':
      volume -= 0x0010;
      break;
    case ',':
      --font_i;
      if (font_i < 0) font_i = fp-1;
      break;
    case '.':
      ++font_i;
      if (font_i >= fp) font_i = 0;
      break;
    }

    b.write32(0x1F80'1C00 + (0x10 * channelIdx), volume); //音量
    b.write16(0x1F80'1C04 + (0x10 * channelIdx), pitch + mpitch); //音高
    b.write16(0x1F80'1C06 + (0x10 * channelIdx), (font[font_i]) >> 3); // 地址
    b.write32(0x1F80'1D88, 1 << channelIdx); // Kon
    printf(" channel %d pitch %d volume %x tone %x\n", 
        channelIdx, (pitch + mpitch) * (44100/0x1000), volume, font_i);

    if (++channelIdx >= use_channel_n) {
      channelIdx = 0;
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
    printf("+ %d %d %d\n", i, level, cycle);
  }

  level = 0x7fff;
  a.reset(1, 1, 0x10, 3);

  for (int i=0; i<100; ++i) {
    a.next(level, cycle);
    printf("+ %d %d %d\n", i, level, cycle);
  }

  panic("stop");
}


void test_spu() {
  test_spu_reg();
  //test_adsr();
  spu_play_sound();
}

}