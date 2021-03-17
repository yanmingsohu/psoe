#include "inter.h"
#include <stdarg.h>

namespace ps1e {


void DisassemblyMips::gm(char const* iname, mips_reg t, char dir, gte_dr d) const {
  warn(DBG_HD "H/L, $%s %c $cop2.r%02d \t\t \x1b[1;30m# $%s=%x, \n",
        pc, bus.read32(pc), iname, rname(t), dir, d, rname(t), reg.u[t]);
}


void DisassemblyMips::gw(char const* iname, gte_dr t, mips_reg s, u32 i) const {
  warn(DBG_HD "H/L, $cop2.r%02d, $%s, %08x \t\t \x1b[1;30m# $%s=%x, [<<2]=%08x \n",
        pc, bus.read32(pc), iname, t, rname(s), i, rname(s), reg.u[s], i << 2);
}


void DisassemblyMips::gc(char const* iname, u32 op) {
  warn(DBG_HD "%08x [1;30m\n", pc, bus.read32(pc), iname, op);
}


void DisassemblyMips::rx(char const* iname, mips_reg s, mips_reg t, const char* cm) const {
  debug(DBG_HD "H/L, $%s, $%s \t\t \x1b[1;30m# %s\n",
        pc, bus.read32(pc), iname, rname(s), rname(t), cm);
}


void DisassemblyMips::rr(char const* iname, mips_reg d, mips_reg s, mips_reg t, const char* cm) const {
  debug(DBG_HD "$%s, $%s, $%s \t\t \x1b[1;30m# $%s = %s\n",
        pc, bus.read32(pc), iname, rname(d), rname(s), rname(t),
        rname(d), cm);
}


void DisassemblyMips::ii(char const* iname, mips_reg t, mips_reg s, u32 i, const char* cm) const {
  debug(DBG_HD "$%s, $%s, 0x%08x \t \x1b[1;30m# $%s = %s\n",
        pc, bus.read32(pc), iname, rname(t), rname(s), i,
        rname(t), cm);
}


void DisassemblyMips::iw(char const* iname, mips_reg t, mips_reg s, u32 i, const char* cm) const {
  debug(DBG_HD "[$%s + 0x%08x], $%s\t \x1b[1;30m# %s\n",
        pc, bus.read32(pc), iname, rname(s), i, rname(t), cm);
}


void DisassemblyMips::i2(char const* iname, mips_reg t, u32 i, const char* cm) const {
  debug(DBG_HD "$%s, 0x%08x\t\t \x1b[1;30m# %s\n",
        pc, bus.read32(pc), iname, rname(t), i, cm);
}


void DisassemblyMips::jj(char const* iname, u32 i, const char *cm) const {
  info(DBG_HD "0x%08x\t\t\t \x1b[1;30m# %s\n", pc, bus.read32(pc), iname, i, cm);
}


void DisassemblyMips::j1(char const* iname, mips_reg s) const {
  info(DBG_HD "$%s\t\t\t\t \x1b[1;30m# $pc=0x%x\n",
        pc, bus.read32(pc), iname, rname(s), reg.u[s]);
}


void DisassemblyMips::ji(char const* iname, mips_reg t, mips_reg s, u32 i, const char* cm) const {
  info(DBG_HD "$%s, $%s, 0x%08x \t \x1b[1;30m# %s\n",
        pc, bus.read32(pc), iname, rname(t), rname(s), i, cm);
}


void DisassemblyMips::j2(char const* iname, mips_reg t, u32 i, const char* cm) const {
  debug(DBG_HD "$%s, 0x%08x\t\t \x1b[1;30m# %s\n",
        pc, bus.read32(pc), iname, rname(t), i, cm);
}


void DisassemblyMips::nn(char const* iname) const {
  debug(DBG_HD "\n", pc, bus.read32(pc), iname);
}


void DisassemblyMips::m1(char const* iname, mips_reg s) const {
  debug(DBG_HD "$%s\t\t\t\t \x1b[1;30m# $%s=%x\n",
        pc, bus.read32(pc), iname, rname(s), rname(s), reg.u[s]);
}


void DisassemblyMips::show_sys_func(u32 addr) const {
  switch (addr) {
  case 0xA0:
      sys_a_func();
      break;

  case 0xB0:
      sys_b_func();
      break;

  case 0xC0:
      sys_c_func();
      break;
  }
}


void DisassemblyMips::show_sys_call() const {
  switch (reg.a0) {
    case 0x00:
      info("NoFunction()\n");
      break;

    case 0x01:
      info("EnterCriticalSection()\n");
      break;

    case 0x02:
      info("ExitCriticalSection()\n");
      break;

    case 0x03:
      info("ChangeThreadSubFunction(addr=%x)\n", reg.a1);
      break;

    default:
      info("DeliverEvent(F0000010h,4000h)\n");
      break;
  }
}


void DisassemblyMips::show_brk() const {
  //TODO
}


#define A0(num, name) case num: sf(#name, 0); break;
#define A1(num, name, a0) case num: sf(#name, 1, #a0); break
#define A2(num, name, a0, a1) case num: sf(#name, 2, #a0, #a1); break
#define A3(num, name, a0, a1, a2) case num: sf(#name, 3, #a0, #a1, #a2); break
#define A4(num, name, a0, a1, a2, a3) case num: sf(#name, 4, #a0, #a1, #a2, #a3); break
#define A5(num, name, a0, a1, a2, a3, a4) \
  case num: sf(#name, 5, #a0, #a1, #a2, #a3, #a4); break


void DisassemblyMips::sys_a_func() const {
  switch (reg.t1) {
    A2(0x00, FileOpen, filename, accessmode);
    A3(0x01, FileSeek, fd,offset,seektype);
    A3(0x02, FileRead, fd,dst,length);
    A3(0x03, FileWrite, fd,src,length);
    A1(0x04, FileClose, fd);
    A3(0x05, FileIoctl, fd,cmd,arg);
    A1(0x06, exit, exitcode);
    A1(0x07, FileGetDeviceFlag, fd);
    A1(0x08, FileGetc, fd);
    A2(0x09, FilePutc, char,fd);
    A1(0x0A, todigit, char);
    A1(0x0B, atof, src);
    A3(0x0C, strtoul, src,src_end,base);
    A3(0x0D, strtol, src,src_end,base);
    A1(0x0E, abs, val);
    A1(0x0F, labs, val);
    A1(0x10, atoi, src);
    A1(0x11, atol, src);
    A2(0x12, atob, src,num_dst);
    A1(0x13, SaveState, buf);
    A2(0x14, RestoreState, buf,param);
    A2(0x15, strcat, dst,src);
    A3(0x16, strncat, dst,src,maxlen);
    A2(0x17, strcmp, str1,str2);
    A3(0x18, strncmp, str1,str2,maxlen);
    A2(0x19, strcpy, dst,src);
    A3(0x1A, strncpy, dst,src,maxlen);
    A1(0x1B, strlen, src);
    A2(0x1C, index, src,char);
    A2(0x1D, rindex, src,char);
    A2(0x1E, strchr, src,char);
    A2(0x1F, strrchr, src,char);
    A2(0x20, strpbrk, src,list);
    A2(0x21, strspn, src,list);
    A2(0x22, strcspn, src,list);
    A2(0x23, strtok, src,list);
    A2(0x24, strstr, str,substr);
    A1(0x25, toupper, char);
    A1(0x26, tolower, char);
    A3(0x27, bcopy, src,dst,len);
    A2(0x28, bzero, dst,len);
    A3(0x29, bcmp, ptr1,ptr2,len);
    A3(0x2A, memcpy, dst,src,len);
    A3(0x2B, memset, dst,fillbyte,len);
    A3(0x2C, memmove, dst,src,len);
    A3(0x2D, memcmp, src1,src2,len);
    A3(0x2E, memchr, src,scanbyte,len);
    A0(0x2F, rand);
    A1(0x30, srand, seed);
    A4(0x31, qsort, base,nel,width,callback);
    A2(0x32, strtod, src,src_end);
    A1(0x33, malloc, size);
    A1(0x34, free, buf);
    A5(0x35, lsearch, key,base,nel,width,callback);
    A5(0x36, bsearch, key,base,nel,width,callback);
    A2(0x37, calloc, sizx,sizy);
    A2(0x38, realloc, old_buf,new_siz);
    A2(0x39, InitHeap, addr,size);
    A1(0x3A, SystemErrorExit, exitcode);
    A0(0x3B, std_in_getchar);
    A1(0x3C, std_out_putchar, char);
    A1(0x3D, std_in_gets, dst);
    A1(0x3E, std_out_puts, src);
    A4(0x3F, printf, txt,param1,param2, etc);
    A0(0x40, SystemErrorUnresolvedException);
    A2(0x41, LoadExeHeader, filename,headerbuf);
    A2(0x42, LoadExeFile, filename,headerbuf);
    A3(0x43, DoExecute, headerbuf,param1,param2);
    A0(0x44, FlushCache);
    A0(0x45, init_a0_b0_c0_vectors);
    A5(0x46, GPU_dw, Xdst,Ydst,Xsiz,Ysiz,src);
    A5(0x47, gpu_send_dmA, 0xXdst,Ydst,Xsiz,Ysiz,src);
    A1(0x48, SendGP1Command, gp1cmd);
    A1(0x49, GPU_cw, gp0cmd);
    A2(0x4A, GPU_cwp, src,num);
    A1(0x4B, send_gpu_linked_list, src);
    A1(0x4C, gpu_abort_dmA, dma);
    A0(0x4D, GetGPUStatus);
    A0(0x4E, gpu_sync);
    A0(0x4F, SystemError);
    A0(0x50, SystemError);
    A3(0x51, LoadAndExecute, filename,stackbase,stackoffset);
    A0(0x52, GetSysSp);
    A1(0x53, set_ioabort_handler, src);
    A0(0x54, CdInit);
    A0(0x55, _bu_init);
    A0(0x56, CdRemove);
    A0(0x5B, dev_tty_init);
    A3(0x5C, dev_tty_open, fcb,path,accessmode);
    A2(0x5D, dev_tty_in_out, fcb,cmd);
    A3(0x5E, dev_tty_ioctl, fcb,cmd,arg);
    A3(0x5F, dev_cd_open, fcb,path,accessmode);
    A3(0x60, dev_cd_read, fcb,dst,len);
    A1(0x61, dev_cd_close, fcb);
    A3(0x62, dev_cd_firstfile, fcb,path,direntry);
    A2(0x63, dev_cd_nextfile, fcb,direntry);
    A2(0x64, dev_cd_chdir, fcb,path);
    A3(0x65, dev_card_open, fcb,path,accessmode);
    A3(0x66, dev_card_read,fcb,dst,len);
    A3(0x67, dev_card_write, fcb,src,len);
    A1(0x68, dev_card_close, fcb);
    A3(0x69, dev_card_firstfile, fcb,path,direntry);
    A2(0x6A, dev_card_nextfile, fcb,direntry);
    A2(0x6B, dev_card_erase, fcb,path);
    A2(0x6C, dev_card_undelete, fcb,path);
    A1(0x6D, dev_card_format, fcb);
    A4(0x6E, dev_card_rename, fcb1,path1,fcb2,path2);
    A1(0x6F, card_clear_error, fcb);
    A0(0x70, _bu_init);
    A0(0x71, CdInit);
    A0(0x72, CdRemove);
    A1(0x78, CdAsyncSeekL, src);
    A1(0x7C, CdAsyncGetStatus, dst);
    A3(0x7E, CdAsyncReadSector, count,dst,mode);
    A1(0x81, CdAsyncSetMode, mode);
    A0(0x85, CdStop);
    A0(0x90, CdromIoIrqFunc1);
    A0(0x91, CdromDmaIrqFunc1);
    A0(0x92, CdromIoIrqFunc2);
    A0(0x93, CdromDmaIrqFunc2);
    A2(0x94, CdromGetInt5errCode, dst1,dst2);
    A0(0x95, CdInitSubFunc);
    A0(0x96, AddCDROMDevice);
    A0(0x97, AddMemCardDevice);
    A0(0x98, AddDuartTtyDevice);
    A0(0x99, AddDummyTtyDevice);
    A0(0x9A, AddMessageWindowDevice);
    A0(0x9B, AddCdromSimDevice);
    A3(0x9C, SetConf, num_EvCB,num_TCB,stacktop);
    A3(0x9D, GetConf, num_EvCB_dst,num_TCB_dst,stacktop_dst);
    A2(0x9E, SetCdromIrqAutoAbort, type,flag);
    A1(0x9F, SetMemSize, megabytes);

    // DTL-H2000
    A0(0xA0, WarmBoot);
    A2(0xA1, SystemErrorBootOrDiskFailure, type,errorcode);
    A0(0xA2, EnqueueCdIntr);
    A0(0xA3, DequeueCdIntr);
    A1(0xA4, CdGetLbn, filename);
    A3(0xA5, CdReadSector, count,sector,buffer);
    A0(0xA6, CdGetStatus);
    A0(0xA7, bu_callback_okay);
    A0(0xA8, bu_callback_err_write);
    A0(0xA9, bu_callback_err_busy);
    A0(0xAA, bu_callback_err_eject);
    A1(0xAB, _card_info, port);
    A1(0xAC, _card_async_load_directory, port);
    A1(0xAD, set_card_auto_format, flag);
    A0(0xAE, bu_callback_err_prev_write);
    A1(0xAF, card_write_test, port);
    A1(0xB2, ioabort_raw, param);
    A1(0xB4, GetSystemInfo, index);
  }
}


#define B0 A0
#define B1 A1
#define B2 A2
#define B3 A3
#define B4 A4

void DisassemblyMips::sys_b_func() const {
  switch (reg.t1) {
    B1(0x00, alloc_kernel_memory, size);
    B1(0x01, free_kernel_memory, buf);
    B3(0x02, init_timer, t,reload,flags);
    B1(0x03, get_timer, t);
    B1(0x04, enable_timer_irq, t);
    B1(0x05, disable_timer_irq, t);
    B1(0x06, restart_timer, t);
    B2(0x07, DeliverEvent, class, spec);
    B4(0x08, OpenEvent, class,spec,mode,func);
    B1(0x09, CloseEvent, event);
    B1(0x0A, WaitEvent, event);
    B1(0x0B, TestEvent, event);
    B1(0x0C, EnableEvent, event);
    B1(0x0D, DisableEvent, event);
    B3(0x0E, OpenThread, reg_PC,reg_SP_FP,reg_GP);
    B1(0x0F, CloseThread, handle);
    B1(0x10, ChangeThread, handle);
    B4(0x12, InitPad, buf1,siz1,buf2,siz2);
    B0(0x13, StartPad);
    B0(0x14, StopPad);
    B4(0x15, OutdatedPadInitAndStart, type,button_dest,unused,unused);
    B0(0x16, OutdatedPadGetButtons);
    B0(0x17, ReturnFromException);
    B0(0x18, SetDefaultExitFromException);
    B1(0x19, SetCustomExitFromException, addr);
    B2(0x20, UnDeliverEvent, class,spec);
    B2(0x32, FileOpen, filename,accessmode);
    B3(0x33, FileSeek, fd,offset,seektype);
    B3(0x34, FileRead, fd,dst,length);
    B3(0x35, FileWrite, fd,src,length);
    B1(0x36, FileClose, fd);
    B3(0x37, FileIoctl, fd,cmd,arg);
    B1(0x38, exit, exitcode);
    B1(0x39, FileGetDeviceFlag, fd);
    B1(0x3A, FileGetc, fd);
    B2(0x3B, FilePutc, char,fd);
    B0(0x3C, std_in_getchar);
    B1(0x3D, std_out_putchar, char);
    B1(0x3E, std_in_gets, dst);
    B1(0x3F, std_out_puts, src);
    B1(0x40, chdir, name);
    B1(0x41, FormatDevice, devicename);
    B2(0x42, firstfile, filename,direntry);
    B1(0x43, nextfile, direntry);
    B2(0x44, FileRename, old_filename,new_filename);
    B1(0x45, FileDelete, filename);
    B1(0x46, FileUndelete, filename);
    B1(0x47, AddDevice, device_info);
    B1(0x48, RemoveDevice, device_name_lowercase);
    B0(0x49, PrintInstalledDevices);
    
    // DTL-H2000
    B1(0x4A, InitCard, pad_enable);
    B0(0x4B, StartCard);
    B0(0x4C, StopCard);
    B1(0x4D, _card_info_subfunc, port);
    B3(0x4E, write_card_sector, port,sector,src);
    B3(0x4F, read_card_sector, port,sector,dst);
    B0(0x50, allow_new_card);
    B1(0x51, Krom2RawAdd, shiftjis_code);
    B1(0x53, Krom2Offset, shiftjis_code);
    B0(0x54, GetLastError);
    B1(0x55, GetLastFileError, fd);
    B0(0x56, GetC0Table);
    B0(0x57, GetB0Table);
    B0(0x58, get_bu_callback_port);
    B1(0x59, testdevice, devicename);
    B1(0x5B, ChangeClearPad, int);
    B1(0x5C, get_card_status, slot);
    B1(0x5D, wait_card_status, slot);
  }
}


#define C0 A0
#define C1 A1
#define C2 A2
#define C3 A3
#define C4 A4

void DisassemblyMips::sys_c_func() const {
  switch (reg.t1) {
    C1(0x00, EnqueueTimerAndVblankIrqs, priority);
    C1(0x01, EnqueueSyscallHandler, priority);
    C2(0x02, SysEnqIntRP, priority,struc);
    C2(0x03, SysDeqIntRP, priority,struc);
    C0(0x04, get_free_EvCB_slot);
    C0(0x05, get_free_TCB_slot);
    C0(0x06, ExceptionHandler);
    C0(0x07, InstallExceptionHandlers);
    C2(0x08, SysInitMemory, addr,size);
    C0(0x09, SysInitKernelVariables);
    C2(0x0A, ChangeClearRCnt, t,flag);
    C1(0x0C, InitDefInt, priority);
    C2(0x0D, SetIrqAutoAck, irq,flag);
    C0(0x0E, dev_sio_init);
    C0(0x0F, dev_sio_open);
    C0(0x10, dev_sio_in_out);
    C0(0x11, dev_sio_ioctl);
    C1(0x12, InstallDevices, ttyflag);
    C0(0x13, FlushStdInOutPut);
    C2(0x15, tty_cdevinput, circ,char);
    C0(0x16, tty_cdevscan);
    C1(0x17, tty_circgetc, circ);
    C2(0x18, tty_circputc, char,circ);
    C2(0x19, ioabort, txt1,txt2);
    C1(0x1A, set_card_find_mode, mode);
    C1(0x1B, KernelRedirect, ttyflag);
    C0(0x1C, AdjustA0Table);
    C0(0x1D, get_card_find_mode);
  }
}


void DisassemblyMips::sf(const char* fname, u8 c, ...) const {
  va_list __args; 
  va_start(__args, c);

  info("%s", fname);
  putchar('(');

  for (u8 i=0; i<c; ++i) {
    char* val = va_arg(__args, char*);
    u32 x;

    switch (i) {
      case 0: x = reg.a0; break;
      case 1: x = reg.a1; break;
      case 2: x = reg.a2; break;
      case 3: x = reg.a3; break;
      default:
        x = bus.read32(reg.sp + 0x10 + (i-4) * 4);
        break;
    }

    if (i) {
      putchar(',');
      putchar(' ');
    }
    info("%s=%Xh", val, x);
  }

  putchar(')');
  putchar('\n');
  va_end(__args);
}

}