#include "gpu_shader.h"
#include "gpu.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace ps1e {


ShaderSrc MonoColorPolygonShader::vertex = R"shader(
#version 330 core
layout (location = 0) in uint pos;
uniform uint width;
uniform uint height;

void main() {
  float x = float(0xFFFFu & pos) / width  * 2 - 1;
  float y = -(float(0xFFFFu & (pos>>16u)) / height * 2 - 1);
  float z = 0;
  gl_Position = vec4(x, y, z, 1.0);
}
)shader";


ShaderSrc MonoColorPolygonShader::frag = R"shader(
#version 330 core
out vec4 FragColor;
uniform uint color;
uniform float transparent;
  
void main() {
  uint  x = 0xFFu;
  float r = ((color      & x) / 255.0f);
  float g = ((color>> 8) & x) / 255.0f;
  float b = ((color>>16) & x) / 255.0f;
  FragColor = vec4(r, g, b, transparent);
}
)shader";


}