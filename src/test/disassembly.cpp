#include "test.h"
#include "../mips.h"
#include "../disassembly.h"

namespace ps1e_t {
using namespace ps1e;


// https://alanhogan.com/asu/assembler.php
void test_mips() {
  tsize(sizeof(instruction_st), 4, "mips instruction struct");
  mips_instruction i[10] = {
    0x23bdfff8, // addi $sp, $sp, -8
  };
  DisassemblyMips t((u8*)&i, sizeof(i)); 
  t.run();
  eq<s32>(t.getreg().sp, -8, "addi sp");
}


void test_disassembly() {
  test_mips();
}

}