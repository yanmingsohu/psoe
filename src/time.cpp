#include "time.h"
#include <chrono>

namespace ps1e {

#define MODE1_3(m) (m.cs & 1)
#define MODE0_2(m) ((mode.cs & 1) == 0)
#define MODE2_3(m) (mode.mode & 0x2)
#define MODE0_1(m) ((mode.mode & 0x2) == 0)


Timer::Conter::Conter(Timer* _p) : p(_p) {
}


void Timer::Conter::write(u32 value) {
  p->conter = value;
}


u32 Timer::Conter::read() {
  return p->conter;
}


Timer::Target::Target(Timer* _p) : p(_p) {
}


void Timer::Target::write(u32 value) {
  p->target = value;
}


u32 Timer::Target::read() {
  return p->target;
}


Timer::Mode::Mode(Timer* _p) : p(_p) {
}


void Timer::Mode::write(u32 value) {
  p->mode.v = value | (0x1 << 10);
  p->sendedIrq = false;
  p->pause = false;
  p->onModeWrite();
}


u32 Timer::Mode::read() {
  u32 r = p->mode.v; 
  p->mode.rtv = 0;
  p->mode.rfv = 0;
  return r;
}


Timer::Timer(Bus& _bus) 
: creg(this), treg(this), mreg(this), bus(_bus), pause(0), sendedIrq(0) {
}


void Timer::init() {
  installRegTo(bus);
  irqNum = getIrqNum();
}


void Timer::sendIrq() {
  if (mode.irqR == 0 && sendedIrq) {
    return;
  }
  sendedIrq = true;
  if (mode.irqP) {
    mode.ir = !mode.ir;
  } else {
    mode.ir = 0;
  }
  bus.send_irq(irqNum);
}


void Timer::add() {
  if (pause) return;
  ++conter;

  const bool onTarget = (conter == target);
  const bool onMax = (conter == 0xffff);

  if (onTarget) {
    mode.rtv = 1;
    if (mode.reset) {
      conter = 0;
    }
    if (mode.irqT) {
      sendIrq();
    }
  }

  if (onMax) {
    mode.rfv = 1;
    if (mode.irqF) {
      sendIrq();
    }
  }
}


void Timer::syncMode0_1(bool inside) {
  if (mode.enb) {
    switch (mode.mode) {
      case 0:
        pause = inside;
        break;

      case 1:
        if (inside) conter = 0;
        break;

      case 2:
        if (inside) conter = 0;
        pause = !inside;
        break;

      case 3:
        pause = false;
        mode.enb = 0;
        break;
    }
  }
}

// ----------------------------------------------------- T0

Timer0::Timer0(Bus& bus) : Timer(bus) {
  init();
}


IrqDevMask Timer0::getIrqNum() {
  return IrqDevMask::dotclk;
}


void Timer0::installRegTo(Bus& bus) {
  bus.bind_io(DeviceIOMapper::time0_count_val, &creg);
  bus.bind_io(DeviceIOMapper::time0_mode,      &mreg);
  bus.bind_io(DeviceIOMapper::time0_tar_val,   &treg);
}


void Timer0::hblank(bool inside) {
  syncMode0_1(inside);
}


void Timer0::onModeWrite() {
  if (mode.enb && mode.mode == 3) {
    pause = true;
  }
}


void Timer0::systemClock() {
  if (MODE0_2(mode)) {
    add();
  }
}


void Timer0::dotClock() {
  if (MODE1_3(mode)) {
    add();
  }
}

// ----------------------------------------------------- T1

Timer1::Timer1(Bus& bus) : Timer(bus) {
  init();
}


IrqDevMask Timer1::getIrqNum() {
  return IrqDevMask::hblank;
}


void Timer1::installRegTo(Bus& bus) {
  bus.bind_io(DeviceIOMapper::time1_count_val, &creg);
  bus.bind_io(DeviceIOMapper::time1_mode,      &mreg);
  bus.bind_io(DeviceIOMapper::time1_tar_val,   &treg);
}


void Timer1::vblank(bool inside) {
  syncMode0_1(inside);
}


void Timer1::systemClock() {
  if (MODE0_2(mode)) {
    add();
  }
}


void Timer1::hblank(bool inside) {
  if (inside && MODE1_3(mode)) {
    add();
  }
}


void Timer1::onModeWrite() {
  if (mode.enb && mode.mode == 3) {
    pause = true;
  }
}

// ----------------------------------------------------- T2

Timer2::Timer2(Bus& bus) : Timer(bus) {
  init();
}


IrqDevMask Timer2::getIrqNum() {
  return IrqDevMask::hblank;
}


void Timer2::installRegTo(Bus& bus) {
  bus.bind_io(DeviceIOMapper::time2_count_val, &creg);
  bus.bind_io(DeviceIOMapper::time2_mode,      &mreg);
  bus.bind_io(DeviceIOMapper::time2_tar_val,   &treg);
}


void Timer2::onModeWrite() {
  if (mode.enb) {
    if (mode.mode == 3 || mode.mode == 0) {
      pause = true;
    }
  }
}


void Timer2::systemClock() {
  if (MODE0_1(mode)) {
    add();
  }
}


void Timer2::systemClock8() {
  if (MODE2_3(mode)) {
    add();
  } 
}

// ----------------------------------------------------- Timer System

TimerSystem::TimerSystem(Bus& b) 
: t0(b), t1(b), t2(b), screenWidth(320), systemClock8c(0)
, screenHeight(314), exit(0), work1(&TimerSystem::vblank_thread, this)
, work2(&TimerSystem::system_clock_thread, this) {
}


TimerSystem::~TimerSystem() {
  exit = true;
  wait_vblank.notify_all();
  work1.join();
  //wait_sc.notify_all();
  work2.join();
}


void TimerSystem::hblank(bool inside) {
  t0.hblank(inside);
  t1.hblank(inside);

  if (inside) {
    for (u16 i=0; i<screenWidth; ++i) {
      t0.dotClock();
    }
  }
}


void TimerSystem::systemClock() {
  //wait_sc.notify_all();
}


void TimerSystem::system_clock_thread() {
  //std::unique_lock<std::mutex> lk(for_sc);
  auto time = std::chrono::nanoseconds(295);
  while (!exit) {
    //wait_sc.wait(lk);
    std::this_thread::sleep_for(time);

    t0.systemClock();
    t1.systemClock();
    t2.systemClock();

    if (++systemClock8c & 0xF8) {
      t2.systemClock8();
      systemClock8c = 0;
    }
  }
}


void TimerSystem::vblank(bool inside) {
  t1.vblank(inside);
  if (inside) wait_vblank.notify_all();
}


void TimerSystem::vblank_thread() {
  std::unique_lock<std::mutex> lk(for_vblank);
  while (!exit) {
    wait_vblank.wait(lk);
    //printf("SEND hblank\n");
    for (int i=0; i<screenHeight; ++i) {
      hblank(false);
      hblank(true);
    }
  }
}


}