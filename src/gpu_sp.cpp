#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "gpu.h"
#include "gpu_shader.h"

namespace ps1e {


template<int Count>
class MonoPolygon : public IDrawShape {
private:
  static const int EleCount = 1;
  u32 vertices[EleCount *Count];
  u32 color;
  float transparent;
  int step;
  GLHANDLE vao;
  GLHANDLE vbo;

public:
  MonoPolygon(float trans) : transparent(trans), step(0) {}
  ~MonoPolygon() {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
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

  virtual void build(GPU& gpu) {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, EleCount, GL_FLOAT, GL_FALSE, EleCount * sizeof(u32), 0);
    glEnableVertexAttribArray(0);
  }

  virtual void draw(GPU& gpu) {
    OpenGLShader* prog = gpu.getProgram<MonoColorPolygonShader>();
    prog->use();
    prog->setUint("width", gpu.screen_range()->width);
    prog->setUint("height", gpu.screen_range()->height);
    prog->setFloat("transparent", transparent);
    prog->setUint("ps_color", color);
    
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, Count);
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