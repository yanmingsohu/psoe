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

#define IO_SPU_CHANNEL(rw, a, v, n) \
    rw(0x1F801C00 + (0x10 * n), spu_c_volume_ ## n      , a, v) \
    rw(0x1F801C04 + (0x10 * n), spu_c_samp_rate_ ## n   , a, v) \
    rw(0x1F801C06 + (0x10 * n), spu_c_start_addr_ ## n  , a, v) \
    rw(0x1F801C08 + (0x10 * n), spu_c_adsr_ ## n        , a, v) \
    rw(0x1F801C0C + (0x10 * n), spu_c_adsr_vol_ ## n    , a, v) \
    rw(0x1F801C0E + (0x10 * n), spu_c_repeat_addr_ ## n , a, v) \
    rw(0x1F801E00 + (0x04 * n), spu_c_curr_vol_ ## n    , a, v) \

//
// 所有挂接在总线上的设备的 io 定义表
// TODO: DMA 设备识别从 bus 移动到这里.
//
#define IO_MIRRORS_STATEMENTS(rw, a, v) \
    rw(0x1F80'1810, gpu_gp0             , a, v) \
    rw(0x1F80'1814, gpu_gp1             , a, v) \
                                                \
    rw(0x1F80'1050, sio_tx_data         , a, v) \
    rw(0x1F80'1054, sio_stat            , a, v) \
    rw(0x1F80'1058, sio_mode            , a, v) \
    rw(0x1F80'105A, sio_ctrl            , a, v) \
    rw(0x1F80'105C, sio_misc            , a, v) \
    rw(0x1F80'105E, sio_baud            , a, v) \
                                                \
    rw(0x1F80'1D80, spu_main_vol        , a, v) \
    rw(0x1F80'1D84, spu_reverb_vol      , a, v) \
    rw(0x1F80'1D88, spu_n_key_on        , a, v) \
    rw(0x1F80'1D8C, spu_n_key_off       , a, v) \
    rw(0x1F80'1D90, spu_n_fm            , a, v) \
    rw(0x1F80'1D94, spu_n_noise         , a, v) \
    rw(0x1F80'1D98, spu_n_reverb        , a, v) \
    rw(0x1F80'1D9C, spu_n_status        , a, v) \
    rw(0x1F80'1DA0, spu_unknow1         , a, v) \
    rw(0x1F80'1DA2, spu_ram_rev_addr    , a, v) \
    rw(0x1F80'1DA4, spu_ram_irq         , a, v) \
    rw(0x1F80'1DA6, spu_ram_trans_addr  , a, v) \
    rw(0x1F80'1DA8, spu_ram_trans_fifo  , a, v) \
    rw(0x1F80'1DAA, spu_ctrl            , a, v) \
    rw(0x1F80'1DAC, spu_ram_trans_ctrl  , a, v) \
    rw(0x1F80'1DAE, spu_status          , a, v) \
    rw(0x1F80'1DB0, spu_cd_vol          , a, v) \
    rw(0x1F80'1DB4, spu_ext_vol         , a, v) \
    rw(0x1F80'1DB8, spu_curr_main_vol   , a, v) \
    rw(0x1F80'1DBC, spu_unknow2         , a, v) \
                                                \
    rw(0x1F80'1DC0, spu_rb_apf_off1     , a, v) \
    rw(0x1F80'1DC2, spu_rb_apf_off2     , a, v) \
    rw(0x1F80'1DC4, spu_rb_ref_vol1     , a, v) \
    rw(0x1F80'1DC6, spu_rb_comb_vol1    , a, v) \
    rw(0x1F80'1DC8, spu_rb_comb_vol2    , a, v) \
    rw(0x1F80'1DCA, spu_rb_comb_vol3    , a, v) \
    rw(0x1F80'1DCC, spu_rb_comb_vol4    , a, v) \
    rw(0x1F80'1DCE, spu_rb_ref_vol2     , a, v) \
    rw(0x1F80'1DD0, spu_rb_apf_vol1     , a, v) \
    rw(0x1F80'1DD2, spu_rb_apf_vol2     , a, v) \
    rw(0x1F80'1DD4, spu_rb_same_ref1    , a, v) \
    rw(0x1F80'1DD8, spu_rb_comb1        , a, v) \
    rw(0x1F80'1DDC, spu_rb_comb2        , a, v) \
    rw(0x1F80'1DE0, spu_rb_same_ref2    , a, v) \
    rw(0x1F80'1DE4, spu_rb_diff_ref1    , a, v) \
    rw(0x1F80'1DE8, spu_rb_comb3        , a, v) \
    rw(0x1F80'1DEC, spu_rb_comb4        , a, v) \
    rw(0x1F80'1DF0, spu_rb_diff_ref2    , a, v) \
    rw(0x1F80'1DF4, spu_rb_apf_addr1    , a, v) \
    rw(0x1F80'1DF8, spu_rb_apf_addr2    , a, v) \
    rw(0x1F80'1DFC, spu_rb_in_vol       , a, v) \
                                                \
    IO_SPU_CHANNEL(rw, a, v,  0)                \
    IO_SPU_CHANNEL(rw, a, v,  1)                \
    IO_SPU_CHANNEL(rw, a, v,  2)                \
    IO_SPU_CHANNEL(rw, a, v,  3)                \
    IO_SPU_CHANNEL(rw, a, v,  4)                \
    IO_SPU_CHANNEL(rw, a, v,  5)                \
    IO_SPU_CHANNEL(rw, a, v,  6)                \
    IO_SPU_CHANNEL(rw, a, v,  7)                \
    IO_SPU_CHANNEL(rw, a, v,  8)                \
    IO_SPU_CHANNEL(rw, a, v,  9)                \
    IO_SPU_CHANNEL(rw, a, v, 10)                \
    IO_SPU_CHANNEL(rw, a, v, 11)                \
    IO_SPU_CHANNEL(rw, a, v, 12)                \
    IO_SPU_CHANNEL(rw, a, v, 13)                \
    IO_SPU_CHANNEL(rw, a, v, 14)                \
    IO_SPU_CHANNEL(rw, a, v, 15)                \
    IO_SPU_CHANNEL(rw, a, v, 16)                \
    IO_SPU_CHANNEL(rw, a, v, 17)                \
    IO_SPU_CHANNEL(rw, a, v, 18)                \
    IO_SPU_CHANNEL(rw, a, v, 19)                \
    IO_SPU_CHANNEL(rw, a, v, 20)                \
    IO_SPU_CHANNEL(rw, a, v, 21)                \
    IO_SPU_CHANNEL(rw, a, v, 22)                \
    IO_SPU_CHANNEL(rw, a, v, 23)                \
  

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