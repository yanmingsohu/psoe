#include "test.h"
#include "math.h"

namespace ps1e_t {
using namespace ps1e;


void test_mem_jit() {
  MemJit mj;
  for (int i=0; i<3; ++i) {
    void* p1  = mj.get(0x100);
    void* p11 = mj.get(0x100);
    void* p2  = mj.get(0x10000);
    void* p3  = mj.get(0x200);
    // printf("%02d %lx %lx %lx %lx\n", i, p1, p11, p2, p3);
    mj.free(p1);
    mj.free(p2);
    mj.free(p3);
    mj.free(p11);
  }
}


template<class T> void tover(T a, T b, bool over) {
  Overflow<T> o(a, b);
  bool c = o.check(a+b);
  // over = (a+b) != (T)(a+b);
  if (c != over) {
    printf("%d == %d ? %d = %d + %d\n", c, over, (T)(a+b), a, b);
    panic("overflow");
  }
}


void test_overflow() {
  tover(1, 1, false);
  tover(1, -2, false);
  tover<s8>(127, +1, true); 
  tover<s8>(-128, -1, true);
  tover<u8>(-0xFF, 2, false);
  tover<u8>(0xff, -2, false);
  tover<s8>(0xff, 1, false);
  tover<s8>(-0xff, -1, false);
  tover<s32>(0xf000'0000, 1, false);
  tover<s32>(0xffff'ffff, 1, false); 
  tover<s16>(32767, 1, true);
  tover<s8>(120, 10, true);
  tover<s8>(127, -10, false);
}


void test_local() {
  int a = 1;
  {
    auto c = createFuncLocal([&a] { ++a; });
  }
  eq(a, 2, "local function");
}


void inner_window_on_console() {
  for (int c = 0;; ++c) {
    const int size = 2000;
    char buf[size];
    for (int i=0; i<size; ++i) {
      // ASCII: 33-126
      buf[i] = static_cast<char>((float)rand() / RAND_MAX * (126-33) + 33); 
    }
    printf("%d %s\r", c, buf);
  }
}


static u32 __t_io_read(DeviceIO**io, u32 addr) {
  switch (SWITCH_IO_MIRROR(addr)) {
    IO_MIRRORS_STATEMENTS(CASE_IO_MIRROR_READ, io, NULL);
    default:
      error("cannot read %x\n", addr);
      panic("bad read");
  }
}


static void __t_io_write(DeviceIO** io, u32 addr, u32 v) {
  switch (SWITCH_IO_MIRROR(addr)) {
    IO_MIRRORS_STATEMENTS(CASE_IO_MIRROR_WRITE, io, v);
    default:
      error("cannot write %x = %x\n", addr, v);
      panic("bad write");
  }
}


static void __io(DeviceIO**io, u32 addr, u32 v) {
  __t_io_write(io, addr, v);
  eq<u32>(v, __t_io_read(io, addr), "io mirrors");
  ps1e::debug("Util Test IO: %x: %x\n", addr, v);
}


void test_io_mirrors() {
  DeviceIOLatch<> iol;
  DeviceIO* io[io_map_size];
  for (int i=0; i<io_map_size; ++i) {
    io[i] = &iol;
  }

  __io(io, 0x1F80'1810, 0xF0F0'E0E0);
  __io(io, 0x1F80'1814, 0xFFFF'0000);
}


class StaticVar {
public:
  ~StaticVar() {
    ps1e::debug("static released\n");
  }
  StaticVar() {
    ps1e::debug("static init\n");
  }
};


class StaticInClass {
public:
  StaticVar* init() {
    static StaticVar v;
    return &v;
  }
};


static auto test_scope() {
  return createFuncLocal([]{
    printf("test_scope\n");
  });
}


static void _add(u32 a, s32 b, const u32 r) {
  u32 c = add_us(a, b);
  if (c != r) {
    error("Fail add  %x != %x\n", c, r);
    exit(1);
  } else {
    info("Success %x + %x == %x\n", a, b, c);
  }
}


//80100000+ffffbb44=800FBB44
//80100000-ffffbb44=801044BC
//80100000-0000bb44=800F44BC ??
static void test_add() {
  u16 x = 0xbb44;
  // 无视 y 得类型, 符号总是被扩展
  s32 y = s16(x);
  eq(u32(y), 0xffffbb44, "unsigned fail");
  printf("s16 to u32 %x\n", u32(s16(x)));
  printf("u16 to u64 %x\n", u64(x));
  printf("y = %x, -y = %x\n", y, -y);
  
  _add(0x00000005, 1, 6);
  _add(0x00000005, -1, 4);
  _add(0xF0000005, 1, 0xF0000006);
  _add(0xF0000005, -1, 0xF0000004);

  _add(0x80100000, 0xffffbb44, 0x800f44bc);
  _add(0x80100000, 0x0000bb44, 0x8010bb44);
}


void test_util() {
  //static StaticVar v;
  StaticInClass sic;
  //auto l = test_scope();
  sic.init();
  //inner_window_on_console();
  test_mem_jit();
  test_overflow();
  test_local();
  test_io_mirrors();
  //test_add(); // 该测试不正确
}

}