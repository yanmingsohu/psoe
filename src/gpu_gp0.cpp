#include "gpu.h"
#include "gpu_shader.h"
#include <functional>
#include <stdexcept>
#include <condition_variable>
#include <mutex>

namespace ps1e {

static const u32 X_MASK  = 0x0000'03FF;
static const u32 Y_MASK  = 0x01FF'0000;
static const u32 XY_MASK = X_MASK | Y_MASK;
// gl 在绝对位置上绘制像素, 所以 width+height 需要减去这个数
static const u32 XY_SUB  = 0x0001'0001;
static const u32 X_SUB   = 0x0000'0001;
static const u32 Y_SUB   = 0x0001'0000;
// x 总是在 16 字节上对齐
static const u32 offset_limit_x10 = 0x01FF'03F0;


// 实际上 mirror_case 的定义与对应的 case 在渲染上有出入
// 可以用恶魔城进行测试, 进一步确定渲染方式.
#define mirror_case(x)  case x


// w*h 表示 16bit 像素的数量, 返回的长度表示 32bit 缓冲区长度
// 如果像素位奇数, 则缓冲区长度会多出一个 16bit
template<class T> static T get_buffer_len(T w, T h) {
  int l = w * h;
  return (l >> 1) + (l & 1);
}


// ps_coord 是 YyyyXxxx 格式
template<class T> static void get_xy32(u32 ps_coord, T& x, T& y) {
  x = ps_coord & X_MASK;
  y = (ps_coord & Y_MASK) >> 16;
}


class VerticesBase {
private:
  VerticesBase(VerticesBase&);

protected:

  int step;
  const int element;

public:
  static const bool DisableDrawScopeLimit = false;

  VerticesBase(int ele, int st = 0) : step(st), element(ele) {}
  virtual ~VerticesBase() {}

  int elementCount() {
    return element;
  }

  u32 mask_add(u32 a, u32 b, u32 mask) {
    u32 s = (a & mask) + (b & mask);
    return (a & ~mask) | (s & mask);
  }

  void updateTextureInfo(GPU& gpu) {}
};


template<int ElementCount>
class PolygonVertices : public VerticesBase {
private:
  u32 vertices[ElementCount];

public:
  u32 color;

  PolygonVertices() : VerticesBase(ElementCount, -1) {}

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 1);
  }

  bool write(const u32 c) {
    switch (step) {
      case -1:
        color = c;
        break;

      default: {
        vertices[step] = c;
        break;
      }
    }
    return ++step < ElementCount;
  }
};


template<int ElementCount>
class PolyTextureVertices : public VerticesBase {
private:
  u32 vertices[ElementCount *2];

public:
  u32 color;
  u32 clut;
  u32 page;

  PolyTextureVertices() : VerticesBase(ElementCount, -1) {}

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 2, 0);
    vbdata.uintAttr(1, 1, 2, 1);
  }

  bool write(const u32 c) {
    switch (step) {
      case -1:
        color = c;
        break;

      default: {
        switch (step) {
          case 1:
            clut = (c & 0xFFFF'0000)>>16;
            break;
          case 3:
            page = (c & 0xFFFF'0000)>>16;
            break;
        }
        vertices[step] = c;
        break;
      }
    }
    return ++step < (ElementCount *2);
  }
};


template<int ElementCount>
class ShadedPolyVertices : public VerticesBase {
protected:
  u32 vertices[ElementCount *2];

public:
  ShadedPolyVertices() : VerticesBase(ElementCount) {}

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 2, 1);
    vbdata.uintAttr(1, 1, 2, 0);
  }

  bool write(const u32 c) {
    //printf("x %d y %d \n", c&X_MASK, ((c & Y_MASK) >> 16));
    vertices[step] = c;
    return ++step < (ElementCount *2);
  }
};


template<int ElementCount>
class ShadedPolyWithTextureVertices : public VerticesBase {
private:
  u32 vertices[ElementCount *3];

public:
  u32 clut;
  u32 page;

  ShadedPolyWithTextureVertices() : VerticesBase(ElementCount) {}

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 3, 1);
    vbdata.uintAttr(1, 1, 3, 0);
    vbdata.uintAttr(2, 1, 3, 2);
  }

  bool write(const u32 c) {
    vertices[step] = c;
    switch (step) {
      case 2:
        clut = (c & 0xFFFF'0000)>>16;
        break;

      case 5:
        page = (c & 0xFFFF'0000)>>16;
        break;
    }
    return ++step < (ElementCount *3);
  }
};


class MonoLineFixVertices : public VerticesBase {
private:
  u32 vertices[2];

public:
  u32 color;

  MonoLineFixVertices() : VerticesBase(2, -1) {}

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 1, 0);
  }

  bool write(const u32 c) {
    switch (step) {
      case -1:
        color = c;
        break;

      default:
        vertices[step] = c;
    }
    return ++step < 2;
  }
};


class MultipleVertices {
public:
  static const bool DisableDrawScopeLimit = false;

  const u32 END = 0x50005000;
  const int InitBufSize = 0x10;

protected:
  u32 *vertices;

private:
  int capacity;
  int count;
  const int mincount;

  void resize(int size) {
    if (capacity < size) {
      u32 *mm = (u32*) realloc(vertices, size * sizeof(u32));
      if (!mm) {
        throw std::runtime_error("Failed allocated memory");
      }
      vertices = mm;
      capacity = size;
    }
  }

public:
  u32 color;

  MultipleVertices(int initCount, int terminatMin) : 
      capacity(0), count(initCount), mincount(terminatMin)
  {
    capacity = InitBufSize;
    vertices = (u32*) malloc(capacity * sizeof(u32));
    if (!vertices) {
      throw std::runtime_error("Failed allocated memory");
    }
  }

  ~MultipleVertices() {
    free(vertices);
    vertices = 0;
    count = 0;
    capacity = 0;
  }

  int elementCount() {
    return count;
  }

  void updateTextureInfo(GPU& gpu) {}

  bool write(const u32 c) {
    // 55555555h ? 50005000h ??
    if ((count >= mincount) && (c & 0xF000F000)==END) {
      return false;
    }

    writeVertices(count, c);

    if (++count >= capacity) {
      resize(capacity << 1);
    }
    return true;
  }

  inline size_t vsize() {
    return sizeof(u32) * count;
  }

  virtual void writeVertices(int count, const u32 data) = 0;
};


class MonoLineMulVertices : public MultipleVertices {
public:
  MonoLineMulVertices() : MultipleVertices(-1, 2) {}

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, vsize());
    vbdata.uintAttr(0, 1, 1, 0);
  }

  void writeVertices(int count, const u32 data) {
    switch (count) {
      case -1:
        color = data;
        break;

      default:
        vertices[count] = data;
    }
  }
};


class ShadedLineMulVertices : public MultipleVertices {
public:
  ShadedLineMulVertices() : MultipleVertices(0, 3) {}

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, vsize());
    vbdata.uintAttr(0, 1, 2, 1);
    vbdata.uintAttr(1, 1, 2, 0);
  }

  void writeVertices(int count, const u32 data) {
    vertices[count] = data;
  }

  int elementCount() {
    return MultipleVertices::elementCount() >> 1;
  }
};


template<u8 Fixed>
class SquareVertices : public VerticesBase {
protected:
  static const int ElementCount = 4;
  u32 vertices[ElementCount];

  // 用偏移量(H/W) 设置矩形四个顶点
  void update_vertices(u32 offset) {
    vertices[1] = vertices[0] + (X_MASK  & offset);
    vertices[2] = vertices[0] + (Y_MASK  & offset);
    vertices[3] = vertices[0] + (XY_MASK & offset);
  }

  // 修正顶点位置为绝对坐标
  void fix_width_height_offset() {
    vertices[1] -= X_SUB;
    vertices[2] -= Y_SUB;
    vertices[3] -= XY_SUB;
  }

public:
  u32 color;

  SquareVertices() : VerticesBase(ElementCount) {
  }

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 1, 0);
  }

  bool write(const u32 c) {
    switch (step) {
      case 0:
        color = c;
        break;

      case 1:
        vertices[0] = c;
        break;

      case 2:
        update_vertices(c);
        // 如果高度或宽度为 0 会引起显示异常
        fix_width_height_offset();
        break;
    }

    if (Fixed) {
      update_vertices(Fixed + (Fixed << 16));
      return ++step < 2;
    } else {
      return ++step < 3;
    }
  }
};


template<u8 Fixed>
class SquareWithTextureVertices : public VerticesBase {
protected:
  static const int ElementCount = 4;
  u32 vertices[ElementCount*2];

  void update_vertices(u32 offset) {
    vertices[2] = vertices[0] + (X_MASK  & offset);
    vertices[4] = vertices[0] + (Y_MASK  & offset);
    vertices[6] = vertices[0] + (XY_MASK & offset);
    
    const u32 x = (0x000000ff & offset);
    const u32 y = (0x00FF0000 & offset) >> 8;
    vertices[3] = mask_add(vertices[1], x, 0xFF);
    vertices[5] = mask_add(vertices[1], y, 0xFF00);
    vertices[7] = mask_add(vertices[5], x, 0xFF);
  }

public:
  u32 color;
  u32 clut;
  u32 page;

  SquareWithTextureVertices() : VerticesBase(ElementCount) {
  }

  void updateTextureInfo(GPU& gpu) {
    page = textureAttr(gpu.status, gpu.text_flip);
  }

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 2, 0);
    vbdata.uintAttr(1, 1, 2, 1);
  }

  bool write(const u32 c) {
    switch (step) {
      case 0:
        color = c;
        break;

      case 1:
        vertices[0] = c;
        break;

      case 2:
        clut = 0xFFFF & (c >> 16);
        vertices[1] = 0xFFFF & c;
        break;

      case 3:
        update_vertices(c);
        break;
    }

    if (Fixed) {
      update_vertices(Fixed + (Fixed << 16));
      return ++step < 3;
    } else {
      return ++step < 4;
    }
  }
};


class PointVertices : public VerticesBase {
private:
  static const int ElementCount = 1;
  u32 vertices[ElementCount];

public:
  u32 color;

  PointVertices() : VerticesBase(ElementCount) {
  }

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 1, 0);
  }

  bool write(const u32 c) {
    switch (step) {
      case 0:
        color = c;
        break;

      case 1:
        vertices[0] = c;
        break;
    }
    return ++step < 2;
  }
};


class PointTextVertices : public VerticesBase {
private:
  static const int ElementCount = 1;
  u32 vertices[ElementCount * 2];

public:
  u32 color;
  u32 clut;
  u32 page;

  PointTextVertices() : VerticesBase(ElementCount) {
  }

  void updateTextureInfo(GPU& gpu) {
    page = textureAttr(gpu.status, gpu.text_flip);
  }

  void setAttr(GLVerticesBuffer& vbo) {
    GLBufferData vbdata(vbo, vertices, sizeof(vertices));
    vbdata.uintAttr(0, 1, 2, 0);
    vbdata.uintAttr(1, 1, 2, 1);
  }

  bool write(const u32 c) {
    switch (step) {
      case 0:
        color = c;
        break;

      case 1:
        vertices[0] = c;
        break;

      case 2:
        clut = 0xFFFF & (c >> 16);
        vertices[1] = 0xFFFF & c;
        break;
    }
    return ++step < 3;
  }
};


class FillVertices : public SquareVertices<0> {
private:
  void align_x10() {
    vertices[1] &= offset_limit_x10;
    vertices[2] &= offset_limit_x10;
    vertices[3] &= offset_limit_x10;
  }

public:
  static const bool DisableDrawScopeLimit = true;

  bool write(const u32 c) {
    //printf("fv %08x\n", c);
    switch (step) {
      case 0:
        color = c;
        break;

      case 1:
        vertices[0] = c;
        break;

      case 2:
        update_vertices(c);
        align_x10();
        fix_width_height_offset();
        break;
    }
    return ++step < 3;
  }
};


class FillTexture : public IDrawShape {
private:
  u32* buf;
  int buf_length;
  GLTexture text;
  u32 w, h;
  u32 x, y;
  int step;

public:
  FillTexture() : step(-3), buf(0) {
  }

  ~FillTexture() {
    if (buf) {
      delete [] buf;
      buf = 0;
    }
  }

  void draw(GPU& gpu, GLVertexArrays& vao) {
    gpu.enableDrawScope(false);
    text.init2px(w, h, buf);
    text.bind();
    text.copyTo(gpu.useTexture(), 0, 0, x, y, w, h);
  }

  bool write(const u32 c) {
    //printf("FM %d %08x\n", step, c);
    switch (step) {
      case -3:
        break;

      case -2: 
        get_xy32(c & offset_limit_x10, x, y);
        break;

      case -1:
        get_xy32(c, w, h);
        buf_length = get_buffer_len(w, h);
        buf = new u32[buf_length];
        break;

      default:
        //每次写入两个像素的数据
        buf[step] = c;
        break;
    }
    return ++step < buf_length;
  }
};


class DoNothingBeforAfterDraw {
public:
  void beforeDraw(GPU* gpu, GLVertexArrays* vao) {}
  void afterDraw(GPU* gpu, GLVertexArrays* vao) {}
};


class DisableDrawScopeLimit {
public:
  void beforeDraw(GPU* gpu, GLVertexArrays* vao) {
    gpu->enableDrawScope(false);
  }
  void afterDraw(GPU* gpu, GLVertexArrays* vao) {
    gpu->enableDrawScope(true);
  }
};


template< class Vertices, 
          void (*Draw)(GLVertexArrays&, int),
          class Shader = MonoColorShader
          >
class Polygon : public IDrawShape {
private:
  Vertices vertices;
  float transparent;

public:
  Polygon(float trans) : transparent(trans) {
  }

  ~Polygon() {
  }

  virtual bool write(const u32 c) {
    return vertices.write(c);
  }

  virtual void draw(GPU& gpu, GLVertexArrays& vao) {
    gl_scope(vao);
    vbo.init(vao);
    gl_scope(vbo);
    vertices.setAttr(vbo);
    vertices.updateTextureInfo(gpu);

    auto prog = gpu.useProgram<Shader>();
    prog->setShaderUni(vertices, gpu, transparent);

    if (Shader::Texture) gpu.useTexture()->bind();
    if (Vertices::DisableDrawScopeLimit) gpu.enableDrawScope(false);
    Draw(vao, vertices.elementCount());
    if (Shader::Texture) gpu.useTexture()->unbind();
  }
};


// 该数据读取器首先进入锁定状态, 任何尝试读取的线程都会被锁定
// 直到另一个线程解锁(数据).
//template<class Data>
class ReadVram : public IGpuReadData {
private:
  std::mutex m;
  std::condition_variable cv;

  const int len;
  u32* data;
  int p = 0;
  bool lock = true;

public:
  //TODO: 传输受“掩码”设置影响。
  ReadVram(int w, int h) : len(get_buffer_len(w, h)) {
    data = new u32[len];
  }

  ~ReadVram() {
    delete[] data;
  }

  u32 read() {
    if (lock) {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk);
    }
    //printf("Cpu read vram %d[%d]:%x\n", p, len, data[p]);
    return data[p++];
  }

  bool has() {
    return p < len;
  }

  u32* getDataPoint() {
    return data;
  }

  void unlock() {
    std::unique_lock<std::mutex> lk(m);
    lock = false;
    cv.notify_all();
  }
};


class CopyVramToCpu : public IDrawShape {
private:
  GPU& gpu;
  ReadVram* reader = 0;
  int state = 0;
  int x, y;
  int w, h;

  void installReader() {
    reader = new ReadVram(w, h);
    gpu.add(reader);
    //printf("install vram reader w_%d h_%d | %d %d\n", w, h, x, y);
    //ps1e_t::ext_stop = 1;
  }

  void dataReady() {
    gpu.useTexture()->bind();
    const int reverse_y = VirtualFrameBuffer::Height-1 -y;
    GLDrawState::readPsinnerPixel(x, reverse_y, w, h, reader->getDataPoint());
    reader->unlock();
  }

public:
  CopyVramToCpu(GPU& g) : gpu(g) {}

  bool write(const u32 c) {
    switch (state++) {
      case 0:
        return true;

      case 1:
        get_xy32(c, x, y);
        return true;

      case 2:
        get_xy32(c, w, h);
        installReader();
        // no break
      default:
        return false;
    }
  }

  void draw(GPU& gpu, GLVertexArrays& vao) {
    gl_scope(vao);
    gpu.enableDrawScope(false);
    dataReady();
  }
};


class CopyVramToVram : public IDrawShape {
private:
  int step;
  u32 srcX, srcY;
  u32 dstX, dstY;
  u32 w, h;

public:
  bool write(const u32 c) {
    switch (step++) {
      case 0:
        return true;

      case 1:
        get_xy32(c, srcX, srcY);
        return true;

      case 2:
        get_xy32(c, dstX, dstY);
        return true;

      case 3:
        get_xy32(c, w, h);
        // no break

      default:
        return false;
    }
  }

  void draw(GPU& gpu, GLVertexArrays& vao) {
    gl_scope(vao);
    gpu.enableDrawScope(false);
    GLTexture* t = gpu.useTexture();
    t->copyTo(t, srcX, srcY, dstX, dstY, w, h);
  }
};


void drawTriangles(GLVertexArrays& vao, int elementCount) {
  vao.drawTriangles(elementCount);
}


void drawFan(GLVertexArrays& vao, int elementCount) {
  vao.drawTriangleFan(elementCount);
}


void drawTriStrip(GLVertexArrays& vao, int elementCount) {
  vao.drawTriangleStrip(elementCount);
}


void drawQuads(GLVertexArrays& vao, int elementCount) {
  vao.drawQuads(elementCount);
}


void drawLines(GLVertexArrays& vao, int elementCount) {
  vao.drawLineStrip(elementCount);
}


void drawPoints(GLVertexArrays& vao, int elementCount) {
  vao.drawPoints(elementCount);
}
 

 //TODO: 启用绘制范围
bool GPU::GP0::parseCommand(const GpuCommand c) {
  switch (c.cmd) {
    // GPU NOP
    case 0x00:
      //debug("Gpu Nop?\n");
      return false;

    // 清除缓存
    case 0x01:
      //debug("Clear Cache\n");
      return false;

    // 在VRAM中填充矩形
    case 0x02:
      shape = new Polygon<FillVertices, drawTriStrip, FillRectShader>(1);
      break;

    // 写显存
    case 0xA0:
      shape = new FillTexture();
      break;

    // 读显存
    case 0xC0:
      shape = new CopyVramToCpu(p);
      break;

    // 复制显存
    case 0x80:
      shape = new CopyVramToVram();
      break;

    // 中断请求
    case 0x1F:
      p.status.irq_on = 1;
      return false;

    // 绘图模式设置 Texpage
    case 0xE1:
      p.status.v = SET_BIT(p.status.v, 0b11'1111'1111, c.parm);
      p.status.text_off = (c.parm >> 11) & 1;
      p.status.inter_f  = (c.parm >> 13) & 1;
      p.text_flip.x     = (c.parm >> 12) & 1;
      p.text_flip.y     = (c.parm >> 13) & 1;
      p.dirtyAttr();
      return false;

    // 纹理窗口设置
    case 0xE2:
      p.text_win.v = c.parm;
      p.dirtyAttr();
      return false;

    // 设置绘图区域左上角（X1，Y1）
    case 0xE3: 
      p.draw_tp_lf.v = c.parm;
      p.updateDrawScope();
      p.dirtyAttr();
      return false;

    // 设置绘图区域右下角（X2，Y2）绝对位置
    case 0xE4: 
      p.draw_bm_rt.v = c.parm;
      p.updateDrawScope();
      p.dirtyAttr();
      return false;

    // 设置绘图偏移（X，Y）
    case 0xE5:
      p.draw_offset.v = c.parm;
      p.dirtyAttr();
      return false;

    // 屏蔽位设置
    case 0xE6:
      p.status.mask = c.parm & 1;
      p.status.enb_msk = (c.parm >> 1) & 1;
      p.dirtyAttr();
      return false;
      
    // 单色三点多边形，不透明
    case 0x20: mirror_case(0x21):
      shape = new Polygon<PolygonVertices<3>, drawTriangles>(1);
      break;

    // 单色三点多边形，半透明
    case 0x22: mirror_case(0x23):
      shape = new Polygon<PolygonVertices<3>, drawTriangles>(0.5);
      break;
       
    // 单色四点多边形，不透明
    case 0x28: mirror_case(0x29):
      shape = new Polygon<PolygonVertices<4>, drawTriStrip>(1);
      break;

    // 单色四点多边形，半透明
    case 0x2A: mirror_case(0x2B):
      shape = new Polygon<PolygonVertices<4>, drawTriStrip>(0.5);
      break;

    // 带纹理的三点多边形，不透明，混合纹理
    case 0x24:
      shape = new Polygon<PolyTextureVertices<3>, 
                  drawTriangles, MonoColorTextureMixShader>(1);
      break;

    // 带纹理的三点多边形，不透明，原始纹理
    case 0x25:
      shape = new Polygon<PolyTextureVertices<3>, 
                  drawTriangles, TextureOnlyShader>(1);
      break;

    // 带纹理的三点多边形，半透明，混合纹理
    case 0x26:
      shape = new Polygon<PolyTextureVertices<3>, 
                  drawTriangles, MonoColorTextureMixShader>(0.5);
      break;

    // 带纹理的三点多边形，半透明，原始纹理
    case 0x27:
      shape = new Polygon<PolyTextureVertices<3>, 
                  drawTriangles, TextureOnlyShader>(0.5);
      break;

    // 带纹理的四点多边形，不透明，混合纹理
    case 0x2C:
      shape = new Polygon<PolyTextureVertices<4>, 
                  drawTriStrip, MonoColorTextureMixShader>(1);
      break;

    // 带纹理的四点多边形，不透明，原始纹理
    case 0x2D:
      shape = new Polygon<PolyTextureVertices<4>, 
                  drawTriStrip, TextureOnlyShader>(1);
      break;

    // 带纹理的四点多边形，半透明，混合纹理
    case 0x2E:
      shape = new Polygon<PolyTextureVertices<4>, 
                  drawTriStrip, MonoColorTextureMixShader>(0.5);
      break;

    // 带纹理的四点多边形，半透明，原始纹理
    case 0x2F:
      shape = new Polygon<PolyTextureVertices<4>, 
                  drawTriStrip, TextureOnlyShader>(0.5);
      break;

    // 阴影三点多边形，不透明
    case 0x30: mirror_case(0x31):
      shape = new Polygon<ShadedPolyVertices<3>, 
                  drawTriangles, ShadedColorShader>(1);
      break;

    // 阴影三点多边形，半透明
    case 0x32: mirror_case(0x33):
      shape = new Polygon<ShadedPolyVertices<3>, 
                  drawTriangles, ShadedColorShader>(0.5);
      break;

    // 阴影四点多边形，不透明
    case 0x38: mirror_case(0x39):
      shape = new Polygon<ShadedPolyVertices<4>, 
                  drawTriStrip, ShadedColorShader>(1);
      break;

    // 阴影四点多边形，半透明
    case 0x3A: mirror_case(0x3B):
      shape = new Polygon<ShadedPolyVertices<4>, 
                  drawTriStrip, ShadedColorShader>(0.5);
      break;

    // 带阴影的纹理三点多边形，不透明，纹理混合
    case 0x34: mirror_case(0x35):
      shape = new Polygon<ShadedPolyWithTextureVertices<3>,
                  drawTriangles, ShadedColorTextureMixShader>(1);
      break;

    // 带阴影的纹理三点多边形，半透明，纹理混合
    case 0x36: mirror_case(0x37):
      shape = new Polygon<ShadedPolyWithTextureVertices<3>,
                  drawTriangles, ShadedColorTextureMixShader>(0.5);
      break;

    // 带阴影的纹理四点多边形，不透明，纹理混合
    case 0x3C: mirror_case(0x3D):
      shape = new Polygon<ShadedPolyWithTextureVertices<4>,
                  drawTriStrip, ShadedColorTextureMixShader>(1);
      break;

    // 着色纹理四点多边形，半透明，纹理混合
    case 0x3E: mirror_case(0x3F):
      shape = new Polygon<ShadedPolyWithTextureVertices<4>,
                  drawTriStrip, ShadedColorTextureMixShader>(0.5);
      break;

    // 单色线，不透明
    case 0x40:
      shape = new Polygon<MonoLineFixVertices, drawLines, MonoColorShader>(1);
      break;

    // 单色线，半透明
    case 0x42:
      shape = new Polygon<MonoLineFixVertices, drawLines, MonoColorShader>(0.5);
      break;

    // 单色多线，不透明
    case 0x48:
      shape = new Polygon<MonoLineMulVertices, drawLines, MonoColorShader>(1);
      break;

    // 单色多线，半透明
    case 0x4A:
      shape = new Polygon<MonoLineMulVertices, drawLines, MonoColorShader>(0.5);
      break;

    // 阴影线，不透明
    case 0x50: 
      shape = new Polygon<ShadedPolyVertices<2>, drawLines, ShadedColorShader>(1);
      break;

    // 阴影线，半透明
    case 0x52: 
      shape = new Polygon<ShadedPolyVertices<2>, drawLines, ShadedColorShader>(0.5);
      break;

    // 阴影多段线，不透明
    case 0x58:
      shape = new Polygon<ShadedLineMulVertices, drawLines, ShadedColorShader>(1);
      break;

    // 阴影多段线，半透明
    case 0x5A:
      shape = new Polygon<ShadedLineMulVertices, drawLines, ShadedColorShader>(0.5);
      break;

    // 单色矩形（可变大小）（不透明）
    case 0x60:
      shape = new Polygon<SquareVertices<0>, drawTriStrip, MonoColorShader>(1);
      break;

    // 单色矩形（可变大小）（半透明）
    case 0x62:
      shape = new Polygon<SquareVertices<0>, drawTriStrip, MonoColorShader>(0.5);
      break;

    // 单色矩形（1x1）（点）（不透明）
    case 0x68:
      shape = new Polygon<PointVertices, drawPoints, MonoColorShader>(1);
      break;

    // 单色矩形（1x1）（点）（半透明）
    case 0x6A:
      shape = new Polygon<PointVertices, drawPoints, MonoColorShader>(0.5);
      break;

    // 单色矩形（8x8）（不透明）
    case 0x70:
      shape = new Polygon<SquareVertices<7>, drawTriStrip, MonoColorShader>(1);
      break;

    // 单色矩形（8x8）（半透明）
    case 0x72:
      shape = new Polygon<SquareVertices<7>, drawTriStrip, MonoColorShader>(0.5);
      break;

    // 单色矩形（ 16x16）（不透明）
    case 0x78:
      shape = new Polygon<SquareVertices<15>, drawTriStrip, MonoColorShader>(1);
      break;

    // 单色矩形（16x16）（半透明）
    case 0x7A:
      shape = new Polygon<SquareVertices<15>, drawTriStrip, MonoColorShader>(0.5);
      break;

    // 纹理矩形，可变大小，不透明，纹理混合
    case 0x64:
      shape = new Polygon<SquareWithTextureVertices<0>,
                  drawTriStrip, MonoColorTextureMixShader>(1);
      break;

    // 纹理矩形，可变大小，不透明，原始纹理
    case 0x65:
      shape = new Polygon<SquareWithTextureVertices<0>,
                  drawTriStrip, TextureOnlyShader>(1);
      break;

    // 纹理矩形，可变大小，半透明，纹理混合
    case 0x66:
      shape = new Polygon<SquareWithTextureVertices<0>,
                  drawTriStrip, MonoColorTextureMixShader>(0.5);
      break;

    // 纹理矩形，可变大小，半透明，原始纹理
    case 0x67:
      shape = new Polygon<SquareWithTextureVertices<0>,
                  drawTriStrip, TextureOnlyShader>(0.5);
      break;

    // 纹理矩形，1x1（无意义?），不透明，纹理混合
    case 0x6C:
      shape = new Polygon<PointTextVertices,
                  drawPoints, MonoColorTextureMixShader>(1);
      break;

    // 纹理矩形，1x1（无意义），不透明，原始纹理
    case 0x6D:
      shape = new Polygon<PointTextVertices,
                  drawPoints, TextureOnlyShader>(1);
      break;

    // 纹理矩形，1x1（无意义），半透明，纹理混合
    case 0x6E:
      shape = new Polygon<PointTextVertices,
                  drawPoints, MonoColorTextureMixShader>(0.5);
      break;

    // 纹理矩形，1x1（无意义），半透明，原始纹理
    case 0x6F:
      shape = new Polygon<PointTextVertices,
                  drawPoints, TextureOnlyShader>(0.5);
      break;

    // 纹理矩形，8x8，不透明，混合纹理
    case 0x74:
      shape = new Polygon<SquareWithTextureVertices<7>,
                  drawTriStrip, MonoColorTextureMixShader>(1);
      break;

    // 纹理矩形，8x8，不透明，原始纹理
    case 0x75:
      shape = new Polygon<SquareWithTextureVertices<7>,
                  drawTriStrip, TextureOnlyShader>(1);
      break;

    // 带纹理的矩形，8x8，半透明，纹理混合
    case 0x76:
      shape = new Polygon<SquareWithTextureVertices<7>,
                  drawTriStrip, MonoColorTextureMixShader>(0.5);
      break;

    // 带纹理的矩形，8x8，半透明，原始纹理
    case 0x77:
      shape = new Polygon<SquareWithTextureVertices<7>,
                  drawTriStrip, TextureOnlyShader>(0.5);
      break;

    // 带纹理的矩形，16x16，不透明，带纹理混合
    case 0x7C:
      shape = new Polygon<SquareWithTextureVertices<15>,
                  drawTriStrip, MonoColorTextureMixShader>(1);
      break;

    // 带纹理的矩形，16x16，不透明, 原始纹理
    case 0x7D:
      shape = new Polygon<SquareWithTextureVertices<15>,
                  drawTriStrip, TextureOnlyShader>(1);
      break;

    // 带纹理的矩形，16x16，半透明，混合纹理
    case 0x7E:
      shape = new Polygon<SquareWithTextureVertices<15>,
                  drawTriStrip, MonoColorTextureMixShader>(0.5);
      break;

    // 带纹理的矩形，16x16，半透明，原始纹理
    case 0x7F:
      shape = new Polygon<SquareWithTextureVertices<15>,
                  drawTriStrip, TextureOnlyShader>(0.5);
      break;

    default:
      error("Invaild GP0 Command %x %x\n", c.cmd, c.v);
      return false;
  }
  return true;
}


u32 GPU::GP0::read() {
  std::lock_guard<std::recursive_mutex> guard(p.for_read_queue);
  if (p.read_queue.size()) {
    IGpuReadData* data_que = p.read_queue.front();
    last_read = data_que->read();

    if (!data_que->has()) {
      delete data_que;
      p.read_queue.pop_front();
    }
  }
  return last_read;
}


void GPU::GP0::write(u32 v) {
  //debug("GP0 Write 0x%08x\n", v);
  switch (stage) {
    case ShapeDataStage::read_command:
      if (!parseCommand(v)) {
        break;
      }
      //printf("command gpu %08x\n", v); ps1e_t::ext_stop = 1;
      stage = ShapeDataStage::read_data;
      // do not break

    case ShapeDataStage::read_data:
      if (!shape->write(v)) {
        p.send(shape);
        shape = NULL;
        stage = ShapeDataStage::read_command;
      }
      break;
  }
}


void GPU::GP0::reset_fifo() {
  stage = ShapeDataStage::read_command;
  if (shape) {
    delete shape;
    shape = NULL;
  }
}


GPU::GP0::GP0(GPU &_p) : p(_p), stage(ShapeDataStage::read_command), shape(0), last_read(0) {
}


}