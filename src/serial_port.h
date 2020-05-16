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
    using InnerDeviceIO::InnerDeviceIO;
  };

  class Stat : public InnerDeviceIO<SerialPort, SerialPortState> {
    using InnerDeviceIO::InnerDeviceIO;
  };

  class Mode : public InnerDeviceIO<SerialPort> {
    using InnerDeviceIO::InnerDeviceIO;
  };

  class Ctrl : public InnerDeviceIO<SerialPort> {
    using InnerDeviceIO::InnerDeviceIO;
  };

  class Misc : public InnerDeviceIO<SerialPort> {
    using InnerDeviceIO::InnerDeviceIO;
  };

  class Baud : public InnerDeviceIO<SerialPort> {
    using InnerDeviceIO::InnerDeviceIO;
  };

  Data data;
  Stat stat;
  Mode mode;
  Ctrl ctrl;
  Misc misc;
  Baud baud;

public:
  SerialPort(Bus&);
};

}