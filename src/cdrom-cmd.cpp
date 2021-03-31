#include "cdrom.h"
#include <mutex>
#include <cdio/util.h>
#include <memory>

namespace ps1e {

typedef std::shared_ptr<ICDCommand> CmdRef;


class CDCmdSync : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.attr.sync();
    p.pushResponse();
    p.pushResponse(0x40);
    return true;
  }
};


class CDCmdSetfilter : public ICDCommand { 
 public:
  bool docmd(CDrom& p) override {
    u8 f = p.pop_param();
    u8 c = p.pop_param();
    p.setFilter(f, c);

    p.pushResponse();
    p.sendIrq(3);
    return true;
  }
};


class CDCmdSetmode : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.setMode(p.pop_param());
    p.pushResponse();
    p.sendIrq(3);
    return true;
  }
};


class CDCmdInit : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.clearRespFifo();
        p.pushResponse(p.attr.v);
        p.sendIrq(3);
        return false;

      case 1:
        p.setMode(0);
        p.attr.reset();
        //attr.motor = 0; //TODO: 1?0
        p.clearParmFifo();
        p.clearDataFifo();
        p.mute = true;
        p.want_data = 0;
        //attr.shellopen = 1;

        p.pushResponse();
        p.sendIrq(2);
        return true;
    }
  }
};


class CDCmdReset : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.pushResponse();
    p.sendIrq(0x3);
    return true;
  }
};


class CDCmdMotorOn : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.updateStatus();
        
        if (p.attr.motor) {
          p.attr.error = 1;
          p.pushResponse();
          p.pushResponse(0x20);
          p.sendIrq(5);
          return true;
        }

        p.pushResponse();
        p.sendIrq(3);
        return false;

      case 1:
        p.attr.motor = 1;
        p.pushResponse();
        p.sendIrq(2);
        return true;
    }
  }
};


class CDCmdStop : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.moveToFirstTrack();
        p.attr.read = 0;
        p.pushResponse();
        p.sendIrq(3);
        return false;

      case 1:
        p.attr.error = 0;
        p.pushResponse();
        p.sendIrq(2);
        return true;
    }
  }
};



class CDCmdPause : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.pushResponse();
        p.sendIrq(3);
        return false;

      case 1:
        //attr.read = 0;
        //attr.seek = 0;
        //attr.play = 0;
        //attr.error = 0;
        //attr.seeking();
        p.attr.read = 0;
        p.pushResponse();
        p.sendIrq(2);
        return true;
    }
  }
};


class CDCmdSetloc : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    u8 m = p.pop_param();
    u8 s = p.pop_param();
    u8 f = p.pop_param();
    p.setLoc(m, s, f);

    p.want_data = 0;
    p.attr.seeking();
    p.pushResponse();
    p.sendIrq(3);
    return true;
  }
};


class CDCmdSeekL : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.pushResponse();
        p.sendIrq(3);
        return false;

      case 1:
        p.attr.seeking();
        p.syncDriveLoc();
        p.pushResponse();
        p.sendIrq(2);
        return true;
    }
  }
};


class CDCmdSeekP : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.pushResponse();
        p.sendIrq(3);
        return false;

      case 1:
        p.attr.seeking();
        p.syncDriveLoc();
        p.pushResponse();
        p.sendIrq(2);
        return true;
    }
  }
};


//TODO: cannot support SESSION
class CDCmdSetSession : public ICDCommand {
private:
  u8 session;

public:
  bool docmd(CDrom& p) override {
    switch (stage) {
      case 0:
        session = p.pop_param();
        if (session == 0) {
          p.attr.error = 1;
          p.pushResponse();
          p.pushResponse(0x10);
          p.sendIrq(5);
          return true;
        } 
        else if (session == 1) {
          p.pushResponse();
          p.sendIrq(3);
          stage = 1;
        } 
        else {
          p.pushResponse();
          p.sendIrq(3);
          stage = 2;
        }
        return false;

      case 1:
        p.pushResponse();
        p.sendIrq(2);
        return true;

      case 2:
        p.attr.seekerr = 1;
        p.pushResponse();
        p.pushResponse(0x40);
        p.sendIrq(5);
        return true;
    }
  }
};


class CDCmdReadN : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    std::lock_guard<std::mutex> _lk(*p.readLock());
    switch (stage) {
      case 0:
        p.attr.clearerr();
        p.pushResponse();
        p.sendIrq(3);
        stage = 1;
        return false;

      case 3:
        p.moveNextTrack();
        // no break;
      case 1:
        p.attr.reading();
        p.pushResponse();
        p.sendIrq(1);
        stage = 2;
        return false;

      case 2:
        sleep(5);
        if (p.dataIsEmpty() && p.want_data) {
          p.readSectionData();
          stage = 3;
        }
        return p.nextCmdIsStop();
    }
  }
};


class CDCmdReadS : public CDCmdReadN {
};


class CDCmdReadTOC : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.pushResponse();
        p.sendIrq(3);
        return false;

      case 1:
        p.pushResponse();
        p.sendIrq(2);
        return true;
    }
  }
};


class CDCmdGetstat : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.updateStatus();
    p.pushResponse();
    p.attr.shellopen = 0;
    p.sendIrq(3);
    return true;
  }
};


class CDCmdGetparam : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.pushResponse();
    p.pushResponse(p.getMode());
    p.pushResponse(0);
    p.pushResponse(p.file);
    p.pushResponse(p.channel);
    p.sendIrq(3);
    return true;
  }
};


class CDCmdGetlocL : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    if (p.attr.play) {
      p.attr.error = 1;
      p.pushResponse();
      p.pushResponse(0x50);
      p.sendIrq(5);
    } else {
      for (int i=0; i<8; ++i) {
        p.pushResponse(p.locL[i]);
      }
      p.sendIrq(3);
    }
    return true;
  }
};


class CDCmdGetlocP : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    for (int i=0; i<8; ++i) {
      p.pushResponse(p.locP[i]);
    }
    p.sendIrq(3);
    return true;
  }
};


class CDCmdGetTN : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    CDTrack f, e;
    p.getSessionTrack(f, e);
    p.pushResponse(p.attr.v);
    p.pushResponse(f);
    p.pushResponse(e);
    p.sendIrq(3);
    return true;
  }
};


class CDCmdGetTD : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    CDTrack track = p.pop_param();
    CdMsf msf;
    if (!p.getTrackMsf(track, &msf)) {
      p.attr.error = 1;
      p.pushResponse();
      p.pushResponse(0x10);
      p.sendIrq(5);
    } else {
      p.pushResponse();
      p.pushResponse(::cdio_to_bcd8(msf.m));
      p.pushResponse(::cdio_to_bcd8(msf.s));
      p.sendIrq(3);
    }
    return true;
  }
};


class CDCmdGetQ : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    //TODO
    return true;
  }
};


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
class CDCmdGetID : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    switch (stage++) {
      case 0:
        p.updateStatus();
        if (p.attr.shellopen == 1) {
          p.attr.error = 1;
          p.pushResponse();
          p.pushResponse(0x80);
          p.sendIrq(5);
          return true;
        } else {
          p.pushResponse();
          p.sendIrq(3);
        }
        return false;

      case 1:
        p.pushResponse(); 
        //bit7: Licensed (0=Licensed Data CD, 1=Denied Data CD or Audio CD)
        //bit6: Missing  (0=Disk Present, 1=Disk Missing)
        //bit4: Audio CD (0=Data CD, 1=Audio CD) (always 0 when Modchip installed)
        p.pushResponse(0x00); 
        //Disk type (from TOC Point=A0h) (eg. 00h=Audio or Mode1, 20h=Mode2)
        p.pushResponse(0x20);
        // Usually 00h (or 8bit ATIP from Point=C0h)
        p.pushResponse(0x00); 
        p.pushResponse('S');
        p.pushResponse('C');
        p.pushResponse('E');
        p.pushResponse('I');
        p.sendIrq(2);
        return true;
    }
  }
};


class CDCmdMute : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.mute = 1;
    p.pushResponse(); 
    p.sendIrq(3);
    return true;
  }
};


class CDCmdDemute : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.mute = 0;
    p.pushResponse(); 
    p.sendIrq(3);
    return true;
  }
};


//TODO: Report/AutoPause
class CDCmdPlay : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    CDTrack t = p.pop_param();
    p.pushResponse(); 
    p.sendIrq(3);
    return true;
  }
};


class CDCmdForward : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.pushResponse(); 
    p.sendIrq(3);
    return true;
  }
};


class CDCmdBackward : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    p.pushResponse(); 
    p.sendIrq(3);
    return true;
  }
};


class CDCmdSetClock : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    return true;
  }
};


class CDCmdGetClock : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    return true;
  }
};


class CDCmdVideoCD : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    return true;
  }
};


class CDCmdTest : public ICDCommand {
public:
  bool docmd(CDrom& p) override {
    // Region-free debug version --> accepts unlicensed CDRs
    const static char region[] = "for US/AEP";

    const u8 sub = p.pop_param();
    switch (sub) {
      default:
        p.pushResponse(0x11); 
        p.pushResponse(0x10); 
        p.sendIrq(5);
        break;

      case 0x00:
      case 0x01:
      case 0x02:
      case 0x03:
        p.pushResponse(); 
        p.sendIrq(3);
        break;

      // Get Driver version
      case 0x20: 
        // PSX (PU-7) 19 Sep 1994, version vC0 (a)
        p.pushResponse(0x94);
        p.pushResponse(0x09);
        p.pushResponse(0x19);
        p.pushResponse(0xC0);
        p.sendIrq(3);
        break;

      // Get Drive Switches
      case 0x21:
        if (p.hasDisk()) {
          p.pushResponse(0b0000'0010);
        } else {
          p.pushResponse(0);
        }
        p.sendIrq(3);
        break;

      // Get Region ID String
      case 0x22:
        for (int i=0; i<sizeof(region); ++i) {
          p.pushResponse(region[i]);
        }
        p.sendIrq(3);
        break;
      
      case 0x04:
        p.pushResponse();
        p.sendIrq(3);
        break;

      case 0x05:
        p.pushResponse(0x01);
        p.pushResponse(0x01);
        p.sendIrq(3);
        break;
    }
    return true;
  }
};


#define CD_CMD(n, fn) \
  case n: cddbg("CD-ROM CMD: " BLUE("%s") "\n", #fn); \
    return CmdRef(new CDCmd##fn()); 


CmdRef parse_cmd(u8 c) {
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

    //CD_CMD_SE(0x50, Secret, 0);
    //CD_CMD_SE(0x51, Secret, 1);
    //CD_CMD_SE(0x52, Secret, 2);
    //CD_CMD_SE(0x53, Secret, 3);
    //CD_CMD_SE(0x54, Secret, 4);
    //CD_CMD_SE(0x55, Secret, 5);
    //CD_CMD_SE(0x56, Secret, 6);
    //CD_CMD_SE(0x57, Secret, 7);
  }
  return CmdRef();
}


}