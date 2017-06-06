/* Remote debugging interface for JTAG debugging protocol.
   JTAG connects to CSKY target ops.

   Copyright (C) 1988-2016 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "serial.h"
#include "target.h"
#include "target-descriptions.h"
#include "exceptions.h"
#include "string.h"
#include "regcache.h"
#include <ctype.h>
#include "csky-tdep.h"
#include "gdbthread.h"
#include "breakpoint.h"
#include "arch-utils.h"
#include "user-regs.h"
#include "csky-djp.h"
#include "cli/cli-cmds.h"
#include "csky-kernel.h"
#include "opcode/csky.h"

#if !defined(_WIN32) || defined (__CYGWIN__)
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>


#define SOCKET_ERROR -1
typedef int SOCKET;

#define CLOSESOCKET(sock)  \
        {  \
            close (sock); \
            sock = 0;   \
        }

#else  /* _WIN32 && ! __CYGWIN__ */

#include <winsock.h>
#include <signal.h>
#include <windows.h>
#define _WIN32_BOOLEAN_HAS_DEFINED
#define sleep(n) Sleep(1000 * (n))

#define CLOSESOCKET(sock)  \
        {  \
            closesocket (sock); \
            sock = 0; \
            WSACleanup (); \
        }

#endif  /* _WIN32 && !__CYGWIN__ */


#ifndef CORE_REG
typedef unsigned int CORE_REG;
#endif

#define MAGIC_NULL_PID 42000
#define CSKY_MAX_WATCHPOINT     2
#define CSKY_MAX_HW_BREAKPOINT_WATCHPOINT_803 1
#define BKPT_NUM_MASK 0xf000

#define SOCKET_DEBUG_PRINTF(args)  {} /* { printf args; }  */
#define DJP_DEBUG_PRINTF(args)     {} /* { printf args; }  */
#define TARGET_DEBUG_PRINTF(args)  {} /* { printf args; }  */

/* From breakpoint.c.  */
extern struct breakpoint *breakpoint_chain;


static int max_hw_breakpoint_num;
static int max_watchpoint_num;
static int proxy_sub_ver = 0;
static int proxy_main_ver = 0;

static int resume_stepped = 0;
static int hit_watchpoint = 0;
static CORE_ADDR hit_watchpoint_addr = 0;

/* This is the ptid we use while we're connected to the remote.  Its
   value is arbitrary, as the target doesn't have a notion of
   processes or threads, but we need something non-null to place in
   inferior_ptid.  */
ptid_t remote_csky_ptid;

int already_load = 0;
static int load_flag = 1;
static int prereset_flag = 0;

/* Download vars.  */
static unsigned int download_write_size = 4096;
static unsigned long pre_load_block_size = 0;
static unsigned long download_total_size = 0;


/* One object for socket.  */
static SOCKET desc_fd;


/* CSKY new djp support flag. default is support.  */
/* When Server ProxyLayer version < 1.4, will not support.  */
static int CK_NewDJP_support = 0;

const char DJP_HEADER[] = "djp://";
const char JTAG_HEADER[] = "jtag://";

/* Defination of local variables and functions for csky_wait.  */
static int interrupt_count = 0;
static void (*ofunc) (int);

/* Kernel ops.  */
static struct kernel_ops *current_kernel_ops = NULL;


/* For return error info.  */
static char strError[4096];

static void csky_interrupt (int);
static void csky_interrupt_twice (int);
static void csky_interrupt_query (void);


static void seterrorinfo (const char *s);
static char* errorinfo (void);
static void csky_get_hw_breakpoint_num (void);
static void csky_get_hw_breakpoint_num_new (void);
static void csky_get_hw_breakpoint_num_mid (void);
static void csky_get_hw_breakpoint_num_old (void);
static int csky_pctrace (char *args, unsigned int *pclist, int *depth,
                         int from_tty);

/* Funtion for socket.  */
static SOCKET socket_connect_to_server (char* hostname, char* port);
static int    socket_disconnet (SOCKET fd);
static int    socket_send (SOCKET fd, void* buf, int len);
static int    socket_receive (SOCKET fd, void* buf, int len);


#define HAD_VERSION(id) ((id >>4) & 0xf)
/* ----------------------- For read had register -------------------  */
typedef enum
{
  HID = 0x2,         /* had ID.  */
  HTCR = 0x3,        /* HAD trace counter.  */
  MBCA = 0x4,        /* memory bkpt couter A.  */
  MBCB = 0x5,        /* memory bkpt couter B  */
  PCFIFO = 0x6,      /* pc fifo for newest 8 jump instructions.  */
  BABA = 0x7,        /* break address reg A.  */
  BABB = 0x8,        /* break address reg B.  */
  BAMA = 0x9,        /* break address mask reg A.  */
  BAMB = 0xa,        /* break address mask reg B.  */
  CPUSCR = 0xb,      /* cpu scan chain ,128bit.  */
  BYPASS = 0xc,      /* bypass reg.  */
  HCR = 0xd,         /* had control reg.  */
  HSR = 0xe,         /* had status reg.  */
  MMODE =0xf,        /* memory mode reg.  */
  SRAMMODE = 0x10,   /* sram mode reg.  */
  SCRWBBR = 0x11,    /* write back bus reg ,had V2 or newer only.  */
  SCRPSR = 0x12,     /* processor status reg,had V2 or newer only.  */
  SCRPC = 0x13,      /* program counter,had V2 or newer only.  */
  SCRIR = 0x14,      /* instruction reg,had V2 or newer only.  */
  SCRCSR = 0x15,     /* control state reg,had V2 or newer only.  */
  DDCADDR = 0x18,    /* ddc address reg.  */
  DDCDATA = 0x19,    /* ddc data reg.  */
  BSEL = 0x1e,       /* bank select reg.  */
  HCDI = 0x1f        /* had cdi.  */
} HADREG;


#ifndef CSKYGDB_CONFIG_ABIV2     /* For abiv1  */
static int csky_register_conversion_v1[] = {
  /* General register 0~15: 0 ~ 15.  */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
  0x0,   0x1,   0x2,   0x3,   0x4,   0x5,   0x6,   0x7,
  0x8,   0x9,   0xa,   0xb,   0xc,   0xd,   0xe,   0xf,

  /* DSP hilo register: 79, 80.  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       79,      80,   -1,    -1,


  /* FPU reigster: 24~55.  */
  /*
  "cp1gr0", "cp1gr1", "cp1gr2",  "cp1gr3",
  "cp1gr4", "cp1gr5", "cp1gr6",  "cp1gr7",
  "cp1gr8", "cp1gr9", "cp1gr10", "cp1gr11",
  "cp1gr12", "cp1gr13", "cp1gr14", "cp1gr15",
  "cp1gr16", "cp1gr17", "cp1gr18", "cp1gr19",
  "cp1gr20", "cp1gr21", "cp1gr22", "cp1gr23",
  "cp1gr24", "cp1gr25", "cp1gr26", "cp1gr27",
  "cp1gr28", "cp1gr29", "cp1gr30", "cp1gr31",
  */
  0x100, 0x101, 0x102, 0x103,
  0x104, 0x105, 0x106, 0x107,
  0x108, 0x109, 0x10a, 0x10b,
  0x10c, 0x10d, 0x10e, 0x10f,
  0x110, 0x111, 0x112, 0x113,
  0x114, 0x115, 0x116, 0x117,
  0x118, 0x119, 0x11a, 0x11b,
  0x11c, 0x11d, 0x11e, 0x11f,


  /* Hole.  */
  /*
  "",      "",    "",     "",     "",    "",   "",    "",
  "",      "",    "",     "",     "",    "",   "",    "",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,


/* ---- Above all must according to compiler for debug info ----  */

  /*
   "all",   "gr",   "ar",   "cr",   "",    "",    "",    "",
   "cp0",   "cp1",  "",     "",     "",    "",    "",    "",
   "",      "",     "",     "",     "",    "",    "",    "cp15",
  */

  /* PC : 64  */
  /*
   "pc",
  */
  64,


  /* Optional register(ar) :  16 ~ 31.  */
  /*
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  0x10,  0x11,  0x12,  0x13,  0x14,  0x15,  0x16,  0x17,
  0x18,  0x19,  0x1a,  0x1b,  0x1c,  0x1d,  0x1e,  0x1f,


  /* Control registers (cr) : 32 ~ 63.  */
  /*
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "",
  */
  0x20,  0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,
  0x28,  0x29,  0x2a,  0x2b,  0x2c,  0x2d,  0x2e,  0x2f,
  0x30,  0x31,  0x32,  0x33,  0x34,  0x35,  0x36,  0x37,
  0x38,  0x39,  0x3a,  0x3b,  0x3c,  0x3d,  0x3e,  -1,


  /* FPC control register: 0x100 & (32 ~ 38).  */
  /*
  "cp1cr0","cp1cr1","cp1cr2","cp1cr3","cp1cr4","cp1cr5","cp1cr6",
  */
  0x120, 0x121, 0x122, 0x123, 0x124, 0x125, 0x126,


  /* MMU control register: 0xf00 & (32 ~ 40).  */
  /*
  "cp15cr0", "cp15cr1", "cp15cr2", "cp15cr3",
  "cp15cr4", "cp15cr5", "cp15cr6", "cp15cr7",
  "cp15cr8", "cp15cr9", "cp15cr10","cp15cr11",
  "cp15cr12","cp15cr13", "cp15cr14","cp15cr15",
  "cp15cr16","cp15cr29", "cp15cr30","cp15cr31"
  */
  0xf20,  0xf21,  0xf22,  0xf23,
  0xf24,  0xf25,  0xf26,  0xf27,
  0xf28,  0xf29,  0xf2a,  0xf2b,
  0xf2c,  0xf2d,  0xf2e,  0xf2f,
  0xf30,  0xf3d,  0xf3e,  0xf3f
};

static int
csky_target_xml_register_conversion_v1 (int regno)
{
  if ((regno >= 0) || (regno <= CSKY_ABIV1_NUM_REGS))
    return csky_register_conversion_v1[regno];
  return -1;
}

#endif /* not CSKYGDB_CONFIG_ABIV2 */

static int csky_register_conversion_v2[] = {
  /* General register 0 ~ 15: 0 ~ 15.  */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
  0x0,   0x1,   0x2,   0x3,   0x4,   0x5,   0x6,   0x7,
  0x8,   0x9,   0xa,   0xb,   0xc,   0xd,   0xe,   0xf,


  /* General register 16 ~ 31: 96 ~ 111.  */
  /*
  "r16",   "r17",  "r18",   "r19",  "r20",  "r21",  "r22",  "r23",
  "r24",   "r25",  "r26",   "r27",  "r28",  "r29",  "r30",  "r31",
  */
  0x60,    0x61,   0x62,    0x63,   0x64,   0x65,   0x66,   0x67,
  0x68,    0x69,   0x6a,    0x6b,   0x6c,   0x6d,   0x6e,   0x6f,


  /* DSP hilo register: 36 ~ 37 : 79 ~ 80.  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       79,      80,   -1,    -1,


  /* FPU/VPU general reigsters: 40~71 : 0x4200 ~ 0x422f.  */
  /*
  "fr0", "fr1",  "fr2",  "fr3",  "fr4",  "fr5",  "fr6",  "fr7",
  "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
  "vr0", "vr1",  "vr2",  "vr3",  "vr4",  "vr5",  "vr6",  "vr7",
  "vr8", "vr9", "vr10", "vr11", "vr12", "vr13", "vr14", "vr15",
  */
  0x4220, 0x4221, 0x4222, 0x4223, 0x4224, 0x4225, 0x4226, 0x4227,
  0x4228, 0x4229, 0x422a, 0x422b, 0x422c, 0x422d, 0x422e, 0x422f,
  0x4200, 0x4201, 0x4202, 0x4203, 0x4204, 0x4205, 0x4206, 0x4207,
  0x4208, 0x4209, 0x420a, 0x420b, 0x420c, 0x420d, 0x420e, 0x420f,

/* ---- Above all must according to compiler for debug info ----  */

  /* "all",   "gr",   "ar",   "cr",   "",    "",    "",    "",  */
  /* "cp0",   "cp1",  "",     "",     "",    "",    "",    "",  */
  /* "",      "",     "",     "",     "",    "",    "",    "cp15",  */

  /* PC : 72 : 64  */
  /*
  "pc",
  */
  64,


  /* Optional register(ar) :  73~88 : 16 ~ 31.  */
  /*
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  0x10,  0x11,  0x12,  0x13,  0x14,  0x15,  0x16,  0x17,
  0x18,  0x19,  0x1a,  0x1b,  0x1c,  0x1d,  0x1e,  0x1f,


  /* Control registers (cr) : 89 ~ 119 : 32 ~ 62.  */
  /*
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "cr31",
  */
  0x20,  0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,
  0x28,  0x29,  0x2a,  0x2b,  0x2c,  0x2d,  0x2e,  0x2f,
  0x30,  0x31,  0x32,  0x33,  0x34,  0x35,  0x36,  0x37,
  0x38,  0x39,  0x3a,  0x3b,  0x3c,  0x3d,  0x3e,  0x3f,


  /* FPU/VPU control register: 121 ~ 123 : 0x4210 ~ 0x4212.  */
  /* User SP : 127 : 0x410e.  */
  /*
  "fid",   "fcr",  "fesr",    "",    "",    "",    "usp",
  */
  0x4210,  0x4211,  0x4212,   -1,    -1,    -1,    0x410e,


  /* MMU control register: 128 ~ 136 : 0x4f00.  */
  /*
  "mcr0", "mcr2", "mcr3", "mcr4", "mcr6", "mcr8", "mcr29", "mcr30",
  "mcr31", "",     "",     "",
  */
  0x4f00, 0x4f01, 0x4f02, 0x4f03, 0x4f04, 0x4f05, 0x4f06, 0x4f07,
  0x4f08,   -1,     -1,     -1,


  /* Profiling software general registers: 140 ~ 153 : 0x6000 ~ 0x600d.  */
  /* Profiling control registers: 154 ~ 157 : 0x6030 ~ 0x6033.  */
  /*
  "profcr0",  "profcr1",  "profcr2",  "profcr3", "profsgr0", "profsgr1",
  "profsgr2", "profsgr3", "profsgr4", "profsgr5","profsgr6", "profsgr7",
  "profsgr8", "profsgr9", "profsgr10","profsgr11","profsgr12","profsgr13",
  "",         "",
  */
  0x6030,     0x6031,     0x6032,     0x6033,     0x6000,     0x6001,
  0x6002,     0x6003,     0x6004,     0x6005,     0x6006,     0x6007,
  0x6008,     0x6009,     0x600a,     0x600b,     0x600c,     0x600d,
    -1,         -1,

  /* Profiling architecture general registers: 160 ~ 174: 0x6010 ~ 0x601e.  */
  /*
  "profagr0", "profagr1", "profagr2", "profagr3", "profagr4", "profagr5",
  "profagr6", "profagr7", "profagr8", "profagr9", "profagr10","profagr11",
  "profagr12","profagr13","profagr14", "",
  */
  0x6010,     0x6011,     0x6012,     0x6013,     0x6014,     0x6015,
  0x6016,     0x6017,     0x6018,     0x6019,     0x601a,     0x601b,
  0x601c,     0x601d,     0x601e,      -1,


  /* Profiling extension general registers: 176 ~ 188 : 0x6020 ~ 0x602c.  */
  /*
  "profxgr0", "profxgr1", "profxgr2", "profxgr3", "profxgr4", "profxgr5",
  "profxgr6", "profxgr7", "profxgr8", "profxgr9", "profxgr10","profxgr11",
  "profxgr12",
  */
  0x6020,     0x6021,     0x6022,     0x6023,     0x6024,     0x6025,
  0x6026,     0x6027,     0x6028,     0x6029,     0x602a,     0x602b,
  0x602c,


  /* Control reg in bank1.  */
  /*
  "",   "",   "",   "",   "",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   "",
  "cp1cr16",  "cp1cr17",  "cp1cr18",  "cp1cr19",  "cp1cr20",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   "",
  */
  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  0x4110,    0x4111,    0x4112,    0x4113,   0x4114,  -1,  -1,  -1,
  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,



  /* Control reg in bank3.  */
  /*
  "sepsr",   "sevbr",   "seepsr",   "",   "seepc",   "",   "nsssp",   "seusp",
  "sedcr",   "",   "",   "",   "",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   ""
  */
  0x4300,   0x4301,   0x4302,   -1,   0x4304,   -1,   0x4306,   0x4307,
  0x4308,-1,   -1,   -1,   -1,   -1,   -1,   -1,
  -1,    -1,   -1,   -1,   -1,   -1,   -1,   -1,
  -1,    -1,   -1,   -1,   -1,   -1,   -1,   -1
};

static int
csky_target_xml_register_conversion_v2 (int regno)
{
  if ((regno >= 0) && (regno <= 31)) /* GPR r0~r31.  */
    return regno;
  else if (regno == 36) /* HI.  */
    return 79;
  else if (regno == 37) /* LO.  */
    return 80;
  else if ((regno >= 40) && (regno <= 55)) /* FPU.  */
    return (0x4220 + (regno - 40));
  else if ((regno >= 56) && (regno <= 71)) /* VPU.  */
    return (0x4200 + (regno - 55));
  else if (regno == 72) /* PC.  */
    return 64;
  else if ((regno >= 73) && (regno <= 120)) /* ar0~ar15 vs cr0~vr31.  */
    return (0x10 + (regno - 73));
  else if ((regno >= 121) && (regno <= 123)) /* FPU/VPU_CR.  */
    return (0x4210 + (regno - 121));
  else if (regno == 127) /* USP.  */
    return 0x410e;
  else if ((regno >= 128) && (regno <= 136)) /* MMU Cr_bank15.  */
    return (0x4f00 + (regno - 128));
  else if ((regno >= 140) && (regno <= 143)) /* Prof-soft-general.  */
    return (0x6030 + (regno - 140));
  else if ((regno >= 144) && (regno <= 157)) /* Prof-cr.  */
    return (0x6000 + (regno - 144));
  else if ((regno >= 160) && (regno <= 174)) /* Prof-arch.  */
    return (0x6010 + (regno - 160));
  else if ((regno >= 176) && (regno <= 188)) /* Prof-exten.  */
    return (0x6020 + (regno - 188));
  else if ((regno >= 189) && (regno <= 220) && (regno != 203))/* Cr_brank1.  */
    return (0x4100 + (regno - 189));
  else if ((regno >= 221) && (regno <= 252)) /* Cr_bank3.  */
    return (0x4300 + (regno - 221));
  else if ((regno >= 253) && (regno <= 275)) /* Cr_bank15 left.  */
    return (0x4f09 + (regno - 253));
  else if ((regno >= 276) && (regno <= 307)) /* Cr_bank2.  */
    return (0x42e0 + (regno - 276));
  else if ((regno >= 308) && (regno <= 659)) /* Cr_bank4 ~ bank14.  */
    return (0x4400 + (((regno - 308)/32)<<8) + (regno - 308)%32);
  else if ((regno >= 660) && (regno < 1171)) /* Cr_bank16 ~ bank31.  */
    return (0x5000 + (regno - 660));
  else
    return -1;
}

static int
csky_register_convert (int regno, struct regcache *regcache)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
#ifndef CSKYGDB_CONFIG_ABIV2 /* FOR ABIV1 */
  if (((gdbarch_bfd_arch_info (get_regcache_arch (regcache))->mach
                               & CSKY_ARCH_MASK) == CSKY_ARCH_510)
       ||
      ((gdbarch_bfd_arch_info (get_regcache_arch (regcache))->mach
                               & CSKY_ARCH_MASK) == CSKY_ARCH_610))
    {
      if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
        return csky_target_xml_register_conversion_v1 (regno);
      return csky_register_conversion_v1[regno];
    }
  else
    {
      if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
        return csky_target_xml_register_conversion_v2 (regno);
       return csky_register_conversion_v2[regno];
    }
#else
  if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
      return csky_target_xml_register_conversion_v2 (regno);
  return csky_register_conversion_v2[regno];
#endif
}

/* ------------------------------------
           Functions for socket
   ------------------------------------  */

/* Create the socket handle for communicating to server
   HOSTNAME: ip address or hostname of server
   SPORT   : the port number of server with decimal string.  */

static SOCKET
socket_connect_to_server (char* hostname, char* sPort)
{
  struct hostent *host;
  struct sockaddr_in sin;
  struct servent *service;
  struct protoent *protocol;
  SOCKET sock;
  int nTimeOut; /* Set timeout.  */
  struct timeval tTimeOut ;
  fd_set rdevents, wrevents, exevents;
  char sTemp[256], sTemp2[256];
  char *s;
  int result, err, len;
  char* proto_name = "tcp";
  int nPort = 0;
  int on_off = 0; /* Turn off Nagel's algorithm on the socket. */
  unsigned long block = 0, nonblock = 1;


#if defined(_WIN32) && !defined (__CYGWIN__)
  WSADATA wsaData;
  WORD wVersionRequested;

  wVersionRequested = MAKEWORD (2, 2);
  err = WSAStartup (wVersionRequested, &wsaData);
  if (err != 0)
    {
      /* csky_status = TARGET_STOPPED;  */
      seterrorinfo ("Cant find WinSock DLL\n");
      return 0;
    }
#endif

  if (!(protocol = getprotobyname (proto_name)))
    {
      sprintf (sTemp, "connect_to_server: Protocol \"%s\" not available.\n",
               proto_name);
      seterrorinfo (sTemp);
      return 0;
    }

  /* Convert name to an integer only if it is well formatted.
     Otherwise, assume that it is a service name.  */

  nPort = strtol (sPort, &s, 10);
  if (*s)
    nPort = 0;

  if (!nPort)
    {
      if (!(service = getservbyname (hostname, protocol->p_name)))
        {
          sprintf (sTemp, "connect_to_server: Unknown service \"%s\".\n",
                   hostname);
          seterrorinfo (sTemp);
          return 0;
        }

      nPort = ntohs (service->s_port);
    }

  if (!(host = gethostbyname (hostname)))
    {
      sprintf (sTemp, "connect_to_server: Unknown host \"%s\"\n", hostname);
      seterrorinfo (sTemp);
      return 0;
    }

  if ((sock = socket (PF_INET,SOCK_STREAM, 0)) < 0)
    {
      sprintf (sTemp, "connect_to_server: can't create socket errno = %d\n",
               errno);
      sprintf (sTemp2, "%s\n", strerror (errno));
      strcat (sTemp, sTemp2);
      seterrorinfo (sTemp);
      return 0;
    }

#if defined(_WIN32) && !defined (__CYGWIN__)
  if (ioctlsocket (sock, FIONBIO, &nonblock) < 0)
#else
  if (ioctl (sock, FIONBIO, &nonblock) < 0)
#endif
    {
      sprintf (sTemp, "Unable to set socket %d as nonblock mode failure.",
               sock);
      seterrorinfo (sTemp);
      CLOSESOCKET (sock);
      return -1;
    }

  memset (&sin, 0, sizeof (sin));
  sin.sin_family = host->h_addrtype;
  memcpy (&sin.sin_addr, host->h_addr_list[0], host->h_length);
  sin.sin_port = htons (nPort);

  SOCKET_DEBUG_PRINTF (("Before connect.\n"));
  result = connect (sock, (const struct sockaddr *)&sin, sizeof (sin));
#if !defined(_WIN32) || defined (__CYGWIN__)
  if (result && (errno != EINPROGRESS))
#else
  if (result && (WSAGetLastError () != WSAEWOULDBLOCK))
#endif
    {
      /* Failure, set error info.  */
      seterrorinfo ("connect to proxy server failure,"
                    " please check the server.\n");
      return 0;
    }
  SOCKET_DEBUG_PRINTF (("After connect.\n"));

  /* Wait for success, if RESULT == 0, socket created successfully.  */
  if (result != 0)
    {
      /* Select.  */
      SOCKET_DEBUG_PRINTF (("Before select.\n"));
      FD_ZERO (&rdevents);
      FD_SET (sock, &rdevents);
      wrevents = rdevents;   /* Wrtie.  */
      exevents = rdevents;   /* Exception.  */

      tTimeOut.tv_sec = 5; /* Set the connect timeout.  */
      tTimeOut.tv_usec =0;
      result = select (sock + 1, &rdevents, &wrevents, &exevents, &tTimeOut);
      if (result < 0)
        {
          /* Create socket error.  */
          SOCKET_DEBUG_PRINTF (("select result < 0.\n"));
          goto error_ret;
        }
      if (result == 0)  /* Time out.  */
        {
          SOCKET_DEBUG_PRINTF (("select result == 0.\n"));
          goto error_ret;
        }
      SOCKET_DEBUG_PRINTF (("After select.\n"));

      if (!FD_ISSET (sock, &rdevents) && !FD_ISSET (sock, &wrevents))
        {
          SOCKET_DEBUG_PRINTF (("isset error.\n"));
          goto error_ret;
        }

    }

  /* Set block mode.  */
#if defined(_WIN32) && !defined (__CYGWIN__)
  if (ioctlsocket (sock, FIONBIO, &block) < 0)
#else
  if (ioctl (sock, FIONBIO, &block) < 0)
#endif
    {
      sprintf (sTemp, "Unable to set socket %d as block mode failure.", sock);
      seterrorinfo (sTemp);
      CLOSESOCKET (sock);
      return -1;
    }

  if (setsockopt (sock, protocol->p_proto, TCP_NODELAY, (const char*)&on_off,
                  sizeof (int)) < 0)
    {
      sprintf (sTemp, "Unable to disable Nagel's algorithm for socket %d.\n"
                      "setsockopt", sock);
      seterrorinfo (sTemp);
      CLOSESOCKET (sock);
      return 0;
    }

#if 0
  /* Set Recv and Send time out.  */
  if ( setsendtimeout (sock, 3) < 0)
    {
      /* Failure, the error info filled in sub function.  */
      return 0;
    }
  if (setrecvtimeout (sock, 3) < 0)
    {
      /* Failure, the error info filled in sub function.  */
      return 0;
    }
#endif

  return sock;

error_ret:
  sprintf (sTemp, "connect_to_server: connect failed  errno = %d\n", errno);
  sprintf (sTemp2, "%s\n", strerror (errno));
  strcat (sTemp, sTemp2);
  seterrorinfo (sTemp);
  CLOSESOCKET (sock);
  return 0;

}

/* Close the socket handle fd.  */

static int
socket_disconnet (SOCKET fd)
{
  int block = 0;
  struct linger linger;
  char sTemp[256];

  linger.l_onoff = 0;
  linger.l_linger = 0;

  /* First, make sure we're non blocking.  */
#if !defined(_WIN32) || defined (__CYGWIN__)
  ioctl (fd, FIONBIO, &block);
#else
  ioctlsocket (fd, FIONBIO, (u_long *)&block);
#endif

  /* Now, make sure we don't linger around.  */
  if (setsockopt (fd, SOL_SOCKET, SO_LINGER,
                 (const char *) &linger, sizeof (linger)) < 0)
    {
      sprintf (sTemp, "Unable to disable SO_LINGER for socket %d.", fd);
      seterrorinfo (sTemp);
    }

  CLOSESOCKET (fd);
  return 0;
}

/* Send data to the socket.  */

static int
socket_send (SOCKET fd, void* buf, int len)
{
    int n;
    char* w_buf = (char*)buf;

    if (fd == 0)
      {
        error ("You cannot do nothing about remote target, "
               "when the connectting is valid.\n");
      }

    while (len)
      {
        n = send (fd, w_buf, len, 0);
        if (n == SOCKET_ERROR)
          {
            if (errno == EINTR)
              {
                continue;
              }
#if !defined(_WIN32) || defined (__CYGWIN__)
            if (errno != EWOULDBLOCK)
#else
            if (GetLastError() != WSAEWOULDBLOCK)
#endif
              {
                return JTAG_PROXY_SERVER_TERMINATED;
              }
            continue;
          }
        else
          {
            len -= n;
            w_buf += n;
          }
      }

    return 0;
}

/* Receive data from the socket.  */

static int
socket_receive (SOCKET fd, void* buf, int len)
{
  int n, to;
  char* r_buf = (char*)buf;

  if (fd == 0)
    {
      error ("You cannot do nothing about remote target, "
             "when the connectting is valid.\n");
    }

  to = 0;
  while (len)
    {
      n = recv (fd, r_buf, len, 0);
      if (n == 0)
        {
          CLOSESOCKET (fd);
          return JTAG_PROXY_SERVER_TERMINATED;
        }
      else if (n == SOCKET_ERROR)
        {
          if (errno == EINTR)
            {
              continue;
            }
#if !defined(_WIN32) || defined (__CYGWIN__)
          if (errno != EWOULDBLOCK || (to < 5))
#else
          if ((GetLastError () != WSAEWOULDBLOCK
               && GetLastError () != WSAETIMEDOUT)
              || (to > 5))
#endif
            {
              CLOSESOCKET (fd);
              return JTAG_PROXY_SERVER_TERMINATED;
            }
          to++;
          continue;
        }
      else
        {
          len -= n;
          r_buf += n;
        }
    }

  return 0;
}

/* ------------------------------------
              Error Info
   ------------------------------------  */
static void
seterrorinfo (const char *s)
{
  strcpy (strError, s);
}

static char*
errorinfo (void)
{
  return strError;
}


/* ----------------------------------
      Struct of CSKY hardware ops
   ----------------------------------  */
struct csky_hardware_ops
{
  char* to_shortname;   /* Name this target type.   */
  int (*to_open) (const char *args);  /* Connect and  Init target.  */
  int (*to_close) (void);   /* Destruct target.  */
  int (*to_read_reg)  (int regno, CORE_REG* val);
/* ---------------------- For read HAD reg -------------------  */
  int (*to_hw_bkpt_num) (unsigned int* val1, unsigned int* val2);
  int (*to_read_had_reg) (int regno, unsigned int* val);
  int (*to_write_had_reg) (int regno, unsigned int val);
/* ---------------------- End read HAD reg -------------------  */
  int (*to_write_reg) (int regno, CORE_REG val);
  int (*to_read_xreg) (struct regcache *regcache, int regno);
  int (*to_write_xreg) (struct regcache *regcache, int regno);
  int (*to_read_mem)  (CORE_ADDR addr, void* data, int nSize, int nCount);
  int (*to_write_mem) (CORE_ADDR addr, void* data, int nSize, int nCount);
  int (*to_insert_hw_breakpoint) (CORE_ADDR addr);
  int (*to_remove_hw_breakpoint) (CORE_ADDR addr);
  int (*to_insert_watchpoint) (CORE_ADDR addr, int len, int type,
                               int counter);
  int (*to_remove_watchpoint) (CORE_ADDR addr, int len, int type,
                               int counter);
  int (*to_reset) (void);
  int (*to_soft_reset) (int insn);
  int (*to_enter_debugmode) (void);
  int (*to_check_debugmode) (int* status);
  int (*to_exit_debug) (void);
  int (*to_singlestep) (void);
  int (*to_set_profiler) (void *args);
  int (*to_get_profiler) (void **data, int *length);
  int (*to_trace_pc) (U32 *pclist, int *depth);
  int (*to_endianinfo) (int *endian);
  int (*to_server_version) (int *version);
  enum target_xfer_status (*to_read_xmltdesc) (ULONGEST offset,
                                               void *data,
                                               ULONGEST len,
                                               ULONGEST *xfered_len);
  void (*to_exec_command) (char *args, int from_tty);
  /* Executes extended command on the target.  */
  int to_magic;    /* Should be OPS_MAGIC.  */
};


/* -----------------------------------------------------
    Abstract protocol level for hardware operations
   -----------------------------------------------------  */
static struct csky_hardware_ops* current_hardware_ops = NULL;

static int hardware_open (const char* args);
static int hardware_close (void);
static int hardware_read_reg (int regno, CORE_REG* val);
/* --------------------- For read HAD reg ---------------------  */
static int hardware_read_had_reg (int regno, unsigned int* val);
static int hardware_write_had_reg (int regno, unsigned int val);
static int hardware_hw_bkpt_num (unsigned int *val1, unsigned int *val2);
/* --------------------- End read HAD reg----------------------  */
static int hardware_write_reg (int regno, CORE_REG val);
static int hardware_read_xreg (struct regcache *regcache, int regno);
static int hardware_write_xreg (struct regcache *regcache, int regno);
static int hardware_read_mem (CORE_ADDR addr, void* data, int nSize,
                              int nCount);
static int hardware_write_mem (CORE_ADDR addr, void* data, int nSize,
                               int nCount);
static int hardware_insert_hw_breakpoint (CORE_ADDR addr);
static int hardware_remove_hw_breakpoint (CORE_ADDR addr);
static int hardware_insert_watchpoint (CORE_ADDR addr, int len, int type,
                                       int counter);
static int hardware_remove_watchpoint (CORE_ADDR addr, int len, int type,
                                       int counter);
static int hardware_enter_debugmode (void);
static int hardware_check_debugmode (int* status);
static int hardware_reset (void);
static int hardware_soft_reset (int insn);
static int hardware_singlestep (void);
static int hardware_exit_debugmode (void);
static int hardware_set_profiler (void *args);
static int hardware_get_profiler (void **data, int *length);
static int hardware_trace_pc (U32 *pclist, int* depth);
static int hardware_endianinfo (int *endian);
static int hardware_server_version (int *version);
static enum target_xfer_status
hardware_read_xmltdesc (ULONGEST offset, void *data,
                        ULONGEST len, ULONGEST *xfered_len);

/* --------------------------------------------
   D&JP protocol level for hardware operations
   --------------------------------------------  */
static int djp_open (const char* args);
static int djp_close (void);
static int djp_read_reg (int regno, CORE_REG* val);
/* --------------------- For read HAD reg ---------------------  */
static int djp_read_had_reg (int regno, unsigned int* val);
static int djp_write_had_reg (int regno, unsigned int val);
static int djp_hw_bkpt_num (unsigned int *val1, unsigned int *val2);
/* --------------------- End read HAD reg----------------------  */
static int djp_write_reg (int regno, CORE_REG val);
static int djp_read_xreg (struct regcache *regcache, int regno);
static int djp_write_xreg (struct regcache *regcache, int regno);
static int djp_read_mem (CORE_ADDR addr, void* data, int nSize, int nCount);
static int djp_write_mem (CORE_ADDR addr, void* data, int nSize, int nCount);
static int djp_insert_hw_breakpoint (CORE_ADDR addr);
static int djp_remove_hw_breakpoint (CORE_ADDR addr);
static int djp_insert_watchpoint (CORE_ADDR addr, int len, int type,
                                  int counter);
static int djp_remove_watchpoint (CORE_ADDR addr, int len, int type,
                                  int counter);
static int djp_reset (void);
static int djp_soft_reset (int insn);
static int djp_enter_debug (void);
static int djp_exit_debug (void);
static int djp_singlestep (void);
static int djp_check_debugmode (int* status);
static int djp_set_profiler (void *args);
static int djp_get_profiler (void **data, int *length);
static int djp_trace_pc (U32 *pclist, int *depth);
static int djp_endianinfo (int *endian);
static int djp_server_version (int *version);
static int djp_do_error (S32 status);
static enum target_xfer_status
djp_read_xmltdesc (ULONGEST offset, void *data,
                   ULONGEST len, ULONGEST *xfered_len);

static struct csky_hardware_ops djp_ops =
{
  "djp",
  djp_open,
  djp_close,
  djp_read_reg,
/* --- For read HAD reg ---  */
  djp_hw_bkpt_num,
  djp_read_had_reg,
  djp_write_had_reg,
/* --- End read HAD reg ---  */
  djp_write_reg,
  djp_read_xreg,
  djp_write_xreg,
  djp_read_mem,
  djp_write_mem,
  djp_insert_hw_breakpoint,
  djp_remove_hw_breakpoint,
  djp_insert_watchpoint,
  djp_remove_watchpoint,
  djp_reset,
  djp_soft_reset,
  djp_enter_debug,
  djp_check_debugmode,
  djp_exit_debug,
  djp_singlestep,
  djp_set_profiler,
  djp_get_profiler,
  djp_trace_pc,
  djp_endianinfo,
  djp_server_version,
  djp_read_xmltdesc,
  NULL,
  OPS_MAGIC
};


/* CSKY djp ops.  */

const char *strdjperrorinfo[] = {
  "No Error.\n",
  "Undefined Error.\n",
  "Undefined Error.\n",
  "Undefined Error.\n",
  "Undefined Error.\n",
  "Undefined Error.\n",
  /* DJP_PROTOCOL_ERROR.  */
  "When Comunication with server, the protocol mismatched.\n",
  "Using DJP protocol, the command not implemented on server.\n",
  "Undefined Error.\n",
  "Undefined Error.\n",
  "Undefined Error.\n",
  "Undefined Error.\n",
  "The server memory limited.\n",
  "Ther server operator time out(maybe the target power down or halted).\n",
  "Ther server operator time out(maybe ice/easyjtag halted.\n)",
  "Target is not connected"
       "(maybe power down or physical connectting broken).\n",
  "The register number is not defined.\n",
  "When access memory, the address is not align to operator mode.\n",
  "When access memory, the protocol orgnize error.\n",
  "The memory breakpoint setting error.\n",
  "Try to delete one nonexistent memory breakpoint.\n",
  "When setting a memory breakpoint, the address not match the mask.\n",
  "The server or target not support profiling.\n",
  "When accessing cpu register limited.\n",
  "When accessing memory limited.\n",
  "The djp command packet format error.\n",
  "When execute one djp command error.\n",
};

/* Do with djp error and output relative messages.  */

static int
djp_do_error (S32 status)
{
  /* Output messages.  */
  switch (status)
    {
    case DJP_ERR_NONE:           /*  0: success.  */
    case DJP_PROTOCOL_ERROR:     /* -6: command actual length is error.  */
    case DJP_COMMAND_NOT_IMPLEMENTED: /* -7: command is not implemented.  */
    case DJP_OUT_OF_MEMORY:      /* -12: fail in mallocing memory.  */
    case DJP_OPERATION_TIME_OUT: /* -13: time out when entering debug mode.  */
    case DJP_COMMUNICATION_TIME_OUT: /* -14: communication(ICE) time out.  */
    case DJP_TARGET_NOT_CONNECTED: /* -15: no connect with target board.  */
    case DJP_REG_NOT_DEFINED:    /* -16: register ID is not defined.  */
    case DJP_ADDRESS_MISALIGNED: /* -17: address is not matched with mode.  */
    case DJP_DATA_LENGTH_INCONSISTENT: /* -18: data length error.  */
    case DJP_INVALID_BKPT:       /* -19: third BKPT is not allowed.  */
    case DJP_DEL_NONEXISTENT_BKPT: /* -20: no BKPT in specified address.  */
    case DJP_MASK_MISALIGNED:    /* -21: address isn't cosistent with mask.  */
    case DJP_NO_PROFILING:       /* -22: no profilling function in target.  */
    case DJP_CPU_ACCESS:         /* -23: Reserved (CPU access right).  */
    case DJP_ADDRESS_ACCESS:     /* -24: Reserved (address access right).  */
    case DJP_COMMAND_FORMAT:     /* -25: Command format error.  */
    case DJP_COMMAND_EXECUTE:    /* -26: Command is not finished.  */
      seterrorinfo (strdjperrorinfo[-status]);
      break;
    default:
      seterrorinfo ("Undefined Error.\n");
    }

  return status;
}

static int
djp_open (const char* args)
{
  /* Find the socket port number.  */
  char hostname[256];
  char defaultport[8];
  char *port = strchr (args, ':');
  int  version[5]  = {0};

  DJP_DEBUG_PRINTF (("djp_open: \"%s\".\n", args));
  if (port)
    {
      int len = port - args;
      strncpy (hostname, args, len);
      hostname[len] = '\0';
      port++;
    }
  else /* Use the default socket port.  */
    {
      strcpy (defaultport, "1025");
      printf_unfiltered ("Default socket port: 1025\n");
      port = defaultport;
      strcpy (hostname, args);
    }

  if (desc_fd > 0)
    {
      CLOSESOCKET (desc_fd);
    }

  if ((desc_fd= socket_connect_to_server (hostname, port)) <= 0)
    {
      /* Failure, the error info filled in sub function.  */
      return -1;
    }

  if (prereset_flag)
    {
      if (djp_reset () != 0)
        return -1;
    }

  if (djp_enter_debug () != 0)
    {
      return -1;
    }

  /* Check the version for djp.  */
  if (djp_server_version (version) < 0)
    {
      return -1;
    }

  if (version[0] == 0)
    {
      printf_unfiltered ("Server version is too old, please update it.\n");
      return -1;
    }

  printf_unfiltered ("Using remote CSKY debugger server on %s\n", hostname);

  return 0;
}

static int
djp_close ()
{
  DJP_DEBUG_PRINTF (("djp_close.\n"));
  if (desc_fd> 0)
    {
      CLOSESOCKET (desc_fd);
    }

  return 0;
}

static int
djp_hw_bkpt_num (unsigned int *val1, unsigned int *val2)
{
  int result;
  HwBkptNumMsg msg;
  HwBkptNumRsp rsp;
  msg.command = htonl (DBGCMD_HW_BKPT_NUM);
  msg.length = htonl (sizeof (msg) - 8);
  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }
  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }
  *val1 = ntohl (rsp.data1);
  *val2 = ntohl (rsp.data2);
  return result;
}

/* ----------------- For operating HAD reg ---------------  */
static int
djp_read_had_reg (int regno, unsigned int* val)
{
  ReadHadRegMsg msg;
  ReadHadRegRsp rsp;
  int result;

  msg.command = htonl (DBGCMD_HAD_REG_READ);
  msg.length = htonl (sizeof (msg) - 8);
  msg.address = htonl (regno);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }
  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }
  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }
  *val = ntohl (rsp.data);
  return result;
}

static int
djp_write_had_reg (int regno, unsigned int val)
{
  WriteHadRegMsg msg;
  WriteHadRegRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_HAD_REG_WRITE, regno = %d, regval = 0x%x.\n",
                     regno, val));
  msg.command = htonl (DBGCMD_HAD_REG_WRITE);
  msg.length = htonl (sizeof (msg) - 8);

  msg.address = htonl (regno);
  msg.data = htonl (val);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

   /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

/* -------------------- End for operating HAD reg ---------------  */

static int
djp_read_reg (int regno, CORE_REG* val)
{
  ReadRegMsg msg;
  ReadRegRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_REG_READ, regno = %d.", regno));

  msg.command = htonl (DBGCMD_REG_READ);
  msg.length = htonl (sizeof (msg) - 8);

  msg.address = htonl (regno);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  *val = ntohl (rsp.data_L);
  DJP_DEBUG_PRINTF (("reg val = 0x%x.\n", *val));
  return result;
}

static int
djp_write_reg (int regno, CORE_REG val)
{
  WriteRegMsg msg;
  WriteRegRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_REG_WRITE, regno = %d, regval = 0x%x.\n",
                    regno, val));

  msg.command = htonl (DBGCMD_REG_WRITE);
  msg.length = htonl (sizeof (msg) - 8);

  msg.address = htonl (regno);
  msg.data_L = htonl (val);
  msg.data_H = htonl (0);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

/* For djp_*_xreg
   fix endian info(the same as store_unsigned_integer() defined in findvar.c)
   function name: csky_adjust_byteorder_xreg()
   parameter    :
                gdb_byte *addr,
                int len,
                enum bfd_endian byte_order,
                gdb_byte *val.  */

static void
csky_adjust_byteorder_xreg (gdb_byte *addr, int len,
                            enum bfd_endian byte_order, gdb_byte *val)
{
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *) addr;
  unsigned char *endaddr = startaddr + len;

  /* Start at the least significant end of the integer, and work towards
     the most significant.  */
  if (byte_order == BFD_ENDIAN_BIG)
    {
      for (p = endaddr - 1; p >= startaddr; --p)
        {
          *p = *val;
          *val ++ ;
        }
    }
  else
    {
      for (p = startaddr; p < endaddr; ++p)
        {
          *p = *val;
          *val ++;
        }
    }

}

static int
djp_read_xreg (struct regcache *regcache, int regno)
{
  if (CK_NewDJP_support)
    {
      /* Server support this djp.  */
      ReadXRegMsg msg;
      ReadXRegRsp rsp;
      int result;

      U8 buf[CSKY_MAX_REGISTER_SIZE];
      U8 val[CSKY_MAX_REGISTER_SIZE];
      int server_regnr = csky_register_convert (regno, regcache);
      int reg_size = register_size (get_regcache_arch (regcache), regno);
      struct gdbarch *gdbarch = get_regcache_arch (regcache);
      enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

      DJP_DEBUG_PRINTF (("DBGCMD_XREG_READ, regno = %d.", server_regnr));

      msg.command = htonl (DBGCMD_XREG_READ);
      msg.length = htonl (sizeof (msg) - 8);
      msg.address = htonl (server_regnr);
      msg.size = htonl (reg_size);

      result = socket_send (desc_fd, &msg, sizeof (msg));
      if (result < 0)
        {
          DJP_DEBUG_PRINTF ((" socket send error.\n"));
          seterrorinfo ("proxy server broken, please check your server.\n");
          inferior_ptid = null_ptid;
          return result;
        }

      /* Receive the header of response.  */
      result = socket_receive (desc_fd, &rsp, sizeof (rsp));
      if (result < 0)
        {
          DJP_DEBUG_PRINTF ((" socket receive error.\n"));
          seterrorinfo ("proxy server broken, please check your server.\n");
          inferior_ptid = null_ptid;
          return result;
        }

      if (rsp.status)
        {
          DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
          return djp_do_error (ntohl (rsp.status));
        }

      /* Receive data FIXME endian problem ?  */
      result = socket_receive (desc_fd, val, reg_size);
      if (result < 0)
        {
          DJP_DEBUG_PRINTF ((" socket receive error.\n"));
          seterrorinfo ("proxy server broken, please check your server.\n");
          return result;
        }

      /* Fill regcache according to buf FIXME endian problem!  */
      csky_adjust_byteorder_xreg (buf, register_size (gdbarch, regno),
                                  byte_order, val);
      regcache_raw_supply (regcache, regno, buf);
      return result;
    }
  else
    {
      /* Server do not support this djp.  */
      warning ("CSKY Server ProxyLayer version is low,"
               " 1.4 or above is needed.\n");
      /* Supply regcache with zero, see regcache_raw_supply in regcache.c.  */
      regcache_raw_supply (regcache, regno, NULL);
      return 0;
    }
}

static int
djp_write_xreg (struct regcache *regcache, int regno)
{
  if (CK_NewDJP_support)
    {
      /* Server support this djp.  */
      WriteXRegMsg msg;
      WriteXRegRsp rsp;
      int result;
      struct cleanup *old_cleanups;
      U8 *buffer;
      U8 data[CSKY_MAX_REGISTER_SIZE];
      U8 val[CSKY_MAX_REGISTER_SIZE];
      struct gdbarch *gdbarch = get_regcache_arch (regcache);
      enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

      int server_regnr = csky_register_convert (regno, regcache);
      int reg_size = register_size(get_regcache_arch (regcache), regno);

      regcache_raw_collect (regcache, regno, val);
      csky_adjust_byteorder_xreg (data, reg_size, byte_order, val);

      DJP_DEBUG_PRINTF (("DBGCMD_XREG_WRITE, regno = %d.", server_regnr));

      msg.command = htonl (DBGCMD_XREG_WRITE);
      msg.length = htonl (sizeof (msg) - 8 + reg_size);
      msg.address = htonl (server_regnr);
      msg.size = htonl (reg_size);

      buffer = (U8 *) xmalloc (sizeof (msg) + reg_size);
      old_cleanups = make_cleanup (xfree, buffer);
      memcpy (buffer, &msg, sizeof (msg));
      memcpy (buffer + sizeof (msg), data, reg_size);

      result = socket_send (desc_fd, buffer, sizeof (msg) + reg_size);
      /* Free buffer.  */
      do_cleanups (old_cleanups);

      if (result < 0)
        {
          DJP_DEBUG_PRINTF ((" socket send error.\n"));
          seterrorinfo ("proxy server broken, please check your server.\n");
          inferior_ptid = null_ptid;
          return result;
        }

      result = socket_receive (desc_fd, &rsp, sizeof (rsp));
      if (result < 0)
        {
          DJP_DEBUG_PRINTF ((" socket receive error.\n"));
          seterrorinfo ("proxy server broken, please check your server.\n");
          inferior_ptid = null_ptid;
          return result;
        }

      /* Do with rsp, status.  */
      if (rsp.status)
        {
          DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
          return djp_do_error (ntohl (rsp.status));
        }

      return result;
    }
  else
    {
      /* Server do not support this djp.  */
      warning ("CSKY Server ProxyLayer version is low,"
               " 1.4 or above is needed.\n");
      return 0;
    }
}

static int
djp_read_mem (CORE_ADDR addr, void* data, int nSize, int nCount)
{
  ReadMemMsg msg;
  ReadMemRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_MEM_READ, addr = 0x%x, nSize = %d,"
                    " nCount = %d.\n", (U32)addr, nSize, nCount));

  msg.command = htonl (DBGCMD_MEM_READ);
  msg.length = htonl (sizeof (msg) - 8);
  msg.address = htonl (addr);
  msg.nLength = htonl (nCount);
  msg.nSize = htonl (nSize);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Receive the header of response, the length of data[0] is 0,
     same as write.  */
  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Receive data, has endian problem??? if with, fix me.  */
  result = socket_receive (desc_fd, data, nSize * nCount);
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_write_mem (CORE_ADDR addr, void* data, int nSize, int nCount)
{
  WriteMemMsg msg;
  WriteMemRsp rsp;
  int result;
  struct cleanup *old_cleanups;

  U8 *buffer;

  DJP_DEBUG_PRINTF(("DBGCMD_MEM_WRITE, addr = 0x%x, nSize = %d,"
                    " nCount = %d.\n", (U32)addr, nSize, nCount));

  msg.command = htonl (DBGCMD_MEM_WRITE);
  msg.length = htonl (sizeof (msg) - 8 + nSize * nCount);
  msg.address = htonl (addr);
  msg.nLength = htonl (nCount);
  msg.nSize = htonl (nSize);

  buffer = (U8 *) xmalloc (sizeof (msg) + nSize * nCount);
  old_cleanups = make_cleanup (xfree, buffer);
  memcpy (buffer, &msg, sizeof (msg));
  memcpy (buffer + sizeof (msg), data, nSize * nCount);

  result = socket_send (desc_fd, buffer, sizeof (msg) + nSize * nCount);
  /* Free(buffer);  */
  do_cleanups (old_cleanups);
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_do_hw_breakpoint (CORE_ADDR addr, int set)
{
  HwBkptMsg msg;
  HwBkptRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_%s_HW_BREAKPOINT, addr = 0x%x.",
                    set? "INSERT" : "REMOVE", (U32)addr));

  msg.command = htonl (set ? DBGCMD_INSERT_HW_BREAKPOINT
                       : DBGCMD_REMOVE_HW_BREAKPOINT);
  msg.length = htonl (sizeof (msg) - 8);
  msg.address = htonl (addr);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_insert_hw_breakpoint (CORE_ADDR addr)
{
  return djp_do_hw_breakpoint (addr, 1);
}

static int
djp_remove_hw_breakpoint (CORE_ADDR addr)
{
  return djp_do_hw_breakpoint (addr, 0);
}

static int
djp_do_watchpoint (CORE_ADDR addr, int len, int type,
                   int counter, int set)
{
  WatchpointMsg msg;
  WatchpointRsp rsp;
  int result;

  unsigned long mask;

  DJP_DEBUG_PRINTF (("DBGCMD_%s_WATCHPOINT, addr = 0x%x, len = %d, type = %d",
                    set ? "INSERT" : "REMOVE", (U32)addr, len, type));

  msg.command = htonl (set ? DBGCMD_INSERT_WATCHPOINT
                       : DBGCMD_REMOVE_WATCHPOINT );

  mask = ~(len - 1);
  msg.length = htonl (sizeof (msg) - 8);
  msg.address = htonl (addr & mask);
  msg.mask = htonl (mask);

  switch (type)
    {
    case hw_write:
      type = HW_WRITE;   /* All user write memory.  */
      break;
    case hw_read:
      type = HW_READ;    /* All user read memory.  */
      break;
    case hw_access:
      type = HW_ACCESS;  /* All user r/w memory.  */
      break;
    case hw_execute:
      type = HW_EXECUTE; /* All user fetch instruction.  */
      break;
    default:
      type = HW_NONE;    /* No hit.  */
    }
  msg.mode = htonl (type);

  msg.counter = htonl (counter);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_insert_watchpoint (CORE_ADDR addr, int len, int type, int counter)
{
  return djp_do_watchpoint (addr, len, type, counter, 1);
}

static int
djp_remove_watchpoint (CORE_ADDR addr, int len, int type, int counter)
{
  return djp_do_watchpoint (addr, len, type, counter, 0);
}

static int
djp_reset ()
{
  SystemResetMsg msg;
  SystemResetRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_SYSTEM_RESET.\n"));

  msg.command = htonl (DBGCMD_SYSTEM_RESET);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_soft_reset (int insn)
{
  SystemSoftResetMsg msg;
  SystemSoftResetRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_SYSTEM_SOFT_RESET.\n"));

  msg.command = htonl (DBGCMD_SYSTEM_SOFT_RESET);
  msg.length = htonl (sizeof (msg) - 8);
  msg.insn = htonl (insn);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_enter_debug ()
{
  EnterDebugMsg msg;
  EnterDebugRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_ENTER_DEBUG.\n"));

  msg.command = htonl (DBGCMD_ENTER_DEBUG);
  msg.length = htonl (sizeof (msg) - 8);
  msg.chain = htonl (SC_RISC_DEBUG);  /* Ignored.  */

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      if ((ntohl (rsp.status)) == DJP_NON_DEBUG_REGION)
        {
          result = JTAG_NON_DEBUG_REGION; /* CPU in debug region.  */
        }
      else
        {
          DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
          return djp_do_error (ntohl (rsp.status));
        }
    }

  return result;
}

static int
djp_exit_debug ()
{
  CmdRunMsg msg;
  CmdRunRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_RUN.\n"));

  msg.command = htonl (DBGCMD_RUN);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_singlestep ()
{
  SingleStepMsg msg;
  SingleStepRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_SINGLESTEP.\n"));

  msg.command = htonl (DBGCMD_SINGLESTEP);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return 0;
}

static int
djp_check_debugmode (int* status)
{
  CheckDebugMsg msg;
  CheckDebugRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_CHECK_DEBUG.\n"));

  msg.command = htonl (DBGCMD_CHECK_DEBUG);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  *status = ntohl (rsp.debugmode);
  return result;
}

static int
djp_set_profiler (void *args)
{
  SetProfilingMsg msg;
  SetProfilingRsp rsp;
  int result;

  int arglen = strlen ((const char *)args);

  DJP_DEBUG_PRINTF (("DBGCMD_SET_PROFILING_INFO.\n"));

  msg.command = htonl (DBGCMD_SET_PROFILING_INFO);
  msg.length = htonl (sizeof (msg) - 8 + arglen);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_send (desc_fd, args, arglen);
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  return result;
}

static int
djp_get_profiler (void **data, int* length)
{
  GetProfilingMsg msg;
  GetProfilingRsp rsp;
  int result;
  char flags[8];
  struct cleanup *old_cleanups;

  DJP_DEBUG_PRINTF (("DBGCMD_GET_PROFILING_INFO.\n"));

  msg.command = htonl (DBGCMD_GET_PROFILING_INFO);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  *length = htonl (rsp.length);
  /* Receive the profiling data, howto???  FIXME?  */
  /* The proxy forget to set the length.  */
  if (*length == 0 || *length == 0xcccccccc)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("the debugger server version is older,"
                    " please update to newest one.\n");
      return -1;  /* Version too old  */
    }

  *data = xmalloc (*length);
  old_cleanups = make_cleanup (xfree, *data);
  if (*data)
    {
      result = socket_receive (desc_fd, *data, *length);
    }

  do_cleanups (old_cleanups);
  return result;
}

static int
djp_trace_pc (U32 *pclist, int *depth)
{
  PcJumpMsg msg;
  PcJumpRsp rsp;
  int result;
  int i;

  DJP_DEBUG_PRINTF (("DBGCMD_TRACE_PC: "));

  msg.command = htonl (DBGCMD_TRACE_PC);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }


  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  i = 0;
  while (i < PCFIFO_DEPTH)
    {
      pclist[i] = rsp.pc[PCFIFO_DEPTH - i - 1]; /* Set the newest pc first.  */
      i++;
    }
  *depth = PCFIFO_DEPTH;

#ifdef DJP_DEBUG_PRINTF
  {
    int i = 0;
    while (i < PCFIFO_DEPTH)
      {
        DJP_DEBUG_PRINTF (("  PC[%d] = 0x%x.\n", i, (U32)pclist[i]));
        i++;
      }
  }
#endif
  return result;
}

static int
djp_endianinfo (int *endian)
{
  EndianMsg msg;
  EndianRsp rsp;
  int result;

  DJP_DEBUG_PRINTF (("DBGCMD_ENDIAN_INFO.\n"));

  msg.command = htonl (DBGCMD_ENDIAN_INFO);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  *endian = ntohl (rsp.endian);

  return result;
}

static int
djp_server_version (int *version)
{
  VersionMsg msg;
  VersionRsp rsp;
  int result;
  int i;

  DJP_DEBUG_PRINTF (("DBGCMD_VERSION_INFO.\n"));

  msg.command = htonl (DBGCMD_VERSION_INFO);
  msg.length = htonl (sizeof (msg) - 8);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  result = socket_receive (desc_fd, &rsp.status, sizeof (rsp.status));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  result = socket_receive (desc_fd, ((char*)&rsp + sizeof (rsp.status)),
                           sizeof (rsp) - sizeof (rsp.status));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return result;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      return djp_do_error (ntohl (rsp.status));
    }

  version[0] = ntohl (rsp.srv_version);
  i = 0;
  while (i < 4)
    {
      version[i + 1] = ntohl (rsp.had_version[i]);
      i ++;
    }
  return result;
}

static enum target_xfer_status
djp_read_xmltdesc (ULONGEST offset, void *data,
                   ULONGEST len, ULONGEST *xfered_len)
{
  ReadXmlTdescMsg msg;
  ReadXmlTdescRsp rsp;
  int result;
  int rlen;

  DJP_DEBUG_PRINTF (("DBGCMD_XML_TDESC_READ, offset = 0x%x, len = %d.\n",
                     offset, len));

  msg.command = htonl (DBGCMD_XML_TDESC_READ);
  msg.length = htonl (sizeof (msg)-8);
  msg.address = htonl (offset);
  msg.nLength = htonl (len);

  result = socket_send (desc_fd, &msg, sizeof (msg));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket send error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return TARGET_XFER_E_IO;
    }

  /* Receive the header of response, the length of data[0] is 0.  */
  result = socket_receive (desc_fd, &rsp, sizeof (rsp));
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return TARGET_XFER_E_IO;
    }
  /* If all xml-tdesc.xml contents gotten, return 0.  */
  *xfered_len = ntohl (rsp.rlen);
  if (*xfered_len == 0)
    return TARGET_XFER_EOF;

  result = socket_receive (desc_fd, data, *xfered_len);
  if (result < 0)
    {
      DJP_DEBUG_PRINTF ((" socket receive error.\n"));
      seterrorinfo ("proxy server broken, please check your server.\n");
      inferior_ptid = null_ptid;
      return TARGET_XFER_E_IO;
    }

  /* Do with rsp.status.  */
  if (rsp.status)
    {
      DJP_DEBUG_PRINTF ((" djp status of response error.\n"));
      djp_do_error (ntohl (rsp.status));
      return TARGET_XFER_E_IO;
    }

  return TARGET_XFER_OK;
}

/* ----------------------------------------------
            CSKY Hardware ops
   ----------------------------------------------  */

static int
hardware_open (const char* args)
{
  char **argv;

  if (args == NULL || strlen (args) == 0)
    {
      seterrorinfo ("To open a CSKY remote debugging connection, you need to "
                    "specify a remote server which will proxy these services "
                    "for you.\nExample: djp://debughost.mydomain.com:8100.\n");
      return -1;
    }

  if (strlen (args) < sizeof (DJP_HEADER))
    {
      char sTemp[128];
      sprintf (sTemp, "%s is not a valid args for csky remote target.\n", args);
      seterrorinfo (sTemp);
    }

  /* Parse the port name.  */
  /* Djp protocol level.  */
  if (strncmp (args, DJP_HEADER, (sizeof (DJP_HEADER) - 1)) == 0)
    {
      current_hardware_ops = &djp_ops;
      return current_hardware_ops->to_open (&args[strlen (DJP_HEADER)]);
    }

  if (strncmp (args, JTAG_HEADER, (sizeof (JTAG_HEADER) - 1)) == 0)
    {
      current_hardware_ops = &djp_ops;
      return current_hardware_ops->to_open (&args[strlen (JTAG_HEADER)]);
    }

  seterrorinfo ("We don\'t support this protocol for csky remote debug.\n");
  return -1;
}

static int
hardware_close (void)
{
  int result;
  if (!current_hardware_ops)
    {
      /* seterrorinfo("internal error: without hardware ops for"
                      " target ops.");  */
      return 0;
    }

  result = current_hardware_ops->to_close ();
  current_hardware_ops = NULL;
  return result;
}

static int
hardware_hw_bkpt_num (unsigned int *val1, unsigned int *val2)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }
  return current_hardware_ops->to_hw_bkpt_num (val1, val2);
}

/*  --------------- For read HAD reg--------------------------  */
static int
hardware_read_had_reg (int regno, unsigned int *val)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }
  return current_hardware_ops->to_read_had_reg (regno, val);
}

static int
hardware_write_had_reg (int regno, unsigned int val)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }
  return current_hardware_ops->to_write_had_reg (regno, val);
}
/* ---------------- End read HAD reg ------------------------  */

static int 
hardware_read_reg (int regno, CORE_REG* val)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }
  return current_hardware_ops->to_read_reg (regno, val);
}

static int
hardware_write_reg (int regno, CORE_REG val)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_write_reg (regno, val);
}

static int
hardware_read_xreg (struct regcache *regcache, int regno)
{
   /* When proxy_ver<1.4,xreg read/write not support.  */
  if ((proxy_main_ver > 1) || ((proxy_main_ver == 1) && proxy_sub_ver >= 4))
    {
      CK_NewDJP_support = 1;
    }
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_read_xreg (regcache, regno);
}

static int
hardware_write_xreg (struct regcache *regcache, int regno)
{
  if ((proxy_main_ver > 1) || ((proxy_main_ver == 1) && proxy_sub_ver >= 4))
    {
      CK_NewDJP_support = 1;
    }

  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_write_xreg (regcache, regno);
}


static int
hardware_read_mem (CORE_ADDR addr, void* data, int nSize, int nCount)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_read_mem (addr, data, nSize, nCount);
}

static int
hardware_write_mem (CORE_ADDR addr, void* data, int nSize, int nCount)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_write_mem (addr, data, nSize, nCount);
}

static int
hardware_insert_hw_breakpoint (CORE_ADDR addr)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_insert_hw_breakpoint (addr);
}

static int
hardware_remove_hw_breakpoint (CORE_ADDR addr)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_remove_hw_breakpoint (addr);
}

static int
hardware_insert_watchpoint (CORE_ADDR addr, int len, int type,
                            int counter)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_insert_watchpoint (addr, len, type,
                                                     counter);
}
static int
hardware_remove_watchpoint (CORE_ADDR addr, int len, int type, int counter)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_remove_watchpoint (addr, len, type,
                                                     counter);
}

static int
hardware_enter_debugmode ()
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_enter_debugmode ();
}

static int
hardware_check_debugmode (int* status)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_check_debugmode (status);
}

static int
hardware_reset ()
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_reset ();
}

static int
hardware_soft_reset (int insn)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_soft_reset (insn);
}

static int
hardware_singlestep ()
{
  if (!current_hardware_ops)
  {
    seterrorinfo ("internal error: without hardware ops for target ops.");
    return -1;
  }

  return current_hardware_ops->to_singlestep ();
}

static int
hardware_exit_debugmode ()
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_exit_debug ();
}

static int
hardware_set_profiler (void *args)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_set_profiler (args);
}

static int
hardware_get_profiler (void **data, int *length)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_get_profiler (data, length);
}

static int
hardware_trace_pc (U32* pclist, int *depth)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_trace_pc (pclist, depth);
}

static int
hardware_endianinfo (int *endian)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_endianinfo (endian);
}

static int
hardware_server_version (int *version)
{
  if (!current_hardware_ops)
    {
      seterrorinfo ("internal error: without hardware ops for target ops.");
      return -1;
    }

  return current_hardware_ops->to_server_version (version);
}

static enum target_xfer_status
hardware_read_xmltdesc (ULONGEST offset, void *data,
                        ULONGEST len, ULONGEST *xfered_len)
{
  if ((proxy_main_ver > 1) || ((proxy_main_ver == 1) && proxy_sub_ver >= 8))
    {
      if (!current_hardware_ops)
        {
          seterrorinfo ("internal error: without hardware ops for target ops.");
          return TARGET_XFER_E_IO;
        }
      return current_hardware_ops->to_read_xmltdesc (offset, data,
                                                     len, xfered_len);
    }
  return TARGET_XFER_E_IO;
}

/* --------------------------------------------------
  Abstract level for kernel operations
  An array to include all the kernal ops.
  An new kernel_ops should be registered here.
 ----------------------------------------------------  */

struct kernel_ops* kernel_ops_array[] =
{
  /* The first member in the array would be NULL for the convenience of
   function implementation in file: remote-csky.c.  */
  NULL,
  &eCos_kernel_ops
  /*
  &uCOS_kernel_ops
  &uClinux_kernel_ops,
  */
};

/* An array to include all the check function.
   To check which kernel_ops in kernel_ops_array[] is
   current_kernel_ops.  */

int (*check_kernel_ops_fun_array[])(void) =
{
  NULL,
  is_eCos_kernel_ops,
  /*
  is_uCOS_kernel_ops,
  is_uClinux_kernel_ops,
  */
  NULL /* Must end with NULL.  */
};

static void kernel_init_thread_info (int intensity);
static int  kernel_update_thread_info (ptid_t *inferior_ptid);
static void kernel_fetch_registers (ptid_t ptid, int regno, unsigned int *val);
static int  kernel_thread_alive (ptid_t ptid);
static void kernel_pid_to_str (ptid_t ptid, char *buf);
static void kernel_command_implement (char* args, int from_tty);
static int  csky_choose_kernel_ops (enum kernel_ops_sel sel);
static void csky_info_mthreads_command (char* args, int from_tty);
static void csky_set_mthreads_mode_command (char* args, int from_tty,
                                            struct cmd_list_element *c);

static void
kernel_init_thread_info (int intensity)
{
  if (current_kernel_ops)
    {
      return current_kernel_ops->to_init_thread_info (intensity);
    }
  return;
}

static int
kernel_update_thread_info (ptid_t *inferior_ptid)
{
  if (current_kernel_ops)
    {
      return current_kernel_ops->to_update_thread_info (inferior_ptid);
    }
  return NO_KERNEL_OPS;
}

static void
kernel_fetch_registers (ptid_t ptid, int regno, unsigned int *val)
{
  if (current_kernel_ops)
    {
      return current_kernel_ops->to_fetch_registers (ptid, regno, val);
    }
  return;
}

static int
kernel_thread_alive (ptid_t ptid)
{
  if (current_kernel_ops)
    {
      return current_kernel_ops->to_thread_alive (ptid);
    }
  return 0;
}

static void
kernel_pid_to_str (ptid_t ptid, char* buf)
{
  if (current_kernel_ops)
    {
      return current_kernel_ops->to_pid_to_str (ptid, buf);
    }
  return ;
}

static void
kernel_command_implement (char* args, int from_tty)
{
  if (current_kernel_ops)
    {
      return current_kernel_ops->to_command_implement (args, from_tty);
    }
  return;
}

/* Choose a kernel_ops, defined in file: csky-kernel.h
   Function to check in check_kernel_ops_fun_array[i] 
                             is implemented by user-defined.
   SEL : DEFAULT -->  choose kernel_ops automatically.
        ECOS    -->  choose eCos_kernel_ops manually.
   return : 1 -> success
            0 -> failure.  */

static int
csky_choose_kernel_ops (enum kernel_ops_sel sel)
{
  if (sel == DEFAULT)
    {
      int i;
      for (i = 1; check_kernel_ops_fun_array[i]; i++)
        {
          if ((check_kernel_ops_fun_array[i]) ())
            {
              /* Kernel_ops have been choose automatically.  */
              current_kernel_ops = kernel_ops_array[i];
              break;
            }
        }
      return 1;
    }
  else if (sel == ECOS)
    {
      /* kernel_ops_array[ECOS] == eCos_kernel_ops.  */
      if (check_kernel_ops_fun_array[ECOS] ())
        {
          current_kernel_ops = kernel_ops_array[ECOS];
          return 1;
        }
    }

  return 0;
}

/* Csky multi-threads commands.
   If no current_kernel_ops, this commands will do nothing
   except warning(). This is only an interface function which
   is implemented by current_kernel_ops.
   ARGS: parameter of commands
   FROM_TTY: 1: command from CLI
             0: command from MI  */

static void
csky_info_mthreads_command (char* args, int from_tty)
{
  if (current_kernel_ops)
    {
      current_kernel_ops->to_command_implement (args, from_tty);
      return;
    }
  else if (from_tty)
    {
      printf_filtered ("\"info mthreads\" is a multi-threads' command,"
                       " and not support in single thread debugging.\nTry"
                       " \"help info mthreads\" for more information.\n");
      return;
    }
  else /* Info thread command from MI command.  */
    {
      struct cleanup *cleanup_error;
      cleanup_error =
            make_cleanup_ui_out_tuple_begin_end (current_uiout, "mthreadError");
      ui_out_field_string (current_uiout, "error", "1");
      do_cleanups (cleanup_error);
      return;
    }
}


/* ---------------------------
     Target operations level
   ---------------------------  */

static struct target_ops* current_ops = NULL;
static struct target_ops csky_ops;
static struct target_ops csky_ecos_ops;


static void csky_open  (const char *name, int from_tty);
static void csky_ecos_open (const char *name, int from_tty);
static void csky_close (struct target_ops *ops);
static void csky_ecos_attach (struct target_ops *ops,
                              const char * args, int from_tty);
static void csky_detach (struct target_ops *ops,
                         const char *args, int from_tty);
static void csky_resume (struct target_ops *ops,
                         ptid_t pid, int step, enum gdb_signal siggnal);
static ptid_t csky_wait (struct target_ops *ops, ptid_t ptid,
                         struct target_waitstatus *status, int options);
static void csky_fetch_registers (struct target_ops *ops,
                                  struct regcache *regcache, int regno);
/* -------------------for read had reg-------------------------------  */
static void csky_fetch_had_registers (HADREG regno, unsigned int *val);
static void csky_store_had_registers (HADREG regno, unsigned int val);
/* ------------------------------------------------------------------  */
static void csky_store_registers (struct target_ops *ops,
                                  struct regcache *regcache, int regno);
static void csky_prepare_to_store (struct target_ops *ops,
                                   struct regcache *regcache);
static int  csky_xfer_memory (CORE_ADDR memaddr,
                              gdb_byte *myaddr,
                              int len,
                              int write,
                              struct target_ops *ignore);
static void csky_files_info  (struct target_ops *ignore);
static int  csky_insert_breakpoint (struct target_ops *ops,
                                    struct gdbarch *gdbarch,
                                    struct bp_target_info *bp_tgt);
static int  csky_remove_breakpoint (struct target_ops *ops,
                                    struct gdbarch *gdbarch,
                                    struct bp_target_info *bp_tgt,
                                    enum remove_bp_reason reason);
static int  csky_insert_watchpoint (struct target_ops *self,
                                    CORE_ADDR addr, int len,
                                    enum target_hw_bp_type type,
                                    struct expression *cond);
static int  csky_remove_watchpoint (struct target_ops *self,
                                    CORE_ADDR addr, int len,
                                    enum target_hw_bp_type type,
                                    struct expression *cond);
static void csky_kill (struct target_ops *ops);
static void csky_load (struct target_ops *self, const char *args, int from_tty);
static void csky_create_inferior (struct target_ops *ops, char *execfile,
                                  char *args, char **env, int from_tty);
static int  csky_can_use_hardware_watchpoint (struct target_ops *self,
                                              enum bptype bp_type,
                                              int cnt,
                                              int other_type_used);
static int  csky_stopped_by_watchpoint (struct target_ops *ops);
static int  csky_stopped_data_address (struct target_ops *target,
                                       CORE_ADDR *addr_p);
static char *csky_pid_to_str (struct target_ops *ops, ptid_t ptid);
static int  csky_thread_alive (struct target_ops *ops, ptid_t ptid);
static void csky_mourn_inferior (struct target_ops *ops);
static CORE_ADDR csky_get_current_hw_address ();


/* Defined CSKY target ops  */

/* 1.Prepare target ops for gdb.
   2.Connect target board by hardware_open
   3.Get server version.  */

static void
csky_target_ops_prepare (const char *name, int from_tty)
{
  int result;
  int version[5];

  TARGET_DEBUG_PRINTF (("csky_open: name = %s.\n", name));

  target_preopen (from_tty);

  if (current_ops)
    {
      unpush_target (current_ops);
      current_ops = NULL;
    }
  /* Open hardware_ops module and create the connection to server.  */
  if (hardware_open (name))
    {
      error ("%sconnect to host %s failure.\n", errorinfo (), name);
    }

  /* Printf the version info to user.  */
  result = hardware_server_version (version);
  if (result == 0)
    {
      if (((((version[0] >> 14) & 0x03) == 0x00)
            && ((version[0] >> 8) & 0x3f < 2))
          /* EASYJTAG main version < 2.  */
          || ((((version[0] >> 14) & 0x03) == 0x01)  /* USBICE.  */
              /* Version < 1.3  */
              && ((((version[0] >> 8) & 0x3f) == 1)
                    && ((version[0]) & 0xff) < 3)))
        {
          csky_close (current_ops);
          error ("Target version: Unknown. CKcore Debug Server 3.0 or"
                 " newer version needed.\n");
        }

      printf_unfiltered ("CSKY Target Server ProxyLayer version %d.%d, "
                         "Operator Module %s version %d.%d.\n",
            (version[0] >> 24) & 0xff, (version[0] >> 16) & 0xff,
            ((((version[0] >> 14) & 0x03) == 0x00) ? "EASYJTAG" :
            (((version[0] >> 14) & 0x03) == 0x01) ? "USBICE" : "Simulator"),
            (version[0] >> 8) & 0x3f, (version[0]) & 0xff);
      proxy_main_ver = (version[0] >> 24 & 0xff);/* Get proxy main version.  */
      proxy_sub_ver = (version[0] >> 16 & 0xff); /* Get proxy sub version.  */

      if (((version[0] >> 24 & 0xff) > 1)
          || (((version[0]>> 24 & 0xff) == 1)
              && ((version[0] >> 16 & 0xff) >= 3)))
        {
          /* ProxyLayer version >= 1.4  */
          /* CK_NewDJP_support = 1;  */
        }
      else
        {
          /* DebugServer ProxyLayer version is low, warning user.  */
          csky_close (current_ops);
          error ("CSKY Target Server ProxyLayer version is low,"
                 " ProxyLayer 1.3 or above version is needed.");
          /* CK_NewDJP_support  = 0;  */
        }
    }
  else if (result == DJP_COMMAND_NOT_IMPLEMENTED)
    {
      csky_close (current_ops);
      error ("Rmote Target version unknown. Please check your server version"
             " for no older than V2.1, or use the remote target [jtag jtag://"
             "remote-host:port]. \n");
    }
  else
    {
      csky_close (current_ops);
      error ("Target version: Unknown. CKcore Debug Server 3.0 or newer"
             " version needed.\n");
    }
  pctrace = csky_pctrace;
  hardware_version = version[0];
}

/* When csky_target_ops_prepare, we should init gdb internal info including:
   1. inferior info
   2. thread info
   3. frame info
   4. register value
   at last we should print current frame info.  */

static void
csky_init_gdb_state (void)
{
  struct thread_info *thread;
  init_thread_list ();
  inferior_appeared (current_inferior (), ptid_get_pid (remote_csky_ptid));

  inferior_ptid = remote_csky_ptid;
  add_thread_silent (inferior_ptid);

  reinit_frame_cache ();
  registers_changed ();
  stop_pc = regcache_read_pc (get_current_regcache ());
  print_stack_frame (get_selected_frame (NULL), 0, SRC_AND_LOC, 1);

  thread = inferior_thread ();
  set_last_target_status (inferior_ptid, thread->suspend.waitstatus);

  already_load = 0;
}

static void
csky_get_hw_breakpoint_num (void)
{
  if (((proxy_main_ver == 1) && (proxy_sub_ver < 5))|| proxy_main_ver == 0)
    {
      csky_get_hw_breakpoint_num_old ();
    }
  else if (((proxy_main_ver == 1) && ((proxy_sub_ver == 5)
                                      || (proxy_sub_ver == 6))))
    {
      csky_get_hw_breakpoint_num_mid ();
    }
  else
    {
      csky_get_hw_breakpoint_num_new ();
    }
}

static void
csky_get_hw_breakpoint_num_new (void)
{
  unsigned int hw_bkpt_num = 0;
  unsigned int watchpoint_num = 0;
  if (hardware_hw_bkpt_num (&hw_bkpt_num,&watchpoint_num) < 0)
    {
      error (errorinfo ());
    }
  max_hw_breakpoint_num = hw_bkpt_num;
  max_watchpoint_num = watchpoint_num;
  return;
}

static void
csky_get_hw_breakpoint_num_mid (void)
{
  unsigned int val = 0; /* For bkpt num.  */
  unsigned int hcr_reg, tmp,test_reg;
  test_reg = 0x8c0; /* Write the RCB&BCB of HCR to test if bkpt_B exist.  */
  max_watchpoint_num = CSKY_MAX_WATCHPOINT;
  csky_fetch_had_registers (HID, &val);
  val &= BKPT_NUM_MASK;
  val >>= 12;   /* Get bkpt_num.  */
  if (val == 0) /* Old version had without BKPT_NUM.  */
    {
      /* Write hcr and read back.  */
      csky_fetch_had_registers (HCR, &hcr_reg); /* Save hcr.  */
      csky_store_had_registers (HCR, test_reg);
      csky_fetch_had_registers (HCR, &tmp);
      if (tmp == test_reg) /* Hwbkpt B exist.  */
        {
          max_hw_breakpoint_num = CSKY_MAX_WATCHPOINT;
        }
      else  /* Hwbkpt B doesn't exist , only hwbkpt A.  */
        {
          max_hw_breakpoint_num = CSKY_MAX_HW_BREAKPOINT_WATCHPOINT_803;
        }
      csky_store_had_registers (HCR, hcr_reg); /* Restore hcr.  */
    }
  else
    {
      max_hw_breakpoint_num = val;
    }
  if (max_hw_breakpoint_num <  CSKY_MAX_WATCHPOINT)
    {
      max_watchpoint_num = max_hw_breakpoint_num;
    }
}

static void
csky_get_hw_breakpoint_num_old (void)
{
  int mach = gdbarch_bfd_arch_info (get_current_arch ())->mach;
  if (((mach & CSKY_ARCH_MASK)== CSKY_ARCH_803)
      || ((mach & CSKY_ARCH_MASK)== CSKY_ARCH_802))
    {
      max_hw_breakpoint_num = CSKY_MAX_HW_BREAKPOINT_WATCHPOINT_803;
    }
  else
    {
      max_hw_breakpoint_num = CSKY_MAX_WATCHPOINT;
    }
  max_watchpoint_num = max_hw_breakpoint_num;
}

static void
csky_open (const char *name, int from_tty)
{
  csky_target_ops_prepare (name, from_tty);

  /* Push target into stack, ?? current_ops maybe xxx_ops ???  */
  current_ops = &csky_ops;
  push_target (current_ops);

  /* Delete current_kernel_ops.  */
  current_kernel_ops = NULL;
  csky_init_gdb_state ();
  /* Get the hw_breakpoint_num of target.  */
  csky_get_hw_breakpoint_num ();
}

static void
csky_ecos_open (const char *name, int from_tty)
{
  csky_target_ops_prepare (name, from_tty);

  /* push target into stack, ?? current_ops maybe xxx_ops ???   */
  current_ops = &csky_ecos_ops;
  push_target(current_ops);

  /* Choose kernel ops here.  */
  if (!csky_choose_kernel_ops (ECOS))
    {
      csky_close (NULL);
      error ("Fail to start debugging, please use csky-elf-gdb or"
             " csky-abiv2-elf-gdb.\n");
    }

  /* Initialize current kernel ops no matter whatever.  */
  kernel_init_thread_info (1);

  /* Make sure the state is a initial state after target ecos.  */
  current_kernel_ops->csky_target_ptid = remote_csky_ptid;

  csky_init_gdb_state ();
  csky_get_hw_breakpoint_num();
}

static void
csky_close (struct target_ops *ops)
{
  TARGET_DEBUG_PRINTF(("csky_close.\n"));

  /* Close the hardware ops. */
  hardware_close ();
  generic_mourn_inferior ();

  /* Initial thread_list in multi-threads module totally.  */
  kernel_init_thread_info(1);
  pctrace = NULL;
}

static void
csky_detach (struct target_ops *ops, const char *args, int from_tty)
{
  TARGET_DEBUG_PRINTF (("csky_detach.\n"));
  if (args)
    {
      error ("Argument given to \"detach\" when remotely debugging.");
    }
  pop_all_targets ();
  csky_close (ops);
  if (from_tty)
    {
      printf_unfiltered ("Ending remote jtag debugging.\n");
    }
}

static void
csky_ecos_attach (struct target_ops *ops, const char * args, int from_tty)
{
  int return_flag;
  if (args)
    {
      warning ("csky attach command ignore all prameters.\n");
    }

  inferior_appeared (current_inferior (), ptid_get_pid (remote_csky_ptid));
  return_flag = kernel_update_thread_info (&inferior_ptid);

  if ((return_flag == NO_KERNEL_OPS) ||
      (return_flag == NO_INIT_THREAD))
    {
      /* No current_kernel_ops or no multi-threads temporary.  */
      inferior_ptid = remote_csky_ptid;
      add_thread_silent (inferior_ptid);
    }

  reinit_frame_cache ();
  registers_changed ();
  stop_pc = regcache_read_pc (get_current_regcache ());
}

static void
csky_resume (struct target_ops *ops,
             ptid_t pid, int step,
             enum gdb_signal siggnal)
{
  int result;
  TARGET_DEBUG_PRINTF (("csky_resume: step = %d.\n", step));
  /* Check the cpu status.  */
  if (step) /* For single step.  */
    {
      resume_stepped = 1;
      result = hardware_singlestep ();
    }
  else /* Run or continue.  */
    {
      resume_stepped = 0;
      result = hardware_exit_debugmode ();
    }

  already_load = 0;
  if (result < 0)
    {
      error (errorinfo ());
    }
}

static void
csky_interrupt_query (void)
{
  TARGET_DEBUG_PRINTF (("csky_interrupt_query.\n"));
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n"
              "Give up (and stop debugging it)? "))
    {
      struct gdb_exception exception = exception_none;

      exception.reason = RETURN_QUIT;
      target_mourn_inferior ();
      exception_print (gdb_stdout, exception);
    }

  target_terminal_inferior ();
}

static void
csky_interrupt_twice (int signo)
{
  TARGET_DEBUG_PRINTF (("csky_interrupt_twice"));
  /* Try everything.  */
  set_quit_flag ();
  signal (signo, ofunc);
  csky_interrupt_query ();
  signal (signo, csky_interrupt_twice);
}

static void
csky_interrupt (int signo)
{
  TARGET_DEBUG_PRINTF (("csky_interrupt.\n"));
  /* If this doesn't work, try more severe steps.  */
  signal (signo, csky_interrupt_twice);

  /* If we are stepping we should stop the command, rather than stop
     the processor.  */
  if (resume_stepped)
    {
      set_quit_flag ();
    }
  interrupt_count++;
  if (remote_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "jtag_interrupt called\n");
    }
}

static unsigned int
get_pre_cur_insn (int insn_version, int get_pre)
{
  CORE_ADDR pc;
  enum bfd_endian byte_order;
  unsigned int insn;

  pc = regcache_read_pc (get_current_regcache ());
  byte_order = gdbarch_byte_order (get_current_arch ());
#ifndef CSKYGDB_CONFIG_ABIV2    /* FOR ABIV1 */
  /* Get the pre insn.  */
  if (insn_version == CSKY_INSN_V2P)
    {
      insn = read_memory_unsigned_integer (get_pre ? pc - 4 : pc,
                                           2, byte_order);
      if (insn & 0x8000)  /* V2 32bit.  */
        {
          insn = (insn << 16) | (0xffff & read_memory_unsigned_integer
                         (get_pre ? pc - 2 : pc + 2, 2, byte_order));
        }
      else
        {
          /* Set the 16bit at high position.  */
          insn = read_memory_unsigned_integer (get_pre ? pc - 2 : pc,
                                               2, byte_order) << 16;
          if (insn & 0x80000000)  /* 32bit insn is exception.  */
            {
              /* Not the watchpoint do nothing.  */
              insn = -1;
            }
        }
    }
  else  /* Insn_version == CSKY_INSN_V1.  */
    {
      insn = read_memory_unsigned_integer (get_pre ? pc - 2 : pc,
                                           2, byte_order);
    }
#else  /* CSKYGDB_CONFIG_ABIV2 */
  insn = read_memory_unsigned_integer (get_pre ? pc - 4 : pc,
                                       2, byte_order);
  if ((insn & 0xc000) == 0xc000) /* V2 32bit.  */
    {
      insn = (insn << 16) | (0xffff & read_memory_unsigned_integer
                       (get_pre ? pc - 2 : pc + 2, 2, byte_order));
    }
  else
    {
      /* Set the 16bit at high position.  */
      insn = read_memory_unsigned_integer (get_pre ? pc - 2 : pc,
                                           2, byte_order) << 16;
      if ((insn & 0xc0000000) == 0xc0000000) /* 32bit insn is exception.  */
        {
          /* Not the watchpoint do nothing.  */
          insn = -1;
        }
    }
#endif   /* CSKYGDB_CONFIG_ABIV2 */

  return insn;
}


#ifndef CSKYGDB_CONFIG_ABIV2
static void
csky_get_pre_st_ld_info_v1 (unsigned int insn, int insn_version,
                            int *r_st_ld_len, ULONGEST *r_st_ld_addr,
                            int *r_is_st_ld)
{
  int st_ld_len;
  int is_st_ld;
  ULONGEST st_ld_addr;
  st_ld_len = *r_st_ld_len;
  st_ld_addr = *r_st_ld_addr;
  is_st_ld = *r_is_st_ld;
  if (insn_version == CSKY_INSN_V2P)
    {
      if (!(insn & 0x80000000))
        {
          /* Insn v2p is 16bits.  */
          insn >>= 16;
          if (V2P_16_IS_STM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_16_STM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_16_STM_SIZE (insn) * 4;
              is_st_ld = 1;
            }
          else if (V2P_16_IS_LDM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_16_LDM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_16_LDM_SIZE (insn) * 4;
              is_st_ld = 2;
            }
          else if (V2P_16_IS_ST (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_16_ST_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              /* Size 1/2/4 ~ .b/h/w  */
              st_ld_len = V2P_16_ST_SIZE (insn);
              st_ld_addr += st_ld_len * V2P_16_ST_OFFSET (insn);
              is_st_ld = 1;
            }
          else if (V2P_16_IS_LD (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_16_LD_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              /* Size 1/2/4 ~ .b/h/w  */
              st_ld_len = V2P_16_LD_SIZE (insn);
              st_ld_addr += st_ld_len * V2P_16_LD_OFFSET (insn);
              is_st_ld = 2;
            }
        }
      else
        {
          /* Insn v2p is 32bits.  */
          if (V2P_32_IS_STM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_STM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_32_STM_SIZE (insn) * 4;
              is_st_ld = 1;
            }
          else if (V2P_32_IS_LDM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_LDM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_32_LDM_SIZE (insn) * 4;
              is_st_ld = 2;
            }
          else if (V2P_32_IS_ST (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_ST_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              /* Size 1/2/4 ~ .b/h/w  */
              st_ld_len = V2P_32_ST_SIZE (insn);
              st_ld_addr += st_ld_len * V2P_32_ST_OFFSET (insn);
              is_st_ld = 1;
            }
          else if (V2P_32_IS_STR (insn))
            {
              ULONGEST ry;
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_STR_X_REGNUM (insn),
                                          &st_ld_addr);
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_STR_Y_REGNUM (insn),
              &ry);
              st_ld_addr += ry * V2P_32_STR_OFFSET (insn);
              st_ld_len = V2P_32_STR_SIZE (insn);
              is_st_ld = 1;
            }
          else if (V2P_32_IS_STEX (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_STEX_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = 4;
              st_ld_addr += st_ld_len * V2P_32_STEX_OFFSET (insn);
              is_st_ld = 1;
            }
          else if (V2P_32_IS_LD (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_LD_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              /* Size 1/2/4 ~ .b/h/w  */
              st_ld_len = V2P_32_LD_SIZE (insn);
              st_ld_addr += st_ld_len * V2P_32_LD_OFFSET (insn);
              is_st_ld = 2;
            }
          else if (V2P_32_IS_LDR (insn))
            {
              ULONGEST ry;
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_LDR_X_REGNUM (insn),
                                          &st_ld_addr);
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_LDR_Y_REGNUM (insn),
                                          &ry);
              st_ld_addr += ry * V2P_32_LDR_OFFSET (insn);
              st_ld_len = V2P_32_LDR_SIZE (insn);
              is_st_ld = 2;
            }
          else if (V2P_32_IS_LDEX (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_LDEX_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = 4;
              st_ld_addr += st_ld_len * V2P_32_LDEX_OFFSET (insn);
              is_st_ld = 2;
            }
        }
    }
  else /* CSKY insn v1.  */
    {
      if (V1_IS_STM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_R0_REGNUM,
                                      &st_ld_addr);
          st_ld_len = V1_STM_SIZE (insn) * 4;
          is_st_ld = 1;
        }
      else if (V1_IS_STQ (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V1_STQ_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = 4 * 4;
          is_st_ld = 2;
        }
      else if (V1_IS_LDM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_R0_REGNUM,
                                      &st_ld_addr);
          st_ld_len = V1_LDM_SIZE (insn) * 4;
          is_st_ld = 2;
        }
      else if (V1_IS_LDQ (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V1_LDQ_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = 4 * 4;
          is_st_ld = 2;
        }
      else if (V1_IS_ST (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V1_ST_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V1_ST_SIZE (insn);
          st_ld_addr += st_ld_len * V1_ST_OFFSET (insn);
          is_st_ld = 1;
        }
      else if (V1_IS_LD (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V1_LD_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V1_LD_SIZE (insn);
          st_ld_addr += st_ld_len * V1_LD_OFFSET (insn);
          is_st_ld = 2;
        }
    }
  *r_st_ld_len = st_ld_len;
  *r_st_ld_addr = st_ld_addr;
  *r_is_st_ld = is_st_ld;
}
#else  /* CSKYGDB_CONFIG_ABIV2 */
static void
csky_get_pre_st_ld_info_v2 (unsigned int insn, int insn_version,
                            int *r_st_ld_len, ULONGEST *r_st_ld_addr,
                            int *r_is_st_ld)
{
  int st_ld_len;
  int is_st_ld;
  ULONGEST st_ld_addr;
  st_ld_len = *r_st_ld_len;
  st_ld_addr = *r_st_ld_addr;
  is_st_ld = *r_is_st_ld;

  if (!((insn & 0xc0000000) == 0xc0000000))
    {
      /* Insn v2 is 16bits.  */
      insn >>= 16;
      if (V2_16_IS_PUSH (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM,
                                      &st_ld_addr);
          st_ld_len = 4 * (V2_16_IS_PUSH_R15 (insn) + V2_16_PUSH_LIST1 (insn));
          st_ld_addr -= st_ld_len;
          is_st_ld = 1;
        }
      else if (V2_16_IS_ST (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_16_ST_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_16_ST_SIZE (insn);
          st_ld_addr += V2_16_ST_OFFSET (insn);
          is_st_ld = 1;
        }
      else if (V2_16_IS_STWx0 (insn))
        {
          /* sp_reg_num == 14.  */
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM,
                                      &st_ld_addr);
          /* addr == sp + offset.  */
          st_ld_addr += V2_16_STWx0_OFFSET (insn);
          st_ld_len = 4;
          is_st_ld = 1;
        }
      else if (V2_16_IS_LD (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_16_LD_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_16_LD_SIZE (insn);
          st_ld_addr += V2_16_LD_OFFSET (insn);
          is_st_ld = 2;
        }
      else if(V2_16_IS_LDWx0 (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM,
                                      &st_ld_addr);
          st_ld_len = 4;
          st_ld_addr += V2_16_LDWx0_OFFSET (insn);
          is_st_ld = 2;
        }
    }
  else
    {
      /* Insn v2 is 32bits.  */
      if (V2_32_IS_STM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_STM_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_32_STM_SIZE (insn) * 4;
          is_st_ld = 1;
        }
      else if (V2_32_IS_PUSH (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM, &st_ld_addr);
          st_ld_len = 4 * (V2_32_IS_PUSH_R29 (insn)
                           + V2_32_IS_PUSH_R15 (insn)
                           + V2_32_PUSH_LIST1 (insn)
                           + V2_32_PUSH_LIST2 (insn));
          st_ld_addr -= st_ld_len;
          is_st_ld = 1;
        }
      else if (V2_32_IS_LDM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_LDM_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_32_LDM_SIZE (insn) * 4;
          is_st_ld = 2;
        }
      else if (V2_32_IS_ST (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_ST_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_32_ST_SIZE (insn);
          st_ld_addr += V2_32_ST_OFFSET (insn);
          is_st_ld = 1;
        }
      else if (V2_32_IS_STR (insn))
        {
          ULONGEST ry;
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_STR_X_REGNUM (insn),
                                      &st_ld_addr);
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_STR_Y_REGNUM (insn),
                                      &ry);
          st_ld_addr += ry * V2_32_STR_OFFSET (insn);
          st_ld_len = V2_32_STR_SIZE (insn);
          is_st_ld = 1;
        }
      else if (V2_32_IS_STEX (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_STEX_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = 4;
          st_ld_addr += V2_32_STEX_OFFSET (insn);
          is_st_ld = 1;
        }
      else if (V2_32_IS_LD (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_LD_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_32_LD_SIZE (insn);
          st_ld_addr += V2_32_LD_OFFSET (insn);
          is_st_ld = 2;
        }
      else if (V2_32_IS_LDR (insn))
        {
          ULONGEST ry;
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_LDR_X_REGNUM (insn),
                                      &st_ld_addr);
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_LDR_Y_REGNUM (insn),
                                      &ry);
          st_ld_addr += ry * V2_32_LDR_OFFSET (insn);
          st_ld_len = V2_32_LDR_SIZE (insn);
          is_st_ld = 2;
        }
      else if (V2_32_IS_LDEX (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_LDEX_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = 4;
          st_ld_addr += V2_32_LDEX_OFFSET (insn);
          is_st_ld = 2;
        }
    }
  *r_st_ld_len = st_ld_len;
  *r_st_ld_addr = st_ld_addr;
  *r_is_st_ld = is_st_ld;
}
#endif  /* CSKYGDB_CONFIG_ABIV2 */



#ifndef  CSKYGDB_CONFIG_ABIV2
static void
csky_get_cur_st_ld_info_v1 (unsigned int insn, int insn_version,
                            int *r_st_ld_len, ULONGEST *r_st_ld_addr,
                            int *r_is_st_ld)
{
  int st_ld_len;
  int is_st_ld;
  ULONGEST st_ld_addr;
  st_ld_len = *r_st_ld_len;
  st_ld_addr = *r_st_ld_addr;
  is_st_ld = *r_is_st_ld;
  if (insn_version == CSKY_INSN_V2P)
    {
      if (!(insn & 0x80000000))
        {
          /* Insn v2p is 16bits.  */
          insn >>= 16;
          if (V2P_16_IS_STM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_16_STM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_16_STM_SIZE (insn) * 4;
              is_st_ld = 1;
            }
          else if (V2P_16_IS_LDM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_16_LDM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_16_LDM_SIZE (insn) * 4;
              is_st_ld = 2;
            }
        }
      else
        {
          /* Insn v2p is 32bits.  */
          if (V2P_32_IS_STM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_STM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_32_STM_SIZE (insn) * 4;
              is_st_ld = 1;
            }
          else if (V2P_32_IS_LDM (insn))
            {
              regcache_raw_read_unsigned (get_current_regcache (),
                                          V2P_32_LDM_ADDR_REGNUM (insn),
                                          &st_ld_addr);
              st_ld_len = V2P_32_LDM_SIZE (insn) * 4;
              is_st_ld = 2;
            }
        }
    }
  else /* Insn v1.  */
    {
      if (V1_IS_STM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_R0_REGNUM,
                                      &st_ld_addr);
          st_ld_len = V1_STM_SIZE (insn) * 4;
          is_st_ld = 1;
        }
      else if (V1_IS_STQ (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V1_STQ_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = 4 * 4;
          is_st_ld = 2;
        }
      else if (V1_IS_LDM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_R0_REGNUM,
                                      &st_ld_addr);
          st_ld_len = V1_LDM_SIZE (insn) * 4;
          is_st_ld = 2;
        }
      else if (V1_IS_LDQ (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V1_LDQ_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = 4 * 4;
          is_st_ld = 2;
        }
    }
  *r_st_ld_len = st_ld_len;
  *r_st_ld_addr = st_ld_addr;
  *r_is_st_ld = is_st_ld;
}

#else  /* CSKYGDB_CONFIG_ABIV2 */
static void
csky_get_cur_st_ld_info_v2 (unsigned int insn, int insn_version,
                            int *r_st_ld_len, ULONGEST *r_st_ld_addr,
                            int *r_is_st_ld)
{
  int st_ld_len;
  int is_st_ld;
  ULONGEST st_ld_addr;
  st_ld_len = *r_st_ld_len;
  st_ld_addr = *r_st_ld_addr;
  is_st_ld = *r_is_st_ld;

  if (!((insn & 0xc0000000) == 0xc0000000))
    {
      /* Insn v2 16bits.  */
      insn >>= 16;
      if (V2_16_IS_PUSH (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM,
                                      &st_ld_addr);
          st_ld_len = 4 * (V2_16_IS_PUSH_R15 (insn)
                           + V2_16_PUSH_LIST1 (insn));
          st_ld_addr -= st_ld_len;
          is_st_ld = 1;
        }
      else if (V2_16_IS_POP (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM,
                                      &st_ld_addr);
          st_ld_len = 4 * (V2_16_IS_POP_R15 (insn)
                           + V2_16_POP_LIST1 (insn));
          is_st_ld = 2;
        }
    }
  else
    {
      /* Insn v2 32bits.  */
      if (V2_32_IS_STM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_STM_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_32_STM_SIZE (insn) * 4;
          is_st_ld = 1;
        }
      else if (V2_32_IS_PUSH (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM,
                                      &st_ld_addr);
          st_ld_len = 4 * (V2_32_IS_PUSH_R29 (insn)
                           + V2_32_IS_PUSH_R15 (insn)
                           + V2_32_PUSH_LIST1 (insn)
                           + V2_32_PUSH_LIST2 (insn));
          st_ld_addr -= st_ld_len;
          is_st_ld = 1;
        }
      else if (V2_32_IS_LDM (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      V2_32_LDM_ADDR_REGNUM (insn),
                                      &st_ld_addr);
          st_ld_len = V2_32_LDM_SIZE (insn) * 4;
          is_st_ld = 2;
        }
      else if (V2_32_IS_POP (insn))
        {
          regcache_raw_read_unsigned (get_current_regcache (),
                                      CSKY_SP_REGNUM,
                                      &st_ld_addr);
          st_ld_len = 4 * (V2_16_IS_POP_R15 (insn)
                           + V2_16_POP_LIST1 (insn));
          is_st_ld = 2;
        }
    }
  *r_st_ld_len = st_ld_len;
  *r_st_ld_addr = st_ld_addr;
  *r_is_st_ld = is_st_ld;
}
#endif  /* CSKYGDB_CONFIG_ABIV2 */



static CORE_ADDR
csky_get_current_hw_address ()
{
  register struct breakpoint *b;
  int insn_version;

  unsigned int insn = -1;
  int is_st_ld = 0; /* 0 ~ not st or ld, 1 st, 2 ld.  */
  int hit_hw_bkpt = 0;
  ULONGEST st_ld_addr = 0;
  int st_ld_len = 0;
  int is_current_insn = 0;

#ifndef CSKYGDB_CONFIG_ABIV2    /* FOR ABIV1 */
  int mach = gdbarch_bfd_arch_info (get_current_arch ())->mach;
  if (((mach & CSKY_ARCH_MASK) == CSKY_ARCH_510)
       || ((mach & CSKY_ARCH_MASK) == CSKY_ARCH_610)
       || (mach == 0))
    insn_version = CSKY_INSN_V1;
  else
    insn_version = CSKY_INSN_V2P;
#endif    /* CSKYGDB_CONFIG_ABIV2 */

  for (b = breakpoint_chain; b; b = b->next)
    {
      if (b->enable_state == bp_enabled)
        {
          if (b->type == bp_hardware_watchpoint
              || b->type == bp_read_watchpoint
              || b->type == bp_access_watchpoint)
            {
              /* Begin to insn analyze for one watchpoint
                 in breakpoint chain.  */
              /* Get previous insn.  */
              insn = get_pre_cur_insn (insn_version, 1);

              /* Analyze previous insn.  */
#ifndef CSKYGDB_CONFIG_ABIV2
             csky_get_pre_st_ld_info_v1 (insn, insn_version,
                                         &st_ld_len, &st_ld_addr,
                                         &is_st_ld);
#else /* CSKYGDB_CONFIG_ABIV2 */
             csky_get_pre_st_ld_info_v2 (insn, insn_version,
                                         &st_ld_len, &st_ld_addr,
                                         &is_st_ld);
#endif /* CSKYGDB_CONFIG_ABIV2 */
        /* Check the address, judge whether a valid watchpoint or not.  */
        if ((is_st_ld == 1
              && (b->type == bp_hardware_watchpoint
                  || b->type == bp_access_watchpoint))
            || (is_st_ld == 2
                 && (b->type == bp_read_watchpoint
                     || b->type == bp_access_watchpoint)))
          {
            if (((b->loc->address >= st_ld_addr)
                  && (b->loc->address < (st_ld_addr + st_ld_len)))
                || (((b->loc->address +  b->loc->length) > st_ld_addr)
                     && ((b->loc->address +  b->loc->length)
                          <= (st_ld_addr + st_ld_len))))
              {
                /* Is valid hw addr.  */
                is_current_insn = 0; /* Previous insn lead to break.  */
                break;
              }
          }
        /* Previous insn is not valid addr,
           we begin to chech the current insn.  */
        is_st_ld = 0;
        insn = get_pre_cur_insn (insn_version, 0);
        /* Analyze current insn.  */
#ifndef CSKYGDB_CONFIG_ABIV2
        csky_get_cur_st_ld_info_v1 (insn, insn_version,
                                    &st_ld_len, &st_ld_addr,
                                    &is_st_ld);
#else  /* CSKYGDB_CONFIG_ABIV2 */
        csky_get_cur_st_ld_info_v2 (insn, insn_version,
                                    &st_ld_len, &st_ld_addr,
                                    &is_st_ld);
#endif /* CSKYGDB_CONFIG_ABIV2 */

        /* Check the addr, judge whether a valid watchpoint or not.  */
        if ((is_st_ld == 1
             && (b->type == bp_hardware_watchpoint
                 || b->type == bp_access_watchpoint))
            || (is_st_ld == 2
                && (b->type == bp_read_watchpoint
                    || b->type == bp_access_watchpoint)))
          {
            /* Check the address in location of b.  */
            if (((b->loc->address >= st_ld_addr)
                  && (b->loc->address < (st_ld_addr + st_ld_len)))
                || (((b->loc->address +  b->loc->length) > st_ld_addr)
                    && ((b->loc->address +  b->loc->length)
                         <= (st_ld_addr + st_ld_len))))
              {
                /* Is valid hw address.  */
                is_current_insn = 1; /* Current insn lead to break.  */
                break;
              }
          }
        is_st_ld = 0;
        /* End of insn analyze for one watchpoint in breakpoint chain,
           go to next watchpoint.  */
      }
    }
  }

  if (is_st_ld)
    {
      hit_watchpoint = 1;
      /* If current insn lead to break, we should step over it.  */
      if (is_current_insn)
        set_gdbarch_have_nonsteppable_watchpoint (get_current_arch (), 1);
      else
        set_gdbarch_have_nonsteppable_watchpoint (get_current_arch (), 0);

      return b->loc->address;
    }

  return 0;
}
static ptid_t
csky_wait (struct target_ops *ops, ptid_t ptid,
           struct target_waitstatus *status, int options)
{
  int cpu_status;
  TARGET_DEBUG_PRINTF (("csky_wait.\n"));

  interrupt_count = 0;

  /* Set new signal handler.  */
  ofunc =  signal (SIGINT, csky_interrupt);

  do
    {
      if (interrupt_count)
        {
          int ret = hardware_enter_debugmode ();
          if (ret == JTAG_NON_DEBUG_REGION)
            {
              interrupt_count  = 0;
              warning ("CPU is running in non-debug region,"
                       " please try later.\n");
            }
          else if (ret  == JTAG_PROXY_SERVER_TERMINATED)
            {
              /* Connection broken.  */
              inferior_ptid = null_ptid;
              error ("the proxy server was broken.\n");
            }
        }

      usleep (10);
     if (hardware_check_debugmode (&cpu_status) < 0)
       {
         /* Connection broken.  */
         inferior_ptid = null_ptid;
         error ("the proxy server was broken.\n");
       }
    }
  while (!cpu_status);  /* When debugmode status == 0.  */

  if (check_quit_flag ())
    sleep (1);

  /* Still dont know the necanism yet.  */
  signal (SIGINT, ofunc);

  registers_changed ();

  hit_watchpoint = 0;
  hit_watchpoint_addr = 0;

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = GDB_SIGNAL_TRAP;

  if (cpu_status & 0x080)    /* hw/wp.  */
    {
      /* FIXME :  when enable it, watchpoint can work fine
                  but hbreak cannot stop
         NOTE: hit_watchpoint will be set in csky_get_current_hw_address.  */
      hit_watchpoint_addr = csky_get_current_hw_address ();
    }
  /* Ctrl + c, dro, hdro, edro.  */
  else if((cpu_status & 0x700) && ((cpu_status & 0x20) != 0x20))
    {
      status->value.sig = GDB_SIGNAL_INT;
    }
  else if (cpu_status & 0x040)    /* swo, bkpt.  */
    {
    }
  else /* TODO : other status.  */
    {
    }


  kernel_update_thread_info(&inferior_ptid);

  return inferior_ptid;
}


/* ----------------- For operating HAD register ---------------  */
static void
csky_fetch_had_registers (HADREG regno, unsigned int *val)
{
  if (regno < HID || regno > HCDI)
    {
      error ("internal error, invalid had register number");
    }
  if (hardware_read_had_reg (regno, val) < 0)
    {
      error (errorinfo ());
    }
}

static void
csky_store_had_registers(HADREG regno, unsigned int val)
{
  if (regno < HID || regno > HCDI)
    {
      error ("internal error, invalid had register number");
    }
  if (hardware_write_had_reg (regno, val) < 0)
    {
      error (errorinfo ());
    }

}
/* ----------------- End for operate HAD register ---------------  */

static void
csky_fetch_registers (struct target_ops *ops,
                      struct regcache *regcache, int regno)
{
  unsigned int val;
  int i;
  int csky_total_regnum;
  ptid_t current_thread_ptid;
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  if (regno == -1)
    {
      error ("internal error, invalid register number");
    }

  TARGET_DEBUG_PRINTF (("csky_fetch_registers: regno = %d,", regno));
  /* In multi-threads' condition,if context is
     not the current_context of the csky-target,
     GDB cann't fetch the register.So,here a dummy
     way can solve the problem. */
  /* FIXME: use inferior_ptid ???
  current_thread_ptid = get_ptid_regcache (regcache);  */
  current_thread_ptid = inferior_ptid;   /* FIXME:??  */
  if ((current_kernel_ops != NULL)
      && !ptid_equal (current_kernel_ops->csky_target_ptid,
                      current_thread_ptid))
    {
       current_kernel_ops->to_fetch_registers (current_thread_ptid,
                                               regno, &val);
    }
  else
    {
      int serverReg_nr = csky_register_convert (regno, regcache);
#ifndef CSKYGDB_CONFIG_ABIV2     /* FOR ABIV1 */
      csky_total_regnum = CSKY_ABIV1_NUM_REGS;
#else  /* CSKYGDB_CONFIG_ABIV2 */
      /* Main ver above 3 supporting all cr_banks regs.  */
      if (HW_ICE_MAIN_VER (hardware_version) > 3)
        csky_total_regnum = CSKY_ABIV2_XML10_NUM_REGS;
      else if ((HW_ICE_MAIN_VER (hardware_version) == 3)
               && (HW_ICE_SUB_VER (hardware_version)  >= 3))
        csky_total_regnum = CSKY_NUM_REGS;
      else
        /* Version below 3.3 doesn't support cr_bank3.  */
        csky_total_regnum = CSKY_NUM_REGS - CSKY_CRBANK_NUM_REGS;
#endif /* CSKYGDB_CONFIG_ABIV2 */
      if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
        {
          if (regno > csky_total_regnum || serverReg_nr == -1)
            {
              error ("Current remote server doesn't support regnum %d, the "
                     "value of reg \"%s\" will not be credible and meaningful.",
                     regno,
                     user_reg_map_regnum_to_name (gdbarch, regno));
              return;
            }
        }

      if (serverReg_nr & 0x4000)
        {
          /* New debugserver register number coding.  */
          if (hardware_read_xreg (regcache, regno) < 0)
            {
              error (errorinfo ());
            }
          return;
        }
      else
        {
          /* Old Server register number coding.  */
          if (hardware_read_reg (serverReg_nr, &val) < 0)
            {
              error (errorinfo ());
            }
        }
    }
  {
    gdb_byte buf[MAX_REGISTER_SIZE];
    /* We got the number the register holds, but gdb expects to see a
       value in the target byte ordering.  */

    store_unsigned_integer (buf, register_size (gdbarch, regno),
                            byte_order, val);
    regcache_raw_supply (regcache, regno, buf);
  }

  TARGET_DEBUG_PRINTF ((" 0x%x.\n", val));
}

static void
csky_store_registers (struct target_ops *ops,
                      struct regcache *regcache, int regno)
{
  int serverReg_nr;
  int csky_total_regnum;
  ULONGEST regval;
  ptid_t current_thread_ptid;
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  if (regno == -1)
    {
      error ("internal error, invalid register number");
    }

  serverReg_nr = csky_register_convert (regno, regcache);
#ifndef CSKYGDB_CONFIG_ABIV2     /* FOR ABIV1 */
  csky_total_regnum = CSKY_ABIV1_NUM_REGS;
#else  /* CSKYGDB_CONFIG_ABIV2 */
  /* Main ver above 3 supporting all cr_banks regs.  */
  if (HW_ICE_MAIN_VER (hardware_version) > 3)
    csky_total_regnum = CSKY_ABIV2_XML10_NUM_REGS;
  else if ((HW_ICE_MAIN_VER (hardware_version) == 3)
           && (HW_ICE_SUB_VER (hardware_version)  >= 3))
    csky_total_regnum = CSKY_NUM_REGS;
  else
    /* Version below 3.3 doesn't support cr_bank3.  */
    csky_total_regnum = CSKY_NUM_REGS - CSKY_CRBANK_NUM_REGS;
#endif /* CSKYGDB_CONFIG_ABIV2 */

  if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
    {
      if (regno > csky_total_regnum || serverReg_nr == -1)
        {
          error ("Current remote server doesn't support regnum %d,"
                 " write reg \"%s\" will be ignored.",
                 regno,
                 user_reg_map_regnum_to_name (gdbarch, regno));
          return;
        }
    }

  if (serverReg_nr & 0x4000)
    {
      /* New debugserver register number coding.  */
      if (hardware_write_xreg (regcache, regno) < 0)
        {
          error (errorinfo ());
        }
      return;
    }

  regcache_cooked_read_unsigned (regcache, regno, &regval);

  TARGET_DEBUG_PRINTF (("csky_store_registers: regno = %d, val = 0x%x",
                       regno, regval));

  /* In multi-thread condition, if you want to change register value
     which is not belong to inferior_ptid, csky-elf-gdb will do nothing.  */
  /*  FIXME: how to get ptid, use inferior_ptid or write function
             get_ptid_regcache()
  current_thread_ptid = get_ptid_regcache (regcache);  */
  current_thread_ptid = inferior_ptid; /* FIXME:??  */
  if ((current_kernel_ops != NULL)
      && !ptid_equal (current_kernel_ops->csky_target_ptid,
                      current_thread_ptid))
    {
      current_kernel_ops->to_store_registers (current_thread_ptid,
                                              regno, regval);
    }
  else
    {
      if (hardware_write_reg (serverReg_nr, (U32)regval) < 0)
        {
          error (errorinfo ());
        }
    }
}

static void
csky_prepare_to_store (struct target_ops *ops, struct regcache *arg1)
{
  /* Do nothing or ??  */
  return;
}

static int
csky_xfer_memory (CORE_ADDR memaddr,
                  gdb_byte *myaddr,
                  int len,
                  int write,
                  struct target_ops *ignore)
{
  register int count = len;
  int status;
  int i;
  unsigned char *__buf;
  register unsigned char *buffer = myaddr;
  register CORE_ADDR addr = memaddr;
  int block_xfer_size = CSKY_SOCKET_PACKAGE_SIZE;
  int nBlocks = (count + block_xfer_size -1) / block_xfer_size;

  TARGET_DEBUG_PRINTF (("csky_xfer_memory: %s addr=%x, len=%i, \n",
                       write ? "write":"read", (U32)memaddr, len));

  if (write)
    {
      /* CZ: rewrote the block transfer routines to make the code
         a little more efficient for implementations that can handle
         variable sized scan chains. Might be useful in the future.
         Certainly makes downloads to the simulator more efficient.  */

      for (i=0; i<nBlocks; i++,count-=block_xfer_size,addr += block_xfer_size)
         {
           int n = count < block_xfer_size ? count : block_xfer_size;
           int nSize ;  /* Mode of write memory.  */
           int nCount ;
#define ALIGN_ATTR(addr, len)  (((addr) << 2) | (len) )
           switch (ALIGN_ATTR ((addr & 3), (n & 3)))
             {
             case ALIGN_ATTR (0, 0):
               nSize = 4;  /* Mode of read memory.  */
               break;
             case ALIGN_ATTR (0, 2):
             case ALIGN_ATTR (2, 0):
             case ALIGN_ATTR (2, 2):
               nSize = 2;  /* Mode of read memory.  */
               break;
             default:
               nSize = 1;  /* Mode of read memory.  */
             }
           nCount = n / nSize;
           __buf = (unsigned char*) &(buffer[i * block_xfer_size]);
           status = hardware_write_mem (addr, __buf, nSize, nCount);
           if (status)
             {
               TARGET_DEBUG_PRINTF (("error write memory return.\n"));
               error (errorinfo ());
             }
         }
    }
  else
    {
      for (i=0; i<nBlocks; i++)
        {
          int n = count < block_xfer_size ? count : block_xfer_size;
          int nSize;
          int nCount;

          switch (ALIGN_ATTR ((addr & 3), (n & 3)))
            {
            case ALIGN_ATTR (0, 0):
              nSize = 4;  /* Mode of read memory.  */
              break;
            case ALIGN_ATTR (0, 2):
            case ALIGN_ATTR (2, 0):
            case ALIGN_ATTR (2, 2):
              nSize = 2;  /* Mode of read memory.  */
              break;
            default:
              nSize = 1;  /* Mode of read memory.  */
            }
          nCount = n / nSize;
          __buf = (unsigned char*) &(buffer[i * block_xfer_size]);
          status = hardware_read_mem (addr, __buf, nSize, nCount);
          if (status)
            {
              TARGET_DEBUG_PRINTF (("error read memory return.\n"));
              error (errorinfo ());
            }
          count -= block_xfer_size;
          addr += block_xfer_size;
        }
    /* Copy appropriate bytes out of the buffer.  */
  }
  return len;
}

/* More generic transfers.
   READBUF & WRITEBUF:
       NOTE: one, and only one, of READBUF or WRITEBUF must be non-NULL.
   ANNEX: provide additional data-specific information to the target.
   OFFSET: specifies the starting point.
   XFERED_LEN: the number of bytes actually transfered
   Return the status of transfering.  */

static enum target_xfer_status
csky_xfer_partial (struct target_ops *ops,
                   enum target_object object,
                   const char *annex,
                   gdb_byte *readbuf,
                   const gdb_byte *writebuf,
                   ULONGEST offset,
                   ULONGEST len,
                   ULONGEST *xfered_len)
{
  LONGEST partial_len = download_write_size;
  if (download_write_size > len)
    partial_len = len;

  if (object == TARGET_OBJECT_MEMORY)
    {
      LONGEST xfered;
      if (writebuf != NULL)
        {
          gdb_byte *buffer = (gdb_byte *) xmalloc (partial_len);
          struct cleanup *cleanup = make_cleanup (xfree, buffer);

          memcpy (buffer, writebuf, partial_len);

          *xfered_len = csky_xfer_memory (offset, buffer, partial_len,
                                          1/* Write.  */, ops);
          do_cleanups (cleanup);
        }
      if (readbuf != NULL)
        *xfered_len = csky_xfer_memory (offset, readbuf, partial_len,
                                        0/* Read.  */, ops);
      if (*xfered_len > 0)
        return TARGET_XFER_OK;
      else if (*xfered_len == 0)
        return TARGET_XFER_EOF;
      else
        return TARGET_XFER_E_IO;
    }
  else if (object == TARGET_OBJECT_AVAILABLE_FEATURES)
    {
       return  hardware_read_xmltdesc (offset, readbuf, len, xfered_len);
    }
  else if (ops->beneath != NULL)
    return ops->beneath->to_xfer_partial (ops->beneath, object, annex,
                                          readbuf, writebuf, offset,
                                          partial_len, xfered_len);
  else
    return TARGET_XFER_E_IO;
}

static void
csky_files_info  (struct target_ops *ignore)
{
  char *file = "nothing";

  TARGET_DEBUG_PRINTF (("csky_files_info.\n"));
  if (exec_bfd)
    {
      file = bfd_get_filename (exec_bfd);
    }

  printf_filtered ("csky_files_info: file \"%s\"\n", file);

  if (exec_bfd)
    {
      printf_filtered ("\tAttached to %s running program %s\n",
                       target_shortname, file);
    }
}


static int
csky_insert_breakpoint (struct target_ops *ops,
                        struct gdbarch *gdbarch,
                        struct bp_target_info *bp_tgt)
{
  TARGET_DEBUG_PRINTF (("csky_insert_breakpoint: addr = 0x%x.\n",
                       (U32)bp_tgt->placed_address));
  return memory_insert_breakpoint (ops, gdbarch, bp_tgt);
}

static int  csky_remove_breakpoint (struct target_ops *ops,
                                    struct gdbarch *gdbarch,
                                    struct bp_target_info *bp_tgt,
                                    enum remove_bp_reason reason)
{
  TARGET_DEBUG_PRINTF(("csky_remove_breakpoint: addr = 0x%x.\n",
                      (U32)bp_tgt->placed_address));
  return memory_remove_breakpoint (ops, gdbarch, bp_tgt, reason);
}


static int
csky_can_use_hardware_watchpoint (struct target_ops *self, enum bptype bp_type,
                                  int cnt, int other_type_used)
{
  register struct breakpoint *b;
  int hwwatch = 0;
  int hwbreak = 0;
  int max_bkpt;
  unsigned int val = 0; /* For bkpt num.  */

  max_bkpt = max_hw_breakpoint_num;
  for (b = breakpoint_chain; b; b = b->next)
    {
      if (b->enable_state == bp_enabled)
        {
          if (b->type == bp_hardware_watchpoint
              || b->type == bp_read_watchpoint
              || b->type == bp_access_watchpoint)
            {
              hwwatch++;
            }
          else if (b->type == bp_hardware_breakpoint)
            {
              hwbreak++;
            }
        }
    }

  /* CNT is only the count of bp_type. But pay attition to
     when user setting a watchpoint as "watch q.a + q.b + q.c", GDB
     only call "can_use_hardware_watchpoint" once. If GDB has no
     watchpionts now, the CNT will be 3. As CSKY only supports two
     hardware watchpoints, here not to use "CNT" will be a
     mistake.  */
  /* DESC_FD == NULL, no suport hw bkpt.  */
  /* COUNT > MAX_COUNT, exceeds limit.  */
  if (desc_fd == 0)
    return 0;
  /* For hbreak  */
  if (bp_type == bp_hardware_breakpoint)
    {
      if ((hwwatch <= max_watchpoint_num)
           && ((hwwatch + cnt) <= max_bkpt))
        {
          return 1;
        }
      else
        {
          return -1;
        }
    }
  /* For watchpoint.  */
  else
    {
      if ((cnt <= max_watchpoint_num)
          && ((hwbreak + cnt) <= max_bkpt))
        {
          return 1;
        }
      else
        {
          return -1;
        }
    }
}

static int
csky_insert_hw_breakpoint (struct target_ops *self,
                           struct gdbarch *gdbarch,
                           struct bp_target_info *bp_tgt)
{
  TARGET_DEBUG_PRINTF (("csky_insert_hw_breakpoint: addr = 0x%x.\n",
                       (U32)bp_tgt->reqstd_address));

  return hardware_insert_hw_breakpoint (bp_tgt->reqstd_address);
}

static int
csky_remove_hw_breakpoint (struct target_ops *self,
                           struct gdbarch *gdbarch,
                           struct bp_target_info *bp_tgt)
{
  TARGET_DEBUG_PRINTF (("csky_remove_hw_breakpoint: addr = 0x%x.\n",
                       (U32)bp_tgt->reqstd_address));
  return hardware_remove_hw_breakpoint (bp_tgt->reqstd_address);
}


/* For watchpoint implementation.  */

static int
csky_insert_watchpoint (struct target_ops *self,
                        CORE_ADDR addr, int len,
                        enum target_hw_bp_type type,
                        struct expression *cond)
{
  unsigned long mask;
  TARGET_DEBUG_PRINTF (("csky_insert_watchpoint: addr = 0x%x, len = %d,"
                        " type = %d.\n",
                       (U32)addr, len, type));
  return hardware_insert_watchpoint (addr, len, type, 0);
}

static int
csky_remove_watchpoint (struct target_ops *self,
                        CORE_ADDR addr, int len,
                        enum target_hw_bp_type type,
                        struct expression *cond)
{
  TARGET_DEBUG_PRINTF (("csky_remove_watchpoint: addr = 0x%x, len = %d,"
                       " type = %d.\n",
                      (U32)addr, len, type));
  return hardware_remove_watchpoint (addr, len, type, 0);
}

static int
csky_stopped_by_watchpoint (struct target_ops *ops)
{
  TARGET_DEBUG_PRINTF (("csky_stopped_by_watchpoint: hit_watchpoint = %d.\n",
                       hit_watchpoint));
  return hit_watchpoint;
}


static int
csky_stopped_data_address (struct target_ops *target, CORE_ADDR *addr_p)
{
  if (hit_watchpoint)
    {
      *addr_p = hit_watchpoint_addr;
    }
  return hit_watchpoint;
}

static void csky_kill (struct target_ops *ops)
{
  TARGET_DEBUG_PRINTF (("csky_kill.\n"));

  /* FIXME:???  */
  /* inferior_ptid = null_ptid;  */
  /* delete_thread_silent (remote_csky_ptid);  */
}

/* For implementation of funtion hook: deprecated_show_load_progress
   Implementation of dynamic load.  */

static void
csky_show_load_progress (const char *section_name,
                         unsigned long sent_so_far,
                         unsigned long total_section,
                         unsigned long total_sent,
                         unsigned long grand_total)
{
  unsigned long this_sent;
  static unsigned long section_sent = 0;

  this_sent = total_sent - pre_load_block_size;
  pre_load_block_size = total_sent;
  section_sent += this_sent;

  printf ("\r\tsection progress: %5.1f\%, total progress: %5.1f\% ",
          (100 * (float) section_sent / (float) total_section),
          (100 * (float) total_sent / (float) download_total_size));
  fflush (stdout);

  if (section_sent >= total_section)
    {
      section_sent = 0;
      putchar_unfiltered ('\n');
    }
}

static void
csky_load (struct target_ops *self, const char *args, int from_tty)
{
  /* Check target endian info and tip if conflict.  */
  int endian;
  struct regcache *regcache;
  struct cleanup *old_chain = NULL;

  TARGET_DEBUG_PRINTF (("csky_load.\n"));

  /* 1 ~ big endian 0 ~ little endian.  */
  if (hardware_endianinfo (&endian) < 0)
    {
      error (errorinfo ());
    }

  if ((endian == CSKY_LITTLE_ENDIAN
       && (gdbarch_byte_order (get_current_arch ()) == BFD_ENDIAN_BIG))
      || (endian == CSKY_BIG_ENDIAN
          && (gdbarch_byte_order (get_current_arch ()) == BFD_ENDIAN_LITTLE)))
    {
      /* Conflit.  */
      if (from_tty
          && !query ("The program endian conflit with target,"
                     " load it still(no)?"))
        {
          error ("Program load failure.\n");
        }
      warning ("endian of target conflit with program.\n");
    }

  /* Get download total size.  */
  {
    bfd *abfd;
    asection *s;
    abfd = bfd_openr (args, 0);
    if (!abfd)
      {
        printf_filtered ("Unable to open file %s\n", args);
        return;
      }

    if (bfd_check_format (abfd, bfd_object) == 0)
      {
        printf_filtered ("File is not an object file\n");
        return;
      }

    for (s = abfd->sections; s; s = s->next)
      {
        if (s->flags & SEC_LOAD)
          {
            download_total_size += bfd_get_section_size (s);
          }
      }
  }

  /* When mi, load_flag = false.  */
  if (interpreter_p != NULL && (interpreter_p[0] == 'm'
      && interpreter_p[1] == 'i'))
    {
      load_flag = 0;
    }
  else
    {
      deprecated_show_load_progress = csky_show_load_progress;
    }

  /* Add '\"' to filepath's top and tail, for path which have " " in.  */
  if (args != NULL)
    {
      if (strchr (args, ' ') != NULL)
        {
          char *arg_tmp;
          int len = strlen (args);
          arg_tmp = (char *) malloc (len + 3);
          old_chain = make_cleanup (free, arg_tmp);

          arg_tmp[0] = arg_tmp[len + 1] = '\"';
          arg_tmp[len + 2] = '\0';
          memcpy (arg_tmp + 1, args, len);
          args = arg_tmp;
        }
    }

  generic_load (args, from_tty);
  /* Finish load, initial global variable.  */
  pre_load_block_size = 0;
  download_total_size = 0;

  /* Finally, make the PC point at the start address.  */
  if (current_kernel_ops)
    {
      /* Initialize thread_list in kernel_module.  */
      current_kernel_ops->to_init_thread_info (0);
      current_kernel_ops->csky_target_ptid = remote_csky_ptid;
    }

  regcache = get_current_regcache ();
  if (exec_bfd)
    regcache_write_pc (regcache, bfd_get_start_address (exec_bfd));

  {
    reinit_frame_cache ();
    stop_pc = regcache_read_pc (get_current_regcache ());
    print_stack_frame (get_selected_frame (NULL), 0, SRC_AND_LOC, 1);
  }

  already_load = 1;

  if (old_chain != NULL)
    do_cleanups (old_chain);

}


static void
csky_create_inferior(struct target_ops *ops, char *execfile,
                     char *args, char **env, int from_tty)
{
  CORE_ADDR entry_pt;
  TARGET_DEBUG_PRINTF (("csky_create_inferior.\n"));

  if (args && *args)
    {
      warning ("Can't pass arguments to remote csky board;"
               " arguments ignored.");

      /* And don't try to use them on the next "run" command.  */
      execute_command ("set args", 0);
    }

  if (execfile == 0 || exec_bfd == 0)
    {
      error ("No executable file specified");
    }

  entry_pt = (CORE_ADDR) bfd_get_start_address (exec_bfd);

  /* Needed to get correct instruction in cache.  */
  clear_proceed_status (0);

  init_wait_for_inferior ();

  /* Set up the "saved terminal modes" of the inferior
     based on what modes we are starting it with.  */
  target_terminal_init ();

  /* Install inferior's terminal modes.  */
  target_terminal_inferior ();

  regcache_write_pc (get_current_regcache (), entry_pt);
}

/* Check to see if a thread is still alive.  */

static int
csky_thread_alive
(struct target_ops *ops, ptid_t ptid)
{
  /* Function override from kernel_ops.  */
  if (current_kernel_ops)
    return current_kernel_ops->to_thread_alive (ptid);

  if (ptid_equal (ptid, remote_csky_ptid))
    /* The main task is always alive.  */
    return 1;

  return 0;
}

/* Convert a thread ID to a string.  Returns the string in a static
   buffer.
   when ptid.tid == 0: to str of a inferior
        ptid.tid != 0: to str of a thread.  */

static char *
csky_pid_to_str (struct target_ops *ops, ptid_t ptid)
{
  static char buf[64];
  /* Function voerride from kernel_module.  */
  if (current_kernel_ops
      && !ptid_equal (current_kernel_ops->csky_target_ptid, remote_csky_ptid)
      && ptid.tid != 0)
    {
      current_kernel_ops->to_pid_to_str (ptid, buf);
      return buf;
    }

  if (ptid.pid == remote_csky_ptid.pid)
    {
      if (ptid.tid != 0)
        {
          xsnprintf (buf, sizeof buf, "Thread <main>");
          return buf;
        }
      else if (ptid.tid == 0)
        {
          xsnprintf (buf, sizeof buf, "process <main>");
          return buf;
        }
    }

  return normal_pid_to_str (ptid);
}

static int
csky_return_one (struct target_ops *ops)
{
  return 1;
}

static int
csky_has_execution (target_ops*, ptid_t)
{
  /* !already_load || (interpreter_p == NULL)
     || (interpreter_p[0] == 'm' && interpreter_p[1] == 'i');  */
  return 1;
}

static void
csky_mourn_inferior (struct target_ops* ops)
{
  /* Do nothing.  */
  return;
}


static int
csky_reset (char *args, int from_tty)
{
  if (args)
    {
      if ((args[0] == 'n') && (args[1] == 'f') && ((args[2] == ' ')
         || (args[2] == '\0')))
        {
          return hardware_reset ();
        }
      else
        {
          char errorinfo[256];
          sprintf (errorinfo, "Wrong parameters of '%s',try 'help reset'.",
                   args);
          seterrorinfo (errorinfo);
          return -1;
        }
    }
  hardware_reset ();
  return hardware_enter_debugmode ();
}

static void
csky_reset_command (char *args, int from_tty)
{
  if (!current_ops) /* Not c-sky djp target.  */
    {
      warning("reset command is only supported in jtag debug method.\n");
      return ;
    }

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      error ("target is not connected now.");
    }

  if (csky_reset (args, from_tty) < 0)
    {
      error (errorinfo ());
    }

  /* Target have been reset. Now we should init kernel_ops.  */
  kernel_init_thread_info (1);
  if (current_kernel_ops)
    {
      current_kernel_ops->csky_target_ptid = remote_csky_ptid;
    }

  registers_changed ();

  {
    inferior_ptid = remote_csky_ptid;
    inferior_appeared (current_inferior (), ptid_get_pid (inferior_ptid));
    add_thread_silent (inferior_ptid);

    reinit_frame_cache ();
    registers_changed ();
    stop_pc = regcache_read_pc (get_current_regcache ());
    print_stack_frame (get_selected_frame (NULL), 0, SRC_AND_LOC, 1);
  }

}

static void
csky_pctrace_command (char *args, int from_tty)
{
  U32 *pclist;
  int depth = 0;
  struct symbol* sb;
  struct symtab_and_line sal;
  struct cleanup *cleanup_pctrace, *old_cleanups;
  int i = 0;
  struct ui_out *uiout = current_uiout;

  if (!target_has_execution)
    {
      error (_("The program is not being run."));
    }
  if (ptid_equal (inferior_ptid, null_ptid))
    {
      error ("target is not connected now.");
    }
  depth = args ? parse_and_eval_long (args) : PCFIFO_DEPTH;
  if (depth <= 0)
    {
      error (_("wrong parameter for pctrace command.\nUsage : pctrace n,"
               " parameter n must larger than 0."));
    }
  if (depth < 8)
    {
      pclist = (U32 *)xmalloc(PCFIFO_DEPTH * 8);
      memset (pclist, 0, PCFIFO_DEPTH * 8);
    }
  else
    {
      pclist = (U32 *)xmalloc(depth * 8);
      memset (pclist, 0, depth * 8);
    }
  old_cleanups = make_cleanup (xfree, pclist);
  if (!pctrace)
    {
      warning("pctrace is not supportted in current debug method.\n");
      return;
    }
  /* Support both csky_pctrace and remote_pctrace for jtag
     and remote debug method.  */
  pctrace (args, pclist, &depth, from_tty);

  cleanup_pctrace = make_cleanup_ui_out_tuple_begin_end (uiout, "pctrace");
  while (i < depth)
    {
      struct cleanup *cleanup_frame =
                 make_cleanup_ui_out_tuple_begin_end (uiout, "frame");
      sb = find_pc_function (pclist[i]);
      /* 1 ~ if pc is on the boundary use the previous
         statement's line number.  */
      sal = find_pc_line (pclist[i], 0);
      if (sb)
        {
          ui_out_text (uiout, "#");
          ui_out_field_int (uiout, "level", i);
          ui_out_text (uiout, "  ");
          ui_out_field_core_addr (uiout, "addr",
                                  get_current_arch (), pclist[i]);
          ui_out_text (uiout, "\tin ");
          ui_out_field_string (uiout, "func", sb->ginfo.name);
          if (sal.symtab && sal.symtab->filename)
            {
              ui_out_text (uiout, "()\tat ");
              ui_out_field_string (uiout, "file", sal.symtab->filename);
              ui_out_text (uiout, ":");
              ui_out_field_int (uiout, "line", sal.line);
            }
          else
            {
              ui_out_text (uiout, "()\tat ??");
            }
          ui_out_text (uiout, "\n");
        }
      else
        {
          ui_out_text (uiout, "#");
          ui_out_field_int (uiout, "level", i);
          ui_out_text (uiout, "  ");
          ui_out_field_core_addr (uiout, "addr",
                                  get_current_arch (), pclist[i]);
          ui_out_text (uiout, "\tin ?\n");
        }
      do_cleanups (cleanup_frame);
      i++;
    }
  do_cleanups (cleanup_pctrace);
  do_cleanups (old_cleanups);
}


int
csky_soft_reset (int insn, int from_tty)
{
  return hardware_soft_reset (insn);
}

static void
csky_soft_reset_command (char *args, int from_tty)
{
  char **argv;
  int i;
  unsigned int had_id;
  static int insn = 0;

  if (current_ops != &csky_ops) /* Not c-sky djp target.  */
    {
      warning ("sreset command is only supported in jtag debug method.\n");
      return ;
    }
  else
    {
      if ((proxy_main_ver == 1 && proxy_sub_ver < 6) || (proxy_main_ver < 1))
        {
          warning ("ProxyLayer version is low for sreset command, version 1.6"
                   " or above is needed.\n");
          return ;
        }
    }
  if (args != NULL)
    {
      argv = buildargv (args);
      for (i = 0; argv[i] != NULL; i++)
        {
          if (strcmp (argv[i], "-c\0") == 0)
            {
              if ((argv[i + 1] == NULL) || (argv[i + 1][0] == '-'))
                {
                  error ("wrong parameter of sreset command.");
                }
              else
                {
                  char *s = argv[i + 1];
                  if ((s[0] == '0') && (s[1] == 'x'))
                    {
                      insn = strtoul (argv[i + 1], NULL, 16);
                    }
                  else
                    {
                      error ("Usage : sreset -c parameter, parameter must be"
                             " hex, starting with 0x.");
                    }
                }
              i ++;
            }
          else
            {
              error ("error option : %s is not defined.", argv[i]);
            }
        }
    }
  else
    {
      error ("wrong useage of sreset, sreset -c parameter, parameter"
             " must be hex,starting with 0x.");
    }

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      error ("target is not connected now.");
    }
  if (csky_soft_reset (insn, from_tty) < 0)
    {
      error (errorinfo ());
    }

  /* Target have been reset. Now we should init kernel_ops.  */
  kernel_init_thread_info (1);
  if (current_kernel_ops)
    {
      current_kernel_ops->csky_target_ptid = remote_csky_ptid;
    }

  registers_changed ();

  /* Do it when mi.  */
  /* FIXME:???  */
  /* if(interpreter_p != NULL
        && (interpreter_p[0] == 'm' && interpreter_p[1] == 'i'))  */
  {
    inferior_ptid = remote_csky_ptid;
    inferior_appeared (current_inferior (), ptid_get_pid (inferior_ptid));
    add_thread_silent (inferior_ptid);

    reinit_frame_cache ();
    registers_changed ();
    stop_pc = regcache_read_pc (get_current_regcache ());
    print_stack_frame (get_selected_frame (NULL), 0, SRC_AND_LOC, 1);
  }
}

int
csky_pctrace (char *args, U32 *pclist, int *depth, int from_tty)
{
  U32 tpclist[PCFIFO_DEPTH];
  int result;
  int i;
  int mach = gdbarch_bfd_arch_info (get_current_arch ())->mach;
  if (((mach & CSKY_ARCH_MASK)== CSKY_ARCH_803)
      || ((mach & CSKY_ARCH_MASK)== CSKY_ARCH_802))
    {
      warning ("CK803 & CK802 does not support pctrace function.");
      *depth = -1;
      return 0;
    }

  if (args)
    {
      warning ("In jtag debug method: pctrace command ignore all"
               " prameters.\n");
    }
  result = hardware_trace_pc (tpclist, depth);

  if (result < 0)
    {
      error ("get pc trace info from target error\n");
    }

  i = 0;
  while ((i < PCFIFO_DEPTH) && (i < *depth))
    {
      pclist[i] = tpclist[i];
      i++;
    }
  return 0;
}

static void
init_csky_ops ()
{
  /* Initialize csky_ops.  */
  csky_ops.to_shortname = "csky";
  csky_ops.to_longname = "Remote debugging over JTAG interface";
  csky_ops.to_doc = "Use a CSKY board via a serial line, using jtag protocol.";
  csky_ops.to_open = csky_open;
  csky_ops.to_close = csky_close;
  csky_ops.to_detach = csky_detach;
  csky_ops.to_resume = csky_resume;
  csky_ops.to_wait = csky_wait;
  csky_ops.to_fetch_registers = csky_fetch_registers;
  csky_ops.to_store_registers = csky_store_registers;
  csky_ops.to_prepare_to_store = csky_prepare_to_store;
  csky_ops.to_xfer_partial = csky_xfer_partial;
  csky_ops.to_files_info = csky_files_info;
  csky_ops.to_insert_breakpoint = csky_insert_breakpoint;
  csky_ops.to_remove_breakpoint = csky_remove_breakpoint;
  csky_ops.to_can_use_hw_breakpoint = csky_can_use_hardware_watchpoint;
  csky_ops.to_insert_hw_breakpoint = csky_insert_hw_breakpoint;
  csky_ops.to_remove_hw_breakpoint = csky_remove_hw_breakpoint;
  csky_ops.to_insert_watchpoint = csky_insert_watchpoint;
  csky_ops.to_remove_watchpoint = csky_remove_watchpoint;
  csky_ops.to_stopped_by_watchpoint = csky_stopped_by_watchpoint;
  csky_ops.to_stopped_data_address = csky_stopped_data_address;
  csky_ops.to_kill = csky_kill;
  csky_ops.to_load = csky_load;
  csky_ops.to_create_inferior = csky_create_inferior;
  csky_ops.to_log_command = serial_log_command;
  csky_ops.to_thread_alive = csky_thread_alive;
  csky_ops.to_pid_to_str = csky_pid_to_str;
  csky_ops.to_stratum = process_stratum;
  csky_ops.to_has_all_memory = csky_return_one;
  csky_ops.to_has_memory = csky_return_one;
  csky_ops.to_has_stack = csky_return_one;
  csky_ops.to_has_registers = csky_return_one;
  csky_ops.to_has_execution = csky_has_execution;
  csky_ops.to_magic = OPS_MAGIC;
  csky_ops.to_mourn_inferior = csky_mourn_inferior;

  /* Initialize csky_ecos_ops.  */
  csky_ecos_ops.to_shortname = "ecos";
  csky_ecos_ops.to_longname = "Remote ecos multi-threaded debugging";
  csky_ecos_ops.to_doc = "Use a CSKY board via a serial line, using jtag\
 protocol.";
  csky_ecos_ops.to_open = csky_ecos_open;
  csky_ecos_ops.to_close = csky_close;
  csky_ecos_ops.to_attach = csky_ecos_attach;
  csky_ecos_ops.to_detach = csky_detach;
  csky_ecos_ops.to_resume = csky_resume;
  csky_ecos_ops.to_wait = csky_wait;
  csky_ecos_ops.to_fetch_registers = csky_fetch_registers;
  csky_ecos_ops.to_store_registers = csky_store_registers;
  csky_ecos_ops.to_prepare_to_store = csky_prepare_to_store;
  csky_ecos_ops.to_xfer_partial = csky_xfer_partial;
  csky_ecos_ops.to_files_info = csky_files_info;
  csky_ecos_ops.to_insert_breakpoint = csky_insert_breakpoint;
  csky_ecos_ops.to_remove_breakpoint = csky_remove_breakpoint;
  csky_ecos_ops.to_can_use_hw_breakpoint = csky_can_use_hardware_watchpoint;
  csky_ecos_ops.to_insert_hw_breakpoint = csky_insert_hw_breakpoint;
  csky_ecos_ops.to_remove_hw_breakpoint = csky_remove_hw_breakpoint;
  csky_ecos_ops.to_insert_watchpoint = csky_insert_watchpoint;
  csky_ecos_ops.to_remove_watchpoint = csky_remove_watchpoint;
  csky_ecos_ops.to_stopped_by_watchpoint = csky_stopped_by_watchpoint;
  csky_ecos_ops.to_stopped_data_address = csky_stopped_data_address;
  csky_ecos_ops.to_kill = csky_kill;
  csky_ecos_ops.to_load = csky_load;
  csky_ecos_ops.to_create_inferior = csky_create_inferior;
  csky_ecos_ops.to_log_command = serial_log_command;
  csky_ecos_ops.to_thread_alive = csky_thread_alive;
  csky_ecos_ops.to_pid_to_str = csky_pid_to_str;
  csky_ecos_ops.to_stratum = process_stratum;
  csky_ecos_ops.to_has_all_memory = csky_return_one;
  csky_ecos_ops.to_has_memory = csky_return_one;
  csky_ecos_ops.to_has_stack = csky_return_one;
  csky_ecos_ops.to_has_registers = csky_return_one;
  csky_ecos_ops.to_has_execution = csky_has_execution;
  csky_ecos_ops.to_magic = OPS_MAGIC;
  csky_ecos_ops.to_mourn_inferior = csky_mourn_inferior;
}


extern initialize_file_ftype _initialize_csky_jtag;

void
_initialize_csky_jtag (void)
{
  init_csky_ops ();
  add_target (&csky_ops);

  csky_ops.to_shortname = "jtag";
  add_target (&csky_ops);

  add_target(&csky_ecos_ops);
  /* TODO: add reset/pctrace command.  */
  add_com ("reset", class_run, csky_reset_command,
           _("Reset remote target.\n\
Usage: ALL parameters will be regarded as wrong except 'nf' or null.\n\
       Using 'reset nf',gdb will not do 'enter debug mode' after reset.\n\
       Using 'reset'   ,gdb will do 'enter debug mode'(default)."));

  add_com ("sreset", class_run, csky_soft_reset_command,
           _("Soft Reset remote target.\n\
Usage: sreset -c parameter, parameter must be hex, starting with 0x"));

  add_com ("pctrace", class_run, csky_pctrace_command,
           "Watch the pc jump trace of target machine. Showing pc is the "
           "object address of jump instructions. The first pc is newest\n");

  add_info ("mthreads", csky_info_mthreads_command,
                  "multi-threads commands, only support in multi-threads debugging:\n  info mthreads list:list all threads' info.\n  info mthreads ID:list one thread's detailed info.\n  info mthreads stack all:list all threads' stack info.\n  info mthreads stack depth [ID]:list one or all thread(s) stack depth info.");

  add_setshow_boolean_cmd ("show-load-progress", class_support, &load_flag,
          "Set print progress of program loading.",
          "Show print progress of program loading.",
          NULL,
          NULL,
          NULL, /* FIXME: i18n: */
          &setlist, &showlist);

  add_setshow_boolean_cmd ("prereset", class_support, &prereset_flag,
                           "Set  prereset.",
                           "Show  prereset.",
                           NULL,
                           NULL,
                           NULL, /* FIXME: i18n:  */
                           &setlist, &showlist);

  add_setshow_uinteger_cmd("download-write-size", class_obscure,
          &download_write_size,
          "Set Download size per socket packet.",
          "Show Download size per socket packet.",
          "Only used when downloading a program onto a remote\n"
          "target. Specify zero, or a negative value, to disable\n"
          "blocked writes. The actual size of each transfer is also\n"
          "limited by the size of the target packet and the memory\n"
          "cache.\n",
          NULL, NULL,
          &setlist, &showlist);

  remote_csky_ptid = ptid_build (42000, 0, 42000);
}
