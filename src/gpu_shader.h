#pragma once

#include "gpu.h"

namespace ps1e {


class MonoColorPolygonShader : public OpenGLShader {
private:
  static ShaderSrc vertex;
  static ShaderSrc frag;

public:
  MonoColorPolygonShader() : OpenGLShader(vertex, frag) {}
};


}