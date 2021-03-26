#include "test.h"
#include "../spu.h"
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


static void print_var(PrintfBuf& b, SoundProcessing& spu, SpuChVarFlag f, int begin, int size) {
  for (int i=begin; i<begin+size; ++i) {
    b.printf("%8x ", spu.get_var(f, i));
  }
  b.put('\n');
}


static void print_number(PrintfBuf& b, int begin, int size) {
  b.printf("    "); //4 sp
  for (int i=begin; i<begin+size; ++i) {
    b.printf("     ch%02d", i); //5+4 ch
  }
  b.put('\n');
}


static void print_sw(PrintfBuf& b, SoundProcessing& spu, int begin, int size) {
  static const char d[] = {'_', '<', '_', '>', '_', '|', '_', '!', '_', '*', '_', '~'};
  for (int i=begin; i<begin+size; ++i) {
    b.printf("  %c%c%c%c%c%c ", 
      d[spu.get_var(SpuChVarFlag::key_on, i)],
      d[spu.get_var(SpuChVarFlag::key_off, i)+2],
      d[spu.get_var(SpuChVarFlag::endx, i)+4],
      d[spu.get_var(SpuChVarFlag::fm, i)+6],
      d[spu.get_var(SpuChVarFlag::noise, i)+8],
      d[spu.get_var(SpuChVarFlag::echo, i)+10]
      );
  }
  b.put('\n');
}


static void print_adsr(PrintfBuf& b, SoundProcessing& spu, int begin, int size) {
  ADSRReg a;
  b.printf(" Atk ");
  for (int i=begin; i<begin+size; ++i) {
    a.v = spu.get_var(SpuChVarFlag::adsr, i);
    b.printf(" %c+%d<<%02d ", a.at_md?'^':' ', a.at_st, a.at_sh);
  }

  b.printf("\n Dec ");
  for (int i=begin; i<begin+size; ++i) {
    a.v = spu.get_var(SpuChVarFlag::adsr, i);
    b.printf(" ^-8<<%02d ", a.de_sh);
  }

  b.printf("\n Sus ");
  for (int i=begin; i<begin+size; ++i) {
    a.v = spu.get_var(SpuChVarFlag::adsr, i);
    b.printf(" %c%c%d<<%02d ", a.su_md?'^':' ', a.su_di?'-':'+', a.su_st, a.su_sh);
  }

  b.printf("\n Slv ");
  for (int i=begin; i<begin+size; ++i) {
    a.v = spu.get_var(SpuChVarFlag::adsr, i);
    b.printf("   %5d ", (a.su_lv+1) * 0x800);
  }

  b.printf("\n Rel ");
  for (int i=begin; i<begin+size; ++i) {
    a.v = spu.get_var(SpuChVarFlag::adsr, i);
    b.printf(" %c-3<<%02d ", a.re_md ?'^':' ', a.at_st, a.re_sh);
  }

  b.put('\n');
}


static void print_group(PrintfBuf& b, SoundProcessing& spu, int begin, int size) {
  print_number(b, begin, size);
  for (int i=0; i<9*size + 5; ++i) b.putchar('_');
  b.printf("\n VOL ");
    print_var(b, spu, SpuChVarFlag::volume, begin, size);
  b.printf("Wvol ");
    print_var(b, spu, SpuChVarFlag::work_volume, begin, size);
  b.printf("RATE ");
    print_var(b, spu, SpuChVarFlag::sample_rate, begin, size);
  b.printf("Sadr ");
    print_var(b, spu, SpuChVarFlag::start_address, begin, size);
  b.printf("Radr ");
    print_var(b, spu, SpuChVarFlag::repeat_address, begin, size);
  b.printf("AVol ");
    print_var(b, spu, SpuChVarFlag::adsr_volume, begin, size);
  b.printf("Aste ");
    print_var(b, spu, SpuChVarFlag::adsr_state, begin, size);
  b.printf("SW   ");
    print_sw(b, spu, begin, size);
  b.printf("ADSR ");
    print_var(b, spu, SpuChVarFlag::adsr, begin, size);
  print_adsr(b, spu, begin, size);
  b.put('\n');
}


static void print_register(SoundProcessing& spu) {
  PrintfBuf b;
  b.put('\n');
  print_group(b, spu, 0, 8);
  print_group(b, spu, 8, 8);
  print_group(b, spu, 16, 8);
  b.printf(MAGENTA("NOTE: < Key on  > Key off  | End  ! FM  * Noise  ~ Echo\n\n"));
}


static void print_switch(SoundProcessing& spu) {
  PrintfBuf b;
  b.printf("ECHO\n");
  b.bit(spu.get_var(SpuChVarFlag::echo, -1), "|");
  b.printf("FM\n");
  b.bit(spu.get_var(SpuChVarFlag::fm, -1), "|");
  b.printf("NOISE\n");
  b.bit(spu.get_var(SpuChVarFlag::noise, -1), "|");
  b.printf("END\n");
  b.bit(spu.get_var(SpuChVarFlag::endx, -1), "|");
  b.printf("CTRL\n");
  b.bit(spu.get_var(SpuChVarFlag::ctrl, -1), "|");
}


static bool load_sound_font(Bus& b, const char* filename, bool remove_loop_flag) {
  u32 bufsize = 512*1024;
  u8 *buf = new u8[bufsize];
  std::shared_ptr<u8> ps(buf);
  u32 size = readFile(buf, bufsize, filename);
  if (!size) return false;
  printf("Read %s, size %dB\n", filename, size);

  b.write16(0x1F80'1DA6, 0x200); // address
  b.write16(0x1F80'1DAC, 2 << 1); // mode 2
  b.write16(0x1F80'1DAA, 0); //stop
  const u16 write_mode = 1<<4;
  const u16 busy = 1<<10;
  u32 wait_count = 0;

  u16 *buf2 = (u16*) buf;
  for (u32 i = 0; i<(size >> 1); i+=32) {
    for (u32 x=0; x<32; ++x) {
      if (((x & 0b111) == 0)) {
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
  return true;
}


// 在音频内存中寻找循环点, size 入参表示 font 的数量, size 出参表示找到的数量
static void scan_loop_point(SoundProcessing& spu, u32 *font, u32& size) {
  u8* const spu_mem = spu.get_spu_mem();
  u32 fp = 0;
  PrintfBuf pb;
  pb.printf("\nRepeat point:\n");

  for (u32 i=1; i<SPU_MEM_SIZE; i+= 0x10) {
    if ((spu_mem[i] & 0x03) == 0x03) {
      if (fp < size) {
        pb.printf(BLUE("  %05x->%3d"), i, fp);
        font[fp] = (i) + 0x10;
        if ((fp & 3)==3) pb.putchar('\n');
        ++fp;
      } else {
        warn("Insufficient font point space\n");
        break;
      }
    }
  }
  pb.putchar('\n');
  size = fp;
}


void play_spu_current_font(SoundProcessing& spu, Bus& b, int use_channel_n) {
  u32 font[0xff] = {0x1000};
  u32 fp = sizeof(font) / sizeof(u32);
  u32 volume = 0;
  int font_i = 0;
  u16 pitch = 0x100;
  u16 mpitch = 0;
  u32 channelIdx = 0;
  u32 noise = 0;
  u32 nfm = 0;
  
  scan_loop_point(spu, font, fp);
  printf(YELLOW("Spu Play Mode Press ? to help > "));

  for (;;) {
    int ch = _getch();
    //printf("%d ", ch);

    if (ch > '0' && ch <= '9') {
      pitch = 0x100 + (ch - '1') * 0x400;
    } 
    else if (ch >= 'a' && ch <= 'z') {
      mpitch = toneMap[ch - 'a'] * 0x90;
    }
    else switch (ch) {
    case '?': {
      PrintfBuf b;
      b.printf("\r\tpress =-123456789   chang pitch\n");
      b.printf("\tpress a-z           play note\n");
      b.printf("\tpress [ ]           up left/right volume\n");
      b.printf("\tpress { }           down left/right volume, maybe change to sweet mode\n");
      b.printf("\tpress BS            stop all note\n");
      b.printf("\tpress 0             reset pitch and volume\n");
      b.printf("\tpress /             switch low pass filter\n");
      b.printf("\tpress ,.            change timbre\n");
      b.printf("\tpreee `             switch channel 0 noise\n");
      b.printf("\tpreee TAB           switch channel 1 Pitch Modulation\n");
      b.printf("\tpreee ENTER         show spu register.\n");
      b.printf("\tpreee \\             show spu channel switch.\n");
      b.printf("\tpress ESC           exit\n");
      b.printf("\tpreee ?             show help\n");
      continue;
      }

    case '\\':
      print_switch(spu);
      continue;

    case '\b':
      channelIdx = 0;
      b.write32(0x1F80'1D8C, 0xffff'ffff); // Koff
      continue;

    case 0x1b: // ESC
      printf(" Exit sound play.\n");
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

    case '`':
      if (noise) {
        noise = 0;
        printf("Close ch0 noise\n");
      } else {
        noise = 1;
        printf("Open ch0 noise\n");
      }
      b.write32(0x1F80'1D94, noise);

    case '\t':
      if (nfm) {
        nfm = 0;
        printf("Close ch1 fm\n");
      } else {
        nfm = 1<<1;
        printf("Open ch1 fm\n");
      }
      b.write32(0x1F80'1D90, nfm);
      continue;

    case '\r':
      print_register(spu);
      continue;

    default:
      continue;
    }

    b.write32(0x1F80'1C00 + (0x10 * channelIdx), volume); //音量
    b.write16(0x1F80'1C04 + (0x10 * channelIdx), pitch + mpitch); //音高
    b.write16(0x1F80'1C06 + (0x10 * channelIdx), (font[font_i]) >> 3); // 地址
    b.write32(0x1F80'1D88, 1 << channelIdx); // Kon
    printf(" channel %d pitch %d volume %x tone %d\n", 
        channelIdx, (pitch + mpitch) * (44100/0x1000), volume, font_i);

    if (++channelIdx >= use_channel_n) {
      channelIdx = 0;
    }
  }
}


// 该测试需要手动返回
static void spu_play_sound() {
  const char *font_files[] = {
    "D:\\ps1e\\demo\\YarozeSDK\\PSX\\DATA\\SOUND\\STD0.VB",
    "D:\\game\\bio2\\Pl0\\Rdt\\room1000\\snd0.vb",
  };

  // 调试开关
  const bool remove_loop_flag = 0; // 移除重复标记
  const u32  use_channel_n    = 24; // 使用的通道数量, 从 1-24
  const char *fname           = font_files[0];

  MemJit mj;
  MMU mmu(mj);
  Bus b(mmu);
  SoundProcessing spu(b);
  load_sound_font(b, fname, remove_loop_flag);

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

  printf("BIOS:");
  print_adsr(bios_adsr);
  printf("ADSR:");
  print_adsr(a.v);
  
  for (u32 i=0; i<use_channel_n; ++i) {
    u32 chi = 0x10 * i;
    b.write32(0x1F80'1C08 +chi, a.v); // adsr
    b.write16(0x1F80'1C06 +chi, 0x200); // 必须移动到正确的位置才能发出正确音色
    b.write16(0x1F80'1C04 +chi, 0x1000); // 设定频率 1000h == 44100
    b.write32(0x1F80'1C00 +chi, 0); // 音量
  }
  
  b.write32(0x1F80'1D84, 0xffff); // 混响音量
  b.write16(0x1F80'1DA2, 0x0007'F000 >> 3); // 混响地址
  b.write32(0x1F80'1DAA, 0x0000'F003); // SPUCNT, Unmute, 0x0000'F083启用混响
  play_spu_current_font(spu, b, use_channel_n);
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
  //spu_play_sound();
}

}