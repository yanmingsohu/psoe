#pragma once 

#include "util.h"
#include "mips.h"
#include "mem.h"
#include "bus.h"
#include "gte.h"

namespace ps1e {


class R3000A : public IrqReceiver {
private:
  Bus& bus;
  MipsReg reg;
  Cop0Reg cop0;
  GTE gte;
  u32 pc;
  u32 hi;
  u32 lo;
  u32 slot_over_pc;
  bool on_slot_time;

public:
  // 仅用于统计调试, 无实际用途
  u32 exception_counter = 0;

  R3000A(Bus& _bus)  : 
      bus(_bus), cop0({0}), pc(0), hi(0), lo(0), 
      slot_over_pc(0), on_slot_time(false)
  {
    reset();
  }

  void reset(const u32 regVal = 0xE1E3'7E73) {
    pc = MMU::BOOT_ADDR;
    cop0.sr.cu0 = 1;
    cop0.sr.cu2 = 1;
    cop0.sr.ie  = 1;
    cop0.sr.im  = 0xFF;
    cop0.sr.bev = 1;
    cop0.dcic.v = 0;

    for (int i=0; i<MIPS_REG_COUNT; ++i) {
      reg.u[i] = regVal;
    }
    reg.zero = 0;
  }

  void next() {
    u32 npc = pc;
    ready_recv_irq();
    check_exe_break();
    process_exception();

    if (npc & 0b11) {
      exception(ExeCodeTable::ADEL, true);
      return;
    }

    // 跳过一个延迟槽指令的检测.
    // 如果 npc 指向跳转指令, 该指令会设置延迟槽 prejump
    // 下一条指令则进入延迟槽的处理
    bool is_on_slot = on_slot_time;

    reg.zero = 0;
    u32 code = bus.read32(npc);
    if (!mips_decode(code, this)) {
      exception(ExeCodeTable::RI, true);
    }
    
    if (is_on_slot) {
      pc = slot_over_pc;
      on_slot_time = false;
    }
  }

  MipsReg& getreg() {
    return reg;
  }

  void set_ext_int(CpuCauseInt i) {
    exception(ExeCodeTable::INT, false, i);
  }

  void clr_ext_int(CpuCauseInt i) {
    cop0.cause.ip &= ~(static_cast<u8>(i));
  }

  inline u32 has_exception() {
    if (cop0.sr.ie == 0) return 0;
    return (cop0.sr.im & cop0.cause.ip);
  }

  void send_bus_exception() {
    exception(ExeCodeTable::DBW, false);
  }
  
  // 当 cpu 执行到指定地址时中断
  void set_int_exc_point(u32 addr, u32 mask) {
    cop0.bpc = addr;
    cop0.bpcm = mask;
    cop0.dcic.v = COP0_DCIC_BK_CODE_MK;
  }

  // 读取数据总线时发生中断
  void set_data_rw_point(u32 addr, u32 mask) {
    cop0.bda = addr;
    cop0.bdam = mask;
    cop0.dcic.v = COP0_DCIC_BK_DATA_MK;
  }

  // 返回 pc 的当前值
  u32 getpc() const {
    return pc;
  }

private:
  void exception(ExeCodeTable e, bool from_instruction, CpuCauseInt i = CpuCauseInt::software) {
    ++exception_counter;
    cop0.cause.ip |= static_cast<u8>(i);
    if (!has_exception()) {
      printf(YELLOW("SKIP exception (%X)%s PC=%x\n"), e, MipsCauseStr[static_cast<u32>(e)], pc);
      if (from_instruction) {
        pc += 4;
      }
      return;
    }

    cop0.cause.ExcCode = static_cast<u32>(e);
  }

  void process_exception() {
    if (!has_exception()) return;
    printf(YELLOW("Got exception (%X)%s sr:%x [PC:0x%08x, 0x%08x]\n"), 
      cop0.cause.ExcCode, MipsCauseStr[cop0.cause.ExcCode], cop0.sr.v, pc, bus.read32(pc));
      
    cop0.sr.KUc = 0;
    cop0.cause.wp = 1;

    if (on_slot_time) {
      cop0.cause.bd = 1;
      cop0.epc = pc - 4; 
    } else {
      cop0.cause.bd = 0;
      cop0.epc = pc;
    }

    if (cop0.sr.bev) {
      // if tlb 0xBFC0'0100
      if (cop0.cause.ExcCode == static_cast<u32>(ExeCodeTable::BP)) {
        pc = 0xBFC0'0140;
      } else {
        pc = 0xBFC0'0180;
      }
    } else {
      // if tlb 0x8000'0000
      if (cop0.cause.ExcCode == static_cast<u32>(ExeCodeTable::BP)) {
        pc = 0x8000'0040;
      } else {
        pc = 0x8000'0080;
      }
    }
    cop0.sr.v = SET_BIT(cop0.sr.v, COP0_SR_RFE_SHIFT_MASK, cop0.sr.v << 2);
  }

  void prejump(u32 target_pc) {
    slot_over_pc = target_pc;
    on_slot_time = true;
  }

public:
  void nop() {
    pc += 4;
  }

  void add(mips_reg d, mips_reg s, mips_reg t) {
    Overflow<s32> o(reg.s[s], reg.s[t]);
    reg.s[d] = reg.s[s] + reg.s[t]; 
    if (o.check(reg.s[d])) {
      exception(ExeCodeTable::OVF, true);
      return;
    }
    pc += 4;
  }

  void addu(mips_reg d, mips_reg s, mips_reg t) {
    reg.u[d] = reg.u[s] + reg.u[t]; 
    pc += 4;
  }

  void sub(mips_reg d, mips_reg s, mips_reg t) {
    Overflow<s32> o(reg.s[s], reg.s[t]);
    reg.s[d] = reg.s[s] - reg.s[t];
    if (o.check(reg.s[d])) {
      exception(ExeCodeTable::OVF, true);
      return;
    }
    pc += 4;
  }

  void subu(mips_reg d, mips_reg s, mips_reg t) {
    reg.u[d] = reg.u[s] - reg.u[t];
    pc += 4;
  }

  void mul(mips_reg s, mips_reg t) {
    sethl(static_cast<s64>(reg.s[s]) * reg.s[t]);
    pc += 4;
  }

  void mulu(mips_reg s, mips_reg t) {
    sethl(static_cast<u64>(reg.u[s]) * reg.u[t]);
    pc += 4;
  }

  void div(mips_reg s, mips_reg t) {
    lo = reg.s[s] / reg.s[t];
    hi = reg.s[s] % reg.s[t];
    pc += 4;
  }

  void divu(mips_reg s, mips_reg t) {
    lo = reg.u[s] / reg.u[t];
    hi = reg.u[s] % reg.u[t];
    pc += 4;
  }

  void slt(mips_reg d, mips_reg s, mips_reg t) {
    reg.s[d] = reg.s[s] < reg.s[t] ? 1 : 0;
    pc += 4;
  }

  void sltu(mips_reg d, mips_reg s, mips_reg t) {
    reg.u[d] = reg.u[s] < reg.u[t] ? 1 : 0;
    pc += 4;
  }

  void _and(mips_reg d, mips_reg s, mips_reg t) {
    reg.u[d] = reg.u[s] & reg.u[t];
    pc += 4;
  }

  void _or(mips_reg d, mips_reg s, mips_reg t) {
    reg.u[d] = reg.u[s] | reg.u[t];
    pc += 4;
  }

  void _nor(mips_reg d, mips_reg s, mips_reg t) {
    reg.u[d] = ~(reg.u[s] | reg.u[t]);
    pc += 4;
  }

  void _xor(mips_reg d, mips_reg s, mips_reg t) {
    reg.u[d] = reg.u[s] ^ reg.u[t];
    pc += 4;
  }

  void addi(mips_reg t, mips_reg s, s32 i) {
    Overflow<s32> o(reg.s[s], i);
    reg.s[t] = reg.s[s] + i;
    if (o.check(reg.s[t])) {
      exception(ExeCodeTable::OVF, true);
      return;
    }
    pc += 4;
  }

  void addiu(mips_reg t, mips_reg s, s32 i) {
    reg.u[t] = reg.u[s] + i;
    pc += 4;
  }

  void slti(mips_reg t, mips_reg s, s32 i) {
    reg.s[t] = reg.s[s] < i ? 1 : 0;
    pc += 4;
  }

  void sltiu(mips_reg t, mips_reg s, u32 i) {
    reg.u[t] = reg.u[s] < i ? 1 : 0;
    pc += 4;
  }

  void andi(mips_reg t, mips_reg s, u32 i) {
    reg.u[t] = reg.u[s] & i;
    pc += 4;
  }

  void ori(mips_reg t, mips_reg s, u32 i) {
    reg.u[t] = reg.u[s] | i;
    pc += 4;
  }

  void xori(mips_reg t, mips_reg s, u32 i) {
    reg.u[t] = reg.u[s] ^ i;
    pc += 4;
  }

  void lw(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (addr & 0b11) {
      exception(ExeCodeTable::ADEL, true);
      return;
    }
    if (check_data_read_break(addr)) {
      return;
    }
    reg.u[t] = bus.read32(addr);
    pc += 4;
  }

  void sw(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (addr & 0b11) {
      exception(ExeCodeTable::ADES, true);
      return;
    }
    if (check_data_write_break(addr)) {
      return;
    }
    bus.write32(addr, reg.u[t]);
    pc += 4;
  }

  void lb(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (check_data_read_break(addr)) {
      return;
    }
    reg.s[t] = (s8) bus.read8(addr);
    pc += 4;
  }

  void lbu(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (check_data_read_break(addr)) {
      return;
    }
    reg.u[t] = bus.read8(addr);
    pc += 4;
  }

  void sb(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (check_data_write_break(addr)) {
      return;
    }
    bus.write8(addr, 0xFF & reg.u[t]);
    pc += 4;
  }

  void lh(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (addr & 1) {
      exception(ExeCodeTable::ADEL, true);
      return;
    }
    if (check_data_read_break(addr)) {
      return;
    }
    reg.s[t] = (s16) bus.read16(addr);
    pc += 4;
  }

  void lhu(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (addr & 1) {
      exception(ExeCodeTable::ADEL, true);
      return;
    }
    if (check_data_read_break(addr)) {
      return;
    }
    reg.u[t] = bus.read16(addr);
    pc += 4;
  }

  void sh(mips_reg t, mips_reg s, s32 i) {
    u32 addr = reg.u[s] + i;
    if (addr & 1) {
      exception(ExeCodeTable::ADES, true);
      return;
    }
    if (check_data_write_break(addr)) {
      return;
    }
    bus.write16(addr, reg.u[t]);
    pc += 4;
  }

  void lui(mips_reg t, u32 i) {
    reg.u[t] = i << 16;
    pc += 4;
  }

  void lwl(mips_reg t, mips_reg s, u32 i) {
    u32 addr = reg.u[s] + i;
    u32 v = bus.read32(addr & ~0b11);
    switch (addr & 3) {
      case 0:
        reg.u[t] = (reg.u[t] & 0x00ff'ffff) | (v << 24);
        break;
      case 1:
        reg.u[t] = (reg.u[t] & 0x0000'ffff) | (v << 16);
        break;
      case 2:
        reg.u[t] = (reg.u[t] & 0x0000'00ff) | (v << 8);
        break;
      case 3:
        reg.u[t] = v;
        break;
    }
    pc += 4;
  }

  void lwr(mips_reg t, mips_reg s, u32 i) {
    u32 addr = reg.u[s] + i;
    u32 v = bus.read32(addr & ~0b11);
    switch (addr & 3) {
      case 0:
        reg.u[t] = v;
        break;
      case 1:
        reg.u[t] = (reg.u[t] & 0xff00'0000) | (v >> 8);
        break;
      case 2:
        reg.u[t] = (reg.u[t] & 0xffff'0000) | (v >> 16);
        break;
      case 3:
        reg.u[t] = (reg.u[t] & 0xffff'ff00) | (v >> 24);
        break;
    }
    pc += 4;
  }

  void swl(mips_reg t, mips_reg s, u32 i) {
    u32 addr = reg.u[s] + i;
    u32 v = bus.read32(addr & ~0b11);
    switch (addr & 3) {
      case 0:
        bus.write32(addr & ~0b11, (v & 0xffff'ff00) | (reg.u[t] >> 24));
        break;
      case 1:
        bus.write32(addr & ~0b11, (v & 0xffff'0000) | (reg.u[t] >> 16));
        break;
      case 2:
        bus.write32(addr & ~0b11, (v & 0xff00'0000) | (reg.u[t] >> 8));
        break;
      case 3:
        bus.write32(addr & ~0b11, reg.u[t]);
        break;
    }
    pc += 4;
  }

  void swr(mips_reg t, mips_reg s, u32 i) {
    u32 addr = reg.u[s] + i;
    u32 v = bus.read32(addr & ~0b11);
    switch (addr & 3) {
      case 0:
        bus.write32(addr & ~0b11, reg.u[t]);
        break;
      case 1:
        bus.write32(addr & ~0b11, (v & 0x0000'00ff) | (reg.u[t] << 8));
        break;
      case 2:
        bus.write32(addr & ~0b11, (v & 0x0000'ffff) | (reg.u[t] << 16));
        break;
      case 3:
        bus.write32(addr & ~0b11, (v & 0x00ff'ffff) | (reg.u[t] << 24));
        break;
    }
    pc += 4;
  }

  void beq(mips_reg t, mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.u[t] == reg.u[s]) {
      prejump(pc + (i << 2));
    }
  }

  void bne(mips_reg t, mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.u[t] != reg.u[s]) {
      prejump(pc + (i << 2));
    }
  }

  void blez(mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.s[s] <= 0) {
      prejump(pc + (i << 2));
    }
  }

  void bgtz(mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.s[s] > 0) {
      prejump(pc + (i << 2));
    }
  }

  void bltz(mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.s[s] < 0) {
      prejump(pc + (i << 2));
    }
  }

  void bgez(mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.s[s] >= 0) {
      prejump(pc + (i << 2));
    }
  }

  void bgezal(mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.s[s] >= 0) {
      reg.ra = pc + 4;
      prejump(pc + (i << 2));
    }
  }

  void bltzal(mips_reg s, s32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    if (reg.s[s] < 0) {
      reg.ra = pc + 4;
      prejump(pc + (i << 2));
    }
  }

  void j(u32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    prejump((pc & 0xF000'0000) | (i << 2));
  }

  void jal(u32 i) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    reg.ra = pc + 4;
    prejump((pc & 0xF000'0000) | (i << 2));
  }

  void jr(mips_reg s) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    prejump(reg.u[s]);
  }

  void jalr(mips_reg d, mips_reg s) {
    if (check_jump_break()) {
      return;
    }
    pc += 4;
    reg.u[d] = pc + 4;
    prejump(reg.u[s]);
  }

  void mfhi(mips_reg d) {
    reg.u[d] = hi;
    pc += 4;
  }

  void mflo(mips_reg d) {
    reg.u[d] = lo;
    pc += 4;
  }

  void mthi(mips_reg s) {
    hi = reg.u[s];
    pc += 4;
  }

  void mtlo(mips_reg s) {
    lo = reg.u[s];
    pc += 4;
  }

  // i 在指令解码时被限制到 5 位
  void sll(mips_reg d, mips_reg t, u32 i) {
    reg.u[d] = reg.u[t] << i;
    pc += 4;
  }

  void sllv(mips_reg d, mips_reg t, mips_reg s) {
    reg.u[d] = reg.u[t] << (reg.u[s] & 0x1f);
    pc += 4;
  }

  void sra(mips_reg d, mips_reg t, u32 i) {
    reg.s[d] = reg.s[t] >> i;
    pc += 4;
  }

  void srav(mips_reg d, mips_reg t, mips_reg s) {
    reg.s[d] = reg.s[t] >> (reg.u[s] & 0x1f);
    pc += 4;
  }

  void srl(mips_reg d, mips_reg t, u32 i) {
    reg.u[d] = reg.u[t] >> i;
    pc += 4;
  }

  void srlv(mips_reg d, mips_reg t, mips_reg s) {
    reg.u[d] = reg.u[t] >> (reg.u[s] & 0x1f);
    pc += 4;
  }

  void syscall() {
    exception(ExeCodeTable::SYS, true);
  }

  void brk(u32 code) {
    exception(ExeCodeTable::BP, true);
  }

  void rfe() {
    u32 sr = (COP0_SR_RFE_SHIFT_MASK & cop0.sr.v) >> 2;
    cop0.sr.v = (COP0_SR_RFE_RESERVED_MASK & cop0.sr.v) | sr;
  }

  void mfc0(mips_reg t, mips_reg d) {
    reg.u[t] = cop0.r[d];
    pc += 4;
  }

  void mtc0(mips_reg t, mips_reg d) {
    switch (d) {
      case COP0_CAUSE_REG_IDX:
        cop0.r[d] = setbit_with_mask<u32>(cop0.r[d], reg.u[t], COP0_CAUSE_RW_MASK);
        break;
      case COP0_DCIC_REG_INDEX:
        cop0.r[d] = setbit_with_mask<u32>(cop0.r[d], reg.u[t], COP0_DCIC_WRITE_MASK);
        break;
      case COP0_SR_REG_IDX:
        cop0.r[d] = reg.u[t];
        bus.set_used_dcache(cop0.sr.isc);
        break;
      default:
        cop0.r[d] = reg.u[t];
        break;
    }
    pc += 4;
  }

  void mfc2(mips_reg t, gte_dr d) {
    reg.u[t] = gte.read_data(d);
    pc += 4;
  }

  void mtc2(mips_reg t, gte_dr d) {
    gte.write_data(d, reg.u[t]);
    pc += 4;
  }

  void cfc2(mips_reg t, gte_cr d) {
    reg.u[t] = gte.read_ctrl(d);
    pc += 4;
  }

  void ctc2(mips_reg t, gte_cr d) {
    gte.write_ctrl(d, reg.u[t]);
    pc += 4;
  }

  void bc2f(u32 imm) {
    if (!gte.read_flag()) {
      j(imm);
    }
  }

  void bc2t(u32 imm) {
    if (gte.read_flag()) {
      j(imm);
    }
  }

  void lwc2(gte_dr t, mips_reg s, u32 i) {
    u32 addr = reg.u[s] + i;
    if (addr & 0b11) {
      exception(ExeCodeTable::ADEL, true);
      return;
    }
    if (check_data_read_break(addr)) {
      return;
    }
    gte.write_data(t, bus.read32(addr));
    pc += 4;
  }

  void swc2(gte_dr t, mips_reg s, u32 i) {
    u32 addr = reg.u[s] + i;
    if (addr & 0b11) {
      exception(ExeCodeTable::ADEL, true);
      return;
    }
    if (check_data_read_break(addr)) {
      return;
    }
    bus.write32(addr, gte.read_data(t));
    pc += 4;
  }

  void cmd2(u32 op) {
    pc += 4;
    gte.execute(op);
  }
 
private:

  inline void sethl(u64 x) {
    hi = (x & 0xFFFF'FFFF'0000'0000) >> 32;
    lo =  x & 0xFFFF'FFFF;
  }

  inline bool check_exe_break() {
    if ((cop0.dcic.v & COP0_DCIC_BK_CODE_MK) == COP0_DCIC_BK_CODE_MK) {
      if (((pc ^ cop0.bpc) & cop0.bpcm) == 0) {
        cop0.dcic.tany = 1;
        cop0.dcic.tc   = 1;
        exception(ExeCodeTable::BP, true);
        return true;
      }
    }
    return false;
  }

  inline bool check_data_read_break(u32 addr) {
    if ((cop0.dcic.v & COP0_DCIC_BK_DATA_MK) == COP0_DCIC_BK_DATA_MK) {
      if (((addr ^ cop0.bda) & cop0.bdam) == 0) {
        cop0.dcic.tany = 1;
        cop0.dcic.td   = 1;
        cop0.dcic.tdr  = cop0.dcic.v && COP0_DCIC_BK_DR_MK;
        exception(ExeCodeTable::BP, true);
        return true;
      }
    }
    return false;
  }

  inline bool check_data_write_break(u32 addr) {
    if ((cop0.dcic.v & COP0_DCIC_BK_DATA_MK) == COP0_DCIC_BK_DATA_MK) {
      if (((addr ^ cop0.bda) & cop0.bdam) == 0) {
        cop0.dcic.tany = 1;
        cop0.dcic.td   = 1;
        cop0.dcic.tdw  = cop0.dcic.v && COP0_DCIC_BK_DW_MK;
        exception(ExeCodeTable::BP, true);
        return true;
      }
    }
    return false;
  }

  inline bool check_jump_break() {
    if ((cop0.dcic.v & COP0_DCIC_BK_JMP_MK) == COP0_DCIC_BK_JMP_MK) {
      cop0.dcic.tj   = 1;
      cop0.dcic.tany = 1;
      exception(ExeCodeTable::BP, true);
      return true;
    }
    return false;
  }

friend class DisassemblyMips;
};


class DisassemblyMips {
private:
  Bus& bus;
  const MipsReg& reg;
  const u32& cpc;
  u32 pc;

public:
  DisassemblyMips(R3000A& im) : bus(im.bus), reg(im.reg), cpc(im.pc), pc(im.pc) {}

  // 解析当前pc指向的指令
  void current() {
    pc = cpc;
    mips_decode(bus.read32(pc), this);
  }

  // 解析 pc 偏移指向的指令
  void decode(int offset) {
    pc = cpc + (offset << 2);
    mips_decode(bus.read32(pc), this);
  }

  void nop() const {
    nn("NOP");
  }

  void add(mips_reg d, mips_reg s, mips_reg t) const {
    rr("ADD", d, s, t);
  }

  void addu(mips_reg d, mips_reg s, mips_reg t) const {
    rr("ADDu", d, s, t);
  }

  void sub(mips_reg d, mips_reg s, mips_reg t) const {
    rr("SUB", d, s, t);
  }

  void subu(mips_reg d, mips_reg s, mips_reg t) const {
    rr("SUBu", d, s, t);
  }

  void mul(mips_reg s, mips_reg t) const {
    rx("MUL", s, t);
  }

  void mulu(mips_reg s, mips_reg t) const {
    rx("MULu", s, t);
  }

  void div(mips_reg s, mips_reg t) const {
    rx("DIV", s, t);
  }

  void divu(mips_reg s, mips_reg t) const {
    rx("DIVu", s, t);
  }

  void slt(mips_reg d, mips_reg s, mips_reg t) const {
    rr("SLT", d, s, t);
  }

  void sltu(mips_reg d, mips_reg s, mips_reg t) const {
    rr("SLTu", d, s, t);
  }

  void _and(mips_reg d, mips_reg s, mips_reg t) const {
    rr("AND", d, s, t);
  }

  void _or(mips_reg d, mips_reg s, mips_reg t) const {
    rr("OR", d, s, t);
  }

  void _nor(mips_reg d, mips_reg s, mips_reg t) const {
    rr("NOR", d, s, t);
  }

  void _xor(mips_reg d, mips_reg s, mips_reg t) const {
    rr("XOR", d, s, t);
  }

  void addi(mips_reg t, mips_reg s, s32 i) const {
    ii("ADDi", t, s, i);
  }

  void addiu(mips_reg t, mips_reg s, s32 i) const {
    ii("ADDiu", t, s, i);
  }

  void slti(mips_reg t, mips_reg s, s32 i) const {
    ii("SLTi", t, s, i);
  }

  void sltiu(mips_reg t, mips_reg s, u32 i) const {
    ii("SLTiu", t, s, i);
  }

  void andi(mips_reg t, mips_reg s, u32 i) const {
    ii("ANDi", t, s, i);
  }

  void ori(mips_reg t, mips_reg s, u32 i) const {
    ii("ORi", t, s, i);
  }

  void xori(mips_reg t, mips_reg s, u32 i) const {
    ii("XORi", t, s, i);
  }

  void lw(mips_reg t, mips_reg s, s32 i) const {
    ii("LW", t, s, i);
  }

  void sw(mips_reg t, mips_reg s, s32 i) const {
    iw("SW", t, s, i);
  }

  void lb(mips_reg t, mips_reg s, s32 i) const {
    ii("LB", t, s, i);
  }

  void lbu(mips_reg t, mips_reg s, s32 i) const {
    ii("LBu", t, s, i);
  }

  void sb(mips_reg t, mips_reg s, s32 i) const {
    iw("SB", t, s, i);
  }

  void lh(mips_reg t, mips_reg s, s32 i) const {
    ii("LH", t, s, i);
  }

  void lhu(mips_reg t, mips_reg s, s32 i) const {
    ii("LHu", t, s, i);
  }

  void sh(mips_reg t, mips_reg s, s32 i) const {
    iw("SH", t, s, i);
  }

  void lui(mips_reg t, u32 i) const {
    i2("LUi", t, i);
  }

  void lwl(mips_reg t, mips_reg s, u32 i) const {
    ii("LWL", t, s, i);
  }

  void lwr(mips_reg t, mips_reg s, u32 i) const {
    ii("LWR", t, s, i);
  }

  void swl(mips_reg t, mips_reg s, u32 i) const {
    ii("SWL", t, s, i);
  }

  void swr(mips_reg t, mips_reg s, u32 i) const {
    ii("SWR", t, s, i);
  }

  void beq(mips_reg t, mips_reg s, s32 i) const {
    ji("BEQ", t, s, i);
  }

  void bne(mips_reg t, mips_reg s, s32 i) const {
    ji("BNE", t, s, i);
  }

  void blez(mips_reg s, s32 i) const {
    j2("BLEZ", s, i);
  }

  void bgtz(mips_reg s, s32 i) const {
    j2("BGTZ", s, i);
  }

  void bltz(mips_reg s, s32 i) const {
    j2("BLTZ", s, i);
  }

  void bgez(mips_reg s, s32 i) const {
    j2("BGEZ", s, i);
  }

  void bgezal(mips_reg s, s32 i) const {
    j2("BGEZAL", s, i);
  }

  void bltzal(mips_reg s, s32 i) const {
    j2("BLTZAL", s, i);
  }

  void j(u32 i) const {
    jj("J", i);
  }

  void jal(u32 i) const {
    jj("JAL", i);
  }

  void jr(mips_reg s) const {
    j1("JR", s);
  }

  void jalr(mips_reg d, mips_reg s) const {
    rx("JALR", d, s);
  }

  void mfhi(mips_reg d) const {
    m1("MFHI", d);
  }

  void mflo(mips_reg d) const {
    m1("MFLO", d);
  }

  void mthi(mips_reg s) const {
    m1("MTHI", s);
  }

  void mtlo(mips_reg s) const {
    m1("MTLO", s);
  }

  void mfc0(mips_reg t, mips_reg d) const {
    i2("MFC0", t, d);
  }

  void mtc0(mips_reg t, mips_reg d) const {
    i2("MTC0", t, d);
  }

  void sll(mips_reg d, mips_reg t, u32 i) const {
    ii("SLL", d, t, i);
  }

  void sllv(mips_reg d, mips_reg t, mips_reg s) const {
    rr("SLLV", d, t, s);
  }

  void sra(mips_reg d, mips_reg t, u32 i) const {
    ii("SRA", d, t, i);
  }

  void srav(mips_reg d, mips_reg t, mips_reg s) const {
    rr("SRAV", d, t, s);
  }

  void srl(mips_reg d, mips_reg t, u32 i) const {
    ii("SRL", d, t, i);
  }

  void srlv(mips_reg d, mips_reg t, mips_reg s) const {
    rr("SRLV", d, t, s);
  }

  void syscall() const {
    jj("SYSCAl", reg.v0);
  }

  void brk(u32 code) const {
    jj("BREAK", code);
  }

  void rfe() const {
    nn("Rfe");
  }

  void mfc2(mips_reg t, gte_dr d) {
    gm("MFC2", t, '<', d);
  }

  void mtc2(mips_reg t, gte_dr d) {
    gm("MTC2", t, '>', d);
  }

  void cfc2(mips_reg t, gte_cr d) {
    gm("CFC2", t, '<', d + 32);
  }

  void ctc2(mips_reg t, gte_cr d) {
    gm("CTC2", t, '>', d + 32);
  }

  void bc2f(u32 i) {
    jj("BC2F", i);
  }

  void bc2t(u32 imm) {
    jj("BC2T", imm);
  }

  void lwc2(gte_dr t, mips_reg s, u32 i) {
    gw("LWC2", t, s, i);
  }

  void swc2(gte_dr t, mips_reg s, u32 i) {
    gw("SWC2", t, s, i);
  }

  void cmd2(u32 op) {
    gc("CMD2", op);
  }

private:
#define DBG_HD "%08x | %08x \x1b[35m%6s \033[0m"

  void gm(char const* iname, mips_reg t, char dir, gte_dr d) const {
    warn(DBG_HD "H/L, $%s %c $cop2.r%02d \t\t \x1b[1;30m# $%s=%x, \n", 
      pc, bus.read32(pc), iname, rname(t), dir, d, rname(t), reg.u[t]);
  }

  void gw(char const* iname, gte_dr t, mips_reg s, u32 i) const {
    warn(DBG_HD "H/L, $cop2.r%02d, $%s, %08x \t\t \x1b[1;30m# $%s=%x, [<<2]=%08x \n", 
      pc, bus.read32(pc), iname, t, rname(s), i, rname(s), reg.u[s], i<<2);
  }

  void gc(char const* iname, u32 op) {
    warn(DBG_HD "%08x [1;30m\n", pc, bus.read32(pc), iname, op);
  }

  void rx(char const* iname, mips_reg s, mips_reg t) const {
    debug(DBG_HD "H/L, $%s, $%s \t\t \x1b[1;30m# $%s=%x, $%s=%x\n", 
      pc, bus.read32(pc), iname, rname(s), rname(t), 
      rname(s), reg.u[s], rname(t), reg.u[t]);
  }

  void rr(char const* iname, mips_reg d, mips_reg s, mips_reg t) const {
    debug(DBG_HD "$%s, $%s, $%s \t\t \x1b[1;30m# $%s=%x, $%s=%x, $%s=%x\n",
      pc, bus.read32(pc), iname, rname(d), rname(s), rname(t),
      rname(d), reg.u[d], rname(s), reg.u[s], rname(t), reg.u[t]);
  }
  
  template<class T>
  void mem(char const* iname, mips_reg t, mips_reg s, s16 i) const {
    debug(DBG_HD "$%s, $%s, 0x%08x \t \x1b[1;30m# $%s=%x, $%s=%x, addr[%x]=%x\n",
      pc, bus.read32(pc), iname, rname(t), rname(s), i,
      rname(t), reg.u[t], rname(s), reg.u[s], reg.u[s]+i, bus.read<T>(reg.u[s]+i));
  }

  void ii(char const* iname, mips_reg t, mips_reg s, s16 i) const {
    debug(DBG_HD "$%s, $%s, 0x%08x \t \x1b[1;30m# $%s=%x, $%s=%x\n",
      pc, bus.read32(pc), iname, rname(t), rname(s), i,
      rname(t), reg.u[t], rname(s), reg.u[s]);
  }

  void iw(char const* iname, mips_reg t, mips_reg s, s16 i) const {
    debug(DBG_HD "[$%s + 0x%08x], $%s\t \x1b[1;30m# $%s=%x, $%s=%x\n",
      pc, bus.read32(pc), iname, rname(s), i, rname(t),
      rname(t), reg.u[t], rname(s), reg.u[s]);
  }

  void i2(char const* iname, mips_reg t, s16 i) const {
    debug(DBG_HD "$%s, 0x%08x\t\t \x1b[1;30m# $%s=%x\n",
      pc, bus.read32(pc), iname, rname(t), i, rname(t), reg.u[t]);
  }

  void jj(char const* iname, u32 i) const {
    info(DBG_HD "0x%08x\t\t\t \x1b[1;30m# %x\n", pc, bus.read32(pc), iname, i, i<<2);
  }

  void j1(char const* iname, mips_reg s) const {
    info(DBG_HD "%s\t\t\t\t \x1b[1;30m# $%s=%x\n",
      pc, bus.read32(pc), iname, rname(s), rname(s), reg.u[s]);
  }

  void ji(char const* iname, mips_reg t, mips_reg s, s16 i) const {
    info(DBG_HD "$%s, $%s, 0x%08x \t \x1b[1;30m# $%s=%x, $%s=%x, addr[%x]\n",
      pc, bus.read32(pc), iname, rname(t), rname(s), i,
      rname(t), reg.u[t], rname(s), reg.u[s], pc + (i << 2) +4);
  }

  void j2(char const* iname, mips_reg t, s16 i) const {
    debug(DBG_HD "$%s, 0x%08x\t\t \x1b[1;30m# $%s=%x\n",
      pc, bus.read32(pc), iname, rname(t), i, rname(t), reg.u[t]);
  }

  void nn(char const* iname) const {
    debug(DBG_HD "\n", pc, bus.read32(pc), iname);
  }

  void m1(char const* iname, mips_reg s) const {
    debug(DBG_HD "$%s\t\t\t\t \x1b[1;30m# $%s=%x\n",
      pc, bus.read32(pc), iname, rname(s), rname(s), reg.u[s]);
  }

  const char* rname(mips_reg s) const {
    if (s & (~0x1f)) {
      error("! Invaild reg index %d\n", s);
      return 0;
    }
    return MipsRegName[s];
  }
};


} // namespace ps1e
