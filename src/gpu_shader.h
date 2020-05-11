#pragma once 

#include "gpu.h"

namespace ps1e {


class PSShaderBase : public OpenGLShader {
private:
  GLUniform x;
  GLUniform y;
  GLUniform fw;
  GLUniform fh;
  u32 update_flag;

protected:
  GLUniform transparent;

public:
  virtual ~PSShaderBase() {}

  PSShaderBase(ShaderSrc v, ShaderSrc f) : OpenGLShader(v, f) {
    use();
    x  = getUniform("offx");
    y  = getUniform("offy");
    fw = getUniform("frame_width");
    fh = getUniform("frame_height");
    transparent = getUniform("transparent");
  }

  void update(u32 flag, GPU& gpu) {
    if (update_flag != flag) {
      x.setInt(gpu.draw_offset.offx());
      y.setInt(gpu.draw_offset.offy());
      fw.setUint(gpu.frame.width);
      fh.setUint(gpu.frame.height);
      update_flag = flag;
    }
  }
};


class MonoColorPolygonShader : public PSShaderBase {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;
  GLUniform color;

public:
  MonoColorPolygonShader() : PSShaderBase(vertex, frag) {
    use();
    color = getUniform("ps_color");
  }

  template<class Vertices>
  void setShaderUni(Vertices& v, GPU&, float _transparent) {
    transparent.setFloat(_transparent);
    color.setUint(v.color);
  }
};


class MonoColorTexturePolyShader : public PSShaderBase {
protected:
  static ShaderSrc vertex;
  static ShaderSrc frag_with_color;
  static ShaderSrc frag_no_color;

private:
  GLUniform color;
  GLUniform page;
  GLUniform clut;

public:
  using PSShaderBase::update;

  MonoColorTexturePolyShader() : MonoColorTexturePolyShader(frag_with_color) {}

  MonoColorTexturePolyShader(ShaderSrc frag) : PSShaderBase(vertex, frag) {
    use();
    color = getUniform("ps_color");
    page  = getUniform("page");
    clut  = getUniform("clut");
  }

  template<class Vertices>
  void setShaderUni(Vertices& v, GPU& gpu, float _transparent) {
    transparent.setFloat(_transparent);
    color.setUint(v.color);
    page.setUint(v.page);
    clut.setUint(v.clut);
  }
};


class TextureOnlyPolyShader : public MonoColorTexturePolyShader {
public:
  TextureOnlyPolyShader() : MonoColorTexturePolyShader(frag_no_color) {}
};


class VirtualScreenShader : public OpenGLShader {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;

public:
  VirtualScreenShader() : OpenGLShader(vertex, frag) {}
};


}