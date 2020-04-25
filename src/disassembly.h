#pragma once

#include "util.h"
#include "mips.h"

namespace ps1e {


class DisassemblyMips : public InstructionReceiver {
private:
  int ram_length;
  u8* ram;
  MipsReg reg;
  Cop0Reg cop0;
  u32 pc;
  u32 hi;
  u32 lo;

public:
  DisassemblyMips(u8 *_ram, u32 ram_len) : reg({0}), pc(0), hi(0), lo(0), cop0({0}) {
    ram = _ram;
    ram_length = ram_len;
  }

  void run() {
    u32 code = *((u32*)ram[pc]);
    if (!mips_decode(code, this)) {
      exception(MispException::opcode);
    }
  }

  MipsReg& getreg() {
    return reg;
  }

private:
  void nop() override {
    printf("NOP\n");
  }

  void add(mips_reg d, mips_reg s, mips_reg t) override {
    rr("ADD", d, s, t);
    Overflow<s32> o(reg.s[s], reg.s[t]);
    reg.s[d] = reg.s[s] + reg.s[t]; 
    pc += 4;
    if (o.check(reg.s[d])) {
      exception(MispException::overflow);
    }
  }

  void addu(mips_reg d, mips_reg s, mips_reg t) override {
    rr("ADDu", d, s, t);
    reg.u[d] = reg.u[s] + reg.u[t]; 
    pc += 4;
  }

  void sub(mips_reg d, mips_reg s, mips_reg t) override {
    rr("SUB", d, s, t);
    Overflow<s32> o(reg.s[s], reg.s[t]);
    reg.s[d] = reg.s[s] - reg.s[t];
    pc += 4;
    if (o.check(reg.s[d])) {
      exception(MispException::overflow);
    }
  }

  void subu(mips_reg d, mips_reg s, mips_reg t) override {
    rr("SUBu", d, s, t);
    reg.u[d] = reg.u[s] - reg.u[t];
    pc += 4;
  }

  void mul(mips_reg s, mips_reg t) override {
    rx("MUL", s, t);
    sethl(reg.s[s] * reg.s[t]);
    pc += 4;
  }

  void mulu(mips_reg s, mips_reg t) override {
    rx("MULu", s, t);
    sethl(reg.u[s] * reg.u[t]);
    pc += 4;
  }

  void div(mips_reg s, mips_reg t) override {
    rx("DIV", s, t);
    lo = reg.s[s] / reg.s[t];
    hi = reg.s[s] % reg.s[t];
    pc += 4;
  }

  void divu(mips_reg s, mips_reg t) override {
    rx("DIVu", s, t);
    lo = reg.u[s] / reg.u[t];
    hi = reg.u[s] % reg.u[t];
    pc += 4;
  }

  void slt(mips_reg d, mips_reg s, mips_reg t) override {
    rr("SLT", d, s, t);
    reg.s[d] = reg.s[s] < reg.s[t] ? 1 : 0;
    pc += 4;
  }

  void sltu(mips_reg d, mips_reg s, mips_reg t) override {
    rr("SLTu", d, s, t);
    reg.u[d] = reg.u[s] < reg.u[t] ? 1 : 0;
    pc += 4;
  }

  void _and(mips_reg d, mips_reg s, mips_reg t) override {
    rr("AND", d, s, t);
    reg.u[d] = reg.u[s] & reg.u[t];
    pc += 4;
  }

  void _or(mips_reg d, mips_reg s, mips_reg t) override {
    rr("OR", d, s, t);
    reg.u[d] = reg.u[s] | reg.u[t];
    pc += 4;
  }

  void _nor(mips_reg d, mips_reg s, mips_reg t) override {
    rr("NOR", d, s, t);
    reg.u[d] = ~(reg.u[s] | reg.u[t]);
    pc += 4;
  }

  void _xor(mips_reg d, mips_reg s, mips_reg t) override {
    rr("XOR", d, s, t);
    reg.u[d] = reg.u[s] ^ reg.u[t];
    pc += 4;
  }

  void addi(mips_reg t, mips_reg s, s32 i) override {
    ii("ADDi", t, s, i);
    Overflow<s32> o(reg.s[s], i);
    reg.s[t] = reg.s[s] + i;
    pc += 4;
    if (o.check(reg.s[t])) {
      exception(MispException::overflow);
    }
  }

  void addiu(mips_reg t, mips_reg s, u32 i) override {
    ii("ADDiu", t, s, i);
    reg.u[t] = reg.u[s] + i;
    pc += 4;
  }

  void slti(mips_reg t, mips_reg s, s32 i) override {
    ii("SLTi", t, s, i);
    reg.s[t] = reg.s[s] < i ? 1 : 0;
    pc += 4;
  }

  void sltiu(mips_reg t, mips_reg s, u32 i) override {
    ii("SLTiu", t, s, i);
    reg.u[t] = reg.u[s] < i ? 1 : 0;
    pc += 4;
  }

  void andi(mips_reg t, mips_reg s, u32 i) override {
    ii("ANDi", t, s, i);
    reg.u[t] = reg.u[s] & i;
    pc += 4;
  }

  void ori(mips_reg t, mips_reg s, u32 i) override {
    ii("ORi", t, s, i);
    reg.u[t] = reg.u[s] | i;
    pc += 4;
  }

  void xori(mips_reg t, mips_reg s, u32 i) override {
    ii("XORi", t, s, i);
    reg.u[t] = reg.u[s] ^ i;
    pc += 4;
  }

  void lw(mips_reg t, mips_reg s, s32 i) override {
    ii("LW", t, s, i);
    reg.u[t] = read32(reg.u[s] + i);
    pc += 4;
  }

  void sw(mips_reg t, mips_reg s, s32 i) override {
    iw("SW", t, s, i);
    write32(reg.u[s] + i, reg.u[t]);
    pc += 4;
  }

  void lb(mips_reg t, mips_reg s, s32 i) override {
    ii("LB", t, s, i);
    reg.s[t] = (s8) read8(reg.u[s] + i);
    pc += 4;
  }

  void lbu(mips_reg t, mips_reg s, s32 i) override {
    ii("LBu", t, s, i);
    reg.u[t] = read8(reg.u[s] + i);
    pc += 4;
  }

  void sb(mips_reg t, mips_reg s, s32 i) override {
    iw("SB", t, s, i);
    write8(reg.u[s] + i, 0xFF & reg.u[t]);
    pc += 4;
  }

  void lui(mips_reg t, u32 i) override {
    i2("LUi", t, i);
    reg.u[t] = i;
    pc += 4;
  }

  void beq(mips_reg t, mips_reg s, s32 i) override {
    ii("BEQ", t, s, i);
    if (reg.u[t] == reg.u[s]) {
      pc += i << 2;
    } 
    pc += 4;
  }

  void bne(mips_reg t, mips_reg s, s32 i) override {
    ii("BNE", t, s, i);
    if (reg.u[t] != reg.u[s]) {
      pc += i << 2;
    }
    pc += 4;
  }

  void blez(mips_reg s, s32 i) override {
    i2("BLEZ", s, i);
    if (reg.s[s] <= 0) {
      pc += i << 2;
    }
    pc += 4;
  }

  void bgtz(mips_reg s, s32 i) override {
    i2("BGTZ", s, i);
    if (reg.s[s] > 0) {
      pc += i << 2;
    }
    pc += 4;
  }

  void bltz(mips_reg s, s32 i) override {
    i2("BLTZ", s, i);
    if (reg.s[s] < 0) {
      pc += i << 2;
    }
    pc += 4;
  }

  void j(u32 i) override {
    jj("J", i);
    pc = (pc & 0xF000'0000) | (i << 2);
  }

  void jal(u32 i) override {
    jj("JAL", i);
    reg.ra = pc + 4;
    pc = (pc & 0xF000'0000) | (i << 2);
  }

  void jr(mips_reg s) override {
    j1("JR", s);
    pc = reg.u[s];
  }

  void jalr(mips_reg d, mips_reg s) override {
    rx("JALR", d, s);
    reg.u[d] = pc + 4;
    pc = reg.u[s];
  }

  void mfhi(mips_reg d) override {
    j1("MFHI", d);
    reg.u[d] = hi;
  }

  void mflo(mips_reg d) override {
    j1("MFLO", d);
    reg.u[d] = lo;
  }

  void mfc0(mips_reg t, mips_reg d) override {
    i2("mfc0", t, d);
    reg.u[t] = cop0.r[d];
  }

public:
  s32 read32(u32 addr) {
    if ((addr & 0b11) || addr >= ram_length) {
      exception(MispException::address);
    }
    return *(s32*)(ram[addr]);
  }

  u8 read8(u32 addr) {
    if (addr >= ram_length) {
      exception(MispException::address);
    }
    return ram[addr];
  }

  void write8(u32 addr, u8 val) {
    if (addr >= ram_length) {
      exception(MispException::address);
    }
    ram[addr] = val;
  }

  void write32(u32 addr, u32 val) {
    if ((addr & 0b11) || addr >= ram_length) {
      exception(MispException::address);
    }
    *(s32*)(ram[addr]) = val;
  }
 
private:
  void exception(MispException e) {
    switch (e) {
      case MispException::overflow:
        printf("Exception overflow\n");
        break;
    }
  }

  void sethl(u64 x) {
    hi = (x & 0xFFFF'FFFF'0000'0000) >> 32;
    lo =  x & 0xFFFF'FFFF;
  }

  void rx(char const* iname, mips_reg s, mips_reg t) {
    printf("%04x %s\t H/L, $%s, $%s", pc, iname, MipsRegName[s], MipsRegName[t]);
  }

  void rr(char const* iname, mips_reg d, mips_reg s, mips_reg t) {
    printf("%04x %s\t $%s, $%s, $%s", pc, iname, MipsRegName[d], MipsRegName[s], MipsRegName[t]);
  }

  void ii(char const* iname, mips_reg t, mips_reg s, s16 i) {
    printf("%04x %s\t $%s, $%s, %d\n", pc, iname, MipsRegName[t], MipsRegName[s], i);
  }

  void iw(char const* iname, mips_reg t, mips_reg s, s16 i) {
    printf("%04x %s\t [$%s + i], $%s\n", pc, iname, MipsRegName[s], i, MipsRegName[t]);
  }

  void i2(char const* iname, mips_reg t, s16 i) {
    printf("%04x %s\t $%s, %d", pc, iname, MipsRegName[t], i);
  }

  void jj(char const* iname, u32 i) {
    printf("%04x %s\t %d", pc, iname, i);
  }

  void j1(char const* iname, mips_reg s) {
    printf("%04x %s\t %s", pc, iname, MipsRegName[s]);
  }
};


} // namespace ps1e
