#include "cdrom.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>

namespace ps1e {

CdDrive::CdDrive() : cd(0) {
}


CdDrive::~CdDrive() {
  close();
}


bool CdDrive::open(CDIO c) {
  close();
  if (c) {
    cd = c;
    first_track = cdio_get_first_track_num(static_cast<CdIo_t*>(cd));
    num_track   = cdio_get_num_tracks(static_cast<CdIo_t*>(cd)); 
    info("CD-ROM Track (%i - %i)\n", first_track, num_track); 
    return true;
  }
  error("Cannot open CDROM\n");
  return false;
}


void CdDrive::close() {
  if (cd) {
    cdio_destroy(static_cast<CdIo_t*>(cd));
    cd = 0;
  }
}


bool CdDrive::openPhysical(char* src) {
  info("Open physical CDROM %s\n", src);
  ::CdIo_t *p_cdio = cdio_open(src, DRIVER_UNKNOWN);
  return open(p_cdio);
}


bool CdDrive::openImage(char* path) {
  info("Open virtual CDROM image from %s\n", path);
  ::CdIo_t *p_cdio = cdio_open_bincue(path);
  return open(p_cdio);
}


int CdDrive::getTrackFormat(CDTrack track) {
  return cdio_get_track_format(static_cast<CdIo_t*>(cd), track);
}


const char* CdDrive::trackFormatStr(int t) {
  switch (t) {
    case TRACK_FORMAT_AUDIO:
      return "AUDIO";
    case TRACK_FORMAT_ERROR:
      return "Error";
    case TRACK_FORMAT_CDI:
      return "CDI";
    case TRACK_FORMAT_XA:
      return "XA";
    case TRACK_FORMAT_DATA:
      return "DATA";
    case TRACK_FORMAT_PSX:
      return "PSX";
  }
  return "Unknow";
}


CDTrack CdDrive::first() {
  return first_track;
}


CDTrack CdDrive::end() {
  return first_track + num_track;
}


bool CdDrive::getTrackMsf(CDTrack t, CdMsf *r) {
  msf_t msf;
  if (!cdio_get_track_msf(static_cast<CdIo_t*>(cd), t, &msf)) {
    return false;
  }
  *r = {msf.m, msf.s, msf.f};
  return true;
}


CDrom::CDrom(Bus& b) : status(this, b), cmd(this, b), parm(this, b), req(this, b), bus(b) {
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
      p.do_cmd(v);
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
      p.irq_enb = static_cast<u8>(v);
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
      p.BFRD = v & (1<<7);
      p.SMEN = v & (1<<5);
      if (p.BFRD) {
        p.status.s.data_empt = 1;
      } else {
        //TODO: Reset Data Fifo?
      }
      break;

    case 1: // 中断标志寄存器
      p.irq_flag = p.irq_flag & (0xff ^ static_cast<u8>(v));
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
      return p.irq_enb;
      break;

    case 1: // 中断标志寄存器
    case 3: // 镜像
      return p.irq_flag;
      break;
  }
}


#define CD_CMD(n, fn) \
  case n: Cmd##fn(); break

#define CD_CMD_SE(n, fn, x) \
  case n: Cmd##fn(x); break


void CDrom::do_cmd(u32 c) {
  status.s.busy = 1;
  switch (c) {
    CD_CMD(0x00, Sync);
    CD_CMD(0x01, Getstat);
    CD_CMD(0x02, Setloc);
    CD_CMD(0x03, Play);
    CD_CMD(0x04, Forward);
    CD_CMD(0x05, Backward);
    CD_CMD(0x06, ReadN);
    CD_CMD(0x07, MotorOn);
    CD_CMD(0x08, Stop);
    CD_CMD(0x09, Pause);
    CD_CMD(0x0a, Init);
    CD_CMD(0x0b, Mute);
    CD_CMD(0x0c, Demute);
    CD_CMD(0x0d, Setfilter);
    CD_CMD(0x0e, Setmode);
    CD_CMD(0x0f, Getparam);

    CD_CMD(0x10, GetlocL);
    CD_CMD(0x11, GetlocP);
    CD_CMD(0x12, SetSession);
    CD_CMD(0x13, GetTN);
    CD_CMD(0x14, GetTD);
    CD_CMD(0x15, SeekL);
    CD_CMD(0x16, SeekP);
    CD_CMD(0x17, SetClock);
    CD_CMD(0x18, GetClock);
    CD_CMD(0x19, Test);
    CD_CMD(0x1a, GetID);
    CD_CMD(0x1b, ReadS);
    CD_CMD(0x1c, Reset);
    CD_CMD(0x1d, GetQ);
    CD_CMD(0x1e, ReadTOC);
    CD_CMD(0x1f, VideoCD);

    CD_CMD_SE(0x50, Secret, 0);
    CD_CMD_SE(0x51, Secret, 1);
    CD_CMD_SE(0x52, Secret, 2);
    CD_CMD_SE(0x53, Secret, 3);
    CD_CMD_SE(0x54, Secret, 4);
    CD_CMD_SE(0x55, Secret, 5);
    CD_CMD_SE(0x56, Secret, 6);
    CD_CMD_SE(0x57, Secret, 7);
  }
}

}