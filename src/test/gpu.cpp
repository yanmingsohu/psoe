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


static int random(int max, int min = 0) {
  return rand() % (max - min) + min;
}


static void draw_p3_1(Bus& bus, int x, int y, int count, int cmd) {
  for (int i=0; i<count; ++i) {
    bus.write32(gp0, Color(cmd, random(0xff, 0x10), random(0xff, 0x10), 0x8*i).v);
    bus.write32(gp0, pos(100+x, 10+y).v);
    bus.write32(gp0, pos(200+x, (30+y)+i*30).v);
    bus.write32(gp0, pos(11+x, 100+y).v);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}


static void draw_p4_1(Bus& bus, int x, int y, int cmd = 0x28) {
  bus.write32(gp0, Color(cmd, 0,0xa0,0).v);
  bus.write32(gp0, pos(30+x, 60+y).v);
  bus.write32(gp0, pos(30+x, 30+y).v);
  bus.write32(gp0, pos(60+x, 30+y).v);
  bus.write32(gp0, pos(60+x, 60+y).v);
}


void test_gpu(GPU& gpu, Bus& bus) {
  gpu_basic();
  
  bus.write32(gp0, Color(0x20, 0xff,0,0).v);
  bus.write32(gp0, pos(80, 160).v);
  bus.write32(gp0, pos(100, 100).v);
  bus.write32(gp0, pos(120, 80).v);
  
  draw_p3_1(bus, 100, 100, 5, 0x20);
  draw_p3_1(bus, 10, 10, 5, 0x22);
  draw_p4_1(bus, 20, 20, 0x28);
  draw_p4_1(bus, 50, 80, 0x2A);
}

}