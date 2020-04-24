#include "mips.h"

namespace ps1e {


// https://alanhogan.com/asu/assembler.php
bool mips_decode(mips_instruction op, InstructionReceiver* r) {
  instruction_st i(op);

  switch (i.R.op) {
    case 0b000000:
      switch (i.R.ft) {
        case 0b100000:
          r->add(i.R.rs, i.R.rt, i.R.rd);
          return true;
      }
      return false;

    case 0b001000:
      r->addi(i.I.rs, i.I.rt, i.I.imm);
      return true; 

    default:
      break;
  }
  return false;
}


}