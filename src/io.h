#pragma once 

#include "util.h"

namespace ps1e {

#define CASE_IO_MIRROR(x) CASE_MEM_MIRROR(x)

#define CASE_IO_MIRROR_WRITE(addr, io_enum, io_arr, v, wide) \
    CASE_IO_MIRROR_WRITE_##wide(addr, io_enum, io_arr, v)

#define CASE_IO_MIRROR_WRITE_1(addr, io_enum, io_arr, v) \
    CASE_IO_MIRROR(addr): \
      io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write(v); \
      return;

#define CASE_IO_MIRROR_WRITE_2(addr, io_enum, io_arr, v) \
    CASE_IO_MIRROR(addr): \
      io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write(v); \
      return; \
    CASE_IO_MIRROR(addr+1): \
      io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write1(v); \
      return; \

#define CASE_IO_MIRROR_WRITE_4(addr, io_enum, io_arr, v) \
    CASE_IO_MIRROR(addr): \
      io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write(v); \
      return; \
    CASE_IO_MIRROR(addr+1): \
      io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write1(v); \
      return; \
    CASE_IO_MIRROR(addr+2): \
      io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write2(v); \
      return; \
    CASE_IO_MIRROR(addr+3): \
      io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->write3(v); \
      return; \


#define CASE_IO_MIRROR_READ(addr, io_enum, io_arr, _, wide) \
    CASE_IO_MIRROR_READ_##wide(addr, io_enum, io_arr);

#define CASE_IO_MIRROR_READ_1(addr, io_enum, io_arr) \
    CASE_IO_MIRROR(addr): \
      return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read();

#define CASE_IO_MIRROR_READ_2(addr, io_enum, io_arr) \
    CASE_IO_MIRROR(addr): \
      return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read(); \
    CASE_IO_MIRROR(addr+1): \
      return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read1(); \

#define CASE_IO_MIRROR_READ_4(addr, io_enum, io_arr) \
    CASE_IO_MIRROR(addr): \
      return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read(); \
    CASE_IO_MIRROR(addr+1): \
      return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read1(); \
    CASE_IO_MIRROR(addr+2): \
      return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read2(); \
    CASE_IO_MIRROR(addr+3): \
      return io_arr[ static_cast<size_t>(DeviceIOMapper::io_enum) ]->read3(); \


#define IO_ENUM_DEFINE_FIELDS(_, enumfield, __, ___, ____) \
    enumfield,

#define IO_SPU_CHANNEL(rw, a, v, n) \
    rw(0x1F801C00 + (0x10 * n), spu_c_volume_ ## n      , a, v, 4) \
    rw(0x1F801C04 + (0x10 * n), spu_c_samp_rate_ ## n   , a, v, 2) \
    rw(0x1F801C06 + (0x10 * n), spu_c_start_addr_ ## n  , a, v, 2) \
    rw(0x1F801C08 + (0x10 * n), spu_c_adsr_ ## n        , a, v, 4) \
    rw(0x1F801C0C + (0x10 * n), spu_c_adsr_vol_ ## n    , a, v, 2) \
    rw(0x1F801C0E + (0x10 * n), spu_c_repeat_addr_ ## n , a, v, 2) \
    rw(0x1F801E00 + (0x04 * n), spu_c_curr_vol_ ## n    , a, v, 4) \

//
// 所有挂接在总线上的设备的 io 定义表
// (rw: 方法, a: io数组, v: 数据参数, w: 位宽(字节))
//
#define IO_MIRRORS_STATEMENTS(rw, a, v) \
    rw(0x1F80'1040, joy_data            , a, v, 4) \
    rw(0x1F80'1044, joy_stat            , a, v, 4) \
    rw(0x1F80'1048, joy_mode            , a, v, 2) \
    rw(0x1F80'104A, joy_ctrl            , a, v, 2) \
    rw(0x1F80'104E, joy_baud            , a, v, 2) \
                                                   \
    rw(0x1F80'1050, sio_tx_data         , a, v, 4) \
    rw(0x1F80'1054, sio_stat            , a, v, 4) \
    rw(0x1F80'1058, sio_mode            , a, v, 2) \
    rw(0x1F80'105A, sio_ctrl            , a, v, 2) \
    rw(0x1F80'105C, sio_misc            , a, v, 2) \
    rw(0x1F80'105E, sio_baud            , a, v, 2) \
                                                   \
    rw(0x1F80'1080, dma_mdec_in_base    , a, v, 4) \
    rw(0x1F80'1084, dma_mdec_in_blk     , a, v, 4) \
    rw(0x1F80'1088, dma_mdec_in_ctrl    , a, v, 4) \
    rw(0x1F80'1090, dma_mdec_out_base   , a, v, 4) \
    rw(0x1F80'1094, dma_mdec_out_blk    , a, v, 4) \
    rw(0x1F80'1098, dma_mdec_out_ctrl   , a, v, 4) \
    rw(0x1F80'10A0, dma_gpu_base        , a, v, 4) \
    rw(0x1F80'10A4, dma_gpu_blk         , a, v, 4) \
    rw(0x1F80'10A8, dma_gpu_ctrl        , a, v, 4) \
    rw(0x1F80'10B0, dma_cdrom_base      , a, v, 4) \
    rw(0x1F80'10B4, dma_cdrom_blk       , a, v, 4) \
    rw(0x1F80'10B8, dma_cdrom_ctrl      , a, v, 4) \
    rw(0x1F80'10C0, dma_spu_base        , a, v, 4) \
    rw(0x1F80'10C4, dma_spu_blk         , a, v, 4) \
    rw(0x1F80'10C8, dma_spu_ctrl        , a, v, 4) \
    rw(0x1F80'10D0, dma_pio_base        , a, v, 4) \
    rw(0x1F80'10D4, dma_pio_blk         , a, v, 4) \
    rw(0x1F80'10D8, dma_pio_ctrl        , a, v, 4) \
    rw(0x1F80'10E0, dma_otc_base        , a, v, 4) \
    rw(0x1F80'10E4, dma_otc_blk         , a, v, 4) \
    rw(0x1F80'10E8, dma_otc_ctrl        , a, v, 4) \
                                                   \
    rw(0x1F80'1100, time0_count_val     , a, v, 4) \
    rw(0x1F80'1104, time0_mode          , a, v, 4) \
    rw(0x1F80'1108, time0_tar_val       , a, v, 4) \
    rw(0x1F80'1110, time1_count_val     , a, v, 4) \
    rw(0x1F80'1114, time1_mode          , a, v, 4) \
    rw(0x1F80'1118, time1_tar_val       , a, v, 4) \
    rw(0x1F80'1120, time2_count_val     , a, v, 4) \
    rw(0x1F80'1124, time2_mode          , a, v, 4) \
    rw(0x1F80'1128, time2_tar_val       , a, v, 4) \
                                                   \
    rw(0x1F80'1800, cd_status_index     , a, v, 1) \
    rw(0x1F80'1801, cd_resp_fifo_cmd    , a, v, 1) \
    rw(0x1F80'1802, cd_data_fifo_parm   , a, v, 1) \
    rw(0x1F80'1803, cd_irq_vol          , a, v, 1) \
                                                   \
    rw(0x1F80'1810, gpu_gp0             , a, v, 4) \
    rw(0x1F80'1814, gpu_gp1             , a, v, 4) \
                                                   \
    rw(0x1F80'1820, mdec_cmd_data_parm  , a, v, 4) \
    rw(0x1F80'1824, mdec_ctrl_status    , a, v, 4) \
                                                   \
    rw(0x1F80'1D80, spu_main_vol        , a, v, 4) \
    rw(0x1F80'1D84, spu_reverb_vol      , a, v, 4) \
    rw(0x1F80'1D88, spu_n_key_on        , a, v, 4) \
    rw(0x1F80'1D8C, spu_n_key_off       , a, v, 4) \
    rw(0x1F80'1D90, spu_n_fm            , a, v, 4) \
    rw(0x1F80'1D94, spu_n_noise         , a, v, 4) \
    rw(0x1F80'1D98, spu_n_reverb        , a, v, 4) \
    rw(0x1F80'1D9C, spu_n_status        , a, v, 4) \
    rw(0x1F80'1DA0, spu_unknow1         , a, v, 2) \
    rw(0x1F80'1DA2, spu_ram_rev_addr    , a, v, 2) \
    rw(0x1F80'1DA4, spu_ram_irq         , a, v, 2) \
    rw(0x1F80'1DA6, spu_ram_trans_addr  , a, v, 2) \
    rw(0x1F80'1DA8, spu_ram_trans_fifo  , a, v, 2) \
    rw(0x1F80'1DAA, spu_ctrl            , a, v, 2) \
    rw(0x1F80'1DAC, spu_ram_trans_ctrl  , a, v, 2) \
    rw(0x1F80'1DAE, spu_status          , a, v, 2) \
    rw(0x1F80'1DB0, spu_cd_vol          , a, v, 4) \
    rw(0x1F80'1DB4, spu_ext_vol         , a, v, 4) \
    rw(0x1F80'1DB8, spu_curr_main_vol   , a, v, 4) \
    rw(0x1F80'1DBC, spu_unknow2         , a, v, 4) \
                                                   \
    rw(0x1F80'1DC0, spu_rb_apf_off1     , a, v, 2) \
    rw(0x1F80'1DC2, spu_rb_apf_off2     , a, v, 2) \
    rw(0x1F80'1DC4, spu_rb_ref_vol1     , a, v, 2) \
    rw(0x1F80'1DC6, spu_rb_comb_vol1    , a, v, 2) \
    rw(0x1F80'1DC8, spu_rb_comb_vol2    , a, v, 2) \
    rw(0x1F80'1DCA, spu_rb_comb_vol3    , a, v, 2) \
    rw(0x1F80'1DCC, spu_rb_comb_vol4    , a, v, 2) \
    rw(0x1F80'1DCE, spu_rb_ref_vol2     , a, v, 2) \
    rw(0x1F80'1DD0, spu_rb_apf_vol1     , a, v, 2) \
    rw(0x1F80'1DD2, spu_rb_apf_vol2     , a, v, 2) \
    rw(0x1F80'1DD4, spu_rb_same_ref1    , a, v, 4) \
    rw(0x1F80'1DD8, spu_rb_comb1        , a, v, 4) \
    rw(0x1F80'1DDC, spu_rb_comb2        , a, v, 4) \
    rw(0x1F80'1DE0, spu_rb_same_ref2    , a, v, 4) \
    rw(0x1F80'1DE4, spu_rb_diff_ref1    , a, v, 4) \
    rw(0x1F80'1DE8, spu_rb_comb3        , a, v, 4) \
    rw(0x1F80'1DEC, spu_rb_comb4        , a, v, 4) \
    rw(0x1F80'1DF0, spu_rb_diff_ref2    , a, v, 4) \
    rw(0x1F80'1DF4, spu_rb_apf_addr1    , a, v, 4) \
    rw(0x1F80'1DF8, spu_rb_apf_addr2    , a, v, 4) \
    rw(0x1F80'1DFC, spu_rb_in_vol       , a, v, 4) \
                                                   \
    IO_SPU_CHANNEL(rw, a, v,  0)                   \
    IO_SPU_CHANNEL(rw, a, v,  1)                   \
    IO_SPU_CHANNEL(rw, a, v,  2)                   \
    IO_SPU_CHANNEL(rw, a, v,  3)                   \
    IO_SPU_CHANNEL(rw, a, v,  4)                   \
    IO_SPU_CHANNEL(rw, a, v,  5)                   \
    IO_SPU_CHANNEL(rw, a, v,  6)                   \
    IO_SPU_CHANNEL(rw, a, v,  7)                   \
    IO_SPU_CHANNEL(rw, a, v,  8)                   \
    IO_SPU_CHANNEL(rw, a, v,  9)                   \
    IO_SPU_CHANNEL(rw, a, v, 10)                   \
    IO_SPU_CHANNEL(rw, a, v, 11)                   \
    IO_SPU_CHANNEL(rw, a, v, 12)                   \
    IO_SPU_CHANNEL(rw, a, v, 13)                   \
    IO_SPU_CHANNEL(rw, a, v, 14)                   \
    IO_SPU_CHANNEL(rw, a, v, 15)                   \
    IO_SPU_CHANNEL(rw, a, v, 16)                   \
    IO_SPU_CHANNEL(rw, a, v, 17)                   \
    IO_SPU_CHANNEL(rw, a, v, 18)                   \
    IO_SPU_CHANNEL(rw, a, v, 19)                   \
    IO_SPU_CHANNEL(rw, a, v, 20)                   \
    IO_SPU_CHANNEL(rw, a, v, 21)                   \
    IO_SPU_CHANNEL(rw, a, v, 22)                   \
    IO_SPU_CHANNEL(rw, a, v, 23)                   \
  

// IO 接口枚举
enum class DeviceIOMapper : size_t {
  __zero__ = 0,   // Keep first and value 0
  IO_MIRRORS_STATEMENTS(IO_ENUM_DEFINE_FIELDS, 0, 0)
  __Length__, // Keep last, Do not Index.
};
const size_t io_map_size = static_cast<size_t>(DeviceIOMapper::__Length__);


// 通常端口之间相差 1 个 T
template<class T>
DeviceIOMapper operator+(DeviceIOMapper a, T b) {
  T r = static_cast<T>(a) + b;
  return static_cast<DeviceIOMapper>(r);
}


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

  // 应该重写这个方法, 默认 readX 方法是对这个方法的包装
  virtual u32 read() { return 0xFFFF'FFFF; }

  virtual u32 read1() { return read() >> 8; }
  virtual u32 read2() { return read() >> 16; }
  virtual u32 read3() { return read() >> 24; }

  // 应该重写这个方法, 默认其他 writeX 方法是对该方法的包装
  virtual void write(u32 value) {}

  virtual void write(u8 v) { write(u32(v)); }
  virtual void write(u16 v) { write(u32(v)); }
  virtual void write1(u8 v) { write((u32) u32(v)<<8); }
  virtual void write2(u8 v) { write((u32) u32(v)<<16); }
  virtual void write2(u16 v) { write((u32) u32(v)<<16); }
  virtual void write3(u8 v) { write((u32) u32(v)<<24); }
  
  // mips 总线上不可能在这个地址写 32 位值, 这个错误会被 cpu 捕获不会发送到总线
  void write1(u32 v) { write2(v); }
  void write1(u16 v) { write2(v); }
  void write3(u32 v) { write2(v); }
  void write3(u16 v) { write2(v); }
 
  void write2(u32 v) {
    // throw exception
    //((DeviceIO*) (0))->write((u32)0); 
    error("Cannot read address %x, Memory misalignment\n", v);
  }
};


// 带有锁存功能的接口
template<class Data = U32Reg>
class DeviceIOLatch : public DeviceIO {
protected:
  Data reg;
public:
  virtual void write(u32 value) override {
    reg.write(value);
  }
  virtual u32 read() override { 
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

  virtual void write(u32 value) override {
    reg.write(value);
  }

  virtual u32 read() override { 
    return reg.read();
  }
};


}