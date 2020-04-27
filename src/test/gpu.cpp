#include "../gpu.h"
#include "test.h"

namespace ps1e_t {
using namespace ps1e;

void gpu_basic() {
  tsize(sizeof(GpuCtrl), 4, "gpu ctrl reg");
  tsize(sizeof(Command), 4, "gpu command");
}


void test_gpu() {
  gpu_basic();
}

}