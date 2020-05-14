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


class MonoColorShader : public PSShaderBase {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;
  GLUniform color;

public:
  static const bool Texture = false;

  MonoColorShader() : PSShaderBase(vertex, frag) {
    use();
    color = getUniform("ps_color");
  }

  template<class Vertices>
  void setShaderUni(Vertices& v, GPU&, float _transparent) {
    transparent.setFloat(_transparent);
    color.setUint(v.color);
  }
};


class MonoColorTextureMixShader : public PSShaderBase {
protected:
  static ShaderSrc vertex;
  static ShaderSrc frag_with_color;
  static ShaderSrc frag_no_color;

private:
  GLUniform color;
  GLUniform page;
  GLUniform clut;

public:
  static const bool Texture = true;

  MonoColorTextureMixShader() : MonoColorTextureMixShader(frag_with_color) {}

  MonoColorTextureMixShader(ShaderSrc frag) : PSShaderBase(vertex, frag) {
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


class TextureOnlyShader : public MonoColorTextureMixShader {
public:
  TextureOnlyShader() : MonoColorTextureMixShader(frag_no_color) {}
};


class ShadedColorShader : public PSShaderBase {
protected:
  static ShaderSrc vertex;
  static ShaderSrc frag;

public:
  static const bool Texture = false;

  ShadedColorShader() : PSShaderBase(vertex, frag) {}

  template<class Vertices>
  void setShaderUni(Vertices& v, GPU& gpu, float _transparent) {
    transparent.setFloat(_transparent);
  }
};


class ShadedColorTextureMixShader : public PSShaderBase {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;
  
  GLUniform page;
  GLUniform clut;

public:
  static const bool Texture = true;

  ShadedColorTextureMixShader() : PSShaderBase(vertex, frag) {
    use();
    page  = getUniform("page");
    clut  = getUniform("clut");
  }

  template<class Vertices>
  void setShaderUni(Vertices& v, GPU& gpu, float _transparent) {
    transparent.setFloat(_transparent);
    page.setUint(v.page);
    clut.setUint(v.clut);
  }
};


class FillRectShader : public PSShaderBase {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;

  GLUniform color;

public:
  static const bool Texture = false;

  FillRectShader() : PSShaderBase(vertex, frag) {
    color = getUniform("ps_color");
  }

  template<class Vertices>
  void setShaderUni(Vertices& v, GPU& gpu, float _transparent) {
    transparent.setFloat(_transparent);
    color.setUint(v.color);
  }
};


class VirtualScreenShader : public OpenGLShader {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;

public:
  VirtualScreenShader() : OpenGLShader(vertex, frag) {}
};


}