#include <stdexcept>
#include <GLFW/glfw3.h>
#include <thread>
#include "gpu.h"
#include "gpu_shader.h"

namespace ps1e {


u32 textureAttr(GpuStatus& st, GpuTextFlip& flip) {
  u32 a = ((1 << 10) -1) & st.v;
  a |= (1 & st.text_off) << 11;
  a |= (1 & flip.x) << 12;
  a |= (1 & flip.y) << 13;
  return a;
}


GPU::GPU(Bus& bus) : 
    DMADev(bus, DeviceIOMapper::dma_gpu_base), status{0}, screen{0}, display{0},
    gp0(*this), gp1(*this), cmd_respons(0), vram(1), ds(0), disp_hori{0},
    disp_veri{0}, text_win{0}, draw_offset{0}, draw_tp_lf{0}, draw_bm_rt{0},
    status_change_count(0)
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

  vram.init();
  frame = vram.size();

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


void GPU::send(IDrawShape* s) {
  std::lock_guard<std::mutex> guard(for_draw_queue);
  draw_queue.push_back(s);
}


IDrawShape* GPU::pop_drawer() {
  std::lock_guard<std::mutex> guard(for_draw_queue);
  if (draw_queue.size() == 0) {
    return 0;
  }
  IDrawShape *sp = draw_queue.front();
  draw_queue.pop_front();
  return sp;
}


void GPU::gpu_thread() {
  u32 frames = 0;
  glfwMakeContextCurrent(glwindow);
  GLVertexArrays vao;

  while (!glfwWindowShouldClose(glwindow)) {
    vram.drawShape();

    IDrawShape *sp = pop_drawer();
    while (sp) {
      sp->draw(*this, vao);
      delete sp;
      sp = pop_drawer();
    }

    if (status.display == 0) {
      //ds.viewport(&screen);
      vram.drawScreen();
    }

    bus.send_irq(IrqDevMask::vblank);
    //debug("SEND vblank\r");

    if (status.height) {
      status.lcf = ~status.lcf;
    }

    glfwSwapBuffers(glwindow);
    glfwPollEvents();
    //debug("\r\t\t\t\t\t\t%d, %f\r", ++frames, glfwGetTime());
  }
}


void GPU::reset() {
  gp0.reset_fifo();
  status.v = 0x14802000;
  status.r_cmd = 1; //TODO: 与状态同步
  status.r_cpu = 1;
  status.r = 1;
}


void GPU::dma_order_list(psmem addr) {
  //warn("GPU ol [%x] x:%d y:%d\n", addr, draw_offset.offx(), draw_offset.offy());
  u32 header = bus.read32(addr);
  
  while ((header & OrderingTables::LINK_END) != OrderingTables::LINK_END) {
    u32 next = header & 0x001f'fffc;
    u32 size = header >> 24;

    //if (size) {
      //printf("  H [%08x]  %08x\n", addr, header);
    //}

    for (int i=0; i<size; ++i) {
      addr += 4;
      u32 cmd = bus.read32(addr);
      gp0.write(cmd);
      //printf("     [%08x]  %08x\n", addr, cmd);
    }
    
    addr = next;
    header = bus.read32(addr);
  }
  //debug("\nGPU ol end\n");
}


void GPU::dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc) {
  //printf("COPY ram go GPU begin:%x %dbyte\n", addr, bytesize);
  inc = inc *4;
  for (int i = 0; i < bytesize; i += 4) {
    u32 d = bus.read32(addr);
    //debug("CC %x %x\t", addr, d);
    gp0.write(d);
    addr += inc;
  }
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
    multiple(_mul), gsize{0, 0, Width * _mul, Height * _mul}, ds(0.03f)
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
  ds.setDepthTest(false);
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


GLTexture& VirtualFrameBuffer::useTexture() {
  return virtual_screen;
}


}