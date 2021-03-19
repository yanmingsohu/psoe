#pragma once 

#include "util.h"
#include "io.h"

namespace ps1e {

// dma0 : MDEC in
// dma1 : MDEC out
// dma2 : GPU
// dma3 : CD-ROM
// dma4 : SPU
// dma5 : PIO
// dma6 : GPU OTC

class MMU;
class Bus;
class DeviceIO;


enum class DmaDeviceNum : u32 {
  MDECin  = 0,
  MDECout = 1,
  gpu     = 2,
  cdrom   = 3,
  spu     = 4,
  pio     = 5,
  otc     = 6,
};

DmaDeviceNum convertToDmaNumber(DeviceIOMapper s);


enum class ChcrMode : u32 {
  Manual     = 0, // 单块传输
  Stream     = 1, // 传输连续的数据流
  LinkedList = 2, // 传输链表, GPU only
  Reserved   = 3,
};


//  0 1 1 1 0 0 0  1 0 1 1 1 0 1 1  1 0 0 0 0 0 1 1  1 0 0 0 0 0 0 1 1 : Can Write
// 31 - - - - - - 24 - - - - - - - 16 - - - - - - -  8 - - - 4 - - - 0
//        T       B    <cw >   < dw >           <m>  C             S Dir
//        rigger  usy                            ode hoppin g      Tep
//        28{1    24{1 22{3    18{3             10{2 5{1           1{1
union DMAChcr {
  u32 v;
  struct {
    u32 dir       : 1; // 0:从内存到设备, 1:设备到内存
    u32 step      : 1; // 1:每次地址-4, 0:每次地址+4
    u32        _3 : 6; // 
    u32 chopping  : 1; // 启用时间窗口 (dma/cpu交替运行)
    ChcrMode mode : 2; //
    u32        _4 : 5; // 
    u32 dma_wsize : 3; // dma 时间窗口
    u32        _7 : 1; // 
    u32 cpu_wsize : 3; // cpu 时间窗口
    u32        _8 : 1; // 
    u32 start     : 1; // 1: 忙碌, DMA 传输数据开始后置1
    u32        _6 : 3; // 
    u32 trigger   : 1; // 1: 触发传输, 传输开始后置0
    u32        _5 : 3; // 
  };
};


union DMAIrq {
  u32 v;
  struct {
    u32 _0 : 15; //0-14
    u32 force : 1; // 15 bit

    u32 d0_enable : 1; //16 MDECin
    u32 d1_enable : 1; //17 MDECout
    u32 d2_enable : 1; //18 GPU
    u32 d3_enable : 1; //19 CDROM
    u32 d4_enable : 1; //20 SPU
    u32 d5_enable : 1; //21 PIO
    u32 d6_enable : 1; //22 OTC
    u32 master_enable : 1; // 23bit

    u32 d0_flag : 1; //24
    u32 d1_flag : 1; //25
    u32 d2_flag : 1; //26
    u32 d3_flag : 1; //27
    u32 d4_flag : 1; //28
    u32 d5_flag : 1; //29
    u32 d6_flag : 1; //30
    u32 master_flag : 1; //31
  };

  struct{
    u32 _0        : 15;
    u32 _force    : 1;
    u32 dd_enable : 7;
    u32 _mst_enb  : 1;
    u32 dd_flag   : 7;
    u32 _mst_flag : 1;
  };
};


union DMADpcr {
  u32 v;
  struct {
    u32 d0_pri : 3;
    u32 d0_enb : 1;
    u32 d1_pri : 3;
    u32 d1_enb : 1;
    u32 d2_pri : 3;
    u32 d2_enb : 1;
    u32 d3_pri : 3;
    u32 d3_enb : 1;
    u32 d4_pri : 3;
    u32 d4_enb : 1;
    u32 d5_pri : 3;
    u32 d5_enb : 1;
    u32 d6_pri : 3;
    u32 d6_enb : 1;
    u32 offset : 3;
    u32 _0     : 1;
  };
};


enum class dma_chcr_dir : u32 {
  DEV_TO_RAM = 0,  RAM_FROM_DEV = 0,
  RAM_TO_DEV = 1,  DEV_FROM_RAM = 1,
};


class DMADev {
private:
  class RegBase : public DeviceIO {
  public:
    u32 base; // 目标内存基址
    DMADev* parent;

    RegBase(DMADev* p) : parent(p), base(0) {}
    void write(u32 value);
    u32 read();
  };

  class RegBlock : public DeviceIO {
  public:
    u32 blocks = 0;    // stream 模式数据包数量
    u32 blocksize = 0; // 一个数据包 u32 的数量
    DMADev* parent;

    RegBlock(DMADev* p) : parent(p) {}
    void write(u32 value);
    u32 read();
  };

  class RegCtrl : public DeviceIO {
  public:
    DMAChcr chcr;
    Bus& bus;
    DMADev* parent;

    RegCtrl(Bus& b, DMADev* p) : bus(b), parent(p), chcr{0} {}
    void write(u32 value);
    u32 read();
  };

  const DmaDeviceNum devnum;
  u32 _mask;

protected:
  
  Bus& bus;
  RegBase base_io;
  RegBlock blocks_io;
  RegCtrl ctrl_io;
  //TODO: dma 设备进入休眠窗口
  int idle;
  bool is_transferring;

  // DMA 传输过程, 由设备调用该方法开始 DMA 传输,
  // 应该在单独的线程中执行
  void transport();

  // 子类实现内存到设备传输
  virtual void dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc);
  // 子类实现设备到内存传输
  virtual void dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc);
  // 子类实现 otc 传输
  virtual void dma_order_list(psmem addr);

public:
  DMADev(Bus& _bus, DeviceIOMapper dma_x_base);
  virtual ~DMADev() {};

  // 停止 DMA 传输
  //void stop();

  // 使 DMA 启动传输, 由cpu线程启动
  void start();

  // 可以启动 dma 传输返回 true
  bool readyTransfer();

  inline u32 mask() {
    return _mask;
  }

  // 返回设备号
  inline DmaDeviceNum number() {
    return devnum;
  }
};

}