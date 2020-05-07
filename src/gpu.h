#pragma once

#include <list>

#include "util.h"
#include "dma.h"
#include "bus.h"

struct GLFWwindow;

namespace std {
  class thread;
}

namespace ps1e {

class GPU;
class MonoColorPolygonShader;

typedef u32 GLHANDLE;
typedef const char* ShaderSrc;

union GpuStatus {
  u32 v;
  struct {
    u32 tx        : 4; //0-3 纹理页 X = ty * 64 (0~3)
    u32 ty        : 1; //  4 纹理页 Y = ty * 256
    u32 abr       : 2; //5-6 半透明 0{0.5xB+0.5xF}, 1{1.0xB+1.0xF}, 2{1.0xB-1.0xF}, 3{1.0xB+0.25xF}
    u32 tp        : 2; //7-8 纹理页颜色模式 0{4bit CLUT}, 1{8bit CLUT}, 2:{15bit}
    u32 dtd       : 1; //  9 1:开启抖动Dither, 24抖15
    u32 draw      : 1; // 10 1:允许绘图命令绘制显示区域, 0:prohibited
    u32 mask      : 1; // 11 1:绘制时修改蒙板bit (bit15?)
    u32 enb_msk   : 1; // 12 1:启用蒙板, 只能绘制到蒙板区域
    u32 inter_f   : 1; // 13 always 1 when GP1(08h).5=0
    u32 distorted : 1; // 14 GP1(08h).7
    u32 text_off  : 1; // 15 1=Disable Textures

    u32 width1    : 1; // 16 width1=1{width0{0:384?368}}
    u32 width0    : 2; // 17-18 屏幕宽度, width1=0{0:256, 1:320, 2:512, 3:640}
    u32 height    : 1; // 19 屏幕高度, 0:240, 1:480
    u32 video     : 1; // 20 1:PAL, 0:NTSC
    u32 isrgb24   : 1; // 21 1:24bit, 0:15bit
    u32 isinter   : 1; // 22 1:交错开启(隔行扫描)
    u32 display   : 1; // 23 0:开启显示, 1:黑屏
    u32 irq_on    : 1; // 24 1:IRQ
    u32 r         : 1; // 25 dma 状态总开关, 1:dma 就绪
    u32 r_cmd     : 1; // 26 1:可以接受命令来自 gp0
    u32 r_cpu     : 1; // 27 1:可以发送 vram 数据到 cpu
    u32 r_dma     : 1; // 28 1:可以接受 dma 数据块
    u32 dma_md    : 2; // 29-30 DMA 0:关, 1:未知, 2:CPU to GP0, 3:GPU-READ to CPU
    u32 lcf       : 1; // 31 交错时 0:绘制偶数行, 1:绘制奇数行
  };
};


union GpuCommand {
  u32 v;
  struct {
    u32 parm : 24;
    u32 cmd  : 8;
  };
  GpuCommand(u32 _v) : v(_v) {}
};


struct GpuDataRange {
  u16 width;
  u16 height;

  GpuDataRange(u32 w, u32 h);
};


enum class GpuGp1CommandDef {
  rst_gpu    = 0x00, // reset gpu, sets status to $14802000
  rst_buffer = 0x01, // reset command buffer
  rst_irq    = 0x02, // reset IRQ
  display    = 0x03, // Turn on(1)/off display
  dma        = 0x04, // DMA 0:off, 1:unknow, 2:C to G, 3 G toC
  startxy    = 0x05, // 屏幕左上角的内存区域
  setwidth   = 0x06,
  setheight  = 0x07,
  setmode    = 0x08,
  info       = 0x10,
};


enum class ShapeDataStage {
  read_command,
  read_data,
};


class IDrawShape {
public:
  virtual ~IDrawShape() {}
  // 写入命令数据(包含第一次的命令数据), 如果改形状已经读取全部数据则返回 false
  virtual bool write(const u32 c) = 0;
  // 一旦数据全部读取, 则构建 opengl 缓冲区用于绘制
  virtual void build(GPU&) = 0;
  // 绘制图像
  virtual void draw(GPU&) = 0;
};


// 首先声明该对象, 并在生命周期内使用 opengl 函数.
class OpenGLScope {
private:
  OpenGLScope(OpenGLScope&);
  OpenGLScope& operator=(OpenGLScope&);
  
public:
  OpenGLScope();
  ~OpenGLScope();
};


class OpenGLShader {
private:
  GLHANDLE program;
  GLHANDLE createShader(ShaderSrc src, u32 shader_flag);
  int getUniform(const char* name);

public:
  OpenGLShader(ShaderSrc vertex, ShaderSrc fragment);
  virtual ~OpenGLShader();

  void use();
  void setUint(const char* name, u32 v);
  void setFloat(const char* name, float v);
};


class GPU : public DMADev {
private:
  class GP0 : public DeviceIO {
    GPU &p;
    ShapeDataStage stage;
    IDrawShape *shape;
  public:
    GP0(GPU &_p) : p(_p), stage(ShapeDataStage::read_command), shape(0) {}
    bool parseCommand(const GpuCommand c);
    void write(u32 value);
    u32 read();
  };


  class GP1 : public DeviceIO {
    GPU &p;
  public:
    GP1(GPU &_p) : p(_p) {}
    void write(u32 value);
    u32 read();
  };

private:
  GP0 gp0;
  GP1 gp1;
  u32 cmd_respons;
  GpuStatus status;
  GLFWwindow* glwindow;
  std::thread* work;
  GpuDataRange screen;
  GpuDataRange ps;
  std::list<IDrawShape*> build;
  std::list<IDrawShape*> shapes;

  // 这是gpu线程函数, 不要调用
  void gpu_thread();
  void initOpenGL();

public:
  GPU(Bus& bus);
  ~GPU();

  // 发送可绘制图形
  void send(IDrawShape* s) {
    build.push_back(s);
  }

  virtual DmaDeviceNum number() {
    return DmaDeviceNum::gpu;
  }

  virtual bool support(dma_chcr_dir dir) {
    //TODO: 做完gpu寄存器
    return false;
  }

  // 返回已经缓冲的着色器程序
  template<class Shader> OpenGLShader* getProgram() {
    static Shader instance;
    return &instance;
  }

  GpuDataRange* screen_range() {
    return &screen;
  }
};

}