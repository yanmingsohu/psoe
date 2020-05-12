#include "../gpu.h"
#include "test.h"
#include <thread>
#include <chrono>

namespace ps1e_t {
using namespace ps1e;
static const u32 gp0 = 0x1F80'1810;
static const u32 gp1 = 0x1F80'1814;


void gpu_basic() {
  tsize(sizeof(GpuStatus), 4, "gpu ctrl reg");
  tsize(sizeof(GpuCommand), 4, "gpu command");
}


union pos {
  u32 v;
  struct {
    u32 x:16; 
    u32 y:16; 
  };

  pos(u16 _x, u16 _y) : x(_x), y(_y) {}
};

union Color {
  u32 v;
  struct {
    u32 r:8; 
    u32 g:8; 
    u32 b:8; 
    u32 _:8;
  };

  Color(u8 cmd, u8 _r, u8 _g, u8 _b) : r(_r), g(_g), b(_b), _(cmd) {}
};

union TexpageAttr {
  u32 v;
  struct {
    u32 px          : 4; //0-3
    u32 py          : 1; //4
    u32 semi        : 2; //5-6
    u32 color_mode  : 2; //7-8
    u32 c24         : 1; //9
    u32 draw        : 1; //10
    u32 text_off    : 1; //11
    u32 x_flip      : 1; //12
    u32 y_flip      : 1; //13
    u32 _           :10; //14-23
    u32 cmd         : 8; //24-31
  };
};

union TCoord {
  u32 v;
  struct {
    u32 x : 8;
    u32 y : 8;
    u32 other : 16;
  };
  TCoord(u8 xx, u8 yy, u16 o=0) : x(xx), y(yy), other(o) {}
};


static int random(int max, int min = 0) {
  return rand() % (max - min) + min;
}


static void sleep(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


static void draw_p3_1(Bus& bus, int x, int y, int count, int cmd) {
  for (int i=0; i<count; ++i) {
    bus.write32(gp0, Color(cmd, random(0xff, 0x10), random(0xff, 0x10), 0x8*i).v);
    bus.write32(gp0, pos(100+x, 10+y).v);
    bus.write32(gp0, pos(200+x, (30+y)+i*30).v);
    bus.write32(gp0, pos(11+x, 100+y).v);
    sleep(50);
  }
}


static void draw_p4_1(Bus& bus, int cmd = 0x28) {
  bus.write32(gp0, Color(cmd, 0,0xa0,0).v);
  bus.write32(gp0, pos(30, 90).v);
  bus.write32(gp0, pos(30, 60).v);
  bus.write32(gp0, pos(60, 60).v);
  bus.write32(gp0, pos(60, 90).v);
}


static void draw_p3_t1(Bus& bus, int cmd = 0x24) {
  TexpageAttr ta{0};
  ta.cmd = 0xE1;
  ta.px = 0;
  ta.py = 0;
  ta.draw = 1;
  bus.write32(gp0, ta.v);

  bus.write32(gp0, Color(cmd, 0,0,0xc0).v);
  bus.write32(gp0, pos(30, 120).v);
  bus.write32(gp0, TCoord(30, 120).v);
  
  bus.write32(gp0, pos(30, 30).v);
  bus.write32(gp0, TCoord(30, 39).v);

  bus.write32(gp0, pos(120, 120).v);
  bus.write32(gp0, TCoord(120, 120).v);
}


static void draw_offset(Bus& bus, int x, int y) {
  // 变更之前必须等待 gpu 绘制完成
  sleep(300);
  bus.write32(gp0, 0xE500'0000 | (x & 0xfff) | ((y & 0xFFF) << 11));
}


static void draw_p3_s1(Bus& bus, int count = 3, int cmd = 0x30) {
  bus.write32(gp0, Color(cmd, 0xff, 0, 0).v);
  bus.write32(gp0, pos(10, 10).v);
  bus.write32(gp0, Color(0, 0, 0xff, 0).v);
  bus.write32(gp0, pos(100, 50).v);
  bus.write32(gp0, Color(0, 0, 0, 0xff).v);
  bus.write32(gp0, pos(50, 100).v);
  if (count == 4) {
    bus.write32(gp0, Color(0, 99, 99, 99).v);
    bus.write32(gp0, pos(150, 100).v);
  }
}


static void draw_p3_s2(Bus& bus, int count = 3, int cmd = 0x34) {
  bus.write32(gp0, Color(cmd, 0xff, 0, 0).v);
  bus.write32(gp0, pos(10, 10).v);
  bus.write32(gp0, TCoord(30, 120).v);

  bus.write32(gp0, Color(0, 0, 0xff, 0).v);
  bus.write32(gp0, pos(100, 50).v);
  bus.write32(gp0, TCoord(30, 39).v);

  bus.write32(gp0, Color(0, 0, 0, 0xff).v);
  bus.write32(gp0, pos(50, 100).v);
  bus.write32(gp0, TCoord(120, 120).v);

  if (count == 4) {
    bus.write32(gp0, Color(0, 99, 99, 99).v);
    bus.write32(gp0, pos(150, 100).v);
    bus.write32(gp0, TCoord(120, 200).v);
  }
}


void test_gpu(GPU& gpu, Bus& bus) {
  gpu_basic();
  bus.write32(gp1, 0x0300'0001); // open display
  
  bus.write32(gp0, Color(0x20, 0xff,0,0).v);
  bus.write32(gp0, pos(80, 10).v);
  bus.write32(gp0, pos(100, 100).v);
  bus.write32(gp0, pos(120, 80).v);
  
  draw_offset(bus, 0, 0);
  draw_p3_1(bus, 100, 100, 5, 0x20);
  draw_p3_1(bus, 10, 10, 5, 0x22);
  draw_p4_1(bus, 0x28);

  draw_offset(bus, 60, 30);
  draw_p4_1(bus, 0x2A);

  draw_offset(bus, 200, 10);
  draw_p3_t1(bus);

  draw_offset(bus, 300, 40);
  draw_p3_t1(bus, 0x25);

  draw_p3_s1(bus);

  draw_offset(bus, 30, 40);
  draw_p3_s1(bus, 3, 0x32);

  draw_offset(bus, 30, 120);
  draw_p3_s1(bus, 4, 0x38);

  draw_offset(bus, 130, 120);
  draw_p3_s1(bus, 4, 0x3A);

  draw_offset(bus, 240, 120);
  draw_p3_s2(bus, 4, 0x3c);

  draw_offset(bus, 40, 200);
  draw_p3_s2(bus, 3, 0x36);
}

}