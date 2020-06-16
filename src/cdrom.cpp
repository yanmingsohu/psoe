#include "cdrom.h"

namespace ps1e {

CDrom::CDrom(Bus& b) : status(this, b), cmd(this, b), parm(this, b), req(this, b) {
}


CDrom::RegStatus::RegStatus(CDrom* parent, Bus& b) : p(*parent) {
  b.bind_io(DeviceIOMapper::cd_status_index, this);
}


void CDrom::RegStatus::write(u32 value) {
  s.index = 0x03 & value;
}


u32 CDrom::RegStatus::read() {
  return s.v;
}


CDrom::RegCmd::RegCmd(CDrom* parent, Bus& b) : p(*parent) {
  b.bind_io(DeviceIOMapper::cd_resp_fifo_cmd, this);
}


void CDrom::RegCmd::write(u32 v) {
  switch (p.getIndex()) {
    case 0: // command 
      p.status.s.busy = 1;
      break;
    case 1: // 声音映射数据输出
      break;
    case 2: // 声音映射编码信息
      break;
    case 3: 
      p.change.cd_r_spu_r = v;
      break;
  }
}


// command response, 16byte
u32 CDrom::RegCmd::read() {
  u8 r = fifo[pfifo++ & 0xF];
  if (pfifo > fifo_data_len) {
    p.status.s.resp_empt = 0;
  }
  return r;
}


void CDrom::RegCmd::resp_fifo_ready(u8 len) {
  fifo_data_len = len;
  p.status.s.resp_empt = 1;
}


CDrom::RegParm::RegParm(CDrom* parent, Bus& b) : p(*parent) {
  b.bind_io(DeviceIOMapper::cd_data_fifo_parm, this);
}


void CDrom::RegParm::write(u32 v) {
  switch (p.getIndex()) {
    case 0: // Parameter fifo
      break;
    case 1: // 中断使能寄存器
      break;
    case 2:
      p.change.cd_l_spu_l = v;
      break;
    case 3:
      p.change.cd_r_spu_l = v;
      break;
  }
}


// 读取数据
u32 CDrom::RegParm::read() {
  return 0;
}


CDrom::RegReq::RegReq(CDrom* parent, Bus& b) : p(*parent) {
  b.bind_io(DeviceIOMapper::cd_irq_vol, this);
}


void CDrom::RegReq::write(u32 v) {
  switch (p.getIndex()) {
    case 0: // 请求寄存器
      break;
    case 2:
      p.change.cd_l_spu_r = v;
      break;
    case 3:
      p.mute = 0x1 & v;
      if (0x20 & v) {
        p.apply = p.change;
      }
      break;
  }
}


u32 CDrom::RegReq::read() {
  switch (p.getIndex()) {
    case 0: // 中断使能寄存器
    case 2: // 镜像
      break;

    case 1: // 中断标志寄存器
    case 3: // 镜像
      break;
  }
}

}