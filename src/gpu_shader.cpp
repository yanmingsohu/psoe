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

// 对于无纹理的图形，FFh的8位RGB值最亮。但是，对于纹理混合，
// 80h的8位值最亮（值81h..FFh“比亮更亮”）
// 使纹理的亮度比原始存储在内存中的亮度高两倍；
// max -- 无纹理的时候:255.0f, 有纹理的时候:80.0f
vec4 color_ps2gl(uint pscolor, float max) {
  float r = ((pscolor      & 0xFFu) / max);
  float g = ((pscolor>> 8) & 0xFFu) / max;
  float b = ((pscolor>>16) & 0xFFu) / max;
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
  return (float(y) / frame_height  * 2 - 1);
}

float norm_x(uint n) { 
  return float(n) / frame_width  * 2 - 1;
}

float norm_y(uint n) {
  return (float(n) / frame_height  * 2 - 1);
}

// TODO: Textwin Texcoord = (Texcoord AND (NOT (Mask*8))) OR ((Offset AND Mask)*8)
vec2 to_textcoord(uint coord, uint page, uint textwin) {
  float pagex = float(0x0Fu & page) * 0x40;
  float pagey = float(1u & (page >> 4)) * 0x100;
  // TODO: 纹理页的大小来自纹理格式字段 64/128/256
  float x = float(coord & 0x0FFu) / 0xff * 0x39; 
  float y = float((coord >> 8) & 0x0FFu);
  return vec2((x + pagex)/frame_width, (y + pagey)/frame_height);
}
)shader"


#define TextureFragShaderHeader  GSGL_VERSION R"shader(
uniform uint page;
uniform uint clut;
uniform float transparent;
uniform uint frame_width;
uniform uint frame_height;

vec4 mix_color(vec4 b, vec4 f) {
  vec4 r;
  if (b.rgb == 0) {
    r = f;
  } else {
    r = b * f;
  }
  r.a = transparent;
  return r;
}

float color_gradient(int c) {
  float a = float(c & 0x10);
  float b = float(c & 0x0F);
  return (a * b) / 0xff;
}

vec4 text_pscolor_rgb(int red16) {
  float r = color_gradient(red16);
  float g = color_gradient(red16 >> 5);
  float b = color_gradient(red16 >> 10);
  float a = ((red16 >> 15) & 1)==1 ? 0.5 : 1.0;
  return vec4(r, g, b, a);
}

vec4 texture_red16(sampler2D text, vec2 coord) {
  int red = int(texture(text, coord).r * 0xffffu);
  return text_pscolor_rgb(red);
}

uint get_clut_index(sampler2D text, vec2 coord, uint mask, int rol) {
  uint rm   = (~mask) & 0xFFFFu;
  uint fx   = uint(coord.x * frame_width);
  coord.x   = float(fx & mask) / frame_width;
  uint bit  = (fx & rm) << rol;
  uint word = uint(texture(text, coord).r * 0xffff);
  //TODO: bit 顺序正确性需要测试
  return (word >> bit);
}

vec2 get_clut_coord(uint clut_index) {
  uint clut_x = ((clut & 0x3fu) << 4) + uint(clut_index);
  uint clut_y = ((clut >> 6) & 0x1ffu);
  return vec2(float(clut_x)/frame_width, float(clut_y)/frame_height);
}

vec4 texture_mode(sampler2D text, vec2 coord) {
  switch (int(page >> 7) & 0x03) {
    case 0: // 4bit
      uint index4 = get_clut_index(text, coord, 0xFFFCu, 2) & 0xFu;
      return texture_red16(text, get_clut_coord(index4));
      
    case 1: // 8bit
      uint index8 = get_clut_index(text, coord, 0xFFFEu, 3) & 0xFFu;
      return texture_red16(text, get_clut_coord(index8));

    default:
    case 2: // 16bit
      return texture_red16(text, coord);
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
  oColor = color_ps2gl(ps_color, 255.0f);
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
uniform uint textwin;
out vec4 oColor;
out vec2 oCoord;

void main() {
  gl_Position = vec4(get_x(pos), get_y(pos), 0, 1.0);
  oColor = color_ps2gl(ps_color, 80.0f);
  oCoord = to_textcoord(coord, page, textwin);
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
  oColor = color_ps2gl(ps_color, 255.0f);
}
)shader";

// ---------- ---------- Shaded Polygon With Texture

static ShaderSrc shaded_polygon_texture_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
layout (location = 1) in uint ps_color;
layout (location = 2) in uint coord;
uniform uint page;
uniform uint clut;
uniform uint textwin;
out vec4 oColor;
out vec2 oCoord;

void main() {
  oColor = color_ps2gl(ps_color, 80.0f); 
  oCoord = to_textcoord(coord, page, textwin);
  gl_Position = vec4(get_x(pos), get_y(pos), 0, 1.0);
}
)shader";

// ---------- ---------- Fill Rectangle

static ShaderSrc fill_rect_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
uniform uint ps_color;
out vec4 oColor;

void main() {
  uint x = 0x03FFu & pos;
  uint y = 0x01FFu & (pos >> 16);
  gl_Position = vec4(norm_x(x), norm_y(y), 0, 1);
  oColor = color_ps2gl(ps_color, 255.0f);
}
)shader";

// ---------- ---------- Draw texture to screen, direct pos/coord

static ShaderSrc copy_texture_v = VertexShaderHeader R"shader(
layout (location = 0) in uint pos;
layout (location = 1) in uint coord;
out vec2 TexCoord;

void main() {
  uint x = 0x03FFu & pos;
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
  gl_Position = vec4(pos.x, -pos.y, 0, 1.0);
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