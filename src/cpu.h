#pragma once 
#include "util.h"

namespace ps1e {


extern const char*  MipsRegName[];
static const int MIPS_REG_COUNT = 32;


// Cpu Cause Reg 8-15
// bit: 8 7 6 5 4 3 2 1
// use: - - - - - H s s
// ps 把硬件中断放在一个单独的芯片上做管理
enum class CpuCauseInt : u8 {
  software = 1,
  hardware = 1<<2,
};


typedef union {
  u32 u[MIPS_REG_COUNT];
  s32 s[MIPS_REG_COUNT];

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


#define COP0_SR_RFE_SHIFT_MASK 0xb0011'1111
#define COP0_SR_RFE_RESERVED_MASK (~0xb1111)
#define COP0_SR_REG_IDX 12
typedef union {
  u32 v;
  struct {
    u32 ie  : 1; // bit 0;  1:使能全局中断 
    u32 KUc : 1; // 1: 关中断进入异常处理程序, 0=Kernel, 1=User
    u32 IEp : 1; // ECC 错误
    u32 KUp : 1; // not use cpu 特权级别
    u32 IEo : 1;
    u32 KUo : 1; 
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


#define COP0_CAUSE_RW_MASK 0b0000'0000'0000'0000'0000'0011'0000'0000
#define COP0_CAUSE_REG_IDX 13
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


#define COP0_DCIC_WRITE_MASK 0b0000'0000'0000'0000'1111'0000'0011'1111
#define COP0_DCIC_BK_JMP_MK  ((1 << 31) | (1 << 23) | (1 << 28) | (1 << 29))
#define COP0_DCIC_BK_CODE_MK ((1 << 31) | (1 << 23) | (1 << 24) | (1 << 30))
#define COP0_DCIC_BK_DATA_MK ((1 << 31) | (1 << 23) | (1 << 25) | (1 << 30))
#define COP0_DCIC_BK_DW_MK   (1 << 27)
#define COP0_DCIC_BK_DR_MK   (1 << 26)
#define COP0_DCIC_REG_INDEX  7
typedef union {
  u32 v;
  struct {
    u32 tany  : 1; //00 Any break set
    u32 tc    : 1; //01 When BPC code break set 1
    u32 td    : 1; //02 When DBA data break set 1
    u32 tdr   : 1; //03 When DBA data read set 1
    u32 tdw   : 1; //04 When DBA data write set 1
    u32 tj    : 1; //05 any jump set 1
    u32 _z0   : 6; //06-11
    u32 jr    : 2; //12-13 Jump Redirection (0=Disable, 1..3=Enable)
    u32 _z1   : 2; //14-15
    u32 _z2   : 7; //16-22
    u32 sme1  : 1; //23 Super-Master Enable 1 for bit24-29
    u32 be    : 1; //24 Execution breakpoint 1:Enabled
    u32 bd    : 1; //25 Data access breakpoint 1:Enabled
    u32 bdr   : 1; //26 Enable Data read break
    u32 bdw   : 1; //27 Enable Data write break
    u32 bj    : 1; //28 Enable any jump break
    u32 mej   : 1; //29 Master Enable for bit28
    u32 med   : 1; //30 Master Enable for bit24-27
    u32 sme2  : 1; //31 Super-Master Enable 2 for bit24-29
  };
} Cop0Dcic;


typedef union {
  u32 r[32];

  struct {
    u32 index; // r00 not use
    u32 rand;  // r01 not use
    u32 tlbl;  // r02 not use
    u32 bpc;   // r03 执行断点
    u32 ctxt;  // r04
    u32 bda;   // r05 数据访问断点
    u32 pidm;  // r06
    Cop0Dcic dcic;  // r07 断点控制 TODO!!!
    u32 badv;  // r08; 无效虚拟地址(ps无用)
    u32 bdam;  // r09 数据访问断点掩码, 数据地址 & bdam 再与 bpc 比较
    u32 tlbh;  // r10 not use
    u32 bpcm;  // r11 执行断点掩码
    Cop0SR sr; // r12
    Cop0Cause cause; // r13
    u32 epc;   // r14; 异常返回地址
    u32 prid;  // r15; 处理器id
    u32 unused16;
    u32 unused17;
    u32 unused18;
    u32 unused19;
    u32 unused20;
    u32 unused21;
    u32 unused22;
    u32 unused23;
    u32 unused24;
    u32 unused25;
    u32 unused26;
    u32 unused27;
    u32 unused28;
    u32 unused29;
    u32 unused30;
    u32 unused31;
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
  BP   = 9,   // Breakpoint(Debug)
  RI   = 10,  // 保留指令异常
  CPU  = 11,  // 协处理器不可用异常
  OVF  = 12,  // 数学溢出异常
};


typedef u32 GteDataRegisterType;

union Gte {
  GteDataRegisterType d[32];
  struct {
    // r0-r1 Vector 0 (X,Y,Z)
    GteDataRegisterType vxy0, vz0; 
    // r2-r3 Vector 1 (X,Y,Z)
    GteDataRegisterType vxy1, vz1; 
    // r4-r5 Vector 2 (X,Y,Z)
    GteDataRegisterType vxy2, vz2; 
    // r6 4xU8 Color/code value
    GteDataRegisterType rgbc;      
    // r7 Average Z value (for Ordering Table)
    GteDataRegisterType otz;       
    // r8 16bit Accumulator (Interpolate)
    GteDataRegisterType ir0;
    // r9-r11 16bit Accumulator (Vector)
    GteDataRegisterType ir1, ir2, ir3;
    // r12-r15 Screen XY-coordinate FIFO  (3 stages)
    GteDataRegisterType sxy0, sxy1, sxy2, sxyp;
    // r16-r19 Screen Z-coordinate FIFO   (4 stages)
    GteDataRegisterType sz0, sz1, sz2, sz3;
    // r20-r22 Color CRGB-code/color FIFO (3 stages)
    GteDataRegisterType rgb0, rgb1, rgb2;
    // Prohibited
    GteDataRegisterType _res1;
    // r24 32bit Maths Accumulators (Value)
    GteDataRegisterType mac0;
    // r25-27 32bit Maths Accumulators (Vector)
    GteDataRegisterType mac1, mac2, mac3;
    // r28-r29 Convert RGB Color (48bit vs 15bit)
    GteDataRegisterType irgb, orgb;
    // r30-r31 Count Leading-Zeroes/Ones (sign bits)
    GteDataRegisterType lzcs, lzcr;
  };
};


void printSR(Cop0SR sr);
void printMipsReg(MipsReg&);


}