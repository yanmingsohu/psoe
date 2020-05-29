#pragma once

#include "util.h"

namespace ps1e {

#define GteReg63WriteMask 0x7FFFF000
#define GteFlagLogSum     0x7F87E000
#define GteCommandFix     0x4A000000
#define GteOF43           (static_cast<long long>(1) << 43)
#define GteOF31           (static_cast<long long>(1) << 31)
#define GteOF15           (1 << 15)
#define GteOf12           (1 << 12)

class GTE;


enum class GteReg63Error : u32 {
  Error = 1u<<31,// (Bit30..23, and 18..13 ORed together) (Read only)
  Mac1p = 1<<30, // MAC1结果大于43位且为正
  Mac2p = 1<<29, // MAC2 Result larger than 43 bits and positive
  Mac3p = 1<<28,
  Mac1n = 1<<27, // MAC1结果大于43位且为负
  Mac2n = 1<<26, // MAC2 Result larger than 43 bits and negative
  Mac3n = 1<<25,
  Ir1   = 1<<24, // IR1 饱和至 +0000h .. +7FFFh（lm = 1）或 -8000h .. +7FFFh（lm = 0）
  Ir2   = 1<<23, // IR2 saturated to +0000h..+7FFFh (lm=1) or to -8000h..+7FFFh (lm=0)
  Ir3   = 1<<22, 
  R     = 1<<21, // Color-FIFO-R 饱和至 +00h .. +FFh
  G     = 1<<20, // Color-FIFO-G saturated to +00h..+FFh
  B     = 1<<19,
  Sz3   = 1<<18, // SZ3 or OTZ saturated to +0000h..+FFFFh
  Div   = 1<<17, // Divide overflow. RTPS/RTPT division result saturated to max=1FFFFh
  Mac0p = 1<<16, // MAC0 Result larger than 31 bits and positive
  Mac0n = 1<<15, // MAC0 Result larger than 31 bits and negative
  Sx2   = 1<<14, // SX2 saturated to -0400h..+03FFh
  Sy2   = 1<<13, // SY2 saturated to -0400h..+03FFh
  Ir0   = 1<<12, // IR0 saturated to +0000h..+1000h
};


union GteCommand {
  u32 v;
  struct {
    u32 num     : 6; //00-05 Real GTE Command Number (00h..3Fh)
    u32 _z1     : 4; //06-09
    u32 lm      : 1; //10    Saturate IR1,IR2,IR3 result (0=To -8000h..+7FFFh, 1=To 0..+7FFFh)
    u32 _z2     : 2; //11,12
    u32 mv_tv   : 2; //13,14 see 'MVMVA_Trans_Vec'
    u32 mv_mv   : 2; //15,16 see 'MVMVA_Mul_Vec'
    u32 mv_mm   : 2; //17,18 see 'MVMVA_Mul_Mx'
    u32 sf      : 1; //19    Shift Fraction in IR registers (0=No fraction, 1=12bit fraction)
    u32 fake    : 5; //20-24 Fake GTE Command Number (00h..1Fh) (ignored by hardware)
    u32 _fix    : 7; //25-31 Must be 0100101b for "COP2 imm25" instructions, 'GteCommandFix'
  };

  GteCommand(u32 i) : v(i) {}
};


enum class MVMVA_Mul_Mx : u32 {
  rotation = 0,
  light = 1,
  color = 2,
  reserved = 3,
};


enum class MVMVA_Mul_Vec : u32 {
  v0 = 0,
  v1 = 1,
  v2 = 2,
  ir = 3,
};


enum class MVMVA_Trans_Vec : u32 {
  tr = 0,
  bk = 1,
  fc = 2,
  none = 3,
};


struct GteFlag {
  u32 v;

  u32 read() { return v; }
  void write(u32 d);
  void set(GteReg63Error f);
  void clear();
  void update();
};


// 没有副作用的原生寄存器
template<class Save, class Convert>
struct GteSrcReg {
  Save v;

  virtual ~GteSrcReg() {}
  virtual u32 read() { 
    return static_cast<Convert>(v); 
  }
  virtual void write(u32 d) { 
    v = static_cast<Convert>(d); 
  }
};


struct GteVectorXY {
  float fx, fy;

  void write(u32 xy);
  u32 read();
};


struct GteVectorZ : public GteSrcReg<float, s16> {
};


//cop2r6  - RGBC  rw|CODE |B    |G    |R    | Color/code
//cop2r20 - RGB0  rw|CD0  |B0   |G0   |R0   | Characteristic color fifo.
//cop2r21 - RGB1  rw|CD1  |B1   |G1   |R1   |
//cop2r22 - RGB2  rw|CD2  |B2   |G2   |R2   |
//cop2r23 - (RES1)  |                       | Prohibited
struct GteRgb {
  u32 v;

  virtual ~GteRgb() {}
  void write(u32 r);
  u32 read();
  u32 r();
  u32 g();
  u32 b();
  u32 code();
  u32 cd() { return code(); };
};


struct GteRgbFifo : public GteRgb {
  GTE& r;
  GteRgbFifo(GTE&);
  void write(u32);
};


struct GteIR {
  s32 v;

  const int Lbit;
  const GteReg63Error IRE;
  GTE& r;

  GteIR(GTE& _r, int bit, GteReg63Error e);
  virtual ~GteIR() {}
  u32 read();
  void write(u32 _v);
};


struct GteIR0 : public GteIR {
using GteIR::GteIR;
  void write(u32);
};


struct GteIrgb {
  u32 v;
  GTE& r;

  GteIrgb(GTE& _r) : r(_r) {}
  u32 read() { return 0; }
  void write(u32 _v);
};


struct GteOrgb {
  GTE &r;

  GteOrgb(GTE& _r) : r(_r) {}
  void write(u32) {}
  u32 read();
};


struct GteMac : public GteSrcReg<double, s32> {
  GTE& r;
  const GteReg63Error positive;

  GteMac(GTE&, GteReg63Error);
};


struct GteMac0 : public GteMac {
using GteMac::GteMac;
};


struct GteSxyFifo {
  GTE& r;

  GteSxyFifo(GTE&);
  u32 read();
  void write(u32 v);
  void push(float x, float y);
};


struct GteZFifo : public GteSrcReg<float, s16> {
  GTE& r;

  GteZFifo(GTE&);
  void push(float);
};


union GteMatrix {
  float v[9];
  struct {
    float r11, r12, r13;
    float r21, r22, r23;
    float r31, r32, r33;
  };
  struct {
    float lr1, lr2, lr3;
    float lg1, lg2, lg3;
    float lb1, lb2, lb3;
  };
};


struct GteMatrixReg {
  float &m, &l;

  // | 31 <--- MSB ---> 16 | 15 <--- LSB ---> 0 |
  GteMatrixReg(float& lsb, float& msb);
  u32 read();
  void write(u32 v);
};


struct GteMatrixReg1 {
  float &v;

  GteMatrixReg1(float &p);
  u32 read();
  void write(u32 v);
};


template<class T>
struct GteReadonly {
  T v;
  u32 read() { return v; }
  void write(u32) {}
};


struct GteLeadingZeroes {
  s32 v;
  GTE& r;

  GteLeadingZeroes(GTE&);
  u32 read();
  void write(u32 v);
};


struct GteOTZ : public GteSrcReg<float, u16>  {
};


struct GteFarColor : public GteSrcReg<float, u32> {
};


struct GteVec3 {
  float x, y, z;

  GteVec3(GteVectorXY& xy, GteVectorZ z) : x(xy.fx), y(xy.fy), z(z.v) {}
  GteVec3(float a, float b, float c) : x(a), y(b), z(c) {}
  GteVec3() {}
  void add(float a, float b, float c) {
    x += a; y += b; z += c;
  }
};


class GTE {
private:
  // r0-r1 Vector 0 (X,Y,Z)
  GteVectorXY vxy0;
  GteVectorZ  vz0; 
  // r2-r3 Vector 1 (X,Y,Z)
  GteVectorXY vxy1; 
  GteVectorZ  vz1; 
  // r4-r5 Vector 2 (X,Y,Z)
  GteVectorXY vxy2;
  GteVectorZ  vz2; 
  // r6 4xU8 Color/code value
  GteRgb rgbc;      
  // r7 Average Z value (for Ordering Table)
  GteOTZ otz;       
  // r8 16bit Accumulator (Interpolate)
  GteIR0 ir0;
  // r9-r11 16bit Accumulator (Vector)
  GteIR ir1, ir2, ir3; 
  // r12-r15 Screen XY-coordinate FIFO  (3 stages)
  GteVectorXY sxy0, sxy1, sxy2;
  GteSxyFifo sxyp;
  // r16-r19 Screen Z-coordinate FIFO   (4 stages)
  GteVectorZ sz0, sz1, sz2;
  GteZFifo sz3;
  // r20-r22 Color CRGB-code/color FIFO (3 stages)
  GteRgb rgb0, rgb1;
  GteRgbFifo rgb2;
  // r23 Prohibited
  GteRgb res1;
  // r24 32bit Maths Accumulators (Value)
  GteMac0 mac0;
  // r25-27 32bit Maths Accumulators (Vector)
  GteMac mac1, mac2, mac3;
  // r28-r29 Convert RGB Color (48bit vs 15bit)
  GteIrgb irgb;
  GteOrgb orgb;
  // r30-r31 Count Leading-Zeroes/Ones (sign bits)
  GteLeadingZeroes lzcs;
  GteReadonly<u32> lzcr;

  // r32-r36 / cnt0-4 Rotation matrix(3x3)
  GteMatrix rt;
  GteMatrixReg r32;
  GteMatrixReg r33;
  GteMatrixReg r34;
  GteMatrixReg r35;
  GteMatrixReg1 r36;
  // r37-r39 / cnt5-7 Translation vector (X,Y,Z) 
  GteSrcReg<float, s32> trX, trY, trZ;
  // r40-r44 / cnt8-12 Light source matrix
  GteMatrix llm;
  GteMatrixReg r40;
  GteMatrixReg r41;
  GteMatrixReg r42;
  GteMatrixReg r43;
  GteMatrixReg1 r44;
  // r45-r47 / cnt13-15 Background color
  GteSrcReg<float, u32> rbk, gbk, bbk;
  // r48-r52 / cnt16-20 Light color matrix source
  GteMatrix lcm;
  GteMatrixReg r48;
  GteMatrixReg r49;
  GteMatrixReg r50;
  GteMatrixReg r51;
  GteMatrixReg1 r52;
  // r53-r55 / cnt21-23 Far color
  GteSrcReg<float, u32> rfc, gfc, bfc;
  // r56,r57 / cnt24-25 Screen offset
  GteSrcReg<float, u32> offx, offy;
  // r58 / cnt26 Projection plane distance
  // 读取H寄存器时，硬件意外地对<unsigned> 16bit值进行了 <sign-expand>
  //（即，值 +8000h..+FFFFh 返回为 FFFF8000h..FFFFFFFFh）此错误仅适用于 `mov rd`
  GteSrcReg<float, u16> H;
  // r59 / cnt27 Depth queing parameter A (coeff)
  GteSrcReg<s16, s16> dqa;
  // r60 / cnt28 Depth queing parameter B (offset)
  GteSrcReg<float, s32> dqb;
  // r61,r62 / cnt29-30 Average Z scale factors
  GteSrcReg<s16, s16> zsf3, zsf4;
  // r63 / cnt31 Returns any calculation errors
  GteFlag flag;

public:
  GTE();

  void write_data(const u8 reg_index, const u32 data);
  u32 read_data(const u8 reg_index);

  void write_ctrl(const u8 reg_index, const u32 data);
  u32 read_ctrl(const u8 reg_index);

  bool execute(const GteCommand c);
  u32 read_flag();

private:
  void RTPS(GteCommand);
  void NCLIP(GteCommand);
  void OP(GteCommand);
  void DPCS(GteCommand);
  void INTPL(GteCommand);
  void MVMVA(GteCommand);
  void NCDS(GteCommand);
  void CDP(GteCommand);
  void NCDT(GteCommand);
  void NCCS(GteCommand);
  void CC(GteCommand);
  void NCS(GteCommand);
  void NCT(GteCommand);
  void SQR(GteCommand);
  void DCPL(GteCommand);
  void DPCT(GteCommand);
  void AVSZ3(GteCommand);
  void AVSZ4(GteCommand);
  void RTPT(GteCommand);
  void GPF(GteCommand);
  void GPL(GteCommand);
  void NCCT(GteCommand);

private:
  void PerspectiveTransform(GteCommand c, GteVectorXY& xy, GteVectorZ& z);

  void write_ir(GteIR& ir, s32 d, u32 lm);
  void write_ir0(s32 d, u32 lm = 0);
  // 只设置溢出标志, 不改变溢出数据
  void write_mac(GteMac& mac, float d);
  void write_mac0(float d);
  // 从 mac123 读取, 并写入 fifo
  void write_color_fifo();
  void write_z_fifo(float d);
  void write_otz(float d);
  void write_xy_fifo(float x, float y);
  // 总是写入 mac123/ir123 并且在 c.sf 的时候 ir=mX/0x1000
  void write_ir_mac123(GteCommand c, float m1, float m2, float m3);
  void write_ir_mac123(GteCommand c, GteVec3 v);

  // (( (H*20000h/SZ3) +1)/2)
  float overflow_div1();
  void normalColor(GteCommand c, GteVec3 v);
  void normalColorDepth(GteCommand c);
  void normalColorColor(GteCommand c);
  void dptchCueColor(GteCommand c, GteRgb&);

  // 计算矩阵和向量的乘积
  GteVec3 mulMatrix(GteMatrix& mm, GteVec3 v);
  
  // 初始化一个奇怪的矩阵
  void init_reserved_mm(GteMatrix&);

friend struct GteLeadingZeroes;
friend struct GteIR;
friend struct GteRgbFifo;
friend struct GteZFifo;
friend struct GteIrgb;
friend struct GteOrgb;
friend struct GteSxyFifo;
};


}