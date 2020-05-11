#pragma once 

#include "util.h"

namespace ps1e {

class FileSystem {
};


struct System {
  MemJit jitmem;
};


void boot(FileSystem& fs);

}