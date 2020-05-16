#pragma once 

#include "util.h"

namespace ps1e {

#define CASE_IO_MIRROR(x) CASE_MEM_MIRROR(x)

#define CASE_IO_MIRROR_WRITE(addr, io_enum, io_arr, v) \
    CASE_IO_MIRROR(addr): \
    io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write(v); \
    return;

#define CASE_IO_MIRROR_READ(addr, io_enum, io_arr, _) \
    CASE_IO_MIRROR(addr): \
    return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read();

#define IO_ENUM_DEFINE_FIELDS(_, enumfield, __, ___) \
    enumfield,


//
// 所有挂接在总线上的设备的 io 定义表
// TODO: DMA 设备识别从 bus 移动到这里.
//
#define IO_MIRRORS_STATEMENTS(rw, io_arr, v) \
    rw(0x1F80'1810, gpu_gp0,      io_arr, v) \
    rw(0x1F80'1814, gpu_gp1,      io_arr, v) \
                                             \
    rw(0x1F80'1050, sio_tx_data,  io_arr, v) \
    rw(0x1F80'1054, sio_stat   ,  io_arr, v) \
    rw(0x1F80'1058, sio_mode   ,  io_arr, v) \
    rw(0x1F80'105A, sio_ctrl   ,  io_arr, v) \
    rw(0x1F80'105C, sio_misc   ,  io_arr, v) \
    rw(0x1F80'105E, sio_baud   ,  io_arr, v) \
  

// IO 接口枚举
enum class DeviceIOMapper : size_t {
  __zero__ = 0,   // Keep first and value 0
  IO_MIRRORS_STATEMENTS(IO_ENUM_DEFINE_FIELDS, 0, 0)
  __Length__, // Keep last, Do not Index.
};
const size_t io_map_size = static_cast<size_t>(DeviceIOMapper::__Length__);


class NullReg {
public:
  inline void write(u32 value) {}
  inline u32 read() { return 0xFFFF'FFFF; }
};


class U32Reg {
protected:
  u32 reg;
public:
  inline void write(u32 value) {
    reg = value;
  }
  inline u32 read() {
    return reg;
  }
};


// 设备上的一个 IO 端口, 默认什么都不做
class DeviceIO {
public:
  virtual ~DeviceIO() {}
  virtual void write(u32 value) {}
  virtual u32 read() { return 0xFFFF'FFFF; }
};


// 带有锁存功能的接口
template<class Data = U32Reg>
class DeviceIOLatch : public DeviceIO {
protected:
  Data reg;
public:
  virtual void write(u32 value) {
    reg.write(value);
  }
  virtual u32 read() { 
    return reg.read();
  }
};


// 带有父类引用的抽象基类, 用于内部类
template<class Parent, class Data = NullReg> 
class InnerDeviceIO : public DeviceIO {
protected:
  Parent& parent;
  Data reg;

public:
  InnerDeviceIO(Parent& p) : parent(p) {}
  virtual ~InnerDeviceIO() {}

  virtual void write(u32 value) {
    reg.write(value);
  }

  virtual u32 read() { 
    return reg.read();
  }
};


}