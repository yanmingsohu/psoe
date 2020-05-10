#include <stdexcept>
#include <GLFW/glfw3.h>
#include <thread>
#include "gpu.h"
#include "gpu_shader.h"

namespace ps1e {


GPU::GPU(Bus& bus) : 
    DMADev(bus, DmaDeviceNum::gpu), status{0}, screen{0}, display{0},
    gp0(*this), gp1(*this), cmd_respons(0), vfb(1), ds(0), disp_hori{0},
    disp_veri{0}, text_win{0}, draw_offset{0}, draw_tp_lf{0}, draw_bm_rt{0}
{
  initOpenGL();

  bus.bind_io(DeviceIOMapper::gpu_gp0, &gp0);
  bus.bind_io(DeviceIOMapper::gpu_gp1, &gp1);

  work = new std::thread(&GPU::gpu_thread, this);
}


void GPU::initOpenGL() {
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  screen = {0, 0, (u32)mode->width, (u32)mode->height};

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
  ds.setBlend(true);

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
  GLVertexArrays vao;

  while (!glfwWindowShouldClose(glwindow)) {
    vfb.drawShape();

    while (draw_queue.size()) {
      IDrawShape *sp = draw_queue.front();
      sp->draw(*this, vao);
      draw_queue.pop_front();
      delete sp;
    }

    //ds.viewport(&screen);
    vfb.drawScreen();
    glfwSwapBuffers(glwindow);
    glfwPollEvents();
    transport();
    debug("%d, %f\r", ++frames, glfwGetTime());
  }
}


void GPU::reset() {
  gp0.reset_fifo();
  status.v = 0x14802000;
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
    multiple(_mul), gsize{0, 0, Width * _mul, Height * _mul}, ds(0.03)
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

  frame_buffer.init(gsize.width, gsize.height);
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
  ds.viewport(&gsize);
}


void VirtualFrameBuffer::drawScreen() {
  frame_buffer.unbind();
  ds.clear();
  shader->use();
  gl_scope(vao);
  gl_scope(virtual_screen);
  ds.setDepthTest(false);
  vao.drawTriangles(6);
}


GpuDataRange& VirtualFrameBuffer::size() {
  return gsize;
}


}