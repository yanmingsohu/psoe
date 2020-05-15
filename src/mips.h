#pragma once 

#include "util.h"
#include "cpu.h"

namespace ps1e {

typedef u32 mips_instruction;
typedef u8  mips_reg;


#pragma pack(1)
union instruction_st {
  mips_instruction i;

  struct {
    u32 ft : 6;
    u32 sa : 5;
    u32 rd : 5;
    u32 rt : 5;
    u32 rs : 5;
    u32 op : 6;
  } R;

  struct {
    union {
      s16 imm;
      u16 immu;
    };
    u16 rt  : 5;
    u16 rs  : 5;
    u16 op  : 6;
  } I;

  struct {
    u32 jt : 26;
    u32 op : 6;
  } J;

  instruction_st(mips_instruction _i) : i(_i) {}
};
#pragma pack()


//
// 如果指令解析出错返回 false
//
template<class InstructionReceiver>
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
          // $d = $s + $t
          r->add(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 33:
          // $d = $s + $t
          r->addu(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 34:
          // $d = $s - $t 
          r->sub(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 35:
          // $d = $s - $t
          r->subu(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 24:
          // Hi|Lo = $s * $t
          r->mul(i.R.rs, i.R.rt);
          break;
        case 25:
          // Hi|Lo = $s * $t
          r->mulu(i.R.rs, i.R.rt);
          break;
        case 26:
           // HI|Lo = $s / $t
          r->div(i.R.rs, i.R.rt);
          break;
        case 27:
           // HI|Lo = $s / $t
          r->divu(i.R.rs, i.R.rt);
          break;
        case 42:
          // $d = $s < %t ? 1 : 0
          r->slt(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 43:
          // $d = $s < $t ? 1 : 0
          r->sltu(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 36:
          // $d = $s & $t 
          r->_and(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 37:
          // $d = $s | $t 
          r->_or(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 39:
          // $d = ~($s | $t)
          r->_nor(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 40:
          // $d = $s ^ $t 
          r->_xor(i.R.rd, i.R.rs, i.R.rt);
          break;
        case 8:
          // pc = $s;
          r->jr(i.R.rs);
          break;
        case 9:
          // $d = pc + 4; pc = $s;
          r->jalr(i.R.rd, i.R.rs);
          break;
        case 16:
          // $d = HI 
          r->mfhi(i.R.rd);
          break;
        case 17:
          // HI = $s
          r->mthi(i.R.rs);
          break;
        case 18:
          // $d = LO
          r->mflo(i.R.rd);
          break;
        case 19:
          // LO = $s
          r->mtlo(i.R.rs);
          break;
        case 0:
          // $d = $t << i
          r->sll(i.R.rd, i.R.rt, i.R.sa);
          break;
        case 4:
          // $d = $t << $s
          r->sllv(i.R.rd, i.R.rt, i.R.rs);
          break;
        case 3:
          // $d = $t >> i, fill by sign
          r->sra(i.R.rd, i.R.rt, i.R.sa);
          break;
        case 7:
          // $d = $t >> $s, fill by sign
          r->srav(i.R.rd, i.R.rt, i.R.rs);
          break;
        case 2:
          // $d = $t >> i, fill by zero
          r->srl(i.R.rd, i.R.rt, i.R.sa);
          break;
        case 6:
          // $d = $t >> %s, fill by zero
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
      // $t = $s + i
      r->addi(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 9:
      // $t = $s + i
      r->addiu(i.I.rt, i.I.rs, i.I.immu); 
      break;

    case 10:
      // $t = $s < i ? 1 : 0
      r->slti(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 11:
      // $t = $s < i ? 1 : 0
      r->sltiu(i.I.rt, i.I.rs, i.I.immu);
      break;

    case 12:
      // $t = $s & i
      r->andi(i.I.rt, i.I.rs, i.I.immu);
      break;

    case 13:
      // $t = $s | i
      r->ori(i.I.rt, i.I.rs, i.I.immu);
      break;
    
    case 14:
      // $t = $s ^ i
      r->xori(i.I.rt, i.I.rs, i.I.immu);
      break;

    case 15:
      // $t = i;
      r->lui(i.I.rt, i.I.immu);
      break;

    case 35:
      // $t = [$s + i]
      r->lw(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 43:
      // [$s + i] = $t
      r->sw(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 32:
      // $t = byte[$s + i]
      r->lb(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 36:
      // $t = byte[$s + i]
      r->lbu(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 33:
      // $t = s16[$s + i]
      r->lh(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 37:
      // $t = u16[$s + i]
      r->lhu(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 41:
      // s[$s + i] = $t
      r->sh(i.I.rt, i.I.rs, i.I.imm);

    case 40:
      // byte[$s + i] = $t
      r->sb(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 4:
      // if ($t == $s) then pc += 4 + i<<2
      r->beq(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 5:
      // if ($t != $s) then pc += 4 + i<<2
      r->bne(i.I.rt, i.I.rs, i.I.imm);
      break;

    case 6:
      // if $s <= 0 then pc += 4 + i<<2
      r->blez(i.I.rs, i.I.imm);
      break;

    case 7:
      // if $s > 0 then pc += 4 + i<<2
      r->bgtz(i.I.rs, i.I.imm);
      break;

    case 1:
      switch (i.I.rt) {
        case 0:
          // if $s < 0 then pc += 4 + i<<2
          r->bltz(i.I.rs, i.I.imm);
          break;
        case 1:
          // if $s >= 0 then pc += 4 + i<<2
          r->bgez(i.I.rs, i.I.imm);
          break;
        case 17:
          // if $s >= 0 then $ra = pc; pc += 4 + i<<2
          r->bgezal(i.I.rs, i.I.imm);
          break;
        case 16:
          // if $s < 0 then $ra = pc; pc += 4 + i<<2
          r->bltzal(i.I.rs, i.I.imm);
          break;

        default:
          return false;
      }
      break;

    case 2:
      // pc = (0xF000'0000 & pc) | (i << 2)
      r->j(i.J.jt);
      break;

    case 3:
      // $ra = pc + 4; pc = (0xF000'0000 & pc) | (i << 2)
      r->jal(i.J.jt);
      break;

    case 16:
      switch (i.R.rs) {
        case 0:
          // $t = cop0[$d];
          r->mfc0(i.R.rt, i.R.rd);
          break;
        case 4:
          // cop0[$d] = $t;
          r->mtc0(i.R.rt, i.R.rd);
          break;
        case 16:
          switch (i.R.ft) {
            case 16:
              // cop0.exl = 0; pc = epc; 
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