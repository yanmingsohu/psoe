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
    //vao.drawTriangles(vertices.elementCount());
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


}