#pragma once

#include "util.h"
#include "cpu.h"

namespace ps1e {

typedef u32 mips_instruction;
typedef u8  mips_reg;


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
    u32 imm : 16;
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
  // $t = $s + i
  virtual void addi(mips_reg s, mips_reg t, s16 i) = 0;
  // $d = $s + $t
  virtual void add(mips_reg s, mips_reg d, mips_reg t) = 0;
};



class DisassemblyMips : public InstructionReceiver {
public:
  MipsReg reg;

  DisassemblyMips() : reg({0}) {}

  void addi(mips_reg s, mips_reg t, s16 i) {
    printf("ADDI $%d, $%d, %d\n", s, t, i);
    reg.r[t] = (s32)reg.r[s] + i;
  }

  void add(mips_reg s, mips_reg d, mips_reg t) {
    printf("ADD $%d, $%d, $%d", s, t, t);
    reg.r[d] = reg.r[s] + reg.r[t]; 
  }
};



// 如果指令解析出错返回 false
bool mips_decode(mips_instruction, InstructionReceiver*);

}