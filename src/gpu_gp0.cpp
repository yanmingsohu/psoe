#include "gpu.h"
#include "gpu_shader.h"
#include <functional>

namespace ps1e {


template<int ElementCount>
class PolygonU32Vertices {
private:
  u32 vertices[ElementCount];
  int step;

public:
  u32 color;

  PolygonU32Vertices() : step(0) {
  }

  int elementCount() {
    return ElementCount;
  }

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 1);
  }

  bool write(const u32 c) {
    switch (step) {
      case 0:
        color = c;
        break;

      default: {
        vertices[step - 1] = c;
        break;
      }
    }
    return ++step <= ElementCount;
  }
};


template<int ElementCount>
class PolyTextureVertices {
private:
  u32 vertices[ElementCount * 2];
  int step;

public:
  u32 color;
  u32 clut;
  u32 page;

  PolyTextureVertices() : step(0) {
  }

  int elementCount() {
    return ElementCount;
  }

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 2, 0);
    vbdata.uintAttr(1, 1, 2, 1);
  }

  bool write(const u32 c) {
    switch (step) {
      case 0:
        color = c;
        break;

      default: {
        switch (step) {
          case 2:
            clut = c & 0xFFFF'0000;
            break;
          case 4:
            page = c & 0xFFFF'0000;
            break;
        }
        vertices[step - 1] = c;
        break;
      }
    }
    return ++step <= ElementCount*2;
  }
};


template< class Vertices, 
          void (*Draw)(GLVertexArrays&, int),
          class Shader = MonoColorPolygonShader,
          bool active_texture = false
          >
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

    auto prog = gpu.useProgram<Shader>();
    prog->setShaderUni(vertices, gpu, transparent);

    if (active_texture) gpu.useTexture().bind();
    Draw(vao, vertices.elementCount());
    if (active_texture) gpu.useTexture().unbind();
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
      p.text_flip.x     = (c.parm >> 12) & 1;
      p.text_flip.y     = (c.parm >> 13) & 1;
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

    case 0x24:
      shape = new MonoPolygon<PolyTextureVertices<3>, 
                  drawTriangles, MonoColorTexturePolyShader, true>(1);
      break;

    case 0x25:
      shape = new MonoPolygon<PolyTextureVertices<3>, 
                  drawTriangles, TextureOnlyPolyShader, true>(1);
      break;

    case 0x26:
      shape = new MonoPolygon<PolyTextureVertices<3>, 
                  drawTriangles, MonoColorTexturePolyShader, true>(0.5);
      break;

    case 0x27:
      shape = new MonoPolygon<PolyTextureVertices<3>, 
                  drawTriangles, TextureOnlyPolyShader, true>(0.5);
      break;

    case 0x2C:
      shape = new MonoPolygon<PolyTextureVertices<4>, 
                  drawTriangles, MonoColorTexturePolyShader, true>(1);
      break;

    case 0x2D:
      shape = new MonoPolygon<PolyTextureVertices<4>, 
                  drawTriangles, TextureOnlyPolyShader, true>(1);
      break;

    case 0x2E:
      shape = new MonoPolygon<PolyTextureVertices<4>, 
                  drawTriangles, MonoColorTexturePolyShader, true>(0.5);
      break;

    case 0x2F:
      shape = new MonoPolygon<PolyTextureVertices<4>, 
                  drawTriangles, TextureOnlyPolyShader, true>(0.5);
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
  //debug("GP0 Write 0x%08x\n", v);
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