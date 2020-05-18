#include "util.h"
#include "io.h"
#include "bus.h"

namespace ps1e {

union SerialPortState {
  u32 v;
  struct {
    u32 tx_ready          : 1; // 1=Ready/Stated
    u32 rx_fifo_not_empty : 1; // 1=Not Empty
    u32 tx_fini           : 1; // 1=Ready/Finished
    u32 rx_parity_err     : 1; // 1=Error; Wrong Parity, when enabled
    u32 rx_fifo_over      : 1; // 1=Error; Received more than 8 bytes
    u32 rx_bad_stop       : 1; // 1=Error; Bad Stop Bit
    u32 rx_in_level       : 1; // 1=Inverted, 0=Normal
    u32 dsr_in_level      : 1; // (0=Off, 1=On) (remote DTR)
    u32 cst_in_level      : 1; // (0=Off, 1=On) (remote RTS)
    u32 irq               : 1; // 1=IRQ
    u32 _                 : 1; // not use
    u32 baudrate          :15; // (15bit timer, decrementing at 33MHz)
    u32 __                : 6; // zero
  };

  SerialPortState() : v(0b0101) {}

  // 禁止写入
  void write(u32 v) {}

  u32 read() {
    return v;
  }
};


class ConsoleDataPort {
public:
  void write(u32 value);
  u32 read();
};


class SerialPort {
private:
  class Data : public InnerDeviceIO<SerialPort, ConsoleDataPort> {
  public:
    using InnerDeviceIO::InnerDeviceIO;
    static const DeviceIOMapper Port = DeviceIOMapper::sio_tx_data;
  };

  class Stat : public InnerDeviceIO<SerialPort, SerialPortState> {
  public:
    using InnerDeviceIO::InnerDeviceIO;
    static const DeviceIOMapper Port = DeviceIOMapper::sio_stat;
  };

  template<DeviceIOMapper P>
  class Nomr : public InnerDeviceIO<SerialPort, U32Reg> {
  public:
    using InnerDeviceIO::InnerDeviceIO;
    static const DeviceIOMapper Port = P;
  };

  Data data;
  Stat stat;
  Nomr<DeviceIOMapper::sio_mode> mode;
  Nomr<DeviceIOMapper::sio_ctrl> ctrl;
  Nomr<DeviceIOMapper::sio_misc> misc;
  Nomr<DeviceIOMapper::sio_baud> baud;

public:
  SerialPort(Bus&);
};

}