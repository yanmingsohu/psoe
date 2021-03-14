#include "cdrom.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/util.h>
#include <chrono>

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
    info("CD-ROM Track (%i - %i)\n", first_track, num_track); 
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
  info("Open physical CDROM %s\n", src);
  ::CdIo_t *p_cdio = cdio_open(src, DRIVER_UNKNOWN);
  return loadDisk(p_cdio);
}


bool CdDrive::loadImage(char* path) {
  info("Open virtual CDROM image from %s\n", path);
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
  DMADev(b, DeviceIOMapper::dma_cdrom_base), thread_running(true),
  th(&CDrom::command_processor, this)
{
  data_buf[0] = new u8[CdDrive::AUDIO_BUF_SIZE];
  data_buf[1] = new u8[CdDrive::AUDIO_BUF_SIZE];
  data_read_point = 0;
  swap_buf();
}


void CDrom::swap_buf() {
  if (_swap_buf) {
    accept_buf = data_buf[0];
    read_buf   = data_buf[1];
    _swap_buf  = false;
  } else {
    accept_buf = data_buf[1];
    read_buf   = data_buf[0];
    _swap_buf  = true;
  }
}


CDrom::~CDrom() {
  delete [] data_buf[0];
  delete [] data_buf[1];
  thread_running = false;
  //TODO: 线程可能无法退出
  _wait(1000);
  wait_irq.notify_all();
  _wait(1000);
  wait_irq.notify_all();
  th.join();
}


void CDrom::send_irq(u8 irq) {
  std::unique_lock<std::mutex> lk(for_irq);
  // 等待前一个中断处理完成
  if (irq_flag & 0x1F) {
    u8 waitirq = irq_flag;
    debug("CD-rom wait irq %x\n", waitirq);
    wait_irq.wait(lk);
    debug("CD-rom irq %x release\n", waitirq);
  }

  irq_flag = irq;
  if (irq & irq_enb) {
    debug("CD-rom send IRQ %x - %x\n", irq, irq_enb);
    bus.send_irq(IrqDevMask::cdrom);
  }
}


void CDrom::push_response(u8 v) {
  status.resp_empt = 1;
  if (!response.canRead()) response.reset();
  response.write(v);
}


u8 CDrom::read_param() {
  status.parm_full = 1;
  u8 v = param.read();
  if (!param.canRead()) {
    status.parm_empt = 1;
    //param.reset();
  }
  return v;
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


u8 CdromFifo::read() {
  u8 v = d[pread & mask];
  pread++;
  return v;
}


bool CdromFifo::canRead() {
  return pread < pwrite;
}


void CdromFifo::write(u8 v) {
  d[pwrite & mask] = v;
  pwrite++;
}


bool CdromFifo::canWrite() {
  return pwrite < len;
}


CDROM_REG::CDROM_REG(CDrom &_p, Bus& b) : p(_p) {
  b.bind_io(DeviceIOMapper::cd_rom_io, this);
}


// 0x1F80'1800
u32 CDROM_REG::read() {
  info("CD-ROM r 1f801800\n");
  u32 v = p.status.v;
  return v | (v << 8) | (v << 16) | (v << 24);
}


// 0x1F80'1801 
// command response, 16byte
u32 CDROM_REG::read1() {
  info("CD-ROM r 1f801801\n");
  u8 r = p.response.read();
  if (!p.response.canRead()) {
    p.status.resp_empt = 0;
  }
  return r;
}


// 0x1F80'1802 读取数据
u32 CDROM_REG::read2() {
  info("CD-ROM r 1f801802\n");
  return p.read_buf[p.data_read_point++];
}


// 0x1F80'1803
u32 CDROM_REG::read3() {
  info("CD-ROM r 1f801803\n");
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


void CDROM_REG::write(u32 v) {
  error("CD-ROM w 1f801800(32) %08x\n", v);
}


// 0x1F80'1800
void CDROM_REG::write(u8 v) {
  info("CD-ROM w 1f801800(8) %08x\n", v);
  p.status.index = 0x03 & v;
}


void CDROM_REG::write(u16 v) {
  error("CD-ROM w 1f801800(16) %08x\n", v);
}


// 0x1F80'1801
void CDROM_REG::write1(u8 v) {
  info("CD-ROM w 1f801801(8) %08x\n", v);
  switch (p.status.index) {
    case 0: // command 
      //ps1e_t::ext_stop = 1;
      if (!p.cmd.canRead()) p.cmd.reset();
      p.cmd.write(v);
      debug("CD-ROM cmd 0x%X\n", v);
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
  info("CD-ROM w 1f801802(8) %08x\n", v);
  switch (p.status.index) {
    case 0: // Parameter fifo
      p.param.write(v);
      if (!p.param.canWrite()) {
        p.status.parm_full = 0;
      }
      p.status.parm_empt = 0;
      break;

    case 1: // 中断使能寄存器
      debug("CD-ROM irq enable %08x\n", v);
      p.irq_enb = static_cast<u8>(v);
      ps1e_t::ext_stop = 1;
      break;

    case 2:
      p.change.cd_l_spu_l = v;
      break;

    case 3:
      p.change.cd_r_spu_l = v;
      break;
  }
}


void CDROM_REG::write2(u16 v) {
  info("CD-ROM w 1f801802(16) %08x\n", v);
}


// 0x1F80'1803
void CDROM_REG::write3(u8 v) {
  info("CD-ROM w 1f801803(8) %08x\n", v);
  switch (p.status.index) {
    case 0: // 请求寄存器
      p.BFRD = v & (1<<7);
      p.SMEN = v & (1<<5);
      if (p.BFRD) {
        p.status.data_empt = 1;
        p.data_read_point = 0;
        p.swap_buf();
      }
      break;

    case 1: // 中断标志寄存器
      /*debug("CD-ROM irq flag %02x < %02x\n", p.irq_flag, v);*/
      p.irq_flag = p.irq_flag & (~static_cast<u8>(v));
      if (v & (1<<6)) {
        p.param.reset();
      }
      if ((p.irq_flag & 0x1F) == 0) {
        //std::unique_lock<std::mutex> lk(p.for_irq);
        p.status.busy = 0;
        p.wait_irq.notify_all();
        debug("CD-ROM ask irq %02x < %02x\n", p.irq_flag, v);
        //ps1e_t::ext_stop = 1;
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
  case n: debug("CD-ROM CMD: %s\n", #fn); Cmd##fn(); break

#define CD_CMD_SE(n, fn, x) \
  case n: debug("CD-ROM CMD-SE: %s\n", #fn #x); Cmd##fn(x); break


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
    if (attr.read) {
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

    if (cmd.canRead()) {
      status.busy = 1;
      attr.clearerr();
      u8 c = cmd.read();
      do_cmd(c);
    } else {
      _wait(10);
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
  push_response(attr.v);
  send_irq(3);

  mode.v = 0;
  attr.v = 0;
  attr.seeking();
  cmd.reset();
  mute = true;
  //_wait(1000);

  push_response(attr.v);
  send_irq(2);
}


void CDrom::CmdReset() {
  push_response(attr.v);
  send_irq(0x3);

  status.v = 0x18;
  mode.v = 0;
  attr.v = 0;
  if (drive.hasDisk()) {
    attr.seeking();
  }
  response.reset();
  param.reset();
  cmd.reset();
  change.setall(0);
  apply.setall(0);
  mute = true;
  BFRD = false;
  SMEN = false;
  data_read_point = 0;
}


void CDrom::CmdMotorOn() {
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
    send_irq(2);
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
  send_irq(2);
}


void CDrom::CmdSetloc() {
  loc.m = read_param();
  loc.s = read_param();
  loc.f = read_param();
  attr.donothing();

  push_response(attr.v);
  send_irq(3);
}


void CDrom::CmdSeekL() {
  push_response(attr.v);
  send_irq(3);
  
  attr.donothing();

  push_response(attr.v);
  send_irq(2);
}


void CDrom::CmdSeekP() {
  push_response(attr.v);
  send_irq(3);
  
  attr.donothing();

  push_response(attr.v);
  send_irq(2);
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
    send_irq(2);
  } 
  else {
    push_response(attr.v);
    send_irq(3);
    attr.seekerr = 1;

    push_response(attr.v);
    push_response(0x40);
    send_irq(5);

    push_response(attr.v);
    push_response(0x40);
    send_irq(5);
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
  debug("[%2d:%2d:%2d] ", loc.m, loc.s, loc.f);
  data_size = mode.data_size();
  data_read_point = 0;
  drive.readData((void*) accept_buf);
  //TODO: 正确读取扇区头
  if (attr.read) {
    memcpy(locL, accept_buf, 8);
  } else {
    memcpy(locP, accept_buf, 8);
  }
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
  send_irq(2);
}


void CDrom::CmdGetstat() {
  push_response(attr.v);
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
  if (attr.shellopen == 1) {
    attr.error = 1;
    push_response(attr.v);
    push_response(0x80);
    send_irq(5);
  } else {
    push_response(attr.v); 
    send_irq(3);

    push_response(0x02);
    push_response(0x00);
    push_response(0x20);
    push_response(0x00);
    push_response(0x53);
    push_response(0x43);
    push_response(0x45);
    push_response(0x49);
    send_irq(2);
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


void CDrom::CmdTest() {
  u8 sub = read_param();
  attr.error = 1;
  push_response(attr.v); 
  push_response(0x10);
  send_irq(5);
}


void CDrom::CmdSecret(int) {
  push_response(attr.v); 
  push_response(0x40);
  send_irq(5);
}


void CDrom::CmdSetClock() {}
void CDrom::CmdGetClock() {}
void CDrom::CmdVideoCD() {}

}