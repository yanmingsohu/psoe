#pragma once 

#include "bus.h"

namespace ps1e {

class CDrom;


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


struct CdSpuVol {
  u8 cd_l_spu_l;
  u8 cd_l_spu_r;
  u8 cd_r_spu_r;
  u8 cd_r_spu_l;
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

  CdSpuVol change;
  CdSpuVol apply;
  bool mute;

  u8 getIndex() {
    return status.s.index;
  }

public:
  CDrom(Bus&);
};

}