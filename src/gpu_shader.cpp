#include "gpu_shader.h" 
#include "gpu.h"

namespace ps1e {


#define VertexShaderHeader R"shader(
#version 330 core
uniform uint frame_width;
uniform uint frame_height;
uniform uint offx;
uniform uint offy;

vec4 color_ps2gl(uint pscolor, float a) {
  uint  x = 0xFFu;
  float r = ((pscolor      & x) / 255.0f);
  float g = ((pscolor>> 8) & x) / 255.0f;
  float b = ((pscolor>>16) & x) / 255.0f;
  return vec4(r, g, b, a);
}

float get_x(uint ps_pos) {
  uint x = (0xFFFFu & ps_pos) + offx;
  return float(x) / frame_width  * 2 - 1;
}

float get_y(uint ps_pos) {
  uint y = (0xFFFFu & (ps_pos>>16u)) + offy;
  return -(float(y) / frame_height  * 2 - 1);
}
)shader"


static ShaderSrc mono_color_polygon_vertex = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
uniform uint ps_color;
out vec4 color;
uniform float transparent;

void main() {
  gl_Position = vec4(get_x(pos), get_y(pos), 0, 1.0);
  color = color_ps2gl(ps_color, transparent);
}
)shader";


static ShaderSrc color_polygon_frag = R"shader(
#version 330 core
out vec4 FragColor;
in vec4 color;
  
void main() {
  FragColor = color;
}
)shader";


static ShaderSrc draw_virtual_screen_vertex = R"shader(
#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 coord;
out vec2 TexCoord;

void main() {
  gl_Position = vec4(pos.x, pos.y, 0, 1.0);
  TexCoord = coord;
}
)shader";


static ShaderSrc draw_virtual_screen_frag = R"shader(
#version 330 core
out vec4 FragColor;
uniform sampler2D text;
in vec2 TexCoord;
  
void main() {
  FragColor = texture(text, TexCoord);
}
)shader";


ShaderSrc MonoColorPolygonShader::vertex = mono_color_polygon_vertex;
ShaderSrc MonoColorPolygonShader::frag = color_polygon_frag;

ShaderSrc VirtualScreenShader::vertex = draw_virtual_screen_vertex;
ShaderSrc VirtualScreenShader::frag = draw_virtual_screen_frag;

}