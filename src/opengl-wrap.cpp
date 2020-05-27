#include <glad/glad.h>
#include <GLFW/glfw3.h> 
#include <stdexcept>

#include "opengl-wrap.h"
#include "gpu.h"

namespace ps1e {


static void showglError() {
  const int e = glGetError();
  if (e) {
    switch (e) {
      case GL_INVALID_OPERATION:
        error("GL_INVALID_OPERATION");
        break;
      case GL_INVALID_ENUM:
        error("GL_INVALID_ENUM");
        break;
      case GL_INVALID_VALUE:
        error("GL_INVALID_VALUE");
        break;
      default:
        error("GL [unknow]");
    }
    error(" ERR %x \n", e);
  } else {
    debug("GL no error\n");
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


void GLVertexArrays::init() {
  glGenVertexArrays(1, &vao);
}


GLVertexArrays::GLVertexArrays() : vao(0) {
}


GLVertexArrays::~GLVertexArrays() {
  if (vao) glDeleteVertexArrays(1, &vao);
}


void GLVertexArrays::bind() {
  glBindVertexArray(vao);
}


void GLVertexArrays::unbind() {
  glBindVertexArray(0);
}


void GLVertexArrays::setColor(u32 ps_color) {
  float r =  (ps_color & 0xff) /255.0;
  float g = ((ps_color >>  8) & 0xff) /255.0;
  float b = ((ps_color >> 16) & 0xff) /255.0;
  glColor3f(r, g, b);
}


void GLVertexArrays::drawTriangles(u32 indices_count) {
  glDrawArrays(GL_TRIANGLES, 0, indices_count);
}


void GLVertexArrays::drawLines(u32 indices_count) {
  glDrawArrays(GL_LINES, 0, indices_count);
}


void GLVertexArrays::drawTriangleFan(u32 indices_count) {
  glDrawArrays(GL_TRIANGLE_FAN, 0, indices_count);
}


void GLVertexArrays::drawTriangleStrip(u32 indices_count) {
  glDrawArrays(GL_TRIANGLE_STRIP, 0, indices_count);
}


void GLVertexArrays::drawQuads(u32 i) {
  glDrawArrays(GL_QUADS, 0, i);
}


void GLVertexArrays::drawLineStrip(u32 i) {
  glDrawArrays(GL_LINE_STRIP, 0, i);
}


void GLVertexArrays::drawPoints(u32 i) {
  glDrawArrays(GL_POINTS, 0, i);
}


GLVerticesBuffer::GLVerticesBuffer() : vbo(0) {
}


void GLVerticesBuffer::init(GLVertexArrays& vao) {
  vao.bind();
  glGenBuffers(1, &vbo);
}


GLVerticesBuffer::~GLVerticesBuffer() {
  if (vbo) glDeleteBuffers(1, &vbo);
}


void GLVerticesBuffer::bind() {
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
}


void GLVerticesBuffer::unbind() {
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}


GLFrameBuffer::GLFrameBuffer() : fbo(0), w(0), h(0) {
}


GLFrameBuffer::~GLFrameBuffer() {
  glDeleteFramebuffers(1, &fbo);
}


void GLFrameBuffer::init(int width, int height) {
  glGenFramebuffers(1, &fbo);
  w = width;
  h = height;
}


void GLFrameBuffer::bind() {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}


void GLFrameBuffer::unbind() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void GLFrameBuffer::check() {
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	  throw std::runtime_error("Framebuffer is not complete!");
  }
}


GLBufferData::GLBufferData(GLVerticesBuffer& b, void* data, size_t length) : vbo(b) {
  vbo.bind();
  glBufferData(GL_ARRAY_BUFFER, length, data, GL_STATIC_DRAW);
}


// loc - Location on GSGL
// ele - Element count byte
// spc - Element spacing byte
// beg - Begin of element byte
void GLBufferData::floatAttr(u32 loc, u32 ele, u32 spc, u32 beg) {
  vbo.bind();
  const u8 UZ = sizeof(float);
  glVertexAttribPointer(loc, ele, GL_FLOAT, GL_FALSE, spc *UZ, (void*)(beg *UZ));
  glEnableVertexAttribArray(loc);
}


// loc - Location on GSGL
// ele - Element count byte
// spc - Element spacing byte
// beg - Begin of element byte
void GLBufferData::uintAttr(u32 loc, u32 ele, u32 spc, u32 beg) {
  vbo.bind();
  const u8 UZ = sizeof(u32);
  // GL_UNSIGNED_INT not working
  glVertexAttribPointer(loc, ele, GL_FLOAT, GL_FALSE, spc *UZ, (void*)(beg *UZ));
  glEnableVertexAttribArray(loc);
}


GLTexture::GLTexture() : text(0) {
}


GLTexture::~GLTexture() {
  if (text) glDeleteTextures(1, &text);
}


void GLTexture::init(GLFrameBuffer& fb, void* pixeldata) {
  fb.bind();
  init(fb.width(), fb.height(), pixeldata);
  glFramebufferTexture2D(GL_FRAMEBUFFER, 
      GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, text, 0);
}


void GLTexture::init(int w, int h, void* pixeldata) {
  glGenTextures(1, &text);
  bind();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 
      w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixeldata);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


// | 15 | 14 - - - 10 | 9 - - - 5 | 4 - - - 0 |
// | S  |      B      |     G     |     R     |
void GLTexture::init2px(int w, int h, void* pixeldata) {
  glGenTextures(1, &text);
  bind();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 
      w, h, 0, GL_RED, GL_UNSIGNED_SHORT, pixeldata);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


void GLDrawState::readPsinnerPixel(int x, int y, int w, int h, u32* data) {
  glReadPixels(x, y, w, h, GL_RED, GL_UNSIGNED_SHORT, data);
}


void GLTexture::bind() {
  glBindTexture(GL_TEXTURE_2D, text);
}


void GLTexture::unbind() {
  glBindTexture(GL_TEXTURE_2D, 0);
}


void GLTexture::setTexWrap(TexWrap mode) {
  int glmode;
  switch (mode) {
    case TexWrap::REPEAT:
      glmode = GL_REPEAT;
      break;
    case TexWrap::CLAMP_TO_EDGE:
      glmode = GL_CLAMP_TO_EDGE;
      break;
    case TexWrap::MIRRORED_REPEAT:
      glmode = GL_MIRRORED_REPEAT;
      break;
    default:
      throw std::runtime_error("Invaild texture wrap mode");
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glmode);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glmode);
}


void GLTexture::copyTo(GLTexture* dst, int srcX, int srcY, int dstX, int dstY, int srcW, int srcH) {
  glCopyImageSubData(text,      GL_TEXTURE_2D, 0, srcX, srcY, 0,
                     dst->text, GL_TEXTURE_2D, 0, dstX, dstY, 0, 
                     srcW, srcH, 1);
}


GLRenderBuffer::GLRenderBuffer() : rb(0) {
}


GLRenderBuffer::~GLRenderBuffer() {
  if (rb) glDeleteRenderbuffers(1, &rb);
}


void GLRenderBuffer::init(GLFrameBuffer& f) {
  f.bind();
  glGenRenderbuffers(1, &rb);
  bind();
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, f.width(), f.height());  
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, 
      GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
}


void GLRenderBuffer::bind() {
  glBindRenderbuffer(GL_RENDERBUFFER, rb); 
}


void GLRenderBuffer::unbind() {
  glBindRenderbuffer(GL_RENDERBUFFER, 0); 
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


int OpenGLShader::_getUniform(const char* name) {
  int loc = glGetUniformLocation(program, name);
  if (loc < 0) {
    /*char buf[100];
    sprintf(buf, "Cannot get Uniform location '%s'", name);
    error("%s", buf);
    throw std::runtime_error(buf);*/
    warn("Cannot get Uniform location '%s'\n", name);
  }
  return loc;
}


GLUniform OpenGLShader::getUniform(const char* name) {
  return GLUniform(_getUniform(name));
}


void GLUniform::setUint(u32 v) {
  glUniform1ui(uni, v);
}


void GLUniform::setUint2(u32 a, u32 b) {
  glUniform2ui(uni, a, b);
}


void GLUniform::setInt(s32 v) {
  glUniform1i(uni, v);
}


void GLUniform::setFloat(float v) {
  glUniform1f(uni, v);
}


void GLDrawState::clear() {
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


void GLDrawState::clear(float r, float g, float b, float a) {
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


void GLDrawState::clearDepth() {
  glClear(GL_DEPTH_BUFFER_BIT);
}


void GLDrawState::setDepthTest(const bool t) {
  if (t) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
}


void GLDrawState::viewport(int x, int y, int w, int h) {
  glViewport(x, y, w, h);
}


void GLDrawState::setMultismple(const bool t, int hint) {
  if (t) {
    glfwWindowHint(GLFW_SAMPLES, hint);
    glEnable(GL_MULTISAMPLE);
  } else {
    glfwWindowHint(GLFW_SAMPLES, 0);
    glDisable(GL_MULTISAMPLE);
  }
}


void GLDrawState::setBlend(const bool t) {
  if (t) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }
}


void GLDrawState::setSemiMode(u8 mode) {
  switch (mode) {
    case 0: // B/2+F/2
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      break;
    case 1: // B+F
      glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      break;
    case 2: // B-F
      glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
      glBlendEquation(GL_FUNC_SUBTRACT);
      break;
    case 3: // B+F/4
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      break;
  }
}


void GLDrawState::viewport(GpuDataRange* r) {
  viewport(r->offx, r->offy, r->width, r->height);
}


void GLDrawState::initGlad() {
  if(! gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) ) {
    throw std::runtime_error("Cannot init OpenGLAD");
  }
  info("PS1 GPU used OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
}


void GLDrawState::setScissor(int x, int y, int w, int h) {
  glScissor(x, y, w, h);
}


void GLDrawState::setScissorEnable(bool enable) {
  if (enable) {
    glEnable(GL_SCISSOR_TEST);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }
}


}