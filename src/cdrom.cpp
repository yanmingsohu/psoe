#include "cdrom.h"

#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/util.h>
#include <cdio/logging.h>
#include <chrono>

namespace ps1e {

#ifdef DEBUG_CDROM_INFO
  void __cddbg(const char* format, ...) {
    if (ps1e_t::ext_stop) {
      warp_printf(format, "\x1b[30m\x1b[43m");
    }
  }
#endif

static const u8 PARM_EMPTY    = 1;
static const u8 PARM_NON_EMP  = 0;
static const u8 PARM_FULL     = 0;
static const u8 RESP_EMPTY    = 0;
static const u8 RESP_NON_EMP  = 1;
static const u8 DATA_EMPTY    = 0;
static const u8 DATA_NON_EMP  = 1;

typedef std::shared_ptr<ICDCommand> CmdRef;
CmdRef parse_cmd(u8 c);


static void message(const char* oper, const driver_return_code_t t) {
  const char * msg = cdio_driver_errmsg(t);
  error("CD-Drive %s\n", msg);
  switch (t) {
    case DRIVER_OP_ERROR:
      error("CD-Drive %s: error\n", oper); 
      break;

    case DRIVER_OP_UNSUPPORTED:
      error("CD-Drive %s: Unsupported\n", oper); 
      break;

    case DRIVER_OP_UNINIT:
      error("CD-Drive %s: hasn't been initialized\n", oper); 
      break;

    case DRIVER_OP_NOT_PERMITTED:
      error("CD-Drive %s: Not permitted\n", oper); 
      break;

    case DRIVER_OP_BAD_PARAMETER:
      error("CD-Drive %s: Bad parameter\n", oper); 
      break;

    case DRIVER_OP_BAD_POINTER:
      error("CD-Drive %s: Bad pointer to memory area\n", oper); 
      break;

    case DRIVER_OP_NO_DRIVER:
      error("CD-Drive %s: Driver not available on this OS\n", oper); 
      break;

    case DRIVER_OP_MMC_SENSE_DATA:
      error("CD-Drive %s: MMC operation returned sense data, but no other error above recorded.\n", oper); 
      break;
  }
}


void CdAttribute::reading() {
  motor = read = 1;
  seek = play = 0;
  shellopen = 0;
}


void CdAttribute::seeking() {
  motor = seek = 1;
  read = play = 0;
  shellopen = 0;
}


void CdAttribute::playing() {
  motor = play = 1;
  read = seek = 0;
  shellopen = 0;
}


void CdAttribute::idle() {
  play = read = seek = 0;
  motor = 1;
}


void CdAttribute::clearerr() {
  seekerr = iderror = error = 0;
}


void CdAttribute::nodisk() {
  motor = play = read = seek = 0;
  shellopen = 1;
}


void CdAttribute::sync() {
  v = 0x11;
}


void CdAttribute::reset() {
  v = 0;
}


u16 CdMode::data_size() {
  if (ib) return 2328;   // 918h
  if (size) return 2340; // 924h 
  return 2048;           // 800h
}


void CdMsf::next_section() {
  u8 _f = cdio_from_bcd8(f);
  if (++_f >= 75) {
    f = 0;

    u8 _s = cdio_from_bcd8(s);
    if (++_s >= 60) {
      s = 0;
      m = cdio_to_bcd8(1 + cdio_from_bcd8(m));
    } else {
      s = cdio_to_bcd8(_s);
    }
  } else {
    f = cdio_to_bcd8(_f);
  }
}


CdDrive::CdDrive() : cd(0), first_track(0), num_track(0), offset(-1) {
  cdio_loglevel_default = CDIO_LOG_DEBUG; 
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

    printf("CD-ROM Track (%i - %i)\n", first_track, num_track);
    for (CDTrack i = first_track; i <= num_track; ++i) {
      printf("\tTrack %d - %s\n", i, trackFormatStr(cdio_get_track_format(cd, i)));
    }
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


bool CdDrive::loadPhysical(const char* src) {
  cddbg("Open physical CDROM %s\n", src);
  ::CdIo_t *p_cdio = cdio_open(src, DRIVER_DEVICE);
  return loadDisk(p_cdio);
}


bool CdDrive::loadImage(const char* path) {
  cddbg("Open virtual CDROM image from %s\n", path);
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
  cddbg("CD READ data lsn %d\n", offset);
  driver_return_code_t r = cdio_read_mode2_sector(cd, buf, offset, false);
  if (r) message("ReadData", r);
  return r == 0;
}


// 线程安全的
class CDCommandFifo {
private:
  u8 cmd;
  bool _has;
  std::mutex rw;

public: 
  CDCommandFifo() : _has(false) {}

  // 有更多命令返回 true
  bool has() {
    return _has;
  }

  // 提取出下一个命令
  CmdRef next() {
    std::lock_guard<std::mutex> _lk(rw);
    if (_has) {
      CmdRef n = parse_cmd(cmd);
      n->setID(cmd);
      _has = false;
      return n;
    }
    return CmdRef();
  }

  // 推入下一个命令
  void push(u8 cmd) {
    std::lock_guard<std::mutex> _lk(rw);
    this->cmd = cmd;
    _has = 1;
  }

  // 下一个命令如果是 id 则返回 true
  bool nextIs(u8 a0, u8 a1, u8 a2, u8 a3) {
    std::lock_guard<std::mutex> _lk(rw);
    if (!has()) return false;
    return (cmd == a0) || (cmd == a1) || (cmd == a2) || (cmd == a3);
  }

  bool nextIs(const u8 id) {
    std::lock_guard<std::mutex> _lk(rw);
    if (!has()) return false;
    return cmd == id;
  }
};


CdromFifo::CdromFifo(u8 len16bit) : len(0x10 * len16bit) {
  reset();
  d = new u8[len];
  memset(d, 0, len);
}


CdromFifo::~CdromFifo() {
  delete [] d;
}


void CdromFifo::reset() {
  pread = 0;
  pwrite = 0;
}


u8 CdromFifo::read() {
  u8 v = d[pread % len];
  pread++;
  return v;
}


void CdromFifo::write(u8 v) {
  d[pwrite % len] = v;
  pwrite++;
}


bool CdromFifo::isEmpty() {
  return pread == pwrite;
}


bool CdromFifo::isFull() {
  return (pread + len) == pwrite;
}


volatile s32& CdromFifo::getWriter(u8*& p) {
  p = d;
  return pwrite;
}


CDROM_REG::CDROM_REG(CDrom &_p, Bus& b) : p(_p) {
  b.bind_io(DeviceIOMapper::cd_rom_io, this);
}


// 0x1F80'1800
u32 CDROM_REG::read() {
  CdStatus s;
  s.index = p.s_index;
  s.adpcm_empt = 0;
  s.parm_empt = p.param.isEmpty()    ? PARM_EMPTY : PARM_NON_EMP;
  s.parm_full = p.param.isFull()     ? PARM_FULL  : PARM_EMPTY;
  s.resp_empt = p.response.isEmpty() ? RESP_EMPTY : RESP_NON_EMP;
  s.data_empt = p.data.isEmpty()     ? DATA_EMPTY : DATA_NON_EMP;
  s.busy = p.s_busy;

  cddbg("CD-ROM r 1f801800 status %x\n", s.v);
  return s.v | (s.v << 8) | (s.v << 16) | (s.v << 24);
}


// 0x1F80'1801 读取应答, 16byte
u32 CDROM_REG::read1() {
  u8 r = p.response.read();
  cddbg("CD-ROM r 1f801801 response %x\n", r);
  return r;
}


// 0x1F80'1802 读取数据
u32 CDROM_REG::read2() {
  u32 d = p.data.read();
  cddbg("CD-ROM r 1f801802 data %x\n", d);
  return d;
}


// 0x1F80'1803
u32 CDROM_REG::read3() {
  switch (p.s_index) {
    case 0: // 中断使能寄存器
    case 2: // 镜像
      cddbg("CD-ROM r 1f801803 irq enb %x\n", p.irq_enb);
      return p.irq_enb;

    case 1: // 中断标志寄存器
    case 3: // 镜像
      cddbg("CD-ROM r 1f801803 irq flag %x\n", p.irq_flag);
      return p.irq_flag;
      break;
  }
}


void CDROM_REG::write(u32 v) {
  error("CD-ROM w 1f801800(32) %08x\n", v);
}


// 0x1F80'1800 索引/状态寄存器
void CDROM_REG::write(u8 v) {
  cddbg("CD-ROM w 1f801800(8) index %08x\n", v);
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
      cddbg("CD-ROM w 1f801801(8) CMD %08x\n", v);
      p.cmdfifo->push(v);
      p.s_busy = 1;
      p.recovery_processing->notify_all();
      break;

    case 1: // 声音映射数据输出
      break;

    case 2: // 声音映射编码信息
      p.code.v = v;
      break;

    case 3: 
      cddbg("CD-ROM w 1f801801(8) rCD rSPU vol %08x\n", v);
      p.change.cd_r_spu_r = v;
      break;
  }
}


// 0x1F80'1802
void CDROM_REG::write2(u8 v) {
  switch (p.s_index) {
    case 0: // Parameter fifo
      cddbg("CD-ROM w 1f801802(8) param %08x\n", v);
      p.param.write(v);
      break;

    case 1: // 中断使能寄存器
      cddbg("CD-ROM w 1f801802(8) irq enable %08x\n", v);
      p.irq_enb = static_cast<u8>(v);
      //ps1e_t::ext_stop = 1;
      break;

    case 2:
      cddbg("CD-ROM w 1f801802(8) lCD lSPU vol %08x\n", v);
      p.change.cd_l_spu_l = v;
      break;

    case 3:
      cddbg("CD-ROM w 1f801802(8) rCD lSPU vol %08x\n", v);
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
      cddbg("CD-ROM w 1f801803(8) REQ %08x\n", v);
      // 上拉
      if ((p.req.bfrd == 0) && (v & 0x80)) {
        p.want_data = 1;
      }

      p.req.v = v;
      if (p.req.bfrd == 0) {
        p.clearDataFifo();
        p.want_data = 0;
      }
      break;

    case 1: // 中断标志寄存器
      cddbg("CD-ROM w 1f801803(8) ask irq flag %02x | %02x\n", p.irq_flag, v);
      p.irq_flag = p.irq_flag & (~static_cast<u8>(v));

      // bit6 Write: 1=Reset Parameter Fifo
      if (v & 0x40) {
        cddbg("CD-rom clear param fifo\n");
        p.clearParmFifo();
      }
      p.recovery_processing->notify_all();
      break;

    case 2:
      cddbg("CD-ROM w 1f801803(8) lCD rSPU vol %08x\n", v);
      p.change.cd_l_spu_r = v;
      break;

    case 3:
      cddbg("CD-ROM w 1f801803(8) volume change %02x | %02x\n", p.irq_flag, v);
      p.mute = 0x1 & v;
      if (0x20 & v) {
        p.apply = p.change;
      }
      break;
  }
}


CDrom::CDrom(Bus& b, CdDrive& d) 
: DMADev(b, DeviceIOMapper::dma_cdrom_base), thread_running(true),
  bus(b), drive(d), reg(*this, b), response(1), param(4), data(0x96),
  th(0), irq_flag(0), cmdfifo(0), for_processor(0), recovery_processing(0)
{
  th = new std::thread(&CDrom::command_processor, this);
  for_read = new std::mutex();
  cmdfifo = new CDCommandFifo();
  for_processor = new std::mutex();
  recovery_processing = new std::condition_variable();
  s_busy = 0;
}


CDrom::~CDrom() {
  thread_running = false;
  th->join();
  delete th;
  delete for_read;
  delete cmdfifo;
}


void CDrom::sendIrq(u8 irq) {
  irq_flag |= irq;

  if (irq_flag & irq_enb) {
    cddbg("CD-rom send IRQ %x - %x\n", irq_flag, irq_enb);
    bus.send_irq(IrqDevMask::cdrom);
  } else {
    cddbg("CD-rom NON send IRQ %x - %x\n", irq_flag, irq_enb);
  }
}


void CDrom::pushResponse(u8 v) {
  response.write(v);
}


void CDrom::pushResponse() {
  pushResponse(attr.v);
}


u8 CDrom::pop_param() {
  u8 v = param.read();
  return v;
}


void CDrom::setFilter(u8 _file, u8 _channel) {
  file = _file;
  channel = _channel;
}


void CDrom::setMode(u8 m) {
  mode_cdda       = m & 1;
  mode_auto_pause = m & (1<<1);
  mode_play_irq   = m & (1<<2);
  mode_filter     = m & (1<<3);
  mode_ig_pos     = m & (1<<4);
  mode_size       = m & (1<<5);
  mode_to_spu     = m & (1<<6);
  mode_speed      = m & (1<<7);
}


u8 CDrom::getMode() {
  return mode_cdda | 
         mode_auto_pause | 
         mode_play_irq |
         mode_filter |
         mode_ig_pos |
         mode_size |
         mode_to_spu |
         mode_speed;
}


void CDrom::clearDataFifo() {
  data.reset();
}


void CDrom::clearRespFifo() {
  response.reset();
}


void CDrom::clearParmFifo() {
  param.reset();
}


void CDrom::updateStatus() {
  if (drive.hasDisk()) {
    attr.seeking();
  } else {
    attr.nodisk();
  }
}


void CDrom::setLoc(u8 m, u8 s, u8 f) {
  loc.m = m;
  loc.s = s;
  loc.f = f;
}


void CDrom::syncDriveLoc() {
  drive.seek(&loc);
}


void CDrom::moveToFirstTrack() {
  drive.getTrackMsf(drive.first(), &loc);
}


bool CDrom::dataIsEmpty() {
  return data.isEmpty();
}


void CDrom::getSessionTrack(CDTrack& first, CDTrack& end) {
  first = drive.first();
  end   = drive.end();
}


void CDrom::moveNextTrack() {
  loc.next_section();
  drive.seek(&loc);
}


void CDrom::readSectionData() {
  cddbg("\rCD LSF [%02x:%02x:%02x] ", loc.m, loc.s, loc.f);
  //data.reset();
  u8* writer;
  volatile s32& wsize = data.getWriter(writer);
  drive.readData(writer);
  wsize = 2048;
  //TODO: 正确读取扇区头
  if (attr.read) {
    memcpy(locL, writer, 8);
  } else {
    memcpy(locP, writer, 8);
  }
}


bool CDrom::getTrackMsf(CDTrack t, CdMsf *r) {
  return drive.getTrackMsf(t, r);
}


bool CDrom::hasDisk() {
  return drive.hasDisk();
}


bool CDrom::nextCmdIsStop() {
  return cmdfifo->nextIs(0x8, 0x9, 0xA, 0x1C);
}


std::mutex* CDrom::readLock() {
  return for_read;
}


void CDrom::command_processor() {
  info("CD-ROM Thread ID: %x\n", this_thread_id());
  std::unique_lock<std::mutex> lck(*for_processor);
  CmdRef curr;

  while (thread_running) {
    //TODO:待验证, 这里做了特殊处理... 没有响应 ReadN 的 irq1 而是直接发送 pause
    // 用 nextCmdIsStop() 替换会导致异常读取
    if (cmdfifo->nextIs(0x09)) {
      curr.reset();
      irq_flag = 0;
    }

    if (irq_flag) {
      recovery_processing->wait_for(lck, std::chrono::milliseconds(1));
      continue;
    }

    if (curr) {
      // 给 cpu 足够的周期检查 cdrom 状态, 当 cdrom 过快的响应命令, 
      // cpu 甚至会认为 cdrom 没有变化! (导致重复发送 init 命令, 
      // 或者 cpu 开始检测 cdrom 已经发送完成的事件)
      //TODO: 相比固定时间, 最好等待某个信号, 或cpu周期, 等 cpu 启用中断?.
      sleep(1);

      s_busy = 1;
      if (curr->docmd(*this)) {
        curr.reset();
      }
      continue;
    } else {
      s_busy = 0;
    }

    if (cmdfifo->has()) {
      curr = cmdfifo->next();
      s_busy = 1;
      attr.clearerr();
      continue;
    } else {
      recovery_processing->wait_for(lck, std::chrono::milliseconds(1));
    }
  }
}


void CDrom::dma_ram2dev_block(psmem addr, u32 bytesize, s32 inc) {
  error("Not support CD-ROM write\n");
  throw std::runtime_error("cd dma");
}


void CDrom::dma_dev2ram_block(psmem addr, u32 bytesize, s32 inc) {
  std::lock_guard<std::mutex> _lk(*for_read);
  cddbg("CDrom DMA begin %x, %d bytes, %d\n", addr, bytesize, inc);

  u32 cnt = 0;
  do {
    if (data.isEmpty()) readSectionData();

    while (cnt < bytesize && (!data.isEmpty())) {
      bus.write8(addr, data.read());
      addr += inc;
      ++cnt;
    }
  } while(cnt < bytesize);
  //printf("CD rom DMA exit\n");
}

}