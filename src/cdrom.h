#pragma once 

#include "bus.h"


namespace ps1e {

class CDrom;
typedef void* CDIO;
typedef u8 CDTrack;


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
#pragma pack(pop)


#pragma pack(push, 1)
struct CdSpuVol {
  u8 cd_l_spu_l;
  u8 cd_l_spu_r;
  u8 cd_r_spu_r;
  u8 cd_r_spu_l;
};
#pragma pack(pop)


#pragma pack(push, 1)
struct CdMsf {
  u8 m, s, f;
};
#pragma pack(pop)


class CdDrive {
private:
  CDIO cd;
  CDTrack first_track;
  CDTrack num_track;

  bool open(CDIO);

public:
  CdDrive();
  virtual ~CdDrive();

  void close();
  bool openPhysical(char* src);
  bool openImage(char* filepath);
  int getTrackFormat(CDTrack track);
  const char* trackFormatStr(int);
  CDTrack first();
  CDTrack end();
  bool getTrackMsf(CDTrack, CdMsf*);
};


class CDrom {
private:
  class RegStatus : public DeviceIO {
  public:
    CDrom& p;
    CdStatus s;

    RegStatus(CDrom* parent, Bus& b);
    virtual u32 read();
    virtual void write(u32 value);
  };

  class RegCmd : public DeviceIO {
  public:
    CDrom& p;
    // response fifo
    u8 fifo[16];
    u8 pfifo;
    u8 fifo_data_len;
  
    virtual u32 read();
    virtual void write(u32 value);
    void resp_fifo_ready(u8 len);
    RegCmd(CDrom* parent, Bus& b);
  };

  class RegParm : public DeviceIO {
  public:
    CDrom& p;

    virtual u32 read();
    virtual void write(u32 value);
    RegParm(CDrom* parent, Bus& b);
  };

  class RegReq : public DeviceIO {
  public:
    CDrom& p;

    virtual u32 read();
    virtual void write(u32 value);
    RegReq(CDrom* parent, Bus& b);
  };

private:
  RegStatus status;
  RegCmd cmd;
  RegParm parm;
  RegReq req;

  Bus& bus;
  CdSpuVol change;
  CdSpuVol apply;
  u8 irq_enb;
  u8 irq_flag;
  bool mute;
  // Want Data (0=No/Reset Data Fifo, 1=Yes/Load Data Fifo)
  bool BFRD;
  // Want Command Start Interrupt on Next Command (0=No change, 1=Yes)
  bool SMEN;

  u8 getIndex() {
    return status.s.index;
  }

  void do_cmd(u32);

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
  CDrom(Bus&);
};

}