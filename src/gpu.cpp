#include <stdexcept>
#include <GLFW/glfw3.h>
#include <thread>
#include "gpu.h"
#include "gpu_shader.h"

namespace ps1e {


GPU::GPU(Bus& bus) : 
    DMADev(bus, DmaDeviceNum::gpu), status{0}, screen{0,0,0,0}, ps{0,0,320,240},
    gp0(*this), gp1(*this), cmd_respons(0), vfb(1), ds(0)
{
  initOpenGL();

  bus.bind_io(DeviceIOMapper::gpu_gp0, &gp0);
  bus.bind_io(DeviceIOMapper::gpu_gp1, &gp1);

  work = new std::thread(&GPU::gpu_thread, this);
}


void GPU::initOpenGL() {
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  screen = GpuDataRange(0, 0, mode->width, mode->height);

  glwindow = glfwCreateWindow(screen.width, screen.height, "Playstation1 EMU", NULL, NULL);
  if (!glwindow) {
    throw std::runtime_error("Cannot create GL window");
  }
  glfwSetWindowPos(glwindow, 0, 0);
  
  glfwMakeContextCurrent(glwindow);
  ds.initGlad();
  
  ds.viewport(&screen);
  ds.setMultismple(true, 4);
  ds.setDepthTest(true);

  vfb.init();
  frame = vfb.size();

  // 必须释放 gl 上下文, 另一个线程才能绑定.
  glfwMakeContextCurrent(NULL);
}


GPU::~GPU() {
  glfwSetWindowShouldClose(glwindow, true);
  work->join();
  glfwDestroyWindow(glwindow);
  delete work;
  debug("GPU Destoryed\n");
}


void GPU::gpu_thread() {
  u32 frames = 0;
  glfwMakeContextCurrent(glwindow);

  while (!glfwWindowShouldClose(glwindow)) {
    vfb.drawShape();
    while (build.size()) {
      IDrawShape *sp = build.front();
      sp->draw(*this);
      build.pop_front();
    }

    //ds.viewport(&screen);
    vfb.drawScreen();
    glfwSwapBuffers(glwindow);
    glfwPollEvents();
    transport();
    debug("%d, %f\r", ++frames, glfwGetTime());
  }
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


u32 GPU::GP0::read() {
  return 0;
}


void GPU::GP1::write(u32 v) {}


u32 GPU::GP1::read() {
  return p.status.v;
}


static void initBoxVertices(float* vertices) {
  float v[] = {
    -1.0f,  1.0f,     0.0f, 1.0f,
    -1.0f, -1.0f,     0.0f, 0.0f,
     1.0f, -1.0f,     1.0f, 0.0f,

    -1.0f,  1.0f,     0.0f, 1.0f,
     1.0f, -1.0f,     1.0f, 0.0f,
     1.0f,  1.0f,     1.0f, 1.0f
  };
  memcpy(vertices, v, sizeof(v));
}


VirtualFrameBuffer::VirtualFrameBuffer(int _mul) : 
    multiple(_mul), width(Width * _mul), height(Height * _mul), ds(0.03)
{
}


void VirtualFrameBuffer::init() {
  vao.init();
  gl_scope(vao);
  vbo.init(vao);
  gl_scope(vbo);
  
  float vertices[4*6];
  const int UZ = sizeof(float);
  initBoxVertices(vertices);
  GLBufferData bd(vbo, vertices, sizeof(vertices));
  bd.floatAttr(0, 2, 4, 0);
  bd.floatAttr(1, 2, 4, 2);
  vao.addIndices(6);

  frame_buffer.init(width, height);
  gl_scope(frame_buffer);
  virtual_screen.init(frame_buffer);
  gl_scope(virtual_screen);

  rbo.init(frame_buffer);
  frame_buffer.check();
  ds.clear(0, 0, 0);

  shader = new VirtualScreenShader();
}


VirtualFrameBuffer::~VirtualFrameBuffer() {
  delete shader;
}


void VirtualFrameBuffer::drawShape() {
  frame_buffer.bind();
  ds.clearDepth();
  ds.setDepthTest(true);
  ds.viewport(0, 0, width, height);
}


void VirtualFrameBuffer::drawScreen() {
  frame_buffer.unbind();
  ds.clear();
  shader->use();
  gl_scope(vao);
  gl_scope(virtual_screen);
  ds.setDepthTest(false);
  vao.drawTriangles();
}


GpuDataRange VirtualFrameBuffer::size() {
  return GpuDataRange(0, 0, width, height);
}


}