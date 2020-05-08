#include "gpu.h"
#include "gpu_shader.h"

namespace ps1e {


template<int Count, int EleCount = 1>
class MonoPolygon : public IDrawShape {
private:
  u32 vertices[EleCount *Count];
  u32 color;
  float transparent;
  int step;
  GLVertexArrays vao;
  GLVerticesBuffer vbo;

public:
  MonoPolygon(float trans) : transparent(trans), step(0) {
  }

  ~MonoPolygon() {
  }

  virtual bool write(const u32 c) {
    switch (step) {
      case 0:
        color = c;
        warn("!color %x %f %d\n", c, transparent, sizeof(vertices));
        break;

      default: {
        vertices[step - 1] = c;
        break;
      }
    }
    return ++step <= Count;
  }

  virtual void draw(GPU& gpu) {
    vao.init();
    gl_scope(vao);

    vbo.init(vao);
    gl_scope(vbo);

    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, EleCount, EleCount);

    auto prog = gpu.useProgram<MonoColorPolygonShader>();
    prog->setColor(transparent, color);
    
    vao.addIndices(Count);
    vao.drawTriangles();
  }
};


bool GPU::GP0::parseCommand(const GpuCommand c) {
  switch (c.cmd) {
    case 0x20:
      shape = new MonoPolygon<3>(1);
      break;
    default:
      error("Invaild GP0 Command %x %x\n", c.cmd, c.v);
      return false;
  }
  return true;
}


}