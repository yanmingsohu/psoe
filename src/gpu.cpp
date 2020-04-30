#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thread>
#include "gpu.h"

namespace ps1e {


GPU::GPU(Bus& bus) : DMADev(bus, DmaDeviceNum::gpu), ct{0} {
  glwindow = glfwCreateWindow(640, 480, "Playstation1 EMU", NULL, NULL);
  if (!glwindow) {
    throw std::runtime_error("Cannot create GL window");
  }

  glfwMakeContextCurrent(glwindow);
  if(!gladLoadGL()) {
    throw std::runtime_error("Cannot init OpenGLAD");
  }
  debug("PS1 GPU used OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);

  bus.bind_io(DeviceIOMapper::gpu_gp0, &gp0);
  bus.bind_io(DeviceIOMapper::gpu_gp1, &gp1);

  work = new std::thread([this]{
    this->gpu_thread();
  });
}


GPU::~GPU() {
  glfwSetWindowShouldClose(glwindow, true);
  work->join();
  glfwDestroyWindow(glwindow);
  delete work;
}


void GPU::gpu_thread() {
  while (!glfwWindowShouldClose(glwindow)) {
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(glwindow);
    glfwPollEvents();
    transport();
  }
}


OpenGLScope::OpenGLScope() {
  if(gladLoadGL()) {
    throw std::runtime_error("I did load GL with no context!\n");
  }
  if (!glfwInit()) {
    throw std::runtime_error("Cannot init OpenGLfw");
  }
}


OpenGLScope::~OpenGLScope() {
  glfwTerminate();
}


}