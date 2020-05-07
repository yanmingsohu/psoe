#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thread>
#include "gpu.h"
#include "gpu_shader.h"

namespace ps1e {


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


GpuDataRange::GpuDataRange(u32 w, u32 h) : width(w), height(h) {
  if (w > 0xffff) throw std::out_of_range("width");
  if (h > 0xffff) throw std::out_of_range("height");
}


GPU::GPU(Bus& bus) : 
    DMADev(bus, DmaDeviceNum::gpu), status{0}, screen(0,0), ps(255,255),
    gp0(*this), gp1(*this), cmd_respons(0) 
{
  initOpenGL();

  bus.bind_io(DeviceIOMapper::gpu_gp0, &gp0);
  bus.bind_io(DeviceIOMapper::gpu_gp1, &gp1);

  work = new std::thread(&GPU::gpu_thread, this);
}


void GPU::initOpenGL() {
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  screen = GpuDataRange(mode->width, mode->height);

  glfwWindowHint(GLFW_SAMPLES, 4);
  glwindow = glfwCreateWindow(screen.width, screen.height, "Playstation1 EMU", NULL, NULL);
  if (!glwindow) {
    throw std::runtime_error("Cannot create GL window");
  }
  glfwSetWindowPos(glwindow, 0, 0);
  
  glfwMakeContextCurrent(glwindow);
  if(! gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) ) {
    throw std::runtime_error("Cannot init OpenGLAD");
  }
  info("PS1 GPU used OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
  
  glViewport(0, 0, screen.width, screen.height);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_DEPTH_TEST);

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
  glfwMakeContextCurrent(glwindow);
  u32 frames = 0;

  // 必须清除一次缓冲区, 否则绘制失败
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  while (!glfwWindowShouldClose(glwindow)) {
    while (build.size()) {
      IDrawShape *sp = build.front();
      sp->build(*this);
      shapes.push_back(sp);
      build.pop_front();
    }

    for (auto sh : shapes) {
      sh->draw(*this);
    }

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


OpenGLShader::OpenGLShader(ShaderSrc vertex_src, ShaderSrc fragment_src) {
  program = glCreateProgram();
  if (!program) {
    error("OpenGL error %d\n", glGetError());
    throw std::runtime_error("Cannot create shader program");
  }
  GLHANDLE vertexShader = createShader(vertex_src, GL_VERTEX_SHADER);
  GLHANDLE fragShader = createShader(fragment_src, GL_FRAGMENT_SHADER);
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragShader);
  glLinkProgram(program);

  glDeleteShader(vertexShader);
  glDeleteShader(fragShader);  

  int success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if(!success) {
    char info[0xFFF];
    glGetProgramInfoLog(program, sizeof(info), NULL, info);
    error("Program Fail: %s\n", info);
    throw std::runtime_error(info);
  }
}


GLHANDLE OpenGLShader::createShader(ShaderSrc src, u32 shader_flag) {
  GLHANDLE shader = glCreateShader(shader_flag);
  if (!shader) {
    throw std::runtime_error("Cannot create shader object");
  }
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);

  int success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info[GL_INFO_LOG_LENGTH];
    glGetShaderInfoLog(shader, sizeof(info), NULL, info);
    error("<----- ----- ----- Shader Fail ----- ----- ----->\n");
    print_code(src);
    error("%s\n", info);
    throw std::runtime_error(info);
  }
  return shader;
}


OpenGLShader::~OpenGLShader() {
  glDeleteProgram(program);
}


void OpenGLShader::use() {
  glUseProgram(program);
}


int OpenGLShader::getUniform(const char* name) {
  int loc = glGetUniformLocation(program, name);
  if (loc < 0) {
    throw std::runtime_error("Cannot get Uniform location.");
  }
  return loc;
}


void OpenGLShader::setUint(const char* name, u32 v) {
  glUniform1ui(getUniform(name), v);
}


void OpenGLShader::setFloat(const char* name, float v) {
  glUniform1f(getUniform(name), v);
}


}