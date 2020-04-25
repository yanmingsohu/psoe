#pragma once

#include "util.h"
#include "cpu.h"

namespace ps1e {

typedef u32 mips_instruction;
typedef u8  mips_reg;


enum MispException {
  address = 4,
  opcode = 10,
  overflow = 12,
};


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
    u32 rt  : 5;
    u32 rs  : 5;
    u32 op  : 6;
  } I;

  struct {
    u32 jt : 26;
    u32 op : 6;
  } J;

  instruction_st(mips_instruction _i) : i(_i) {}
};


class InstructionReceiver {
public:
  virtual ~InstructionReceiver() {}

  virtual void nop() = 0;
  // $d = $s + $t
  virtual void add(mips_reg d, mips_reg s, mips_reg t) = 0;
  virtual void addu(mips_reg d, mips_reg s, mips_reg t) = 0;
  // $d = $s - $t
  virtual void sub(mips_reg d, mips_reg s, mips_reg t) = 0;
  virtual void subu(mips_reg d, mips_reg s, mips_reg t) = 0;
  // Hi|Lo = $s * $t
  virtual void mul(mips_reg s, mips_reg t) = 0;
  virtual void mulu(mips_reg s, mips_reg t) = 0;
  // HI|Lo = $s / $t
  virtual void div(mips_reg s, mips_reg t) = 0;
  virtual void divu(mips_reg s, mips_reg t) = 0;
  // $d = $s < %t ? 1 : 0
  virtual void slt(mips_reg d, mips_reg s, mips_reg t) = 0;
  virtual void sltu(mips_reg d, mips_reg s, mips_reg t) = 0;
  // $d = $s & %t 
  virtual void _and(mips_reg d, mips_reg s, mips_reg t) = 0;
  // $d = $s | %t 
  virtual void _or(mips_reg d, mips_reg s, mips_reg t) = 0;
  // $d = ~($s | %t)
  virtual void _nor(mips_reg d, mips_reg s, mips_reg t) = 0;
  // $d = $s ^ %t 
  virtual void _xor(mips_reg d, mips_reg s, mips_reg t) = 0;
  // $t = $s + i
  virtual void addi(mips_reg t, mips_reg s, s32 i) = 0;
  virtual void addiu(mips_reg t, mips_reg s, u32 i) = 0;
  // $t = $s < i ? 1 : 0
  virtual void slti(mips_reg t, mips_reg s, s32 i) = 0;
  virtual void sltiu(mips_reg t, mips_reg s, u32 i) = 0;
  // $t = $s & i
  virtual void andi(mips_reg t, mips_reg s, u32 i) = 0;
  virtual void ori(mips_reg t, mips_reg s, u32 i) = 0;
  virtual void xori(mips_reg t, mips_reg s, u32 i) = 0;
  // $t = [$s + i]
  virtual void lw(mips_reg t, mips_reg s, s32 i) = 0;
  // [$s + i] = $t
  virtual void sw(mips_reg t, mips_reg s, s32 i) = 0;
  // $t = byte[$s + i]
  virtual void lb(mips_reg t, mips_reg s, s32 i) = 0;
  virtual void lbu(mips_reg t, mips_reg s, s32 i) = 0;
  // byte[$s + i] = $t
  virtual void sb(mips_reg t, mips_reg s, s32 i) = 0;
  // $t = i;
  virtual void lui(mips_reg t, u32 i) = 0;
  // if ($t == $s) then pc += 4 + i<<2
  virtual void beq(mips_reg t, mips_reg s, s32 i) = 0;
  // if ($t != $s) then pc += 4 + i<<2
  virtual void bne(mips_reg t, mips_reg s, s32 i) = 0;
  // if $s <= 0 then pc += 4 + i<<2
  virtual void blez(mips_reg s, s32 i) = 0;
  // if $s > 0 then pc += 4 + i<<2
  virtual void bgtz(mips_reg s, s32 i) = 0;
  // if $s < 0 then pc += 4 + i<<2
  virtual void bltz(mips_reg s, s32 i) = 0;
  // pc = (0xF000'0000 & pc) | (i << 2)
  virtual void j(u32 i) = 0;
  // $ra = pc + 4; pc = (0xF000'0000 & pc) | (i << 2)
  virtual void jal(u32 i) = 0;
  // pc = $s;
  virtual void jr(mips_reg s);
  // $d = pc + 4; pc = $s;
  virtual void jalr(mips_reg d, mips_reg s);
  // $d = HI
  virtual void mfhi(mips_reg d);
  virtual void mflo(mips_reg d);
  // $t = cop0[d];
  virtual void mfc0(mips_reg t, mips_reg d);
};


// 如果指令解析出错返回 false
bool mips_decode(mips_instruction, InstructionReceiver*);

}