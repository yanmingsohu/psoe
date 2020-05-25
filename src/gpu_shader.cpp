#include "gpu_shader.h" 
#include "gpu.h"

namespace ps1e {

#define GSGL_VERSION "#version 330 core\n"

#define VertexShaderHeader GSGL_VERSION R"shader(
uniform uint frame_width;
uniform uint frame_height;
uniform int offx;
uniform int offy;
uniform float transparent;

vec4 color_ps2gl(uint pscolor) {
  float r = ((pscolor      & 0xFFu) / 255.0f);
  float g = ((pscolor>> 8) & 0xFFu) / 255.0f;
  float b = ((pscolor>>16) & 0xFFu) / 255.0f;
  return vec4(r, g, b, transparent);
}

float get_x(uint ps_pos) {
  uint t = (0xFFFFu & ps_pos); 
  uint s = t & 0x400u;
  int a  = int(t) & 0x3FF;
  if (s != 0u) a = ~(a) + 1;
  int x = a + offx;
  return float(x) / frame_width  * 2 - 1;
}

float get_y(uint ps_pos) {
  uint t = (0xFFFFu & (ps_pos>>16u)); 
  uint s = t & 0x400u;
  int a  = int(t) & 0x1FF;
  if (s != 0u) a = ~(a) + 1;
  int y = a + offy;
  return -(float(y) / frame_height  * 2 - 1);
}

float norm_x(uint n) { 
  return float(n) / frame_width  * 2 - 1;
}

float norm_y(uint n) {
  return -(float(n) / frame_height  * 2 - 1);
}

vec2 to_textcoord(uint coord, uint page) {
  uint pagex = (page & 0x0Fu) * 64u;
  uint pagey = (1u & (page >> 4)) * 256u;
  uint x = (coord & 0x0FFu) + pagex;
  uint y = ((coord >> 8) & 0x0FFu) + pagey;
  return vec2(float(x)/frame_width, 1- float(y)/frame_height);
}
)shader"


#define TextureFragShaderHeader  GSGL_VERSION R"shader(
uniform uint page;
uniform uint clut;

vec4 mix_color(vec4 b, vec4 f) {
  switch (int(page >> 5) & 0x03) {
    default:
    case 0: return b * 0.5 + f * 0.5;
    case 1: return b + f;
    case 3: return b - f;
    case 4: return b + f * 0.25;
  }
}

vec4 texture_mode(sampler2D text, vec2 coord) {
  switch (int(page >> 7) & 0x03) {
    case 0: // 4bit TODO!
      
    case 1: // 8bit TODO!

    default:
    case 2: // 16bit
      return texture(text, coord);
  }
}
)shader"

// ---------- ---------- Mono color polygon shader

static ShaderSrc mono_color_polygon_vertex = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
uniform uint ps_color;
out vec4 oColor;

void main() {
  gl_Position = vec4(get_x(pos), get_y(pos), 0, 1.0);
  oColor = color_ps2gl(ps_color);
}
)shader";


static ShaderSrc color_direct_frag = R"shader(
#version 330 core
out vec4 FragColor;
in vec4 oColor;
  
void main() {
  FragColor = oColor;
}
)shader";

// ---------- ---------- Mono color with texture polygon shader

static ShaderSrc texture_mono_color_poly_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
layout (location = 1) in uint coord;
uniform uint ps_color;
uniform uint page;
uniform uint clut;
out vec4 oColor;
out vec2 oCoord;

void main() {
  gl_Position = vec4(get_x(pos), get_y(pos), 0, 1.0);
  oColor = color_ps2gl(ps_color);
  oCoord = to_textcoord(coord, page);
}
)shader";


static ShaderSrc texture_color_f = TextureFragShaderHeader R"shader(
out vec4 FragColor;
in vec4 oColor;
in vec2 oCoord;
uniform sampler2D text;
  
void main() {
  vec4 c = mix_color(texture_mode(text, oCoord), oColor);
  FragColor = vec4(c.rgb, oColor.a);
}
)shader";


static ShaderSrc texture_only_f = TextureFragShaderHeader R"shader(
out vec4 FragColor;
in vec4 oColor;
in vec2 oCoord;
uniform sampler2D text;
  
void main() {
  vec4 c = texture_mode(text, oCoord);
  FragColor = vec4(c.rgb, oColor.a);
}
)shader";

// ---------- ---------- Shaded Polygon

static ShaderSrc shaded_polygon_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
layout (location = 1) in uint ps_color;
out vec4 oColor;

void main() {
  gl_Position = vec4(get_x(pos), get_y(pos), 0, 1.0);
  oColor = color_ps2gl(ps_color);
}
)shader";

// ---------- ---------- Shaded Polygon With Texture

static ShaderSrc shaded_polygon_texture_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
layout (location = 1) in uint ps_color;
layout (location = 2) in uint coord;

uniform uint page;
uniform uint clut;
out vec4 oColor;
out vec2 oCoord;

void main() {
  gl_Position = vec4(get_x(pos), get_y(pos), 0, 1.0);
  oColor = color_ps2gl(ps_color); 
  oCoord = to_textcoord(coord, page);
}
)shader";

// ---------- ---------- Fill Rectangle

static ShaderSrc fill_rect_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
uniform uint ps_color;
out vec4 oColor;

void main() {
  uint x = 0x03F0u & pos;
  uint y = 0x01FFu & (pos >> 16);
  gl_Position = vec4(norm_x(x), norm_y(y), 0, 1);
  oColor = color_ps2gl(ps_color);
}
)shader";

// ---------- ---------- Draw texture to screen, direct pos/coord

static ShaderSrc copy_texture_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
layout (location = 1) in uint coord;
out vec2 TexCoord;

void main() {
  uint x = 0x03F0u & pos;
  uint y = 0x01FFu & (pos >> 16);
  gl_Position = vec4(norm_x(x), norm_y(y), 0, 1);

  uint cx = 0xffffu & coord;
  uint cy = 0xffffu & (coord >> 16);
  TexCoord = vec2(cx, cy);
}
)shader";

// ---------- ---------- Shader for virtual ps ram (frame buffer)

static ShaderSrc draw_virtual_screen_vertex = GSGL_VERSION R"shader(
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 coord;
out vec2 TexCoord;

void main() {
  gl_Position = vec4(pos.x, pos.y, 0, 1.0);
  TexCoord = coord;
}
)shader";


static ShaderSrc draw_texture_frag = GSGL_VERSION R"shader(
out vec4 FragColor;
uniform sampler2D text;
in vec2 TexCoord;
  
void main() {
  FragColor = texture(text, TexCoord);
}
)shader";


ShaderSrc MonoColorShader::vertex = mono_color_polygon_vertex;
ShaderSrc MonoColorShader::frag = color_direct_frag;

ShaderSrc VirtualScreenShader::vertex = draw_virtual_screen_vertex;
ShaderSrc VirtualScreenShader::frag = draw_texture_frag;

ShaderSrc MonoColorTextureMixShader::vertex = texture_mono_color_poly_v;
ShaderSrc MonoColorTextureMixShader::frag_with_color = texture_color_f;
ShaderSrc MonoColorTextureMixShader::frag_no_color = texture_only_f;

ShaderSrc ShadedColorShader::vertex = shaded_polygon_v;
ShaderSrc ShadedColorShader::frag = color_direct_frag;

ShaderSrc ShadedColorTextureMixShader::vertex = shaded_polygon_texture_v;
ShaderSrc ShadedColorTextureMixShader::frag = texture_color_f;

ShaderSrc FillRectShader::vertex = fill_rect_v;
ShaderSrc FillRectShader::frag = color_direct_frag;

ShaderSrc CopyTextureShader::vertex = copy_texture_v;
ShaderSrc CopyTextureShader::frag = draw_texture_frag;


}