#pragma once
#include "util.h"

namespace ps1e {


extern const char*  MipsRegName[];


//TODO: ! 这里不完整 ?
enum class CpuCauseInt : u8 {
  software = 1,
  hardware = 1<<1,
};


typedef union {
  u32 u[32];
  s32 s[32];

  struct {
    u32 zero; // r00
    u32 at;   // r01
    u32 v0;   // r02   
    u32 v1;   // r03
    u32 a0;   // r04 - r07
    u32 a1, a2, a3;
    u32 t0;   // r08 - r15
    u32 t1, t2, t3, t4, t5, t6, t7;
    u32 s0;   // r16 - r23
    u32 s1, s2, s3, s4, s5, s6, s7; 
    u32 t8;   // r24
    u32 t9;   // r25
    u32 k0;   // r26
    u32 k1;   // r27
    u32 gp;   // r28
    u32 sp;   // r29
    u32 fp;   // r30
    u32 ra;   // r31
  };
} MipsReg;


typedef union {
  u32 v;
  struct {
    u32 ie  : 1; // bit 0;  1:使能全局中断 
    u32 exl : 1; // 1: 关中断进入异常处理程序
    u32 erl : 1; // ECC 错误
    u32 ksu : 2; // not use cpu 特权级别
    u32 ux  : 1; // not use
    u32 sx  : 1; // not use
    u32 kx  : 1; // not use
    u32 im  : 8; // bit 8; 8个中断使能位, 1:允许中断
    u32 _z  : 3; // bit 16; 占位
    u32 nmi : 1; // 1: 不可屏蔽中断发生? 缓存未命中?
    u32 sr  : 1; // 1: 不可屏蔽中断发生, 或重启
    u32 ts  : 1; // TLB关闭
    u32 bev : 1; // 启动时异常向量, 1:bios
    u32 px  : 1; 
    u32 mx  : 1; // bit 24; 指令集扩展
    u32 re  : 1; // 0:小端
    u32 fr  : 1; // 1:使32个双精度浮点寄存器对软件可见
    u32 rp  : 1; // 减少功率
    u32 cu  : 4; // 协处理器可用掩码
  };
} Cop0SR;


typedef union {
  u32 v; 
  struct {
    u32 _z0 : 2;
    u32 ExcCode : 5; // 中断类型
    u32 _z1 : 1;
    u32 ip  : 8; // 软/硬件中断 see: enum CpuCauseInt
    u32 _z2 : 6;
    u32 wp  : 1; // 1:已经处于异常模式
    u32 iv  : 1; // 1:使用特殊中断入口地址
    u32 _z3 : 2; 
    u32 pci : 1; // cp0 性能计数器溢出
    u32 dc  : 1; // 1:停止count计数
    u32 ce  : 2; // 协处理器错误号码(0-3)
    u32 ti  : 1; // 计时器中断
    u32 bd  : 1; // 1:分支延迟
  };
} Cop0Cause;


typedef union {
  u32 r[16];

  struct {
    u32 index; // r00 not use
    u32 rand;  // r01 not use
    u32 tlbl;  // r02 not use
    u32 bpc;   // r03 执行断点
    u32 ctxt;  // r04
    u32 bda;   // r05 数据访问断点
    u32 pidm;  // r06
    u32 dcic;  // r07 断点控制
    u32 badv;  // r08; 无效虚拟地址(ps无用)
    u32 bdam;  // r09 数据访问断点掩码, 数据地址 & bdam 再与 bpc 比较
    u32 tlbh;  // r10 not use
    u32 bpcm;  // r11 执行断点掩码
    Cop0SR sr; // r12
    Cop0Cause cause; // r13
    u32 epc;   // r14; 异常返回地址
    u32 prid;  // r15; 处理器id
  };
} Cop0Reg;


enum class ExeCodeTable {
  INT  = 0,   // 外部中断
  MOD  = 1,   // TLB 修改异常
  TLBL = 2,   // TLB 加载失败
  TLBS = 3,   // TLB 保存失败
  ADEL = 4,   // 加载(或取指令)时地址错误
  ADES = 5,   // 保存时地址错误
  IBE  = 6,   // 取指令时总线错误
  DBW  = 7,   // 数据读写时总线错误
  SYS  = 8,   // SYSCALL
  BP   = 9,   // Breakpoint
  RI   = 10,  // 保留指令异常
  CPU  = 11,  // 协处理器不可用异常
  OVF  = 12,  // 数学溢出异常
};


void printSR(Cop0SR sr);


}