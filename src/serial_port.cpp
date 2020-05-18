#include "serial_port.h"
#include <stdio.h>

namespace ps1e {


SerialPort::SerialPort(Bus& bus) : 
    data(*this), stat(*this), mode(*this), ctrl(*this), misc(*this), baud(*this) 
{
  bus.bind_io(&data);
  bus.bind_io(&stat);
  bus.bind_io(&mode);
  bus.bind_io(&ctrl);
  bus.bind_io(&misc);
  bus.bind_io(&baud);
}


u32 ConsoleDataPort::read() {
  return 0xEEE1'0000;
}


void ConsoleDataPort::write(u32 d) {
  putc(d, stdout);
}

}