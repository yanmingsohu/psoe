#include "spu.h"

namespace ps1e {

#define SPU_INIT_CHANNEL(name, n)  name ## n(*this, b),
  

SoundProcessing::SoundProcessing(Bus& b) : 
  mainVol(*this, b), reverbVol(*this, b), cdVol(*this, b), externVol(*this, b),
  mainCurrVol(*this, b), nKeyOn(*this, b), nKeyOff(*this, b),
  nFM(*this, b), nNoise(*this, b), nReverb(*this, b), nStatus(*this, b),
  ramReverbStartAddress(*this, b), ramIrqAdress(*this, b),
  ramTransferAddress(*this, b), ramTransferFifo(*this, b),
  ramTransferCtrl(*this, b), ctrl(*this, b), status(*this, b),
  dAPF1(*this, b),  dAPF2(*this, b), 
  vIIR(*this, b),  vCOMB1(*this, b), vCOMB2(*this, b), vCOMB3(*this, b), 
  vCOMB4(*this, b), vWALL(*this, b),  vAPF1(*this, b),  vAPF2(*this, b), 
  mSAME(*this, b), mCOMB1(*this, b), mCOMB2(*this, b),  dSAME(*this, b), 
  mDIFF(*this, b), mCOMB3(*this, b), mCOMB4(*this, b),  dDIFF(*this, b), 
  mAPF1(*this, b),  mAPF2(*this, b),    vIN(*this, b),
  SPU_DEF_ALL_CHANNELS(ch, SPU_INIT_CHANNEL) _un1(*this, b), _un2(*this, b)
{
}

}