#include "gpu.h"
#include "gpu_shader.h"
#include <functional>

namespace ps1e {


template<int ElementCount>
class PolygonU32Vertices {
private:
  u32 vertices[ElementCount];
  u32 _color;
  int step;

public:
  PolygonU32Vertices() : step(0) {
  }

  int elementCount() {
    return ElementCount;
  }

  bool write(const u32 c) {
    switch (step) {
      case 0:
        _color = c;
        break;

      default: {
        vertices[step - 1] = c;
        break;
      }
    }
    return ++step <= ElementCount;
  }

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 1);
  }

  u32 color() { return _color; }
};


template<class Vertices, void (*Draw)(GLVertexArrays&, int)>
class MonoPolygon : public IDrawShape {
private:
  Vertices vertices;
  float transparent;

public:
  MonoPolygon(float trans) : transparent(trans) {
  }

  ~MonoPolygon() {
  }

  virtual bool write(const u32 c) {
    return vertices.write(c);
  }

  virtual void draw(GPU& gpu, GLVertexArrays& vao) {
    gl_scope(vao);
    vbo.init(vao);
    gl_scope(vbo);
    vertices.setAttr(vbo);

    auto prog = gpu.useProgram<MonoColorPolygonShader>();
    prog->setColor(transparent, vertices.color());
    Draw(vao, vertices.elementCount());
  }
};


void drawTriangles(GLVertexArrays& vao, int elementCount) {
  vao.drawTriangles(elementCount);
}


void drawSquare(GLVertexArrays& vao, int elementCount) {
  vao.drawTriangleFan(elementCount);
}


bool GPU::GP0::parseCommand(const GpuCommand c) {
  switch (c.cmd) {
    case 0x00:
      debug("Gpu Nop?\n");
      return false;

    case 0x1F:
      p.status.irq_on = 1;
      return false;

    case 0xE1:
      p.status.v = SET_BIT(p.status.v, 0b11'1111'1111, c.parm);
      p.status.text_off = (c.parm >> 11) & 1;
      p.status.inter_f  = (c.parm >> 13) & 1;
      p.dirtyAttr();
      return false;

    case 0xE2:
      p.text_win.v = c.parm;
      p.dirtyAttr();
      return false;

    case 0xE3: 
      p.draw_tp_lf.v = c.parm;
      p.dirtyAttr();
      return false;

    case 0xE4: 
      p.draw_bm_rt.v = c.parm;
      p.dirtyAttr();
      return false;

    case 0xE5:
      p.draw_offset.v = c.parm;
      p.dirtyAttr();
      return false;

    case 0xE6:
      p.status.mask = c.parm & 1;
      p.status.enb_msk = (c.parm >> 1) & 1;
      p.dirtyAttr();
      return false;
      
    case 0x20:
      shape = new MonoPolygon<PolygonU32Vertices<3>, drawTriangles>(1);
      break;

    case 0x22:
      shape = new MonoPolygon<PolygonU32Vertices<3>, drawTriangles>(0.5);
      break;

    case 0x28:
      shape = new MonoPolygon<PolygonU32Vertices<4>, drawSquare>(1);
      break;

    case 0x2A:
      shape = new MonoPolygon<PolygonU32Vertices<4>, drawSquare>(0.5);
      break;

    default:
      error("Invaild GP0 Command %x %x\n", c.cmd, c.v);
      return false;
  }
  return true;
}


u32 GPU::GP0::read() {
  if (p.read_queue.size()) {
    IGpuReadData* data_que = p.read_queue.front();
    last_read = data_que->read();
    if (!data_que->has()) {
      delete data_que;
      p.read_queue.pop_front();
    }
  }
  return last_read;
}


void GPU::GP0::write(u32 v) {
  switch (stage) {
    case ShapeDataStage::read_command:
      if (!parseCommand(v)) {
        break;
      }
      stage = ShapeDataStage::read_data;
      // do not break

    case ShapeDataStage::read_data:
      if (!shape->write(v)) {
        p.send(shape);
        shape = NULL;
        stage = ShapeDataStage::read_command;
      }
      break;
  }
}


void GPU::GP0::reset_fifo() {
  stage = ShapeDataStage::read_command;
}


GPU::GP0::GP0(GPU &_p) : p(_p), stage(ShapeDataStage::read_command), shape(0), last_read(0) {
}


}