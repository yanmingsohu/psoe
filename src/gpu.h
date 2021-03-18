#pragma once

#include <list>
#include <mutex>

#include "util.h"
#include "dma.h"
#include "bus.h"
#include "opengl-wrap.h"
#include "otc.h"
#include "time.h"

struct GLFWwindow;

namespace std {
  class thread;
}

namespace ps1e {

class GPU;
class MonoColorShader;
class VirtualScreenShader;


union GpuStatus {
  u32 v;
  struct {
    u32 tx        : 4; //0-3 纹理页 X = ty * 64 (0~3)
    u32 ty        : 1; //  4 纹理页 Y = ty * 256
    u32 abr       : 2; //5-6 半透明 0{0.5xB+0.5xF}, 1{1.0xB+1.0xF}, 2{1.0xB-1.0xF}, 3{1.0xB+0.25xF}
    u32 tp        : 2; //7-8 纹理页颜色模式 0{4bit CLUT}, 1{8bit CLUT}, 2:{15bit}
    u32 dtd       : 1; //  9 1:开启抖动Dither, 24抖15
    u32 draw      : 1; // 10 1:允许绘图命令绘制显示区域, 0:prohibited
    u32 mask      : 1; // 11 1:绘制时强制修改缓冲区 bit15 = 1; 0:等于纹理的bit15, 非纹理=0
    u32 enb_msk   : 1; // 12 1:帧缓冲区中bit15 = 1的任何（旧）像素均受写保护; 0:允许绘制
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
    u32 dma_req   : 1; // 25 与 dma_md 得状态有关
    u32 r_cmd     : 1; // 26 1:可以接受命令来自 gp0
    u32 r_cpu     : 1; // 27 1:可以发送 vram 数据到 cpu
    u32 r_dma     : 1; // 28 1:可以接受 dma 数据块
    u32 dma_md    : 2; // 29-30 DMA 0:关, 1:未知, 2:CPU to GP0, 3:GPU-READ to CPU
    u32 lcf       : 1; // 31 交错时 0:绘制偶数行, 1:绘制奇数行
  };

  u32 texturePage() { 
    return (tx * 64) | ((ty * 256) << 16); 
  }
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
  u32 offx, offy, width, height;
};


union GpuRange10 {
  u32 v;
  struct {
    u32 x : 10;
    u32 y : 10;
    u32 _ : 12;
  };
};


union GpuRange12 {
  u32 v;
  struct {
    u32 x : 12;
    u32 y : 12;
    u32 _ : 8;
  };
};


union GpuTextRange {
  u32 v;
  struct {
    u32 mask_x : 5;
    u32 mask_y : 5;
    u32 off_x  : 5;
    u32 off_y  : 5;
    u32 _ : 12;
  };
};


union GpuTextFlip {
  u32 v;
  struct {
    u32 _0 : 20;
    u32 x  : 1;
    u32 y  : 1;
    u32 _1 : 10;
  };
};

u32 textureAttr(GpuStatus& st, GpuTextFlip& flip);


union GpuDrawOffset {
  u32 v;
  struct {
    u32 x  : 10;
    u32 sx : 1;
    u32 y  : 10;
    u32 sy : 1;
    u32 _  : 10;
  };

  int offx() { return sx ? int(~x)+1 : (x); }
  int offy() { return sy ? int(~y)+1 : (y); }
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
protected:
  GLVerticesBuffer vbo;

public:
  virtual ~IDrawShape() {}
  // 写入命令数据(包含第一次的命令数据), 如果形状已经读取全部数据则返回 false
  virtual bool write(const u32 c) = 0;
  // 绘制图像
  virtual void draw(GPU&, GLVertexArrays& vao) = 0;
};


class IGpuReadData {
public:
  virtual ~IGpuReadData() {};
  // 读取下一个数据
  virtual u32 read() = 0;
  // 一旦返回 false 表示当前对象没有更多数据读取, 则立即删除当前对象
  virtual bool has() = 0;
};


// 所有图形都绘制到虚拟缓冲区, 然后再绘制到物理屏幕上
class VirtualFrameBuffer {
public:
  static const u32 Width  = 1024;
  static const u32 Height = 512;

private:
  GLVertexArrays vao;
  GLVerticesBuffer vbo;
  GLFrameBuffer frame_buffer;
  GLTexture virtual_screen;
  GLRenderBuffer rbo;
  VirtualScreenShader* shader;
  GLDrawState ds;
  GpuDataRange gsize;
  int multiple;

public:
  VirtualFrameBuffer(int _multiple =1);
  ~VirtualFrameBuffer();
  void init(GpuDataRange& screen);
  // 设置显示范围和屏幕分辨率
  void setSize(GpuDataRange& screen, GpuDataRange& scope);
  void drawShape();
  void drawScreen();
  GpuDataRange& size();
  GLTexture* useTexture();
};


class GPU : public DMADev, public NonCopy {
private:
  class GP0 : public DeviceIO {
    GPU &p;
    ShapeDataStage stage;
    IDrawShape *shape;
    u32 last_read;
  public:
    GP0(GPU &_p);
    bool parseCommand(const GpuCommand c);
    void write(u32 value);
    u32 read();
    void reset_fifo();
  };


  class GP1 : public DeviceIO {
    GPU &p;
  public:
    GP1(GPU &_p);
    void parseCommand(const GpuCommand c);
    void write(u32 value);
    u32 read();
  };

public:
  GpuDataRange  screen;      // 物理屏幕尺寸
  GpuRange10    display;     // 显存中映射到屏幕的起始位置
  GpuRange12    disp_hori;   // 显示的水平范围
  GpuRange10    disp_veri;   // 显示的垂直范围
  GpuDataRange  frame;       // 整个显存
  GpuTextRange  text_win;    // 纹理窗口
  GpuDrawOffset draw_offset; // 绘图时的偏移
  GpuRange10    draw_tp_lf;  // 绘图左上角
  GpuRange10    draw_bm_rt;  // 绘图右下角
  GpuTextFlip   text_flip;   // 纹理坐标 x,y 递增方向
  GpuStatus     status;

  // 解决线程争用, 从 GpuStatus 中分离
  u8 s_r_dma; // cpu to gpu
  u8 s_r_cpu; // gpu to cpu
  u8 s_irq;

private:
  GP0 gp0;
  GP1 gp1;
  u32 cmd_respons;
  GLFWwindow* glwindow;
  std::thread* work;

  VirtualFrameBuffer vram;
  std::list<IDrawShape*> draw_queue;
  std::recursive_mutex for_draw_queue;
  // 从插入的对象中读取数据, 只要对象存在必须至少能读取一次
  std::list<IGpuReadData*> read_queue;
  std::recursive_mutex for_read_queue;
  GLDrawState ds;
  u32 status_change_count;
  TimerSystem& timer;
    
  // 这是gpu线程函数, 不要调用
  void gpu_thread();
  void initOpenGL();

  // 在修改 draw_tp_lf/draw_bm_rt 后应用绘制范围
  void updateDrawScope();

protected:
  // 通常用于传输纹理, 很少用于命令
  void dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc) override;
  // 按照链表顺序加载绘制的命令
  void dma_order_list(psmem addr) override;
  void dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) override;

public:
  GPU(Bus& bus, TimerSystem&);
  ~GPU();

  void reset();

  // 发送可绘制图形
  void send(IDrawShape* s);

  // 弹出待绘制图形对象, 没有返回 NULL
  IDrawShape* pop_drawer();

  // 将一个数据读取器插入队列, 稍后由总线读出.
  void add(IGpuReadData* r);

  virtual DmaDeviceNum number() {
    return DmaDeviceNum::gpu;
  }

  // 返回已经缓冲的着色器程序
  template<class Shader> Shader* useProgram() {
    static Shader instance;
    instance.use();
    instance.update(status_change_count, *this);
    return &instance;
  }

  inline GpuDataRange* screen_range() {
    return &screen;
  }

  inline GPU& dirtyAttr() {
    status_change_count++;
    return *this;
  }

  // 返回 ps 显存纹理对象, 通常用于将 ps 显存绑定到当前纹理
  inline GLTexture* useTexture() {
    return vram.useTexture();
  }

  // 启用/禁用绘制区域限制, 默认限制总是启用的, 
  void enableDrawScope(bool enableLimit);
};

}