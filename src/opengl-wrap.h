#pragma once 

#include "util.h"

namespace ps1e {

#define _extruct(s) s
#define _conn_name(a, b) _extruct(a)##_extruct(b)
#define gl_scope(x)  auto _conn_name(__gl_scope_, __COUNTER__) = _gl_scope(x);

typedef u32 GLHANDLE;
typedef const char* ShaderSrc;
struct GpuDataRange;


// 首先声明该对象, 并在生命周期内使用 opengl 函数.
class OpenGLScope {
private:
  OpenGLScope(OpenGLScope&);
  OpenGLScope& operator=(OpenGLScope&);
  
public:
  OpenGLScope();
  ~OpenGLScope();
};


template<class GlUnBindClass> class GLBindScope {
private:
  GlUnBindClass& obj;
  bool unbinded;
public:
  GLBindScope(GlUnBindClass& t) : obj(t), unbinded(false) {
    obj.bind();
  }

  ~GLBindScope() { 
    unbind(); 
  }

  // 一定会调用一次
  void unbind() {
    obj.unbind();
  }

  GlUnBindClass& operator->() {
    return obj;
  }
};


template<class C> GLBindScope<C> _gl_scope(C& obj) {
  return GLBindScope<C>(obj);
}


template<class T, class Func> void _gl_scope(T& obj, Func func) {
  obj.bind();
  func();
  obj.unbind();
}


class GLVertexArrays {
private:
  GLHANDLE vao;
public:
  GLVertexArrays();
  ~GLVertexArrays();
  void init();
  void bind();
  void unbind();
  // type : GL_TRIANGLES ...
  void drawTriangles(u32 count);
  void drawLines(u32 count);
  void drawTriangleFan(u32 indices_count);
};


class GLVerticesBuffer {
private:
  GLHANDLE vbo;
public:
  GLVerticesBuffer();
  ~GLVerticesBuffer();
  void init(GLVertexArrays&);
  void bind();
  void unbind();
};


class GLFrameBuffer {
private:
  GLHANDLE fbo;
  int w, h;
public:
  GLFrameBuffer();
  ~GLFrameBuffer();
  void init(int width, int height);
  void bind();
  void unbind();
  int width() { return w; }
  int height() { return h; }
  void check();
};


class GLBufferData {
private:
  GLVerticesBuffer& vbo;
public:
  GLBufferData(GLVerticesBuffer& b, void* data, size_t length);
  void floatAttr(u32 location, u32 elementCount, u32 spaceCount, u32 beginCount = 0);
  void uintAttr(u32 location, u32 elementCount, u32 spaceCount, u32 beginCount = 0);
};


class GLTexture {
private:
  GLHANDLE text;
  
public:
  GLTexture();
  ~GLTexture();
  void init(GLFrameBuffer&, void* pixeldata = 0);
  void bind();
  void unbind();
};


class GLRenderBuffer {
private:
  GLHANDLE rb;
public:
  GLRenderBuffer();
  ~GLRenderBuffer();
  void init(GLFrameBuffer&);
  void bind();
  void unbind();
};


class GLUniform {
private:
  int uni;
  GLUniform(int id) : uni(id) {}

public:
  GLUniform() : uni(0) {}
  void setUint(u32 v);
  void setFloat(float v);
  void setInt(s32 v);
  void setUint2(u32, u32);

friend class OpenGLShader;
};


class OpenGLShader {
private:
  GLHANDLE program;
  GLHANDLE createShader(ShaderSrc src, u32 shader_flag);
  int _getUniform(const char* name);

public:
  OpenGLShader(ShaderSrc vertex, ShaderSrc fragment);
  virtual ~OpenGLShader();

  void use();
  GLUniform getUniform(const char* name);
};


class GLDrawState {
private:
  const float r, g, b, a;

public:
  GLDrawState(float c, float aa = 1) : r(c), g(c), b(c), a(aa) {}
  GLDrawState(float rr, float gg, float bb, float aa = 1) : r(rr), g(gg), b(bb), a(aa) {}

  void clear();
  void clear(float r, float g, float b, float a = 1);
  void clearDepth();
  void viewport(int x, int y, int w, int h);
  void viewport(GpuDataRange*);
  void setDepthTest(const bool);
  void setMultismple(const bool, int hint = 4);
  void setBlend(const bool);
  void initGlad();
};


}