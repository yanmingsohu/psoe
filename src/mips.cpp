#include "mips.h"

namespace ps1e {


// https://alanhogan.com/asu/assembler.php
bool mips_decode(mips_instruction op, InstructionReceiver* r) {
  instruction_st i(op);

  if (op == 0) {
    r->nop();
    return true;
  }

  switch (i.R.op) {
    case 0:
      switch (i.R.ft) {
        case 32:
          r->add(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 33:
          r->addu(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 34:
          r->sub(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 35:
          r->subu(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 24:
          r->mul(i.R.rs, i.R.rt);
          break;

        case 25:
          r->mulu(i.R.rs, i.R.rt);
          break;

        case 26:
          r->div(i.R.rs, i.R.rt);
          break;

        case 27:
          r->divu(i.R.rs, i.R.rt);
          break;

        case 42:
          r->slt(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 43:
          r->sltu(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 36:
          r->_and(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 37:
          r->_or(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 39:
          r->_nor(i.R.rd, i.R.rs, i.R.rt);
          break;
        
        case 40:
          r->_xor(i.R.rd, i.R.rs, i.R.rt);
          break;

        case 8:
          r->jr(i.R.rs);
          break;

        case 9:
          r->jalr(i.R.rd, i.R.rs);
          break;

        case 16:
          r->mfhi(i.R.rd);
          break;

        case 18:
          r->mflo(i.R.rd);
          break;

        default:
          return false;
      }
      return true;

    case 8:
      r->addi(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 9:
      r->addiu(i.I.rt, i.I.rs, i.I.immu); 
      break;

    case 10:
      r->slti(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 11:
      r->sltiu(i.I.rt, i.I.rs, i.I.immu);
      break;

    case 12:
      r->andi(i.I.rt, i.I.rs, i.I.immu);
      break;

    case 13:
      r->ori(i.I.rt, i.I.rs, i.I.immu);
      break;
    
    case 14:
      r->xori(i.I.rt, i.I.rs, i.I.immu);
      break;

    case 35:
      r->lw(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 43:
      r->sw(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 32:
      r->lb(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 36:
      r->lbu(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 40:
      r->sb(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 15:
      r->lui(i.I.rt, i.I.immu);
      break;

    case 4:
      r->beq(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 5:
      r->bne(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 6:
      r->blez(i.I.rs, i.I.imm);
      break;

    case 7:
      r->bgtz(i.I.rs, i.I.imm);
      break;

    case 1:
      r->bltz(i.I.rs, i.I.imm);
      break;

    case 2:
      r->j(i.J.jt);
      break;

    case 3:
      r->jal(i.J.jt);
      break;

    case 16:
      r->mfc0(i.R.rt, i.R.rd);
      break;

    default:
      return false;
  }
  return true;
}


}