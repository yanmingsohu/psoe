﻿#pragma once 

#include "bus.h"
#include <thread>
#include <mutex>
//#include <condition_variable>

struct msf_s;
struct _CdIo;


namespace ps1e {

class CDrom;
typedef _CdIo* CDIO;
typedef u8    CDTrack;
typedef s32   CdLsn;


#pragma pack(push, 1)
union CdStatus {
  u8 v;
  struct {
    u8 index      : 2; //0,1
    u8 adpcm_empt : 1; //2 (0=Empty) ;set when playing XA-ADPCM sound
    u8 parm_empt  : 1; //3 (1=Empty) ;triggered before writing 1st byte
    u8 parm_full  : 1; //4 (0=Full)  ;triggered after writing 16 bytes
    u8 resp_empt  : 1; //5 (0=Empty) ;triggered after reading LAST byte
    u8 data_empt  : 1; //6 (0=Empty) ;triggered after reading LAST byte
    u8 busy       : 1; //7 (1=Busy)  ;Command/parameter transmission busy  
  };
};


union CdReq {
  u8 v;
  struct {
    u8 notuse : 5; //0-4
    u8 smen   : 1; //5 SMEN Want Command Start Interrupt on Next Command (0=No change, 1=Yes)
    u8 bfwr   : 1; //6
    u8 bfrd   : 1; //7 (0=No/Reset Data Fifo, 1=Yes/Load Data Fifo)
  };
};


struct CdSpuVol {
  u8 cd_l_spu_l;
  u8 cd_l_spu_r;
  u8 cd_r_spu_r;
  u8 cd_r_spu_l;

  void setall(u8 v) {
    cd_l_spu_l = cd_l_spu_r = cd_r_spu_r = cd_r_spu_l = v;
  }
};


union CdAudioCode {
  u8 v;
  struct {
    u8 ch   : 2; //0,1 (0=Mono, 1=Stereo)
    u8 rate : 2; //2,3 (0=37800Hz, 1=18900Hz)
    u8 bit  : 2; //4,5 (0=4bit, 1=8bit)
    u8 emp  : 2; //6,7 (0=Off, 1=Emphasis)
  };
};


// 必须与 `struct msf_t` 二进制兼容
struct CdMsf {
  u8 m, s, f;
  void next_section();
};


//  ___These values appear in the FIRST response; with stat.bit0 set___
//  10h - Invalid Sub_function (for command 19h), or invalid parameter value
//  20h - Wrong number of parameters
//  40h - Invalid command
//  80h - Cannot respond yet (eg. required info was not yet read from disk yet)
//           (namely, TOC not-yet-read or so)
//           (also appears if no disk inserted at all)
//
//  ___These values appear in the SECOND response; with stat.bit2 set___
//  04h - Seek failed (when trying to use SeekL on Audio CDs)
//
//  ___These values appear even if no command was sent; with stat.bit2 set___
//  08h - Drive door became opened
union CdAttribute {
  u8 v;
  struct {
    u8 error      : 1; //0 Invalid Command/parameters (followed by Error Byte)
    u8 motor      : 1; //1 Spindle Motor (0=Motor off, or in spin-up phase, 1=Motor on)
    u8 seekerr    : 1; //2 (0=Okay, 1=Seek error)     (followed by Error Byte)
    u8 iderror    : 1; //3 (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
    u8 shellopen  : 1; //4 Once shell open (0=Closed, 1=Is/was Open)
    u8 read       : 1; //5 Reading data sectors  ;/only ONE of these bits can be set
    u8 seek       : 1; //6 Seeking               ; at a time (ie. Read/Play won't get
    u8 play       : 1; //7 Playing CD-DA         ;\set until after Seek completion)
  };
  void reading();
  void seeking();
  void playing();
  void donothing();
  void clearerr();
};


union CdMode {
  u8 v;
  struct {
    u8 cdda   : 1; //0 (0=Off, 1=Allow to Read CD-DA Sectors; ignore missing EDC)
    u8 ap     : 1; //1 (0=Off, 1=Auto Pause upon End of Track) ;for Audio Play 
    u8 rep    : 1; //2 (0=Off, 1=Enable Report-Interrupts for Audio Play)
    u8 filter : 1; //3 (0=Off, 1=Process only XA-ADPCM sectors that match Setfilter)
    u8 ib     : 1; //4 (0=Normal, 1=Ignore Sector Size and Setloc position)
    u8 size   : 1; //5 (0=800h=DataOnly, 1=924h=WholeSectorExceptSyncBytes)
    u8 xa     : 1; //6 (0=Off, 1=Send XA-ADPCM sectors to SPU Audio Input) 
    u8 speed  : 1; //7 (0=Normal speed, 1=Double speed)
  };
  u16 data_size();
};
#pragma pack(pop)


class CdDrive {
public:
  static const int AUDIO_BUF_SIZE = 2352;
  static const int DATA_BUF_SIZE  = 2048;

private:
  CDIO cd;
  CDTrack first_track;
  CDTrack num_track;
  CdLsn offset;

  bool loadDisk(CDIO);

public:
  CdDrive();
  virtual ~CdDrive();

  void releaseDisk();
  bool loadPhysical(char* src);
  bool loadImage(char* filepath);
  int getTrackFormat(CDTrack track);
  const char* trackFormatStr(int);
  CDTrack first();
  CDTrack end();
  bool getTrackMsf(CDTrack, CdMsf*);
  bool seek(const CdMsf*);
  // buf[AUDIO_BUF_SIZE]
  bool readAudio(void *buf);
  // buf[DATA_BUF_SIZE]
  bool readData(void *buf);
  bool hasDisk();
};


class CDROM_REG : public DeviceIO {
private:
  CDrom& p;
public:
  CDROM_REG(CDrom &_p, Bus& b);
  u32 read();
  u32 read1();
  u32 read2();
  u32 read3();
  void write(u32 value);
  void write(u8 v);
  void write(u16 v);
  void write1(u8 v);
  void write2(u8 v);
  void write2(u16 v);
  void write3(u8 v);
};


//TODO: fix 当 pwrite 回绕到 0 逻辑错误
class CdromFifo {
public:
  u8 *d;
  u32 pread;
  u32 pwrite;
  const u32 len;
  const u16 mask;

public:
  // len16bit - 16字节的倍数
  CdromFifo(u8 len16bit);
  ~CdromFifo();
  void reset();
  u8 read();
  void write(u8);
};


class CDrom : public DMADev, public NonCopy {
private:
  Bus& bus;
  CdDrive& drive;
  CdSpuVol change;
  CdSpuVol apply;

  CDROM_REG reg;
  CdAudioCode code;
  CdAttribute attr;
  CdMode mode;
  u8 file;
  u8 channel;
  u8 irq_enb;
  u8 session;

  // 解决多线程冲突
  u8 s_index;
  u8 s_adpcm_empt;
  u8 s_parm_empt;
  u8 s_parm_full;
  u8 s_resp_empt;
  u8 s_data_empt;
  u8 s_busy;

  //0-2   Read: Response Received   Write: 7=Acknowledge   ;INT1..INT7
  //3     Read: Unknown (usually 0) Write: 1=Acknowledge   ;INT8  ;XXX CLRBFEMPT
  //4     Read: Command Start       Write: 1=Acknowledge   ;INT10h;XXX CLRBFWRDY
  //5     Read: Always 1 ;XXX "_"   Write: 1=Unknown              ;XXX SMADPCLR
  //6     Read: Always 1 ;XXX "_"   Write: 1=Reset Parameter Fifo ;XXX CLRPRM
  //7     Read: Always 1 ;XXX "_"   Write: 1=Unknown              ;XXX CHPRST
  u8 irq_flag;
  u8 irq_flag2;
  bool has_irq_flag2;

  bool mute;
  CdReq req;
  CdMsf loc;

  bool thread_running;
  std::thread th;

  CdromFifo response;
  CdromFifo param;
  CdromFifo data;
  volatile u8 cmd;
  volatile bool has_cmd;

  u8 locL[8];
  u8 locP[8];

  void command_processor();

  void push_response(u8);
  // 由cdrom命令调用
  void send_irq(u8);
  // 由cdrom命令调用
  void send_irq2(u8);
  void do_cmd(u8);
  u8 read_param();
  void read_next_section();
  
  void clear_data_fifo();
  void set_busy(bool);
  void clear_resp_fifo(bool resetFifo = true);
  void clear_parm_fifo(bool resetFifo = true);

public:
  void CmdSync();
  void CmdGetstat();
  void CmdSetloc();
  void CmdPlay();
  void CmdForward();
  void CmdBackward();
  void CmdReadN();
  void CmdMotorOn();
  void CmdStop();
  void CmdPause();
  void CmdInit();
  void CmdMute();
  void CmdDemute();
  void CmdSetfilter();
  void CmdSetmode();
  void CmdGetparam();
  void CmdGetlocL();
  void CmdGetlocP();
  void CmdSetSession();
  void CmdGetTN();
  void CmdGetTD();
  void CmdSeekL();
  void CmdSeekP();
  void CmdSetClock();
  void CmdGetClock();
  void CmdTest();
  void CmdGetID();
  void CmdReadS();
  void CmdReset();
  void CmdGetQ();
  void CmdReadTOC();
  void CmdVideoCD();
  void CmdSecret(int);

public:
  CDrom(Bus&, CdDrive&);
  ~CDrom();

friend class CDROM_REG;
};

}