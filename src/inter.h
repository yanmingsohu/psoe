#pragma once

#include "util.h"
#include "mips.h"
#include "mem.h"
#include "bus.h"

namespace ps1e {


class InterpreterMips : public InstructionReceiver, public IrqReceiver {
private:
  Bus& bus;
  MipsReg reg;
  Cop0Reg cop0;
  u32 pc;
  u32 hi;
  u32 lo;
  u32 jump_delay_slot;
  u32 slot_delay_time;
  u32 slot_out_pc;

public:
  InterpreterMips(Bus& _bus) 
  : bus(_bus), reg({0}), cop0({0}), pc(0), hi(0), lo(0), 
    jump_delay_slot(0), slot_delay_time(0) {
  }

  void reset() {
    pc = MMU::BOOT_ADDR;
    cop0.sr.cu = 0b0101;
    cop0.sr.sr = 0;
  }

  void next() {
    if (slot_delay_time > 0) {
      if (--slot_delay_time == 0) {
        pc = slot_out_pc;
      }
    } 
    else if (jump_delay_slot) {
      slot_delay_time++;
      slot_out_pc = pc;
      pc = jump_delay_slot;
      jump_delay_slot = 0;
    }
    else {
      ready_recv_irq();
      process_exception();
    }

    if (pc & 0b11) {
      exception(ExeCodeTable::ADEL);
      return;
    }

    reg.zero = 0;
    u32 code = bus.read32(pc);
    if (!mips_decode(code, this)) {
      exception(ExeCodeTable::RI);
    }
  }

  MipsReg& getreg() {
    return reg;
  }

  void set_ext_int(CpuCauseInt i) {
    exception(ExeCodeTable::INT, i);
  }

  void clr_ext_int(CpuCauseInt i) {
    cop0.cause.ip &= ~(static_cast<u8>(i));
  }

  inline u32 has_exception() {
    if (cop0.sr.ie == 0) return 0;
    if (cop0.sr.exl == 1) return 0; // necessary?
    if (cop0.sr.erl == 1) return 0; // necessary?
    return (cop0.sr.im & cop0.cause.ip);
  }

private:
  void exception(ExeCodeTable e, CpuCauseInt i = CpuCauseInt::software) {
    cop0.cause.ip |= static_cast<u8>(i);
    if (!has_exception()) return;

    cop0.sr.exl = 1;
    cop0.cause.wp = 1;
    cop0.cause.ExcCode = static_cast<u32>(e);

    if (slot_delay_time) {
      cop0.cause.bd = 1;
      cop0.epc = pc - 4; 
    } else {
      cop0.cause.bd = 0;
      cop0.epc = pc;
    }
    // printf("Got exception %x\n", e);
  }

  void process_exception() {
    if (!has_exception()) return;
    debug("Got exception %x\n", cop0.cause.ExcCode);

    if (cop0.sr.bev) {
      // if tlb 0xBFC0'0100
      pc = 0xBFC0'0180;
    } else {
      // if tlb 0x8000'0000
      pc = 0x8000'0080;
    }
  }

  void nop() override {
    jj("NOP", 0);
    pc += 4;
  }

  void add(mips_reg d, mips_reg s, mips_reg t) override {
    rr("ADD", d, s, t);
    Overflow<s32> o(reg.s[s], reg.s[t]);
    reg.s[d] = reg.s[s] + reg.s[t]; 
    if (o.check(reg.s[d])) {
      exception(ExeCodeTable::OVF);
      return;
    }
    pc += 4;
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
    if (o.check(reg.s[d])) {
      exception(ExeCodeTable::OVF);
      return;
    }
    pc += 4;
  }

  void subu(mips_reg d, mips_reg s, mips_reg t) override {
    rr("SUBu", d, s, t);
    reg.u[d] = reg.u[s] - reg.u[t];
    pc += 4;
  }

  void mul(mips_reg s, mips_reg t) override {
    rx("MUL", s, t);
    sethl(static_cast<s64>(reg.s[s]) * reg.s[t]);
    pc += 4;
  }

  void mulu(mips_reg s, mips_reg t) override {
    rx("MULu", s, t);
    sethl(static_cast<u64>(reg.u[s]) * reg.u[t]);
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
    if (o.check(reg.s[t])) {
      exception(ExeCodeTable::OVF);
      return;
    }
    pc += 4;
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
    u32 addr = reg.u[s] + i;
    if (addr & 0b11) {
      exception(ExeCodeTable::ADEL);
      return;
    }
    reg.u[t] = bus.read32(addr);
    pc += 4;
  }

  void sw(mips_reg t, mips_reg s, s32 i) override {
    iw("SW", t, s, i);
    u32 addr = reg.u[s] + i;
    if (addr & 0b11) {
      exception(ExeCodeTable::ADES);
      return;
    }
    bus.write32(addr, reg.u[t]);
    pc += 4;
  }

  void lb(mips_reg t, mips_reg s, s32 i) override {
    ii("LB", t, s, i);
    reg.s[t] = (s8) bus.read8(reg.u[s] + i);
    pc += 4;
  }

  void lbu(mips_reg t, mips_reg s, s32 i) override {
    ii("LBu", t, s, i);
    reg.u[t] = bus.read8(reg.u[s] + i);
    pc += 4;
  }

  void sb(mips_reg t, mips_reg s, s32 i) override {
    iw("SB", t, s, i);
    bus.write8(reg.u[s] + i, 0xFF & reg.u[t]);
    pc += 4;
  }

  void lh(mips_reg t, mips_reg s, s32 i) {
    ii("LH", t, s, i);
    u32 addr = reg.u[s] + i;
    if (addr & 1) {
      exception(ExeCodeTable::ADEL);
      return;
    }
    reg.s[t] = bus.read16(addr);
    pc += 4;
  }

  void lhu(mips_reg t, mips_reg s, s32 i) {
    ii("LHu", t, s, i);
    u32 addr = reg.u[s] + i;
    if (addr & 1) {
      exception(ExeCodeTable::ADEL);
      return;
    }
    reg.u[t] = bus.read16(addr);
    pc += 4;
  }

  void sh(mips_reg t, mips_reg s, s32 i) {
    iw("SH", t, s, i);
    u32 addr = reg.u[s] + i;
    if (addr & 1) {
      exception(ExeCodeTable::ADES);
      return;
    }
    bus.write16(addr, reg.u[t]);
    pc += 4;
  }

  void lui(mips_reg t, u32 i) override {
    i2("LUi", t, i);
    reg.u[t] = i << 16;
    pc += 4;
  }

  void beq(mips_reg t, mips_reg s, s32 i) override {
    ii("BEQ", t, s, i);
    if (reg.u[t] == reg.u[s]) {
      jump_delay_slot = pc + 4;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void bne(mips_reg t, mips_reg s, s32 i) override {
    ii("BNE", t, s, i);
    if (reg.u[t] != reg.u[s]) {
      jump_delay_slot = pc + 4;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void blez(mips_reg s, s32 i) override {
    i2("BLEZ", s, i);
    if (reg.s[s] <= 0) {
      jump_delay_slot = pc + 4;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void bgtz(mips_reg s, s32 i) override {
    i2("BGTZ", s, i);
    if (reg.s[s] > 0) {
      jump_delay_slot = pc + 4;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void bltz(mips_reg s, s32 i) override {
    i2("BLTZ", s, i);
    if (reg.s[s] < 0) {
      jump_delay_slot = pc + 4;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void bgez(mips_reg s, s32 i) override {
    i2("BGEZ", s, i);
    if (reg.s[s] >= 0) {
      jump_delay_slot = pc + 4;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void bgezal(mips_reg s, s32 i) override {
    i2("BGEZAL", s, i);
    if (reg.s[s] >= 0) {
      jump_delay_slot = pc + 4;
      reg.ra = pc + 8;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void bltzal(mips_reg s, s32 i) override {
    i2("BLTZAL", s, i);
    if (reg.s[s] < 0) {
      jump_delay_slot = pc + 4;
      reg.ra = pc + 8;
      pc += i << 2;
    } else {
      pc += 4;
    }
  }

  void j(u32 i) override {
    jj("J", i);
    jump_delay_slot = pc + 4;
    pc = (pc & 0xF000'0000) | (i << 2);
  }

  void jal(u32 i) override {
    jj("JAL", i);
    jump_delay_slot = pc + 4;
    reg.ra = pc + 8;
    pc = (pc & 0xF000'0000) | (i << 2);
  }

  void jr(mips_reg s) override {
    j1("JR", s);
    jump_delay_slot = pc + 4;
    pc = reg.u[s];
  }

  void jalr(mips_reg d, mips_reg s) override {
    rx("JALR", d, s);
    jump_delay_slot = pc + 4;
    reg.u[d] = pc + 8;
    pc = reg.u[s];
  }

  void mfhi(mips_reg d) override {
    j1("MFHI", d);
    reg.u[d] = hi;
    pc += 4;
  }

  void mflo(mips_reg d) override {
    j1("MFLO", d);
    reg.u[d] = lo;
    pc += 4;
  }

  void mthi(mips_reg s) override {
    j1("MTHI", s);
    hi = reg.u[s];
    pc += 4;
  }

  void mtlo(mips_reg s) override {
    j1("MTLO", s);
    lo = reg.u[s];
    pc += 4;
  }

  void mfc0(mips_reg t, mips_reg d) override {
    i2("MFC0", t, d);
    reg.u[t] = cop0.r[d];
    pc += 4;
  }

  void mtc0(mips_reg t, mips_reg d) override {
    i2("MTC0", t, d);
    cop0.r[d] = reg.u[t];
    pc += 4;
  }

  void sll(mips_reg d, mips_reg t, u32 i) override {
    ii("SLL", d, t, i);
    reg.u[d] = reg.u[t] << i;
    pc += 4;
  }

  void sllv(mips_reg d, mips_reg t, mips_reg s) override {
    rr("SLLV", d, t, s);
    reg.u[d] = reg.u[t] << reg.u[s];
    pc += 4;
  }

  void sra(mips_reg d, mips_reg t, u32 i) override {
    ii("SRA", d, t, i);
    reg.u[d] = reg.s[t] >> i;
    pc += 4;
  }

  void srav(mips_reg d, mips_reg t, mips_reg s) override {
    rr("SRAV", d, t, s);
    reg.u[d] = reg.s[t] >> reg.u[s];
    pc += 4;
  }

  void srl(mips_reg d, mips_reg t, u32 i) override {
    ii("SRL", d, t, i);
    reg.u[d] = reg.u[t] >> i;
    pc += 4;
  }

  void srlv(mips_reg d, mips_reg t, mips_reg s) override {
    rr("SRLV", d, t, s);
    reg.u[d] = reg.u[t] >> reg.u[s];
    pc += 4;
  }

  void syscall() {
    jj("SYSCALL", reg.v0);
    exception(ExeCodeTable::SYS);
  }

  void brk(u32 code) {
    jj("BREAK", code);
    exception(ExeCodeTable::BP);
  }

  void rfe() override {
    cop0.sr.exl = 0;
    pc = cop0.epc;
  }
 
private:

  inline void sethl(u64 x) {
    hi = (x & 0xFFFF'FFFF'0000'0000) >> 32;
    lo =  x & 0xFFFF'FFFF;
  }

  inline void rx(char const* iname, mips_reg s, mips_reg t) {
    debug("%08x | %08x %6s H/L, $%s, $%s \t\t # $%s=%x, $%s=%x\n", 
      pc, bus.read32(pc), iname, rname(s), rname(t), 
      rname(s), reg.u[s], rname(t), reg.u[t]);
  }

  inline void rr(char const* iname, mips_reg d, mips_reg s, mips_reg t) {
    debug("%08x | %08x %6s $%s, $%s, $%s \t\t # $%s=%x, $%s=%x, $%s=%x\n",
      pc, bus.read32(pc), iname, rname(d), rname(s), rname(t),
      rname(d), reg.u[d], rname(s), reg.u[s], rname(t), reg.u[t]);
  }

  inline void ii(char const* iname, mips_reg t, mips_reg s, s16 i) {
    debug("%08x | %08x %6s $%s, $%s, 0x%x \t # $%s=%x, $%s=%x, jump=%x\n",
      pc, bus.read32(pc), iname, rname(t), rname(s), i,
      rname(t), reg.u[t], rname(s), reg.u[s], pc + (i << 2));
  }

  inline void iw(char const* iname, mips_reg t, mips_reg s, s16 i) {
    debug("%08x | %08x %6s [$%s + 0x%x], $%s\t\t # $%s=%x, $%s=%x\n",
      pc, bus.read32(pc), iname, rname(s), i, rname(t),
      rname(t), reg.u[t], rname(s), reg.u[s]);
  }

  inline void i2(char const* iname, mips_reg t, s16 i) {
    debug("%08x | %08x %6s $%s, 0x%x\t\t # $%s=%x\n",
      pc, bus.read32(pc), iname, rname(t), i, rname(t), reg.u[t]);
  }

  inline void jj(char const* iname, u32 i) {
    debug("%08x | %08x %6s 0x%x\t\t # %x\n", pc, bus.read32(pc), iname, i, i<<2);
  }

  inline void j1(char const* iname, mips_reg s) {
    debug("%08x | %08x %6s %s\t\t # $%s=%x\n",
      pc, bus.read32(pc), iname, rname(s), rname(s), reg.u[s]);
  }

  inline const char* rname(mips_reg s) {
    #ifdef SAFE_MEM
    if (s & (~0x1f)) {
      error("! Invaild reg index %d\n", s);
      return 0;
    }
    #endif
    return MipsRegName[s];
  }
};


} // namespace ps1e
