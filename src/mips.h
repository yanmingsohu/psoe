#pragma once

#include "util.h"
#include "cpu.h"

namespace ps1e {

typedef u32 mips_instruction;
typedef u8  mips_reg;


#pragma pack(21)
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
#pragma pack(0)


class InstructionReceiver {
public:
  virtual ~InstructionReceiver() {}

  virtual void nop() = 0;
  virtual void syscall() = 0;
  virtual void brk(u32 code) = 0;

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
  // $t = i;
  virtual void lui(mips_reg t, u32 i) = 0;

  // $t = [$s + i]
  virtual void lw(mips_reg t, mips_reg s, s32 i) = 0;
  // [$s + i] = $t
  virtual void sw(mips_reg t, mips_reg s, s32 i) = 0;
  // $t = byte[$s + i]
  virtual void lb(mips_reg t, mips_reg s, s32 i) = 0;
  virtual void lbu(mips_reg t, mips_reg s, s32 i) = 0;
  // byte[$s + i] = $t
  virtual void sb(mips_reg t, mips_reg s, s32 i) = 0;
  // $t = s16[$s + i]
  virtual void lh(mips_reg t, mips_reg s, s32 i) = 0;
  // $t = u16[$s + i]
  virtual void lhu(mips_reg t, mips_reg s, s32 i) = 0;
  // s[$s + i] = $t
  virtual void sh(mips_reg t, mips_reg s, s32 i) = 0;

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
  // if $s >= 0 then pc += 4 + i<<2
  virtual void bgez(mips_reg s, s32 i) = 0;
  // if $s >= 0 then $ra = pc; pc += 4 + i<<2
  virtual void bgezal(mips_reg s, s32 i) = 0;
  // if $s < 0 then $ra = pc; pc += 4 + i<<2
  virtual void bltzal(mips_reg s, s32 i) = 0;

  // pc = (0xF000'0000 & pc) | (i << 2)
  virtual void j(u32 i) = 0;
  // $ra = pc + 4; pc = (0xF000'0000 & pc) | (i << 2)
  virtual void jal(u32 i) = 0;
  // pc = $s;
  virtual void jr(mips_reg s) = 0;
  // $d = pc + 4; pc = $s;
  virtual void jalr(mips_reg d, mips_reg s) = 0;
  // cop0.exl = 0; pc = epc; 
  virtual void rfe() = 0;

  // $d = HI / LO
  virtual void mfhi(mips_reg d) = 0;
  virtual void mflo(mips_reg d) = 0;
  // HI / LO = $s
  virtual void mthi(mips_reg s) = 0;
  virtual void mtlo(mips_reg s) = 0;
  // $t = cop0[$d];
  virtual void mfc0(mips_reg t, mips_reg d) = 0;
  // cop0[$d] = $t;
  virtual void mtc0(mips_reg t, mips_reg d) = 0;

  // $d = $t << i
  virtual void sll(mips_reg d, mips_reg t, u32 i) = 0;
  // $d = $t << %s
  virtual void sllv(mips_reg d, mips_reg t, mips_reg s) = 0;
  // $d = $t >> i, fill by sign
  virtual void sra(mips_reg d, mips_reg t, u32 i) = 0;
  virtual void srav(mips_reg d, mips_reg t, mips_reg s) = 0;
  // $d = $t >> i, fill by zero
  virtual void srl(mips_reg d, mips_reg t, u32 i) = 0;
  // $d = $t >> %s, fill by zero
  virtual void srlv(mips_reg d, mips_reg t, mips_reg s) = 0;
};


// 如果指令解析出错返回 false
bool mips_decode(mips_instruction, InstructionReceiver*);

}