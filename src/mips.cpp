#include "mips.h" 

namespace ps1e {


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
        case 17:
          r->mthi(i.R.rs);
          break;
        case 18:
          r->mflo(i.R.rd);
          break;
        case 19:
          r->mtlo(i.R.rs);
          break;
        case 0:
          r->sll(i.R.rd, i.R.rt, i.R.sa);
          break;
        case 4:
          r->sllv(i.R.rd, i.R.rt, i.R.rs);
          break;
        case 3:
          r->sra(i.R.rd, i.R.rt, i.R.sa);
          break;
        case 7:
          r->srav(i.R.rd, i.R.rt, i.R.rs);
          break;
        case 2:
          r->srl(i.R.rd, i.R.rt, i.R.sa);
          break;
        case 6:
          r->srlv(i.R.rd, i.R.rt, i.R.rs);
          break;
        case 12:
          r->syscall();
          break;
        case 13:
          r->brk(i.J.jt >> 6);
          break;

        default:
          return false;
      }
      break;

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

    case 33:
      r->lh(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 37:
      r->lhu(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 41:
      r->sh(i.I.rt, i.I.rs, i.I.imm);

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
      switch (i.I.rt) {
        case 0:
          r->bltz(i.I.rs, i.I.imm);
          break;
        case 1:
          r->bgez(i.I.rs, i.I.imm);
          break;
        case 17:
          r->bgezal(i.I.rs, i.I.imm);
          break;
        case 16:
          r->bltzal(i.I.rs, i.I.imm);
          break;

        default:
          return false;
      }
      break;

    case 2:
      r->j(i.J.jt);
      break;

    case 3:
      r->jal(i.J.jt);
      break;

    case 16:
      switch (i.R.rs) {
        case 0:
          r->mfc0(i.R.rt, i.R.rd);
          break;
        case 4:
          r->mtc0(i.R.rt, i.R.rd);
          break;
        case 16:
          switch (i.R.ft) {
            case 16:
              r->rfe();
              break;
          }
          break;
        case 2: 
          //r->cfc0(i.R.rt, i.R.rd);
          return false;
        case 6: 
          //r->ctc0(i.R.rt, i.R.rd);
          return false;
        case 8: 
          switch (i.R.rt) {
            case 0:
              //r->bc0f(i.I.imm)
              return false;
            case 1:
              //r->bc0t(i.I.imm)
              return false;
          }
          return false;
        default:
          return false;
      }
      break;

    case 48: // lwc0
      //r->lwc0(i.I.rt, i.I.rs, i.I.imm);
      return false;
    case 56: // swc0
      //r->swc0(i.I.rt, i.I.rs, i.I.imm);
      return false;//TODO: remove

    case 18: // cop2 GTE like op 16
      switch (i.R.rs) {
        case 0:
          //r->mfc2(i.R.rt, i.R.rd);
          return false;
        case 4:
          //r->mtc2(i.R.rt, i.R.rd);
          return false;
        case 16:
          //r->imm25(i.i & ((1<<25)-1));
          return false;
        case 2: 
          //r->cfc2(i.R.rt, i.R.rd);
          return false;
        case 6: 
          //r->ctc2(i.R.rt, i.R.rd);
          return false;
        case 8: 
          switch (i.R.rt) {
            case 0:
              //r->bc2f(i.I.imm)
              return false;
            case 1:
              //r->bc2t(i.I.imm)
              return false;
          }
          return false;
        default:
          return false;
      }
      return false;

    case 50: // lwc2 GTE
      //r->lwc2(i.I.rt, i.I.rs, i.I.imm);

    case 58: // swc2 GTE
      //r->swc2(i.I.rt, i.I.rs, i.I.imm);
      return false;//TODO: remove
    
    case 17: // cop1 not use
    case 49: // lwc1 not use
    case 57: // swc1 not use
      
    case 19: // cop3 not use
    case 51: // lwc3 not use
    case 59: // swc3 not use
    default:
      return false;
  }
  return true;
}


}