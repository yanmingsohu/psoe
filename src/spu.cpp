#include "spu.h"
#include "spu.inl"

namespace ps1e {

#define SPU_INIT_CHANNEL(name, n)  name ## n(*this, b),
  

SoundProcessing::SoundProcessing(Bus& b) : 
  mainVol(*this, b),          reverbVol(*this, b),            cdVol(*this, b), 
  externVol(*this, b),        mainCurrVol(*this, b),          nKeyOn(*this, b),    
  nKeyOff(*this, b),          nFM(*this, b),                  nNoise(*this, b),  
  nReverb(*this, b),          endx(*this, b),                 ramReverbStartAddress(*this, b), 
  ramIrqAdress(*this, b),       
  ramTransferCtrl(*this, b),  ctrl(*this, b),                 status(*this, b),
  dAPF1(*this, b),            dAPF2(*this, b),                vIIR(*this, b),  
  vCOMB1(*this, b),           vCOMB2(*this, b),               vCOMB3(*this, b), 
  vCOMB4(*this, b),           vWALL(*this, b),                vAPF1(*this, b),  
  vAPF2(*this, b),            mSAME(*this, b),                mCOMB1(*this, b), 
  mCOMB2(*this, b),           dSAME(*this, b),                mDIFF(*this, b), 
  mCOMB3(*this, b),           mCOMB4(*this, b),               dDIFF(*this, b), 
  mAPF1(*this, b),            mAPF2(*this, b),                vIN(*this, b),
  SPU_DEF_ALL_CHANNELS(ch, SPU_INIT_CHANNEL) _un1(*this, b), _un2(*this, b),
  ramTransferFifo(*this, b, &SoundProcessing::push_fifo),
  ramTransferAddress(*this, b, &SoundProcessing::set_transfer_address)
{
  memset(mem, 0, SPU_MEM_SIZE);
}


void SoundProcessing::set_transfer_address(u32 a) {
  trans_point = a << 3; // * 8
}


void SoundProcessing::push_fifo(u32 a) {
  fifo[fifo_point++] = u16(a);
}


void SoundProcessing::process() {
  status.r.busy = 1;
  
  if (ctrl.r.dma_trs == u8(SpuDmaDir::ManualWrite)) {
    ctrl.r.dma_trs = u8(SpuDmaDir::Stop);
    manual_write();
  }
  else if (ctrl.r.dma_trs == u8(SpuDmaDir::DMAwrite)) {
    //TODO wait??
  }
  else if (ctrl.r.dma_trs == u8(SpuDmaDir::DMAread)) {
    //TODO
  }

  // END
  status.r.mode = ctrl.r.v & 0B11111;
  status.r.busy = 0;
}


void SoundProcessing::manual_write() {
  u16 *start = (u16*)(&mem[trans_point]);
  const u8 t = 0B111 & (ramTransferCtrl.r.v >> 1);
  u16 v;

  switch (t) {
    case 0: case 1: case 6: case 7:
      v = fifo[SPU_FIFO_INDEX(fifo_point + SPU_FIFO_MASK)];
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = v;
      }
      break;

    case 2:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + i)];
      }
      break;

    case 3:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + (i & 0xFE))];
      }
      break;

    case 4:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + (i & 0xFC))];
      }
      break;

    case 5:
      for (int i=0; i<SPU_FIFO_SIZE; ++i) {
        start[i] = fifo[SPU_FIFO_INDEX(fifo_point + 7 + (i & 0xF8))];
      }
      break;
  }

  // 32 个半字, 64个字节
  trans_point += (SPU_FIFO_SIZE << 1);
}


}