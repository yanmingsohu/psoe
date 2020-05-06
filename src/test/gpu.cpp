#include "../gpu.h"
#include "test.h"
#include <thread>
#include <chrono>

namespace ps1e_t {
using namespace ps1e;

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


void test_gpu(GPU& gpu, Bus& bus) {
  gpu_basic();
  const u32 gp0 = 0x1F80'1810;
  const u32 gp1 = 0x1F80'1814;
  
  bus.write32(gp0, Color(0x20, 0xff,0,0).v);
  bus.write32(gp0, pos(200, 300).v);
  bus.write32(gp0, pos(300, 1080-300).v);
  bus.write32(gp0, pos(1920-300, 1080-300).v);

  for (int i=0; i<20; ++i) {
    bus.write32(gp0, Color(0x20, 0x70, 0x70, 0x8*i).v);
    bus.write32(gp0, pos(100, 10).v);
    bus.write32(gp0, pos(30, 100+i*30).v);
    bus.write32(gp0, pos(160, 100).v);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

}