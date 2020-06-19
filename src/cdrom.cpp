#include "cdrom.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>

namespace ps1e {

static void message(const char* oper, const driver_return_code_t t) {
  switch (t) {
    case DRIVER_OP_ERROR:
      warn("%s: error\n", oper); 
      break;

    case DRIVER_OP_UNSUPPORTED:
      warn("%s: Unsupported\n", oper); 
      break;

    case DRIVER_OP_UNINIT:
      warn("%s: hasn't been initialized\n", oper); 
      break;

    case DRIVER_OP_NOT_PERMITTED:
      warn("%s: Not permitted\n", oper); 
      break;

    case DRIVER_OP_BAD_PARAMETER:
      warn("%s: Bad parameter\n", oper); 
      break;

    case DRIVER_OP_BAD_POINTER:
      warn("%s: Bad pointer to memory area\n", oper); 
      break;

    case DRIVER_OP_NO_DRIVER:
      warn("%s: Driver not available on this OS\n", oper); 
      break;

    case DRIVER_OP_MMC_SENSE_DATA:
      warn("%s: MMC operation returned sense data, but no other error above recorded.\n", oper); 
      break;
  }
}


CdDrive::CdDrive() : cd(0), first_track(0), num_track(0), offset(-1) {
}


CdDrive::~CdDrive() {
  close();
}


bool CdDrive::open(CDIO c) {
  close();
  if (c) {
    cd = c;
    first_track = cdio_get_first_track_num(cd);
    num_track   = cdio_get_num_tracks(cd); 
    info("CD-ROM Track (%i - %i)\n", first_track, num_track); 
    return true;
  }
  error("Cannot open CDROM\n");
  return false;
}


void CdDrive::close() {
  if (cd) {
    cdio_destroy(cd);
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
  return cdio_get_track_format(cd, track);
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
  return cdio_get_track_msf(cd, t, reinterpret_cast<msf_t*>(r));
}


bool CdDrive::seek(const CdMsf* s) {
  // CDIO_INVALID_LSN
  offset = cdio_msf_to_lsn(reinterpret_cast<const msf_t*>(s));
}


bool CdDrive::readAudio(void* buf) {
  driver_return_code_t r = cdio_read_audio_sector(cd, buf, offset);
  if (r) message("ReadAudio", r);
  return r == 0;
}


// mode2[true=M2RAW_SECTOR_SIZE(2336), false=CDIO_CD_FRAMESIZE(2048)]
bool CdDrive::readData(void* buf) {
  driver_return_code_t r = cdio_read_mode2_sector(cd, buf, offset, false);
  if (r) message("ReadData", r);
  return r == 0;
}


CDrom::CDrom(Bus& b, CdDrive& d) 
: bus(b), drive(d), reg(*this, b), response(1), param(4), cmd(1), 
  DMADev(bus, DeviceIOMapper::dma_cdrom_base) {
}


void CDrom::send_irq(u8 irq) {
  irq_flag = irq;
  if (irq_enb & (1<<irq)) {
    bus.send_irq(IrqDevMask::cdrom);
  }
}


void CDrom::resp_fifo_ready() {
  status.resp_empt = 1;
}


CdromFifo::CdromFifo(u8 len16bit) : len(0x10 * len16bit), mask(len-1) {
  reset();
  d = new u8[len];
}


CdromFifo::~CdromFifo() {
  delete [] d;
}


void CdromFifo::reset() {
  pread = 0;
  pwrite = 0;
}


bool CdromFifo::read(u8& v) {
  v = d[pread & mask];
  pread++;
  return pread < pwrite;
}


bool CdromFifo::write(u8 v) {
  d[pwrite & mask] = v;
  pwrite++;
  return pwrite < len;
}


CDROM_REG::CDROM_REG(CDrom &_p, Bus& b) : p(_p) {
  b.bind_io(DeviceIOMapper::cd_rom_io, this);
}


// 0x1F80'1800
u32 CDROM_REG::read() {
  u32 v = p.status.v;
  return v | (v << 8) | (v << 16) | (v << 24);
}


// 0x1F80'1801 
// command response, 16byte
u32 CDROM_REG::read1() {
  u8 r;
  if (!p.response.read(r)) {
    p.status.resp_empt = 0;
  }
  return r;
}


// 0x1F80'1802 读取数据
u32 CDROM_REG::read2() {}


// 0x1F80'1803
u32 CDROM_REG::read3() {
  switch (p.status.index) {
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


void CDROM_REG::write(u32 value) {}


// 0x1F80'1800
void CDROM_REG::write(u8 v) {
  p.status.index = 0x03 & v;
}


void CDROM_REG::write(u16 v) {}


// 0x1F80'1801
void CDROM_REG::write1(u8 v) {
  switch (p.status.index) {
    case 0: // command 
      p.cmd.write(v);
      break;

    case 1: // 声音映射数据输出
      break;

    case 2: // 声音映射编码信息
      p.code.v = v;
      break;

    case 3: 
      p.change.cd_r_spu_r = v;
      break;
  }
}


// 0x1F80'1802
void CDROM_REG::write2(u8 v) {
  switch (p.status.index) {
    case 0: // Parameter fifo
      if (!p.param.write(v)) {
        p.status.parm_full = 0;
      }
      p.status.parm_empt = 0;
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


void CDROM_REG::write2(u16 v) {}


// 0x1F80'1803
void CDROM_REG::write3(u8 v) {
  switch (p.status.index) {
    case 0: // 请求寄存器
      p.BFRD = v & (1<<7);
      p.SMEN = v & (1<<5);
      if (p.BFRD) {
        //TODO: Reset Data Fifo
        p.status.data_empt = 1;
      }
      break;

    case 1: // 中断标志寄存器
      p.irq_flag = p.irq_flag & (~static_cast<u8>(v));
      if (v & (1<<6)) {
        p.param.reset();
      }
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


#define CD_CMD(n, fn) \
  case n: Cmd##fn(); debug("CD-ROM CMD:%s", #fn); break

#define CD_CMD_SE(n, fn, x) \
  case n: Cmd##fn(x); debug("CD-ROM CMD:%s", #fn #x); break


void CDrom::do_cmd(u32 c) {
  status.busy = 1;
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


void CDrom::CmdInit() {
  status.v = 0x18;
  response.reset();
  param.reset();
  cmd.reset();
  change.setall(0);
  apply.setall(0);
  mute = true;
  BFRD = false;
  SMEN = false;
}


void CDrom::CmdReset() {}


void CDrom::CmdSync() {}
void CDrom::CmdGetstat() {}
void CDrom::CmdSetloc() {}
void CDrom::CmdPlay() {}
void CDrom::CmdForward() {}
void CDrom::CmdBackward() {}
void CDrom::CmdReadN() {}
void CDrom::CmdMotorOn() {}
void CDrom::CmdStop() {}
void CDrom::CmdPause() {}
void CDrom::CmdMute() {}
void CDrom::CmdDemute() {}
void CDrom::CmdSetfilter() {}
void CDrom::CmdSetmode() {}
void CDrom::CmdGetparam() {}
void CDrom::CmdGetlocL() {}
void CDrom::CmdGetlocP() {}
void CDrom::CmdSetSession() {}
void CDrom::CmdGetTN() {}
void CDrom::CmdGetTD() {}
void CDrom::CmdSeekL() {}
void CDrom::CmdSeekP() {}
void CDrom::CmdSetClock() {}
void CDrom::CmdGetClock() {}
void CDrom::CmdTest() {}
void CDrom::CmdGetID() {}
void CDrom::CmdReadS() {}
void CDrom::CmdGetQ() {}
void CDrom::CmdReadTOC() {}
void CDrom::CmdVideoCD() {}
void CDrom::CmdSecret(int) {}

}