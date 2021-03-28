#include "cdrom.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/util.h>
#include <chrono>

namespace ps1e {

#ifdef DEBUG_CDROM_INFO
  #define dbgcd __dbgcd
  void __dbgcd(const char* format, ...) {
  //if (ps1e_t::ext_stop) {
    warp_printf(format, "\x1b[30m\x1b[43m");
  //}
}
#else
  #define dbgcd
#endif

static const u8 PARM_EMPTY    = 1;
static const u8 PARM_NON_EMP  = 0;
static const u8 PARM_FULL     = 0;
static const u8 RESP_EMPTY    = 0;
static const u8 RESP_NON_EMP  = 1;
static const u8 DATA_EMPTY    = 0;
static const u8 DATA_NON_EMP  = 1;


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


static void _wait(long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


void CdAttribute::reading() {
  motor = read = 1;
  seek = play = 0;
}


void CdAttribute::seeking() {
  motor = seek = 1;
  read = play = 0;
}


void CdAttribute::playing() {
  motor = play = 1;
  read = seek = 0;
}


void CdAttribute::donothing() {
  motor = play = read = seek = 0;
}


void CdAttribute::clearerr() {
  seekerr = iderror = error = 0;
}


void CdAttribute::nodisk() {
  motor = play = read = seek = 0;
  shellopen = 1;
}


u16 CdMode::data_size() {
  if (ib) return 2328;   // 918h
  if (size) return 2048; // 800h
  else return 2340;      // 924h
}


void CdMsf::next_section() {
  if (++f >= 75) {
    f = 0;
    if (++s >= 60) {
      s = 0;
      ++m;
    }
  }
}


CdDrive::CdDrive() : cd(0), first_track(0), num_track(0), offset(-1) {
}


CdDrive::~CdDrive() {
  releaseDisk();
}


bool CdDrive::loadDisk(CDIO c) {
  releaseDisk();
  if (c) {
    cd = c;
    first_track = cdio_get_first_track_num(cd);
    num_track   = cdio_get_num_tracks(cd); 
    dbgcd("CD-ROM Track (%i - %i)\n", first_track, num_track); 
    return true;
  }
  error("Cannot open CDROM\n");
  return false;
}


void CdDrive::releaseDisk() {
  if (cd) {
    cdio_destroy(cd);
    cd = 0;
  }
}


bool CdDrive::hasDisk() {
  return cd != 0;
}


bool CdDrive::loadPhysical(char* src) {
  dbgcd("Open physical CDROM %s\n", src);
  ::CdIo_t *p_cdio = cdio_open(src, DRIVER_UNKNOWN);
  return loadDisk(p_cdio);
}


bool CdDrive::loadImage(char* path) {
  dbgcd("Open virtual CDROM image from %s\n", path);
  ::CdIo_t *p_cdio = cdio_open_bincue(path);
  return loadDisk(p_cdio);
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
  return offset != CDIO_INVALID_LSN;
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
: bus(b), drive(d), reg(*this, b), response(1), param(4), cmd(1), data(0x96),
  DMADev(b, DeviceIOMapper::dma_cdrom_base), thread_running(true),
  th(&CDrom::command_processor, this), has_irq_flag2(0), irq_flag(0), irq_flag2(0)
{
  s_busy = 0;
  s_data_empt = DATA_EMPTY;
  s_resp_empt = RESP_EMPTY;
  s_parm_empt = PARM_EMPTY;
  s_parm_full = PARM_EMPTY;
}


CDrom::~CDrom() {
  thread_running = false;
  th.join();
}


void CDrom::send_irq(u8 irq) {
  irq_flag = irq;

  if (irq_flag & irq_enb) {
    dbgcd("CD-rom send IRQ %x - %x\n", irq_flag, irq_enb);
    bus.send_irq(IrqDevMask::cdrom);
  } else {
    dbgcd("CD-rom NON send IRQ %x - %x\n", irq_flag, irq_enb);
  }
}


void CDrom::send_irq2(u8 irq) {
  irq_flag2 = irq;
  has_irq_flag2 = 1;
}


void CDrom::push_response(u8 v) {
  s_resp_empt = RESP_NON_EMP;
  response.write(v);
}


u8 CDrom::read_param() {
  u8 v = param.read();
  return v;
}


CdromFifo::CdromFifo(u8 len16bit) : len(0x10 * len16bit), mask(len-1) {
  reset();
  d = new u8[len];
  for (u32 i = 0; i<len; ++i) {
    d[i] = 0;
  }
}


CdromFifo::~CdromFifo() {
  delete [] d;
}


void CdromFifo::reset() {
  pread = 0;
  pwrite = 0;
}


u8 CdromFifo::read() {
  u8 v = d[pread & mask];
  pread++;
  return v;
}


void CdromFifo::write(u8 v) {
  d[pwrite & mask] = v;
  pwrite++;
}


CDROM_REG::CDROM_REG(CDrom &_p, Bus& b) : p(_p) {
  b.bind_io(DeviceIOMapper::cd_rom_io, this);
}


// 0x1F80'1800
u32 CDROM_REG::read() {
  CdStatus s;
  s.index = p.s_index;
  s.adpcm_empt = p.s_adpcm_empt;
  s.parm_empt = p.s_parm_empt;
  s.parm_full = p.s_parm_full;
  s.resp_empt = p.s_resp_empt;
  s.data_empt = p.s_data_empt;
  s.busy = p.s_busy;

  dbgcd("CD-ROM r 1f801800 status %x\n", s.v);
  return s.v | (s.v << 8) | (s.v << 16) | (s.v << 24);
}


// 0x1F80'1801 读取应答, 16byte
u32 CDROM_REG::read1() {
  u8 r = p.response.read();
  if (p.response.pread == p.response.pwrite) {
    p.clear_resp_fifo(false);
  }
  dbgcd("CD-ROM r 1f801801 response %x\n", r);
  return r;
}


// 0x1F80'1802 读取数据
u32 CDROM_REG::read2() {
  u32 d = p.data.read();
  if (p.s_data_empt != DATA_EMPTY && p.data.pread == p.data.pwrite) {
    p.clear_data_fifo();
  }
  dbgcd("CD-ROM r 1f801802 data %x\n", d);
  return d;
}


// 0x1F80'1803
u32 CDROM_REG::read3() {
  switch (p.s_index) {
    case 0: // 中断使能寄存器
    case 2: // 镜像
      dbgcd("CD-ROM r 1f801803 irq enb %x\n", p.irq_enb);
      return p.irq_enb;

    case 1: // 中断标志寄存器
    case 3: // 镜像
      dbgcd("CD-ROM r 1f801803 irq flag %x\n", p.irq_flag);
      return p.irq_flag;
      break;
  }
}


void CDROM_REG::write(u32 v) {
  error("CD-ROM w 1f801800(32) %08x\n", v);
}


// 0x1F80'1800 索引/状态寄存器
void CDROM_REG::write(u8 v) {
  dbgcd("CD-ROM w 1f801800(8) index %08x\n", v);
  p.s_index = (0x03 & v);
}


void CDROM_REG::write(u16 v) {
  error("CD-ROM w 1f801800(16) index %08x\n", v);
}


// 0x1F80'1801
void CDROM_REG::write1(u8 v) {
  switch (p.s_index) {
    case 0: // command 发送命令, 清空数据缓冲区?
      //ps1e_t::ext_stop = 1;
      dbgcd("CD-ROM w 1f801801(8) CMD %08x\n", v);
      p.cmd = v;
      p.has_cmd = true;
      p.s_busy = 1;
      break;

    case 1: // 声音映射数据输出
      break;

    case 2: // 声音映射编码信息
      p.code.v = v;
      break;

    case 3: 
      dbgcd("CD-ROM w 1f801801(8) rCD rSPU vol %08x\n", v);
      p.change.cd_r_spu_r = v;
      break;
  }
}


// 0x1F80'1802
void CDROM_REG::write2(u8 v) {
  switch (p.s_index) {
    case 0: // Parameter fifo
      dbgcd("CD-ROM w 1f801802(8) param %08x\n", v);
      p.param.write(v);
      // 参数队列由游戏重置, cdrom不管理
      if (p.param.pwrite >= p.param.len) {
        p.s_parm_full = PARM_FULL;
      }
      p.s_parm_empt = PARM_NON_EMP;
      break;

    case 1: // 中断使能寄存器
      dbgcd("CD-ROM w 1f801802(8) irq enable %08x\n", v);
      p.irq_enb = static_cast<u8>(v);
      //ps1e_t::ext_stop = 1;
      break;

    case 2:
      dbgcd("CD-ROM w 1f801802(8) lCD lSPU vol %08x\n", v);
      p.change.cd_l_spu_l = v;
      break;

    case 3:
      dbgcd("CD-ROM w 1f801802(8) rCD lSPU vol %08x\n", v);
      p.change.cd_r_spu_l = v;
      break;
  }
}


void CDROM_REG::write2(u16 v) {
  error("CD-ROM w 1f801802(16) %08x\n", v);
}


// 0x1F80'1803
void CDROM_REG::write3(u8 v) {
  switch (p.s_index) {
    case 0: // 请求寄存器
      dbgcd("CD-ROM w 1f801803(8) REQ %08x\n", v);
      p.req.v = v;
      if (p.req.bfrd == 0) {
        p.clear_data_fifo();
      }
      break;

    case 1: // 中断标志寄存器
      dbgcd("CD-ROM w 1f801803(8) ask irq flag %02x | %02x\n", p.irq_flag, v);
      p.irq_flag = p.irq_flag & (~static_cast<u8>(v));

      // bit6 Write: 1=Reset Parameter Fifo
      if (v & 0x40) {
        dbgcd("CD-rom clear param fifo\n");
        p.clear_parm_fifo(false);
      }

      if ((p.irq_flag & 0x1F) == 0 && p.has_irq_flag2) {
        p.has_irq_flag2 = 0;
        p.send_irq(p.irq_flag2);
      }
      break;

    case 2:
      dbgcd("CD-ROM w 1f801803(8) lCD rSPU vol %08x\n", v);
      p.change.cd_l_spu_r = v;
      break;

    case 3:
      dbgcd("CD-ROM w 1f801803(8) volume change %02x | %02x\n", p.irq_flag, v);
      p.mute = 0x1 & v;
      if (0x20 & v) {
        p.apply = p.change;
      }
      break;
  }
}


void CDrom::clear_data_fifo() {
  s_data_empt = DATA_EMPTY;
  data.reset();
}


void CDrom::clear_resp_fifo(bool resetFifo) {
  s_resp_empt = RESP_EMPTY;
  // 防止两个线程同时修改
  if (resetFifo) response.reset();
}


void CDrom::clear_parm_fifo(bool resetFifo) {
  if (resetFifo) param.reset();
  else param.pread = param.pwrite;
  s_parm_full = PARM_EMPTY;
  s_parm_empt = PARM_EMPTY;
}


void CDrom::update_status() {
  if (drive.hasDisk()) {
    attr.seeking();
  } else {
    attr.nodisk();
  }
}


#define CD_CMD(n, fn) \
  case n: dbgcd("CD-ROM CMD: %s\n", #fn); Cmd##fn(); break

#define CD_CMD_SE(n, fn, x) \
  case n: dbgcd("CD-ROM CMD-SE: %s\n", #fn #x); Cmd##fn(x); break


void CDrom::do_cmd(u8 c) {
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
    CD_CMD(0x1d, GetQ);
    CD_CMD(0x1e, ReadTOC);
    CD_CMD(0x1f, VideoCD);
    
    CD_CMD(0x0a, Init);
    CD_CMD(0x1c, Reset);

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


void CDrom::command_processor() {
  while (thread_running) {
    s_busy = 0;
    if (irq_flag) {
      _wait(1);
      continue;
    }

    if (has_cmd) {
      has_cmd = 0;
      s_busy = 1;
      clear_data_fifo();
      //clear_resp_fifo();
      attr.clearerr();
      do_cmd(cmd);
      //clear_parm_fifo();
      continue;
    }

    // 实现 CmdReadN / ReadN 持续读取
    if (attr.read && s_data_empt == DATA_EMPTY) {
      s_busy = 1;
      loc.next_section();
      read_next_section();
      continue;
    }
    if (attr.seek) {
      attr.donothing();
      continue;
    }
    if (attr.play) {
      //TODO
      continue;
    }
  }
}


void CDrom::CmdSync() {
  attr.v = 0x11;
  push_response(attr.v);
  push_response(0x40);
}


void CDrom::CmdSetfilter() {
  file = read_param();
  channel = read_param();

  push_response(attr.v);
  send_irq(3);
}


void CDrom::CmdSetmode() {
  mode.v = read_param();

  push_response(attr.v);
  send_irq(3);
}


void CDrom::CmdInit() {
  u8 old = attr.v;
  clear_resp_fifo();
  push_response(old);
  send_irq(3);

  mode.v = 0;
  attr.v = 0;
  //attr.seeking();
  attr.motor = 0; //TODO: 1?0
  has_cmd = 0;
  clear_parm_fifo();
  clear_data_fifo();
  mute = true;
  //attr.shellopen = 1;

  push_response(attr.v);
  send_irq2(2);
}


void CDrom::CmdReset() {
  push_response(attr.v);
  send_irq(0x3);

  s_index = 0;
  mode.v = 0;
  attr.v = 0;
  if (drive.hasDisk()) {
    attr.seeking();
  }
  
  clear_resp_fifo();
  clear_parm_fifo();
  clear_data_fifo();
  has_cmd = 0;
  change.setall(0);
  apply.setall(0);
  mute = true;
  req.v = 0;
}


void CDrom::CmdMotorOn() {
  update_status();
  if (attr.motor) {
    attr.error = 1;
    push_response(attr.v);
    push_response(0x20);
    send_irq(5);
  } else {
    push_response(attr.v);
    send_irq(3);

    attr.motor = 1;
    push_response(attr.v);
    send_irq2(2);
  }
}


void CDrom::CmdStop() {
  drive.getTrackMsf(drive.first(), &loc);
  CmdPause();
}



void CDrom::CmdPause() {
  push_response(attr.v);
  send_irq(3);
  
  attr.read = 0;
  attr.seek = 0;
  attr.play = 0;
  attr.error = 0;

  push_response(attr.v);
  send_irq2(2);
}


void CDrom::CmdSetloc() {
  loc.m = read_param();
  loc.s = read_param();
  loc.f = read_param();

  attr.seeking();

  push_response(attr.v);
  send_irq(3);
  //ps1e_t::ext_stop = 1;
}


void CDrom::CmdSeekL() {
  push_response(attr.v);
  send_irq(3);
  
  attr.seeking();

  push_response(attr.v);
  send_irq2(2);
}


void CDrom::CmdSeekP() {
  push_response(attr.v);
  send_irq(3);
  
  attr.seeking();

  push_response(attr.v);
  send_irq2(2);
}


//TODO: cannot support SESSION
void CDrom::CmdSetSession() {
  session = read_param();
  if (session == 0) {
    attr.error = 1;
    push_response(attr.v);
    push_response(0x10);
    send_irq(5);
  } 
  else if (session == 1) {
    push_response(attr.v);
    send_irq(3);
    push_response(attr.v);
    send_irq2(2);
  } 
  else {
    push_response(attr.v);
    send_irq(3);
    attr.seekerr = 1;

    push_response(attr.v);
    push_response(0x40);
    send_irq2(5);
  }
}


void CDrom::CmdReadN() {
  attr.clearerr();
  attr.reading();
  push_response(attr.v);
  send_irq(3);

  read_next_section();
}


void CDrom::read_next_section() {
  dbgcd("\r\t\t\t\t[%2d:%2d:%2d] ", loc.m, loc.s, loc.f);
  data.reset();
  data.pwrite = mode.data_size();
  drive.readData((void*) data.d);
  //TODO: 正确读取扇区头
  if (attr.read) {
    memcpy(locL, data.d, 8);
  } else {
    memcpy(locP, data.d, 8);
  }
  s_data_empt = DATA_NON_EMP;
  push_response(attr.v);
  send_irq(1);
}


void CDrom::CmdReadS() {
  CmdReadN();
}


void CDrom::CmdReadTOC() {
  push_response(attr.v);
  send_irq(3);
  push_response(attr.v);
  send_irq2(2);
}


void CDrom::CmdGetstat() {
  update_status();
  push_response(attr.v);
  attr.shellopen = 0;
  send_irq(3);
}


void CDrom::CmdGetparam() {
  push_response(attr.v);
  push_response(mode.v);
  push_response(0);
  push_response(file);
  push_response(channel);
  send_irq(3);
}


void CDrom::CmdGetlocL() {
  if (attr.play) {
    attr.error = 1;
    push_response(attr.v);
    push_response(0x50);
    send_irq(5);
  } else {
    for (int i=0; i<8; ++i) {
      push_response(locL[i]);
    }
    send_irq(3);
  }
}


void CDrom::CmdGetlocP() {
  for (int i=0; i<8; ++i) {
    push_response(locP[i]);
  }
  send_irq(3);
}


void CDrom::CmdGetTN() {
  push_response(attr.v);
  push_response(drive.first());
  push_response(drive.end() -1);
  send_irq(3);
}


void CDrom::CmdGetTD() {
  CDTrack track = read_param();
  CdMsf msf;
  if (!drive.getTrackMsf(track, &msf)) {
    attr.error = 1;
    push_response(attr.v);
    push_response(0x10);
    send_irq(5);
  } else {
    push_response(attr.v);
    push_response(::cdio_to_bcd8(msf.m));
    push_response(::cdio_to_bcd8(msf.s));
    send_irq(3);
  }
}


void CDrom::CmdGetQ() {
}


//Drive Status           1st Response   2nd Response
//Door Open              INT5(11h,80h)  N/A
//Spin-up                INT5(01h,80h)  N/A
//Detect busy            INT5(03h,80h)  N/A
//No Disk                INT3(stat)     INT5(08h,40h, 00h,00h, 00h,00h,00h,00h)
//Audio Disk             INT3(stat)     INT5(0Ah,90h, 00h,00h, 00h,00h,00h,00h)
//Unlicensed:Mode1       INT3(stat)     INT5(0Ah,80h, 00h,00h, 00h,00h,00h,00h)
//Unlicensed:Mode2       INT3(stat)     INT5(0Ah,80h, 20h,00h, 00h,00h,00h,00h)
//Unlicensed:Mode2+Audio INT3(stat)     INT5(0Ah,90h, 20h,00h, 00h,00h,00h,00h)
//Debug/Yaroze:Mode2     INT3(stat)     INT2(02h,00h, 20h,00h, 20h,20h,20h,20h)
//Licensed:Mode2         INT3(stat)     INT2(02h,00h, 20h,00h, 53h,43h,45h,4xh)
//Modchip:Audio/Mode1    INT3(stat)     INT2(02h,00h, 00h,00h, 53h,43h,45h,4xh)
//SCEI (49 日本 / NTSC), SCEA (41 美国 / NTSC), SCEE (45 欧洲 / PAL)
void CDrom::CmdGetID() {
  update_status();
  if (attr.shellopen == 1) {
    attr.error = 1;
    push_response(attr.v);
    push_response(0x80);
    send_irq(5);
  } else {
    push_response(attr.v); 
    send_irq(3);

    push_response(attr.v); 
    //bit7: Licensed (0=Licensed Data CD, 1=Denied Data CD or Audio CD)
    //bit6: Missing  (0=Disk Present, 1=Disk Missing)
    //bit4: Audio CD (0=Data CD, 1=Audio CD) (always 0 when Modchip installed)
    push_response(0x00); 
    //Disk type (from TOC Point=A0h) (eg. 00h=Audio or Mode1, 20h=Mode2)
    push_response(0x20);
    // Usually 00h (or 8bit ATIP from Point=C0h)
    push_response(0x00); 
    push_response('S');
    push_response('C');
    push_response('E');
    push_response('I');
    send_irq2(2);
  }
}


void CDrom::CmdMute() {
  mute = 1;
  push_response(attr.v); 
  send_irq(3);
}


void CDrom::CmdDemute() {
  mute = 0;
  push_response(attr.v); 
  send_irq(3);
}


//TODO: Report/AutoPause
void CDrom::CmdPlay() {
  CDTrack t = read_param();
  push_response(attr.v); 
  send_irq(3);
}


void CDrom::CmdForward() {
  push_response(attr.v); 
  send_irq(3);
}


void CDrom::CmdBackward() {
  push_response(attr.v); 
  send_irq(3);
}


void CDrom::CmdSecret(int) {
  push_response(attr.v);
  push_response(0x40);
  send_irq(5);
}


void CDrom::CmdSetClock() {}
void CDrom::CmdGetClock() {}
void CDrom::CmdVideoCD() {}


void CDrom::CmdTest() {
  // Region-free debug version --> accepts unlicensed CDRs
  static char region[] = "for US/AEP";

  u8 sub = read_param();

  switch (sub) {
    default:
      push_response(0x11); 
      push_response(0x10); 
      send_irq(5);
      break;

    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
      push_response(attr.v); 
      send_irq(3);
      break;

    // Get Driver version
    case 0x20: 
      // PSX (PU-7) 19 Sep 1994, version vC0 (a)
      push_response(0x94);
      push_response(0x09);
      push_response(0x19);
      push_response(0xC0);
      send_irq(3);
      break;

    // Get Drive Switches
    case 0x21:
      if (drive.hasDisk()) {
        push_response(0b0000'0010);
      } else {
        push_response(0);
      }
      send_irq(3);
      break;

    // Get Region ID String
    case 0x22:
      for (int i=0; i<sizeof(region); ++i) {
        push_response(region[i]);
      }
      send_irq(3);
      break;
      
    case 0x04:
      push_response(attr.v);
      send_irq(3);
      break;

    case 0x05:
      push_response(0x01);
      push_response(0x01);
      send_irq(3);
      break;
  }
}

}