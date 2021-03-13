#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include "util.h"
#include "io.h"
#include "bus.h"

namespace ps1e {

class Timer;
class TimerTrigger;


union TimerMode {
  u32 v;
  struct {
    u32 enb   : 1; //00    启用同步模式 (0=Free Run, 1=Synchronize via Bit1-2)
    u32 mode  : 2; //01-02 同步模式 (0-3)
    u32 reset : 1; //03    重置计算器到 0000h  (0= FFFFh时重置, 1=到达目标值时重置)
    u32 irqT  : 1; //04    1:到达目标值时发送 irq, 0:禁用
    u32 irqF  : 1; //05    1:到达ffff时发送 irq, 0:禁用
    u32 irqR  : 1; //06    1:重复发送irq, 0:只发送一次
    u32 irqP  : 1; //07    发送irq后 1:开关bit10, 0:Bit10=0
    u32 cs    : 2; //08-09 时钟源
    u32 ir    : 1; //10    Interrupt Request (0=Yes, 1=No) (W=1) (R)
    u32 rtv   : 1; //11    到达了目标值 设置为1 (0=No, 1=Yes) (读取后=0) (R)
    u32 rfv   : 1; //12    到达了FFFF 设置为1  (0=No, 1=Yes) (读取后=0) (R)   
    u32 Un    : 3; //13-15 未知
    u32 Gb    : 16;//16-31 无用
  };
};


class Timer {
private:
  class Conter : public DeviceIO {
    Timer *p;
  public:
    Conter(Timer *_p);
    void write(u32 value);
    u32 read();
  };

  class Target : public DeviceIO {
    Timer *p;
  public:
    Target(Timer *_p);
    void write(u32 value);
    u32 read();
  };

  class Mode : public DeviceIO {
    Timer *p;
  public:
    Mode(Timer *_p);
    void write(u32 value);
    u32 read();
  };

  void sendIrq();

protected:
  u16 conter;
  u16 target;
  TimerMode mode;
  bool sendedIrq;
  bool pause;

  Conter creg;
  Target treg;
  Mode mreg;

  Bus &bus;
  IrqDevMask irqNum;

  virtual IrqDevMask getIrqNum() = 0;
  virtual void installRegTo(Bus &bus) = 0;
  virtual void onModeWrite() = 0;
  // 计时器 +1
  void add();
  void syncMode0_1(bool inside);
  void init();

public:
  Timer(Bus& bus);
  virtual ~Timer() {};
};


class Timer0 : public Timer {
protected:
  IrqDevMask getIrqNum();
  void installRegTo(Bus &bus);
  void onModeWrite();

public:
  Timer0(Bus& bus);
  void hblank(bool inside);
  void systemClock();
  void dotClock();
};


class Timer1 : public Timer {
protected:
  IrqDevMask getIrqNum();
  void installRegTo(Bus &bus);
  void onModeWrite();

public:
  Timer1(Bus& bus);
  void vblank(bool inside);
  void hblank(bool inside);
  void systemClock();
};


class Timer2 : public Timer {
protected:
  IrqDevMask getIrqNum();
  void installRegTo(Bus &bus);
  void onModeWrite();

public:
  Timer2(Bus& bus);
  void systemClock();
  void systemClock8(); //TODO: 8倍 或 1/8
};


class TimerSystem {
private:
  Timer0 t0;
  Timer1 t1;
  Timer2 t2;
  u16 screenWidth;
  u16 screenHeight;
  u8 systemClock8c;
  bool exit;

  std::thread work1;
  std::mutex for_vblank;
  std::condition_variable wait_vblank;

  std::thread work2;
  //std::mutex for_sc;
  //std::condition_variable wait_sc;

  // 模拟 hblank/dotclock
  void hblank(bool inside);
  void vblank_thread();
  void system_clock_thread();

public:
  TimerSystem(Bus& b);
  ~TimerSystem();

  void vblank(bool inside);
  void systemClock();
};

}