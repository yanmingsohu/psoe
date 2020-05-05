#include "gpu.h"

namespace ps1e {


template<int Count>
class MonoPolygon : public IDrawShape {
private:
  float vertices[3 *Count];
  float color[3 *Count];
  float transparent;
  int step;
public:
  MonoPolygon(float trans) : transparent(trans), step(0) {}

  virtual bool write(const u32 c) {
    switch (step) {
      case 0:
        color[0] = 0xFF & c;
        color[1] = 0xFF & (c>>8);
        color[2] = 0xFF & (c>>16);
        color[3] = color[0];
        color[4] = color[1];
        color[5] = color[2];
        color[6] = color[0];
        color[7] = color[1];
        color[8] = color[2];
        break;

      default:
        vertices[ (step-1)*Count + 0] = 0xFFFF & c;
        vertices[ (step-1)*Count + 1] = 0xFFFF & (c>>16);
        vertices[ (step-1)*Count + 2] = 0;
        break;
    }
    return ++step <= Count;
  }

  virtual void build() {}
};


void GPU::GP0::parseCommand(const GpuCommand c) {
  switch (c.cmd) {
    case 0x20:
      shape = new MonoPolygon<3>(1);
      break;
    default:
      error("Invaild GP0 Command %x %x\n", c.cmd, c.v);
      break;
  }
}

}