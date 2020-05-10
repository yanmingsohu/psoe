#pragma once

#include "gpu.h"

namespace ps1e {


class PSShaderBase : public OpenGLShader {
private:
  GLUniform x;
  GLUniform y;
  GLUniform fw;
  GLUniform fh;

public:
  virtual ~PSShaderBase() {}

  PSShaderBase(ShaderSrc v, ShaderSrc f) : OpenGLShader(v, f) {
    use();
    x  = getUniform("offx");
    y  = getUniform("offy");
    fw = getUniform("frame_width");
    fh = getUniform("frame_height");
  }

  void update(GpuStatus& gs, GpuDataRange& frame, GpuTextRange& text_win, 
              GpuDrawOffset& doff, GpuRange10& dtl, GpuRange10& dbr) {
    x.setUint(doff.offx());
    y.setUint(doff.offy());
    fw.setUint(frame.width);
    fh.setUint(frame.height);
  }
};


class MonoColorPolygonShader : public PSShaderBase {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;
  GLUniform transparent;
  GLUniform color;

public:
  MonoColorPolygonShader() : PSShaderBase(vertex, frag) {
    use();
    transparent = getUniform("transparent");
    color = getUniform("ps_color");
  }

  void setColor(float _transparent, u32 _color) {
    transparent.setFloat(_transparent);
    color.setUint(_color);
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