#include "serial_port.h"
#include <stdio.h>

namespace ps1e {


SerialPort::SerialPort(Bus& bus) : 
    data(*this), stat(*this), mode(*this), ctrl(*this), misc(*this), baud(*this) 
{
  bus.bind_io(DeviceIOMapper::sio_tx_data,  &data);
  bus.bind_io(DeviceIOMapper::sio_stat,     &stat);
  bus.bind_io(DeviceIOMapper::sio_mode,     &mode);
  bus.bind_io(DeviceIOMapper::sio_ctrl,     &ctrl);
  bus.bind_io(DeviceIOMapper::sio_misc,     &misc);
  bus.bind_io(DeviceIOMapper::sio_baud,     &baud);
}


u32 ConsoleDataPort::read() {
  return 0xEEE1'0000;
}


void ConsoleDataPort::write(u32 d) {
  putc(d, stdout);
}

}