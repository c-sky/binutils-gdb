/* Target-dependent code for the CSKY architecture, for GDB, the GNU Debugger.

   Copyright (C) 1988-2016 Free Software Foundation, Inc.

   Contributed by CSKY.

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
#include "gdb_assert.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "value.h"
#include "gdbcmd.h"
#include "language.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "target.h"
#include "arch-utils.h"
#include "regcache.h"
#include "osabi.h"
#include "block.h"
#include "reggroups.h"
#include "elf/csky.h"
#include "elf-bfd.h"
#include "symcat.h"
#include "sim-regno.h"
#include "dis-asm.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "infcall.h"
#include "floatformat.h"
#include "remote.h"
#include "target-descriptions.h"
#include "dwarf2-frame.h"
#include "user-regs.h"
#include "valprint.h"
#include "reggroups.h"
#include "csky-tdep.h"
#include "regset.h"
#include "block.h"
#include "solib-svr4.h" /* Svr4_so_ops.  */
#include "opcode/csky.h"

#ifdef __MINGW32__
#include <windows.h>
#include <winbase.h>
#endif /*__MINGW32__*/

#if 1
#define ckcore_insn_debug(args) { }
#else
#define ckcore_insn_debug(args) { printf args; }
#endif

/* For djp_version msg
   bit[31:24] proxy main ver  bit[23:16] proxy sub ver
   bit[13:8]  ice main ver    bit[7:0]   ice sub ver.  */
unsigned int hardware_version = 0;

/* For pctrace command.  */
pctrace_function_type pctrace = NULL;

static struct reggroup * cr_reggroup;
static struct reggroup * fr_reggroup;
static struct reggroup * vr_reggroup;
static struct reggroup * mmu_reggroup;
static struct reggroup * prof_reggroup;

/*  */
static gdbarch_init_ftype csky_gdbarch_init;

/* Whether the register is cared when conducting mi command
   data_list_changed_registers. This is for debugging speed.  */
static char csky_selected_register_p[CSKY_NUM_REGS] = {0};

#ifndef CSKYGDB_CONFIG_ABIV2
/* Get version of csky instruction set.  */

static int
csky_get_insn_version (struct gdbarch * gdbarch)
{
  int mach = gdbarch_bfd_arch_info (gdbarch)->mach;
  if (((mach & CSKY_ARCH_MASK) == CSKY_ARCH_510) ||
      ((mach & CSKY_ARCH_MASK) == CSKY_ARCH_610) ||
      (mach == 0))
    {
      return CSKY_INSN_V1;
    }

  return CSKY_INSN_V2P;
}
#endif /* not CSKYGDB_CONFIG_ABIV2 */

/*  Only for csky v2 or v2p, check the instruction at ADDR is
    16 bit or not.  */

static int
csky_pc_is_csky16 (struct gdbarch *gdbarch, CORE_ADDR addr, int is_insn_v2)
{
  gdb_byte target_mem[2];
  int status;
  unsigned int insn;
  int ret = 1;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  status = target_read_memory (addr, target_mem, 2);
  if (status)
    {
      memory_error (TARGET_XFER_E_IO, addr);
    }
  /* Get insn by read mem.  */
  insn = extract_unsigned_integer (target_mem, 2, byte_order);
#ifdef CSKYGDB_CONFIG_ABIV2
  /* CSKY_INSN_V2.  */
  if ((insn & 0xc000) == 0xc000)
    ret = 0;
  else
    {
      if (insn == 0x00)
        {
          /* Parse if insn at ADDR is a 32-bits bkpt. It is used
             for GDBARCH_SKIP_PARMANET_BREAKPOINT when breakpoint
             always-insert is on.  */
          status = target_read_memory ((addr + 2), target_mem, 2);
          if (status)
            {
              memory_error (TARGET_XFER_E_IO, (addr + 2));
            }
          /* Get insn by read mem.  */
          insn = extract_unsigned_integer (target_mem, 2, byte_order);
          if (insn == 0x00)
            ret = 0;
        }
    }
#else  /* For the configuration of abiv1, here it only tell insn_v2p. */
  /* CSKY_INSN_V2P.  */
  if (insn & 0x8000)
    ret = 0;
#endif /* Not CSKYGDB_CONFIG_ABIV2 */
  return ret;
}



/* Get one instruction at ADDR
   INSN : instruction
   return: 2 for 16bit insn, 4 for 32bit insn.  */
static int
csky_get_insn (struct gdbarch *gdbarch, CORE_ADDR addr, unsigned int *insn)
{
  gdb_byte target_mem[2];
  unsigned int insn_t;
  int status;
  int insn_len = 2;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  status = target_read_memory (addr, target_mem, 2);
  if (status)
    {
      memory_error (TARGET_XFER_E_IO, addr);
    }

  insn_t = extract_unsigned_integer (target_mem, 2, byte_order);
#ifndef CSKYGDB_CONFIG_ABIV2    /* For abiv1 */
  if (csky_get_insn_version (gdbarch) == CSKY_INSN_V2P && !(insn_t & 0x8000))
#else                           /* For abiv2 */
  if (0xc000 == (insn_t & 0xc000))
#endif  /* For abiv2 end */
    {
      /* Insn_v2 && insn_len == 32bits.  */
      status = target_read_memory (addr + 2, target_mem, 2);
      if (status)
        {
          memory_error (TARGET_XFER_E_IO, addr);
        }
      insn_t = (insn_t << 16) | extract_unsigned_integer (target_mem, 2,
                                                          byte_order);
      insn_len = 4;
    }
  *insn = insn_t;
  return insn_len;
}


/* For set_gdbarch_read_pc
   For set_gdbarch_write_pc
   For set_gdbarch_unwind_sp.  */

static CORE_ADDR
csky_read_pc (struct regcache *regcache)
{
  ULONGEST pc;
  regcache_cooked_read_unsigned (regcache, CSKY_PC_REGNUM, &pc);
  return pc;
}

static void
csky_write_pc (struct regcache *regcache, CORE_ADDR val)
{
  regcache_cooked_write_unsigned (regcache, CSKY_PC_REGNUM, val);
}

static CORE_ADDR
csky_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, CSKY_SP_REGNUM);
}

/* csky_register_name_v1[] and csky_register_name_v2[] are all registers
   names supported by csky abiv1 and abiv2 default.  */

#ifndef CSKYGDB_CONFIG_ABIV2     /* For abiv1 */
static char *csky_register_names_v1[] = {
  /* General register 0~15: 0 ~ 15.  */
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  /*
  0x0,   0x1,   0x2,   0x3,   0x4,   0x5,   0x6,   0x7,
  0x8,   0x9,   0xa,   0xb,   0xc,   0xd,   0xe,   0xf,
  */

  /* DSP hilo register: 79, 80.  */
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  /*
  -1,      -1,    -1,     -1,       79,      80,   -1,    -1,
  */

  /* FPU reigster: 24~55.  */
  "cp1gr0", "cp1gr1", "cp1gr2",  "cp1gr3",
  "cp1gr4", "cp1gr5", "cp1gr6",  "cp1gr7",
  "cp1gr8", "cp1gr9", "cp1gr10", "cp1gr11",
  "cp1gr12", "cp1gr13", "cp1gr14", "cp1gr15",
  "cp1gr16", "cp1gr17", "cp1gr18", "cp1gr19",
  "cp1gr20", "cp1gr21", "cp1gr22", "cp1gr23",
  "cp1gr24", "cp1gr25", "cp1gr26", "cp1gr27",
  "cp1gr28", "cp1gr29", "cp1gr30", "cp1gr31",
  /*
   0x100, 0x101, 0x102, 0x103,
   0x104, 0x105, 0x106, 0x107,
   0x108, 0x109, 0x10a, 0x10b,
   0x10c, 0x10d, 0x10e, 0x10f,
   0x110, 0x111, 0x112, 0x113,
   0x114, 0x115, 0x116, 0x117,
   0x118, 0x119, 0x11a, 0x11b,
   0x11c, 0x11d, 0x11e, 0x11f,
  */

  /* Hole.  */
  "",      "",    "",     "",     "",    "",   "",    "",
  "",      "",    "",     "",     "",    "",   "",    "",
  /*
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  */

/* ---- Above all must according to compiler for debug info ----  */

  /*
   "all",   "gr",   "ar",   "cr",   "",    "",    "",    "",
   "cp0",   "cp1",  "",     "",     "",    "",    "",    "",
   "",      "",     "",     "",     "",    "",    "",    "cp15",
  */

  /* PC : 64.  */
  "pc",
  /*
  64,
  */

  /* Optional register(ar) :  16 ~ 31.  */
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  /*
  0x10,  0x11,  0x12,  0x13,  0x14,  0x15,  0x16,  0x17,
  0x18,  0x19,  0x1a,  0x1b,  0x1c,  0x1d,  0x1e,  0x1f,
  */

  /* Control registers (cr_bank0) : 32 ~ 63.  */
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "",
  /*
  0x20,  0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,
  0x28,  0x29,  0x2a,  0x2b,  0x2c,  0x2d,  0x2e,  0x2f,
  0x30,  0x31,  0x32,  0x33,  0x34,  0x35,  0x36,  0x37,
  0x38,  0x39,  0x3a,  0x3b,  0x3c,  0x3d,  0x3e,  -1,
  */

  /* FPC control register: 0x100 & (32 ~ 38).  */
  "cp1cr0","cp1cr1","cp1cr2","cp1cr3","cp1cr4","cp1cr5","cp1cr6",
  /*
  0x120, 0x121, 0x122, 0x123, 0x124, 0x125, 0x126,
  */

  /* MMU control register: 0xf00 & (32 ~ 40).  */
  "cp15cr0", "cp15cr1", "cp15cr2", "cp15cr3",
  "cp15cr4", "cp15cr5", "cp15cr6", "cp15cr7",
  "cp15cr8", "cp15cr9", "cp15cr10","cp15cr11",
  "cp15cr12","cp15cr13", "cp15cr14","cp15cr15",
  "cp15cr16","cp15cr29", "cp15cr30","cp15cr31"
  /*
  0xf20,  0xf21,  0xf22,  0xf23,
  0xf24,  0xf25,  0xf26,  0xf27,
  0xf28,  0xf29,  0xf2a,  0xf2b,
  0xf2c,  0xf2d,  0xf2e,  0xf2f,
  0xf30,  0xf3d,  0xf3e,  0xf3f
  */
};
#endif /* not CSKYGDB_CONFIG_ABIV2 */

/* For csky abiv2.  */

static char *csky_register_names_v2[] = {
  /* General register 0 ~ 15: 0 ~ 15.  */
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  /*
  0x0,   0x1,   0x2,   0x3,   0x4,   0x5,   0x6,   0x7,
  0x8,   0x9,   0xa,   0xb,   0xc,   0xd,   0xe,   0xf,
  */

  /* General register 16 ~ 31: 96 ~ 111.  */
  "r16",   "r17",  "r18",   "r19",  "r20",  "r21",  "r22",  "r23",
  "r24",   "r25",  "r26",   "r27",  "r28",  "r29",  "r30",  "r31",
  /*
  0x60,    0x61,   0x62,    0x63,   0x64,   0x65,   0x66,   0x67,
  0x68,    0x69,   0x6a,    0x6b,   0x6c,   0x6d,   0x6e,   0x6f,
  */

  /* DSP hilo register: 36 ~ 37 : 79 ~ 80.  */
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  /*
  -1,      -1,    -1,     -1,       79,      80,   -1,    -1,
  */

  /* FPU/VPU general reigsters: 40~71 : 0x4200 ~ 0x422f.  */
  "fr0", "fr1",  "fr2",  "fr3",  "fr4",  "fr5",  "fr6",  "fr7",
  "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
  "vr0", "vr1",  "vr2",  "vr3",  "vr4",  "vr5",  "vr6",  "vr7",
  "vr8", "vr9", "vr10", "vr11", "vr12", "vr13", "vr14", "vr15",
  /*
  0x4220, 0x4221, 0x4222, 0x4223, 0x4224, 0x4225, 0x4226, 0x4227,
  0x4228, 0x4229, 0x422a, 0x422b, 0x422c, 0x422d, 0x422e, 0x422f,
  0x4200, 0x4201, 0x4202, 0x4203, 0x4204, 0x4205, 0x4206, 0x4207,
  0x4208, 0x4209, 0x420a, 0x420b, 0x420c, 0x420d, 0x420e, 0x420f,
  */
/* ---- Above all must according to compiler for debug info ----  */

  /*"all",   "gr",   "ar",   "cr",   "",    "",    "",    "",  */
  /*"cp0",   "cp1",  "",     "",     "",    "",    "",    "",  */
  /*"",      "",     "",     "",     "",    "",    "",    "cp15",  */

  /* pc : 72 : 64.  */
  "pc",
  /*
  64,
  */

  /* Optional register(ar) :  73~88 : 16 ~ 31.  */
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  /*
  0x10,  0x11,  0x12,  0x13,  0x14,  0x15,  0x16,  0x17,
  0x18,  0x19,  0x1a,  0x1b,  0x1c,  0x1d,  0x1e,  0x1f,
  */

  /* Control registers (cr) : 89 ~ 119 : 32 ~ 62.  */
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "cr31",
  /*
  0x20,  0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,
  0x28,  0x29,  0x2a,  0x2b,  0x2c,  0x2d,  0x2e,  0x2f,
  0x30,  0x31,  0x32,  0x33,  0x34,  0x35,  0x36,  0x37,
  0x38,  0x39,  0x3a,  0x3b,  0x3c,  0x3d,  0x3e,  0x3f,
  */

  /* FPU/VPU control register: 121 ~ 123 : 0x4210 ~ 0x4212.  */
  /* User sp : 127 : 0x410e.  */
  "fid",   "fcr",  "fesr",    "",    "",    "",    "usp",
  /*
  0x4210,  0x4211,  0x4212,   -1,    -1,    -1,    0x410e,
  */

  /* MMU control register: 128 ~ 136 : 0x4f00.  */
  "mcr0", "mcr2", "mcr3", "mcr4", "mcr6", "mcr8", "mcr29", "mcr30",
  "mcr31", "",     "",     "",
  /*
  0x4f00, 0x4f01, 0x4f02, 0x4f03, 0x4f04, 0x4f05, 0x4f06, 0x4f07,
  0x4f08,   -1,     -1,     -1,
  */

  /* Profiling software general registers: 140 ~ 153 : 0x6000 ~ 0x600d.  */
  /* Profiling control registers: 154 ~ 157 : 0x6030 ~ 0x6033.  */
  "profcr0",  "profcr1",  "profcr2",  "profcr3", "profsgr0", "profsgr1",
  "profsgr2", "profsgr3", "profsgr4", "profsgr5","profsgr6", "profsgr7",
  "profsgr8", "profsgr9", "profsgr10","profsgr11","profsgr12","profsgr13",
  "",         "",
  /*
  0x6030,     0x6031,     0x6032,     0x6033,     0x6000,     0x6001,
  0x6002,     0x6003,     0x6004,     0x6005,     0x6006,     0x6007,
  0x6008,     0x6009,     0x600a,     0x600b,     0x600c,     0x600d,
    -1,         -1,
  */

  /* Profiling architecture general registers: 160 ~ 174 : 0x6010 ~ 0x601e.  */
  "profagr0", "profagr1", "profagr2", "profagr3", "profagr4", "profagr5",
  "profagr6", "profagr7", "profagr8", "profagr9", "profagr10","profagr11",
  "profagr12","profagr13","profagr14", "",
  /*
  0x6010,     0x6011,     0x6012,     0x6013,     0x6014,     0x6015,
  0x6016,     0x6017,     0x6018,     0x6019,     0x601a,     0x601b,
  0x601c,     0x601d,     0x601e,      -1,
  */

  /* Profiling extension general registers: 176 ~ 188 : 0x6020 ~ 0x602c.  */
  "profxgr0", "profxgr1", "profxgr2", "profxgr3", "profxgr4", "profxgr5",
  "profxgr6", "profxgr7", "profxgr8", "profxgr9", "profxgr10","profxgr11",
  "profxgr12",
  /*
  0x6020,     0x6021,     0x6022,     0x6023,     0x6024,     0x6025,
  0x6026,     0x6027,     0x6028,     0x6029,     0x602a,     0x602b,
  0x602c,
  */

  /* Control reg in bank1.  */

  "",   "",   "",   "",   "",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   "",
  "cp1cr16",  "cp1cr17",  "cp1cr18",  "cp1cr19",  "cp1cr20",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   "",
  /*
  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  0x4110,    0x4111,    0x4112,    0x4113,   0x4114,  -1,  -1,  -1,
  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  */


  /* Control reg in bank3.  */

  "sepsr",   "sevbr",   "seepsr",   "",   "seepc",   "",   "nsssp",   "seusp",
  "sedcr",   "",   "",   "",   "",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   "",
  "",   "",   "",   "",   "",   "",   "",   ""
  /*
  0x4300,   0x4301,   0x4302,   -1,   0x4304,   -1,   0x4306,   0x4307,
  0x4308,-1,   -1,   -1,   -1,   -1,   -1,   -1,
  -1,    -1,   -1,   -1,   -1,   -1,   -1,   -1,
  -1,    -1,   -1,   -1,   -1,   -1,   -1,   -1
  */
};

/* Return the name of register according to REG_NR.  */

static const char *
csky_register_name (struct gdbarch *gdbarch, int reg_nr)
{
  int csky_total_regnum;

  if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
    return tdesc_register_name (gdbarch, reg_nr);

  if ((HW_ICE_MAIN_VER (hardware_version) > 3)
      || ((HW_ICE_MAIN_VER (hardware_version) == 3)
          && (HW_ICE_SUB_VER (hardware_version) >= 3)))
    csky_total_regnum = CSKY_NUM_REGS;
  else
    /* Version below 3.3 doesn't support cr_bank3.  */
    csky_total_regnum = (CSKY_NUM_REGS-CSKY_CRBANK_NUM_REGS);

  if (reg_nr < 0)
    {
      return NULL;
    }

  if (reg_nr >= csky_total_regnum)
    return NULL;
#ifndef CSKYGDB_CONFIG_ABIV2     /* FOR ABIV1  */
  if (csky_get_insn_version (gdbarch) == CSKY_INSN_V1)
    {
      /* csky_register_names_v1[] has only 148 elements.  */
      if (reg_nr < 148)
        return csky_register_names_v1[reg_nr];
      return NULL;
    }
  else
    {
      return csky_register_names_v2[reg_nr];
    }
#else
  return csky_register_names_v2[reg_nr];
#endif
}

#ifdef CSKYGDB_CONFIG_ABIV2    /* FOR ABIV2  */
/* Construct vector type for vrx registers.  */

static struct type *
csky_vector_type (struct gdbarch *gdbarch)
{
  const struct builtin_type *bt = builtin_type (gdbarch);

  struct type *t;

  t = arch_composite_type (gdbarch, "__gdb_builtin_type_vec128i",
                           TYPE_CODE_UNION);

  append_composite_type_field (t, "u32",
                               init_vector_type (bt->builtin_int32, 4));
  append_composite_type_field (t, "u16",
                               init_vector_type (bt->builtin_int16, 8));
  append_composite_type_field (t, "u8",
                               init_vector_type (bt->builtin_int8, 16));

  TYPE_VECTOR (t) = 1;
  TYPE_NAME (t) = "builtin_type_vec128i";

  return t;
}
#endif /* CSKYGDB_CONFIG_ABIV2 */

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

static struct type *
csky_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  int num_regs = gdbarch_num_regs (gdbarch);
  int num_pseudo_regs = gdbarch_num_pseudo_regs (gdbarch);

  /* PC, EPC, FPC is text point.  */
  if ((reg_nr == CSKY_PC_REGNUM)  || (reg_nr == CSKY_EPC_REGNUM)
      || (reg_nr == CSKY_FPC_REGNUM))
    {
      return builtin_type (gdbarch)->builtin_func_ptr;
    }
  /* VBR is data point.  */
  if (reg_nr == CSKY_VBR_REGNUM)
    {
      return builtin_type (gdbarch)->builtin_data_ptr;
    }
#ifdef CSKYGDB_CONFIG_ABIV2    /* FOR ABIV2 */
  /* Float register has 64bits, and only in ck810 (ABIV2).  */
  if ((reg_nr >=CSKY_FR0_REGNUMV2 ) && (reg_nr <= CSKY_FR0_REGNUMV2 + 15))
    {
      return arch_float_type (gdbarch, 64, "builtin_type_csky_ext",
                              floatformats_ieee_double);
    }
  /* Vector register has 128bits, and only in ck810 (ABIv2).  */
  if ((reg_nr >= CSKY_VR0_REGNUM) && (reg_nr <= CSKY_VR0_REGNUM + 15))
    {
      return csky_vector_type (gdbarch);
    }

  /* Profiling general register has 48bits, we use 64bit.  */
  if ((reg_nr >= CSKY_PROFGR_REGNUM) && (reg_nr <= CSKY_PROFGR_REGNUM + 44))
    {
      return builtin_type (gdbarch)->builtin_uint64;
    }
#endif

  if (reg_nr == CSKY_SP_REGNUM)
    {
      return builtin_type (gdbarch)->builtin_data_ptr;
    }
  /* Others are 32bits.  */
  return builtin_type (gdbarch)->builtin_int32;

}

/* In case of call fun in GDB,
   When arguments must be pushed onto the stack,
   They go on in reverse order.
   The code below implements a FILO (stack) to do this.  */

struct stack_item
{
  int len;
  struct stack_item *prev;
  void *data;
};

static struct stack_item *
push_stack_item (struct stack_item *prev, const void *contents, int len)
{
  struct stack_item *si;
  si = (struct stack_item *) xmalloc (sizeof (struct stack_item));
  si->data = xmalloc (len);
  si->len = len;
  si->prev = prev;
  memcpy (si->data, contents, len);
  return si;
}

static struct stack_item *
pop_stack_item (struct stack_item *si)
{
  struct stack_item *dead = si;
  si = si->prev;
  xfree (dead->data);
  xfree (dead);
  return si;
}

#ifndef CSKYGDB_CONFIG_ABIV2   /* FOR ABIV1 */
/* Return the alignment (in bytes) of the given type.  */

static int
csky_type_allign (struct type *t)
{
  int n;
  int allign;
  int fallign;

  t = check_typedef (t);
  switch (TYPE_CODE (t))
    {
    case TYPE_CODE_PTR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
    case TYPE_CODE_SET:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_REF:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
      return TYPE_LENGTH (t);

    case TYPE_CODE_ARRAY:
    case TYPE_CODE_COMPLEX:
      /* TODO: What about vector types?  */
      return csky_type_allign (TYPE_TARGET_TYPE (t));

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      allign = 1;
      for (n = 0; n < TYPE_NFIELDS (t); n++)
        {
          fallign = csky_type_allign (TYPE_FIELD_TYPE (t, n));
          if (fallign > allign)
            allign = fallign;
        }
      return allign;
    default:
      /* Should never happen.  */
      return 4;
    }
}
#endif /* not CSKYGDB_CONFIG_ABIV2 */

/* We currently only support passing parameters in integer registers, which
   conforms with GCC's default model.
   In ABIV1, sp need 8 allign and passing parameters also need 8 allign. At
   this case, parameter will either in register(s) or in stack.
   In ABIV2, there is no need 8 allign in both stack and passing parameters.
   At this case, parameter may be part in register and other in stack.  */

static CORE_ADDR
csky_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
                      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
                      struct value **args, CORE_ADDR sp, int struct_return,
                      CORE_ADDR struct_addr)
{
  int argnum;
  int argreg = CSKY_ABI_A0_REGNUM;
  int last_arg_regnum = CSKY_ABI_LAST_ARG_REGNUM;
  int need_dummy_stack = 0;
  struct stack_item *si = NULL;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);


  /* Set the return address.  For CSKY, the return breakpoint is
     always at BP_ADDR.  */
  regcache_cooked_write_unsigned (regcache, CSKY_LR_REGNUM, bp_addr);
  /* The struct_return pointer occupies the first parameter
     passing register. */
  if (struct_return)
    {
      ckcore_insn_debug (("CKCORE:struct return in %s = %s\n",
                         gdbarch_register_name (gdbarch, argreg),
                         paddress (gdbarch, struct_addr)));
      regcache_cooked_write_unsigned (regcache, argreg, struct_addr);
      argreg++;
    }
  /* Put parameters into argument register of REGCACHE.
     In ABIV1 argument registers are r2 ~ r7,
     In ABIV2 argument registers are r0 ~ r3.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      struct type * arg_type;
      enum type_code typecode;
      const bfd_byte * val;
      int allign;

      arg_type = check_typedef (value_type (args[argnum]));
      len = TYPE_LENGTH (arg_type);
      typecode = TYPE_CODE (arg_type);
      val = value_contents (args[argnum]);

#ifndef CSKYGDB_CONFIG_ABIV2   /* FOR ABIV1 */
      allign = csky_type_allign (arg_type);

      /* 8 alligned quantities must go in even register pairs. */
      if ((argreg <= last_arg_regnum)
          && (allign > 4)
          && (argreg & 1))
        argreg++;
#endif  /* not CSKYGDB_CONFIG_ABIV2 */

    /* Copy the argument to argument registers or the dummy stack.
       Large arguments are split between registers and stack.  */

    /* If len<4,no need to care the endian, the args always stored
       in low address.  */
    if (len < 4)
      {
        CORE_ADDR regval =
          extract_unsigned_integer (val, len, byte_order);
        regcache_cooked_write_unsigned (regcache, argreg, regval);
        argreg++;
      }
    else
      {
        while (len > 0)
          {
            int partial_len = len < 4 ? len : 4;
            if (argreg <= last_arg_regnum)
              {
                /* The argument is passed in argument register.  */
                CORE_ADDR regval =
                   extract_unsigned_integer (val, partial_len, byte_order);
                if (byte_order == BFD_ENDIAN_BIG )
                  {
                    regval <<= (4 - partial_len) * 8;
                  }
                /* Put regval into register of the REGCACHE.  */
                regcache_cooked_write_unsigned (regcache, argreg, regval);
                argreg++;
              }
            else
              {
                /* The argument should be pushed onto the dummy stack.  */
                si = push_stack_item (si, val, 4);
                need_dummy_stack += 4;
              }
        len -= partial_len;
        val += partial_len;
      }
    }
  }

  /* After parameter preparation, we should adjust sp according to
     NEED_DUMMY_STACK and sturct stack_item *SI.  */
#ifndef CSKYGDB_CONFIG_ABIV2       /* FOR ABIV1 */
  /* If we have an odd number of words to push, then decrement the stack
     by one word now, so first stack argument will be 8 alligned.  */
  if (need_dummy_stack & 4)
    sp -= 4;
#endif  /* not CSKYGDB_CONFIG_ABIV2 */

  while (si)
    {
      sp -= si->len;
      write_memory (sp, (const bfd_byte *)si->data, si->len);
      si = pop_stack_item (si);
    }

  /* Finally, update the SP register.  */
  regcache_cooked_write_unsigned (regcache, CSKY_SP_REGNUM, sp);
  return sp;
}

static enum return_value_convention
csky_return_value (struct gdbarch *gdbarch, struct value *function,
                   struct type *valtype, struct regcache *regcache,
                   gdb_byte *readbuf, const gdb_byte *writebuf)
{
  CORE_ADDR regval;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  int len = TYPE_LENGTH (valtype);
  unsigned int ret_regnum = CSKY_RET_REGNUM;

  if (len > 8)
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    {
      if (readbuf != NULL)
        {
          ULONGEST tmp;
          /* By using store_unsigned_integer we avoid having to do
             anything special for small big-endian values.  */
          regcache_cooked_read_unsigned (regcache, ret_regnum, &tmp);
          store_unsigned_integer (readbuf, (len > 4 ? 4 : len),
                                  byte_order, tmp);
          /* Ignore return values more than 8 bytes in size because the csky
             returns anything more than 8 bytes in the stack.  */
          if (len > 4)
            {
              regcache_cooked_read_unsigned (regcache, ret_regnum + 1, &tmp);
              store_unsigned_integer (readbuf + 4,  4, byte_order, tmp);
            }

        }
      if (writebuf != NULL)
        {
          regval = extract_unsigned_integer (writebuf, len > 4 ? 4 : len,
                                             byte_order);
          regcache_cooked_write_unsigned (regcache, ret_regnum, regval);
          if (len > 4)
            {
              regval = extract_unsigned_integer ((gdb_byte *) writebuf + 4,
                                                 4, byte_order);
              regcache_cooked_write_unsigned (regcache, ret_regnum + 1, regval);
            }

        }
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}

/* Adjust the address downward (direction of stack growth) so that it
   is correctly aligned for a new stack frame.
   In ABIV1, frame need 8 allign.
   In ABIV2, there is no 8 allign need in frame.  */

static CORE_ADDR
csky_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
#ifdef CSKYGDB_CONFIG_ABIV2
  return align_down (addr, 4);
#else    /* not CSKYGDB_CONFIG_ABIV2 */
  return align_down (addr, 8);
#endif   /* not CSKYGDB_CONFIG_ABIV2 */
}

struct csky_unwind_cache
{
  /* The stack pointer at the time this frame was created; i.e. the
     caller's stack pointer when this function was called.  It is used
     to identify this frame.  */
  CORE_ADDR prev_sp;

  /* The frame base for this frame is just prev_sp - frame size.
     FRAMESIZE is the distance from the frame pointer to the
     initial stack pointer.  */

  int framesize;

  /* The register used to hold the frame pointer for this frame.  */
  int framereg;

  /* Saved register offsets.  */
  struct trad_frame_saved_reg *saved_regs;
};

#ifndef CSKYGDB_CONFIG_ABIV2
/* FOR ABIV1 whichs includes insn_v1 and insn_v2p.  */
/* Analyze the function prologue from START_PC to LIMIT_PC. Builds
   the associated FRAME_CACHE if not null.
   Return the address of the first instruction past the prologue.  */

static CORE_ADDR
csky_analyze_prologue_v1 (struct gdbarch *gdbarch,
                          CORE_ADDR start_pc,
                          CORE_ADDR limit_pc,
                          CORE_ADDR end_pc,
                          struct frame_info *this_frame,
                          struct csky_unwind_cache *this_cache)
{
  CORE_ADDR addr;
  CORE_ADDR sp;
  CORE_ADDR stack_size;
  unsigned int insn, rn;
  int status;
  int flags;
  int framesize;

#define CSKY_NUM_GREGS_v1  16
#define CSKY_NUM_GREGS_SAVED_v1 (16+4)
  /* 16 general registers + EPSR EPC, FPSR, FPC.  */
  int register_offsets[CSKY_NUM_GREGS_SAVED_v1];

  struct gdbarch_tdep * tdep = gdbarch_tdep (gdbarch);
  int fp_regnum = 0; /* Dummy, valid when (flags & MY_FRAME_IN_FP).  */

  /* When build programe with -fno-omit-frame-pointer, there must be
     mov r8, r0
     in prologue, here, we check such insn, once hit, we set IS_FP_SAVED.  */
  int is_fp_saved = 0;


  /* REGISTER_OFFSETS will contain offsets, from the top of the frame
     (NOT the frame pointer), for the various saved registers or -1
     if the register is not saved.  */
  for (rn = 0; rn < CSKY_NUM_GREGS_SAVED_v1; rn++)
    register_offsets[rn] = -1;

  /* Analyze the prologue. Things we determine from analyzing the
     prologue include:
     * the size of the frame
     * where saved registers are located (and which are saved)
     * FP used?  */
  ckcore_insn_debug (("CKCORE: Scanning prologue: start_pc = 0x%x,"
                      " limit_pc = 0x%x\n", (unsigned int) start_pc,
                      (unsigned int) limit_pc));

  framesize = 0;
  for (addr = start_pc; addr < limit_pc; addr += 2)
    {
      /* Get next insn.  */
      csky_get_insn (gdbarch, addr, &insn);

      if (V1_IS_SUBI0 (insn))
        {
          int offset = 1 + ((insn >> 4) & 0x1f);
          ckcore_insn_debug (("CKCORE: got subi r0,%d; continuing\n", offset));
          if (!is_fp_saved)
            framesize += offset;
          continue;
        }
      else if (V1_IS_STM (insn))
        {
          /* Spill register(s).  */
          int offset;
          int start_register;

          /* BIG WARNING! The CKCore ABI does not restrict functions
             to taking only one stack allocation. Therefore, when
             we save a register, we record the offset of where it was
             saved relative to the current framesize. This will
             then give an offset from the SP upon entry to our
             function. Remember, framesize is NOT constant until
             we're done scanning the prologue.  */
          start_register = (insn & 0xf);
          ckcore_insn_debug (("CKCORE: got stm r%d-r15,(r0)\n",
                              start_register));

          for (rn = start_register, offset = 0; rn <= 15; rn++, offset += 4)
            {
              register_offsets[rn] = framesize - offset;
              ckcore_insn_debug (("CKCORE: r%d saved at 0x%x (offset %d)\n",
                                  rn, register_offsets[rn], offset));
            }
          ckcore_insn_debug (("CKCORE: continuing\n"));
          continue;
        }
      else if (V1_IS_STWx0 (insn))
        {
          /* Spill register: see note for IS_STM above.  */
          int imm;

          rn = (insn >> 8) & 0xf;
          imm = (insn >> 4) & 0xf;
          register_offsets[rn] = framesize - (imm << 2);
          ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                              rn, register_offsets[rn]));
          ckcore_insn_debug (("CKCORE: continuing\n"));
          continue;
        }
      else if (V1_IS_MOV_FP_SP (insn))
        {
          /* FP have saved, so we skip this insn, and go on prologue.  */
          is_fp_saved = 1;
          continue;
        }
      else if (V1_IS_BSR_NEXT (insn))
        {
          /*
           * We skip the following three insn in elf or so built with fPIC
           * We call it "fPIC insns"
           *            bsr label     --------- insn1
           * label :    lrw r14, imm  --------- insn2
           *            addu r14, r15 --------- insn3
           *            ld   r15, (r0, imm)   ---------insn4 (-O0)
           */
          csky_get_insn (gdbarch, addr + 2, &insn);
          if (!(V1_IS_LRW_R14 (insn)))
            {
              /* Not fPIC insns.  */
              break;
            }
          csky_get_insn (gdbarch, addr + 4, &insn);
          if (!(V1_IS_ADDU_R14_R15 (insn)))
            {
              /* Not fPIC insn.  */
              break;
            }
          /* Yes! It is fPIC insn, We continue to prologue.  */
          addr += 4; /* pc -> addr r14, r15.  */
          csky_get_insn (gdbarch, addr + 2, &insn);
          /* When compile without optimization.  */
          if (V1_IS_LD_R15 (insn))
            {
              addr += 2;
            }
          continue;
        }
      else if (V1_IS_MFCR_EPSR (insn))
        {
          unsigned int insn2;
          int stw_regnum;
          int mfcr_regnum = insn & 0xf;
          addr += 2;
          csky_get_insn (gdbarch, addr, &insn2);
          stw_regnum = (insn2 >> 8) & 0xf;
          if (V1_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
            {
              int imm;

              rn = CSKY_NUM_GREGS_v1 ; /* CSKY_EPSR_REGNUM.  */
              imm = (insn2 >> 4) & 0xf;
              register_offsets[rn] = framesize - (imm << 2);
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                 rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          break;
        }
      else if (V1_IS_MFCR_FPSR (insn))
        {
          unsigned int insn2;
          int stw_regnum;
          int mfcr_regnum = insn & 0xf;
          addr += 2;
          csky_get_insn (gdbarch, addr, &insn2);
          stw_regnum = (insn2 >> 8) & 0xf;
          if (V1_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
            {
              int imm;

              rn = CSKY_NUM_GREGS_v1 + 1; /* CSKY_FPSR_REGNUM  */
              imm = (insn2 >> 4) & 0xf;
              register_offsets[rn] = framesize - (imm << 2);
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                 rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          break;
        }
      else if (V1_IS_MFCR_EPC (insn) )
        {
          unsigned int insn2;
          int stw_regnum;
          int mfcr_regnum = insn & 0xf;
          addr += 2;
          csky_get_insn (gdbarch, addr, &insn2);
          stw_regnum = (insn2 >> 8) & 0xf;
          if (V1_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
            {
              int imm;

              rn = CSKY_NUM_GREGS_v1 + 2; /* CSKY_EPC_REGNUM.  */
              imm = (insn2 >> 4) & 0xf;
              register_offsets[rn] = framesize - (imm << 2);
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                  rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          break;
        }
      else if (V1_IS_MFCR_FPC (insn) )
        {
          unsigned int insn2;
          int stw_regnum;
          int mfcr_regnum = insn & 0xf;
          addr += 2;
          csky_get_insn (gdbarch, addr, &insn2);
          stw_regnum = (insn2 >> 8) & 0xf;
          if (V1_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
            {
              int imm;

              rn = CSKY_NUM_GREGS_v1 + 3; /* CSKY_FPC_REGNUM.  */
              imm = (insn2 >> 4) & 0xf;
              register_offsets[rn] = framesize - (imm << 2);
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                 rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          break;
        }
      /* Begin analyze adjust frame by r1.  */
      else if (V1_IS_LRW1 (insn) || V1_IS_MOVI1 (insn)
               || V1_IS_BGENI1 (insn) || V1_IS_BMASKI1 (insn))
        {
          int adjust = 0;
          int offset = 0;
          unsigned int insn2;

          ckcore_insn_debug (("CKCORE: looking at large frame\n"));
          if (V1_IS_LRW1 (insn))
            {
              enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
              int literal_addr = (addr + 2 + ((insn & 0xff) << 2)) & 0xfffffffc;
              adjust = read_memory_unsigned_integer (literal_addr, 4,
                                                     byte_order);
            }
          else if (V1_IS_MOVI1 (insn))
            adjust = (insn >> 4) & 0x7f;
          else if (V1_IS_BGENI1 (insn))
            adjust = 1 << ((insn >> 4) & 0x1f);
          else  /* IS_BMASKI (insn).  */
            adjust = (1 << ((insn >> 4) & 0x1f)) - 1;

          ckcore_insn_debug (("CKCORE: base framesize=0x%x\n", adjust));

          /* May have zero or more insns which modify r1.  */
          ckcore_insn_debug (("CKCORE: looking for r1 adjusters...\n"));
          offset = 2;

          /* If out of prologue range, we should exit right now.  */
          if ((addr + offset) < limit_pc)
            csky_get_insn (gdbarch, addr + offset, &insn2);
          else
            break;

          while (V1_IS_R1_ADJUSTER (insn2) && ((addr + offset) < limit_pc))
            {
              int imm;

              imm = (insn2 >> 4) & 0x1f;
              if (V1_IS_ADDI1 (insn2))
                {
                  adjust += (imm + 1);
                  ckcore_insn_debug (("CKCORE: addi r1,%d\n", imm + 1));
                }
              else if (V1_IS_SUBI1 (insn2))
                {
                  adjust -= (imm + 1);
                  ckcore_insn_debug (("CKCORE: subi r1,%d\n", imm + 1));
                }
              else if (V1_IS_RSUBI1 (insn2))
                {
                  adjust = imm - adjust;
                  ckcore_insn_debug (("CKCORE: rsubi r1,%d\n", imm + 1));
                }
              else if (V1_IS_NOT1 (insn2))
                {
                  adjust = ~adjust;
                  ckcore_insn_debug (("CKCORE: not r1\n"));
                }
              else if (V1_IS_ROTLI1 (insn2))
                {
                  int temp = adjust >> (16 - imm);
                  adjust <<= imm;
                  adjust |= temp;
                  ckcore_insn_debug (("CKCORE: rotli r1,%d\n", imm + 1));
                }
              else if ( V1_IS_LSLI1 (insn2))
                {
                  adjust <<= imm;
                  ckcore_insn_debug (("CKCORE: lsli r1,%d\n", imm));
                }
              else if (V1_IS_BSETI1 (insn2))
                {
                  adjust |= (1 << imm);
                  ckcore_insn_debug (("CKCORE: bseti r1,%d\n", imm));
                }
              else if (V1_IS_BCLRI1 (insn2))
                {
                  adjust &= ~(1 << imm);
                  ckcore_insn_debug (("CKCORE: bclri r1,%d\n", imm));
                }
              else if (V1_IS_IXH1 (insn2))
                {
                  adjust *= 3;
                  ckcore_insn_debug (("CKCORE: ix.h r1,r1\n"));
                }
              else if (V1_IS_IXW1 (insn2))
                {
                  adjust *= 5;
                  ckcore_insn_debug (("CKCORE: ix.w r1,r1\n"));
                }
              else if (V1_IS_STWSP(insn2))
                {
                  /* Junc insn, ignore it.  */
                  offset += 2;
                  csky_get_insn (gdbarch, addr + offset, &insn2);
                  continue;
                }
              else if (V1_IS_SUB01 (insn2))
                {
                  if (!is_fp_saved)
                    framesize += adjust;
                }
              else if (V1_IS_MOVI1 (insn2))
                {
                  adjust = (insn2 >> 4) & 0x7f;
                }

              offset += 2;
              csky_get_insn (gdbarch, addr + offset, &insn2);
            };

          ckcore_insn_debug (("CKCORE: done looking for r1 adjusters\n"));

          /* If the next insn adjusts the stack pointer, we keep everything;
             if not, we scrap it and we've found the end of the prologue.  */
          if (V1_IS_MOV_FP_SP (insn2) && (addr+offset) < limit_pc)
            {
              /* Do not forget to skip this insn.  */
              is_fp_saved = 1;
            }

          /* None of these instructions are prologue, so don't touch
             anything.  */
          ckcore_insn_debug (("CKCORE: no subu r1,r0, NOT altering"
                              " framesize.\n"));
          addr += (offset - 2);
          continue;
        }


      /* This is not a prologue insn, so stop here.  */
      ckcore_insn_debug (("CKCORE: insn is not a prologue insn"
                          " -- ending scan\n"));
      break;

    }

  if (this_cache)
    {
      CORE_ADDR unwound_fp;
      enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
      this_cache->framesize = framesize;
      this_cache->framereg = is_fp_saved ? CSKY_FP_REGNUM : CSKY_SP_REGNUM;

      unwound_fp = get_frame_register_unsigned (this_frame,
                                                this_cache->framereg);
      this_cache->prev_sp = unwound_fp + framesize;

      /* Note where saved registers are stored. The offsets in
         REGISTER_OFFSETS are computed relative to the top of the frame.  */
      for (rn = 0; rn < CSKY_NUM_GREGS_v1; rn++)
        {
          CORE_ADDR rn_value;
          if (register_offsets[rn] >= 0)
            {
              this_cache->saved_regs[rn].addr = this_cache->prev_sp
                                                - register_offsets[rn];
              rn_value = read_memory_unsigned_integer
                            (this_cache->saved_regs[rn].addr,
                             4, byte_order);
              ckcore_insn_debug (("Saved register %s stored at 0x%08x,"
                                  " value=0x%08x\n",
                                  csky_register_names_v1[rn],
                                  this_cache->saved_regs[rn].addr,
                                  rn_value));
            }
        }

        if (tdep->lr_type_p == 1)
          {
            this_cache->saved_regs[CSKY_PC_REGNUM] =
              this_cache->saved_regs[CSKY_EPC_REGNUM];
          }
        else if (tdep->lr_type_p == 2)
          {
            this_cache->saved_regs[CSKY_PC_REGNUM] =
              this_cache->saved_regs[CSKY_FPC_REGNUM];
          }
        else
          {
            this_cache->saved_regs[CSKY_PC_REGNUM] =
              this_cache->saved_regs[CSKY_LR_REGNUM];
          }
    }

  return addr;
}

static CORE_ADDR
csky_analyze_prologue_v2p (struct gdbarch *gdbarch,
                           CORE_ADDR start_pc,
                           CORE_ADDR limit_pc,
                           CORE_ADDR end_pc,
                           struct frame_info *this_frame,
                           struct csky_unwind_cache *this_cache)
{
  CORE_ADDR addr;
  CORE_ADDR sp;
  CORE_ADDR stack_size;
  unsigned int insn, rn;
  int status;
  int flags;
  int framesize;
  int insn_len;
#define CSKY_NUM_GREGS_v2P  32
  int register_offsets[CSKY_NUM_GREGS_v2P];   /* 32? general registers.  */
  /* Dummy, valid when (flags & MY_FRAME_IN_FP).  */
  int fp_regnum = CSKY_SP_REGNUM;

  /* When build programe with -fno-omit-frame-pointer, there must be
     mov r8, r0
     in prologue, here, we check such insn, once hit, we set IS_FP_SAVED.  */
  int is_fp_saved = 0;

  /* REGISTER_OFFSETS will contain offsets, from the top of the frame
     (NOT the frame pointer), for the various saved registers or -1
     if the register is not saved.  */
  for (rn = 0; rn < CSKY_NUM_GREGS_v2P; rn++)
    register_offsets[rn] = -1;

  /* Analyze the prologue. Things we determine from analyzing the
     prologue include:
     * the size of the frame
     * where saved registers are located (and which are saved)
     * FP used ?  */
  ckcore_insn_debug (("CKCORE: Scanning prologue: start_pc = 0x%x,\
 limit_pc = 0x%x\n", (unsigned int) start_pc, (unsigned int) limit_pc));

  framesize = 0;
  insn_len = 2;
  for (addr = start_pc; addr < limit_pc; addr += insn_len)
    {
      /* Get next insn.  */
      insn_len = csky_get_insn (gdbarch, addr, &insn);

      if(insn_len == 4)
        {
          if (V2P_32_IS_SUBI0 (insn))
            {
              int offset = V2_32_SUBI_IMM (insn);
              ckcore_insn_debug (("CKCORE: got subi r0,%d; continuing\n",
                                 offset));
              if (!is_fp_saved)
                framesize += offset;
              continue;
            }
          else if (V2P_32_IS_STMx0 (insn))
            {
              /* Spill register(s).  */
              int start_register;
              int reg_count;
              int offset;

              /* BIG WARNING! The CKCore ABI does not restrict functions
                 to taking only one stack allocation. Therefore, when
                 we save a register, we record the offset of where it was
                 saved relative to the current framesize. This will
                 then give an offset from the SP upon entry to our
                 function. Remember, framesize is NOT constant until
                 we're done scanning the prologue.  */
              start_register = V2P_32_STM_VAL_REGNUM (insn);
              reg_count = V2P_32_STM_SIZE (insn);
              ckcore_insn_debug (("CKCORE: got stm r%d-r%d,(r0)\n",
                                  start_register, start_register + reg_count));

              for (rn = start_register, offset = 0;
                   rn <= start_register + reg_count;
                   rn++, offset += 4)
                {
                  register_offsets[rn] = framesize - offset;
                  ckcore_insn_debug (("CKCORE: r%d saved at 0x%x (offset %d)\n",
                                     rn, register_offsets[rn], offset));
                }
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          else if (V2P_32_IS_MOV_FP_SP (insn))
            {
              /* Do not forget to skip this insn.  */
              is_fp_saved = 1;
              continue;
            }
          else if (V2P_32_IS_STWx0 (insn))
            {
              /* Spill register: see note for IS_STM above.  */
              int imm;

              rn = V2P_32_ST_VAL_REGNUM (insn);
              imm = V2P_32_ST_OFFSET (insn);
              register_offsets[rn] = framesize - (imm << 2);
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                  rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
        }
        else
        {
          if (V2P_16_IS_SUBI0 (insn))
            {
              int offset = V2P_16_SUBI_IMM (insn);
              ckcore_insn_debug (("CKCORE: got subi r0,%d; continuing\n",
                                 offset));
              if (!is_fp_saved)
                framesize += offset;
              continue;
            }
          else if (V2P_16_IS_STMx0 (insn))
            {
              /* Spill register(s).  */
              int start_register;
              int reg_count;
              int offset;

              /* BIG WARNING! The CKCore ABI does not restrict functions
                 to taking only one stack allocation. Therefore, when
                 we save a register, we record the offset of where it was
                 saved relative to the current framesize. This will
                 then give an offset from the SP upon entry to our
                 function. Remember, framesize is NOT constant until
                 we're done scanning the prologue.  */
              start_register = V2P_16_STM_VAL_REGNUM (insn);
              reg_count = V2P_16_STM_SIZE (insn);
              ckcore_insn_debug (("CKCORE: got stm r%d-r%d,(r0)\n",
                                  start_register, start_register + reg_count));

              for (rn = start_register, offset = 0;
                   rn <= start_register + reg_count;
                   rn++, offset += 4)
                {
                  register_offsets[rn] = framesize - offset;
                  ckcore_insn_debug (("CKCORE: r%d saved at 0x%x"
                                      " (offset %d)\n",
                                     rn, register_offsets[rn], offset));
                }
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          else if (V2P_16_IS_MOV_FP_SP (insn))
            {
              /* Do not forget to skip this insn.  */
              is_fp_saved = 1;
              continue;
            }
          else if (V2P_16_IS_STWx0 (insn))
            {
              /* Spill register: see note for IS_STM above.  */
              int imm;

              rn = V2P_16_ST_VAL_REGNUM (insn);
              imm = V2P_16_ST_OFFSET (insn);
              register_offsets[rn] = framesize - (imm << 2);
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                  rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
        }

      /* This is not a prologue insn, so stop here.  */
      ckcore_insn_debug (("CKCORE: insn is not a prologue insn"
                          " -- ending scan\n"));
      break;

    }

  if (this_cache)
    {
      enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
      CORE_ADDR unwound_fp;
      this_cache->framesize = framesize;
      this_cache->framereg = is_fp_saved ? CSKY_FP_REGNUM : CSKY_SP_REGNUM;

      unwound_fp = get_frame_register_unsigned (this_frame,
                                                this_cache->framereg);
      this_cache->prev_sp = unwound_fp + framesize;

      /* Note where saved registers are stored. The offsets in
         REGISTER_OFFSETS are computed relative to the top of the frame.  */
      for (rn = 0; rn < CSKY_NUM_GREGS_v2P; rn++)
        {
          CORE_ADDR rn_value;
          if (register_offsets[rn] >= 0)
            {
              this_cache->saved_regs[rn].addr = this_cache->prev_sp
                                                - register_offsets[rn];
              rn_value =  read_memory_unsigned_integer
                            (this_cache->saved_regs[rn].addr,
                            4, byte_order);
              ckcore_insn_debug (("Saved register %s stored at 0x%08x,"
                                  " value=0x%08x\n",
                                  csky_register_names_v2[rn],
                                  this_cache->saved_regs[rn].addr,
                                  rn_value));
            }
        }

      this_cache->saved_regs[CSKY_PC_REGNUM] =
        this_cache->saved_regs[CSKY_LR_REGNUM];

    }

  return addr;
}
#else /* CSKYGDB_CONFIG_ABIV2 */

static CORE_ADDR
csky_analyze_prologue_v2 (struct gdbarch *gdbarch,
                          CORE_ADDR start_pc,
                          CORE_ADDR limit_pc,
                          CORE_ADDR end_pc,
                          struct frame_info *this_frame,
                          struct csky_unwind_cache *this_cache)
{
  CORE_ADDR addr;
  CORE_ADDR sp;
  CORE_ADDR stack_size;
  unsigned int insn, rn;
  int status;
  int flags;
  int framesize;
  int stacksize;
#define CSKY_NUM_GREGS_v2  32
#define CSKY_NUM_GREGS_v2_SAVED_GREGS   (CSKY_NUM_GREGS_v2+4)
  /* 32 general regs + 4.  */
  int register_offsets[CSKY_NUM_GREGS_v2_SAVED_GREGS];
  int insn_len;
  /* For adjust fp.  */
  int is_fp_saved = 0;
  int adjust_fp = 0;
  /* Dummy, valid when (flags & MY_FRAME_IN_FP).  */
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* REGISTER_OFFSETS will contain offsets, from the top of the frame
     (NOT the frame pointer), for the various saved registers or -1
     if the register is not saved.  */
  for (rn = 0; rn < CSKY_NUM_GREGS_v2_SAVED_GREGS; rn++)
    register_offsets[rn] = -1;

  /* Analyze the prologue. Things we determine from analyzing the
     prologue include:
     * the size of the frame
     * where saved registers are located (and which are saved)
     * FP used ?  */
  ckcore_insn_debug (("CKCORE: Scanning prologue: start_pc = 0x%x,\
 limit_pc = 0x%x\n", (unsigned int) start_pc, (unsigned int) limit_pc));

  stacksize = 0;
  insn_len = 2; /* Instruction is 16bit.  */
  for (addr = start_pc; addr < limit_pc; addr += insn_len)
    {
      /* Get next insn.  */
      insn_len = csky_get_insn (gdbarch, addr, &insn);

      if (insn_len == 4) /* If 32bit.  */
        {
          if (V2_32_IS_SUBI0 (insn)) /* subi32 sp,sp oimm12.  */
            {
              int offset = V2_32_SUBI_IMM (insn); /* Got oimm12.  */
              ckcore_insn_debug (("CKCORE: got subi sp,%d; continuing\n",
                                 offset));
              stacksize += offset;
              continue;
            }
          else if (V2_32_IS_STMx0 (insn)) /* stm32 ry-rz,(sp).  */
            {
              /* Spill register(s).  */
              int start_register;
              int reg_count;
              int offset;

              /* BIG WARNING! The CKCore ABI does not restrict functions
                 to taking only one stack allocation. Therefore, when
                 we save a register, we record the offset of where it was
                 saved relative to the current stacksize. This will
                 then give an offset from the SP upon entry to our
                 function. Remember, stacksize is NOT constant until
                 we're done scanning the prologue.   */
              start_register = V2_32_STM_VAL_REGNUM (insn);
              reg_count = V2_32_STM_SIZE (insn);
              ckcore_insn_debug (("CKCORE: got stm r%d-r%d,(sp)\n",
                                  start_register,
                                  start_register + reg_count));

              for (rn = start_register, offset = 0;
                   rn <= start_register + reg_count;
                   rn++, offset += 4)
                {
                  register_offsets[rn] = stacksize - offset;
                  ckcore_insn_debug (("CKCORE: r%d saved at 0x%x"
                                      " (offset %d)\n",
                                      rn, register_offsets[rn], offset));
                }
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          else if (V2_32_IS_STWx0 (insn))  /* stw ry,(sp,disp).  */
            {
              /* Spill register: see note for IS_STM above.  */
              int disp;

              rn = V2_32_ST_VAL_REGNUM (insn);
              disp = V2_32_ST_OFFSET (insn);
              register_offsets[rn] = stacksize - disp;
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                 rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          else if (V2_32_IS_MOV_FP_SP (insn))
            {
              /* SP is saved to FP reg, means code afer prologue may
                 modify SP.  */
              is_fp_saved = 1;
              adjust_fp = stacksize;
              continue;
            }
          else if (V2_32_IS_MFCR_EPSR (insn))
            {
              unsigned int insn2;
              addr += 4;
              int mfcr_regnum = insn & 0x1f;
              insn_len = csky_get_insn (gdbarch, addr, &insn2);
              if (insn_len == 2)
                {
                  int stw_regnum = (insn2 >> 5) & 0x7;
                  if (V2_16_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2; /* CSKY_EPSR_REGNUM.  */
                      offset = V2_16_STWx0_OFFSET (insn2);
                      register_offsets[rn] = stacksize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                         rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
              else  /* INSN_LEN == 4.  */
                {
                  int stw_regnum = (insn2 >> 21) & 0x1f;
                  if (V2_32_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2; /* CSKY_EPSR_REGNUM.  */
                      offset = V2_32_ST_OFFSET (insn2);
                      register_offsets[rn] = framesize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                          rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
            }
          else if (V2_32_IS_MFCR_FPSR (insn))
            {
              unsigned int insn2;
              addr += 4;
              int mfcr_regnum = insn & 0x1f;
              insn_len = csky_get_insn (gdbarch, addr, &insn2);
              if (insn_len == 2)
                {
                  int stw_regnum = (insn2 >> 5) & 0x7;
                  if (V2_16_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2 + 1; /* CSKY_FPSR_REGNUM.  */
                      offset = V2_16_STWx0_OFFSET (insn2);
                      register_offsets[rn] = stacksize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                         rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
              else  /* INSN_LEN == 4.  */
                {
                  int stw_regnum = (insn2 >> 21) & 0x1f;
                  if (V2_32_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2 + 1; /* CSKY_FPSR_REGNUM.  */
                      offset = V2_32_ST_OFFSET (insn2);
                      register_offsets[rn] = framesize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                         rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
            }
          else if (V2_32_IS_MFCR_EPC (insn))
            {
              unsigned int insn2;
              addr += 4;
              int mfcr_regnum = insn & 0x1f;
              insn_len = csky_get_insn (gdbarch, addr, &insn2);
              if (insn_len == 2)
                {
                  int stw_regnum = (insn2 >> 5) & 0x7;
                  if (V2_16_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2 + 2; /* CSKY_EPC_REGNUM.  */
                      offset = V2_16_STWx0_OFFSET (insn2);
                      register_offsets[rn] = stacksize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                         rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
              else  /* INSN_LEN == 4.  */
                {
                  int stw_regnum = (insn2 >> 21) & 0x1f;
                  if (V2_32_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2 + 2; /* CSKY_EPC_REGNUM.  */
                      offset = V2_32_ST_OFFSET (insn2);
                      register_offsets[rn] = framesize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                         rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
            }
          else if (V2_32_IS_MFCR_FPC (insn))
            {
              unsigned int insn2;
              addr += 4;
              int mfcr_regnum = insn & 0x1f;
              insn_len = csky_get_insn (gdbarch, addr, &insn2);
              if (insn_len == 2)
                {
                  int stw_regnum = (insn2 >> 5) & 0x7;
                  if (V2_16_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2 + 3; /* CSKY_FPC_REGNUM.  */
                      offset = V2_16_STWx0_OFFSET (insn2);
                      register_offsets[rn] = stacksize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                         rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
              else  /* INSN_LEN == 4.  */
                {
                  int stw_regnum = (insn2 >> 21) & 0x1f;
                  if (V2_32_IS_STWx0 (insn2) && (mfcr_regnum == stw_regnum))
                    {
                      int offset;

                      rn  = CSKY_NUM_GREGS_v2 + 3; /* CSKY_FPC_REGNUM.  */
                      offset = V2_32_ST_OFFSET (insn2);
                      register_offsets[rn] = framesize - offset;
                      ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                         rn, register_offsets[rn]));
                      ckcore_insn_debug (("CKCORE: continuing\n"));
                      continue;
                    }
                  break;
                }
            }
          else if (V2_32_IS_PUSH (insn))   /* Push for 32_bit.  */
            {
              int offset = 0;
              if (V2_32_IS_PUSH_R29 (insn))
                {
                  stacksize += 4;
                  register_offsets[29] = stacksize;
                  ckcore_insn_debug (("CKCORE: r29 saved at offset 0x%x\n",
                                     register_offsets[29]));
                  offset += 4;
                }
              if (V2_32_PUSH_LIST2 (insn))
                {
                  int num = V2_32_PUSH_LIST2 (insn);
                  int tmp = 0;
                  stacksize += num * 4;
                  offset += num * 4;
                  ckcore_insn_debug (("CKCORE: push regs_array: r16-r%d\n",
                                     16 + num - 1));
                  for (rn = 16; rn <= 16 + num - 1; rn++)
                    {
                       register_offsets[rn] = stacksize - tmp;
                       ckcore_insn_debug (("CKCORE: r%d saved at 0x%x"
                                           " (offset %d)\n",
                                            rn, register_offsets[rn], tmp));
                       tmp += 4;
                    }
                }
              if (V2_32_IS_PUSH_R15 (insn))
                {
                  stacksize += 4;
                  register_offsets[15] = stacksize;
                  ckcore_insn_debug (("CKCORE: r15 saved at offset 0x%x\n",
                                     register_offsets[15]));
                  offset += 4;
                }
              if (V2_32_PUSH_LIST1 (insn))
                {
                  int num = V2_32_PUSH_LIST1 (insn);
                  int tmp = 0;
                  stacksize += num * 4;
                  offset += num * 4;
                  ckcore_insn_debug (("CKCORE: push regs_array: r4-r%d\n",
                                     4 + num - 1));
                  for (rn = 4; rn <= 4 + num - 1; rn++)
                    {
                       register_offsets[rn] = stacksize - tmp;
                       ckcore_insn_debug (("CKCORE: r%d saved at 0x%x"
                                           " (offset %d)\n",
                                           rn, register_offsets[rn], tmp));
                        tmp += 4;
                    }
                }

              framesize = stacksize;
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }  /* End of push for 32_bit.  */
          else if (V2_32_IS_LRW4(insn) || V2_32_IS_MOVI4(insn)
                        || V2_32_IS_MOVIH4(insn) || V2_32_IS_BMASKI4(insn))
            {
              int adjust = 0;
              int offset = 0;
              unsigned int insn2;

              ckcore_insn_debug (("CKCORE: looking at large frame\n"));
              if (V2_32_IS_LRW4 (insn))
                {
                  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
                  int literal_addr = (addr + ((insn & 0xffff) << 2))
                                     & 0xfffffffc;
                  adjust = read_memory_unsigned_integer (literal_addr, 4,
                                                         byte_order);
                }
              else if (V2_32_IS_MOVI4 (insn))
                adjust = (insn  & 0xffff);
              else if (V2_32_IS_MOVIH4 (insn))
                adjust = (insn & 0xffff) << 16;
              else    /* V2_32_IS_BMASKI4 (insn).  */
                adjust = (1 << (((insn & 0x3e00000) >> 21) + 1)) - 1;

              ckcore_insn_debug (("CKCORE: base stacksize=0x%x\n", adjust));

              /* May have zero or more insns which modify r4.  */
              ckcore_insn_debug (("CKCORE: looking for r4 adjusters...\n"));
              offset = 4;
              insn_len = csky_get_insn (gdbarch, addr + offset, &insn2);
              while (V2_IS_R4_ADJUSTER (insn2))
                {
                  if (V2_32_IS_ADDI4 (insn2))
                    {
                      int imm = (insn2 & 0xfff) + 1;
                      adjust += imm;
                      ckcore_insn_debug (("CKCORE: addi r4,%d\n", imm));
                    }
                  else if (V2_32_IS_SUBI4 (insn2))
                    {
                      int imm = (insn2 & 0xfff) + 1;
                      adjust -= imm;
                      ckcore_insn_debug (("CKCORE: subi r4,%d\n", imm));
                    }
                  else if (V2_32_IS_NOR4 (insn2))
                    {
                      adjust = ~adjust;
                      ckcore_insn_debug (("CKCORE: nor r4,r4,r4\n"));
                    }
                  else if (V2_32_IS_ROTLI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      int temp = adjust >> (32 - imm);
                      adjust <<= imm;
                      adjust |= temp;
                      ckcore_insn_debug (("CKCORE: rotli r4,r4,%d\n", imm));
                    }
                  else if (V2_32_IS_LISI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      adjust <<= imm;
                      ckcore_insn_debug (("CKCORE: lsli r4,r4,%d\n", imm));
                    }
                  else if (V2_32_IS_BSETI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      adjust |= (1 << imm);
                      ckcore_insn_debug (("CKCORE: bseti r4,r4 %d\n", imm));
                    }
                  else if (V2_32_IS_BCLRI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      adjust &= ~(1 << imm);
                      ckcore_insn_debug (("CKCORE: bclri r4,r4 %d\n", imm));
                    }
                  else if (V2_32_IS_IXH4 (insn2))
                    {
                      adjust *= 3;
                      ckcore_insn_debug (("CKCORE: ixh r4,r4,r4\n"));
                    }
                  else if (V2_32_IS_IXW4 (insn2))
                    {
                      adjust *= 5;
                      ckcore_insn_debug (("CKCORE: ixw r4,r4,r4\n"));
                    }
                  else if (V2_16_IS_ADDI4 (insn2))
                    {
                      int imm = (insn2 & 0xff) + 1;
                      adjust += imm;
                      ckcore_insn_debug (("CKCORE: addi r4,%d\n", imm));
                    }
                  else if (V2_16_IS_SUBI4 (insn2))
                    {
                      int imm = (insn2 & 0xff) + 1;
                      adjust -= imm;
                      ckcore_insn_debug (("CKCORE: subi r4,%d\n", imm));
                    }
                  else if (V2_16_IS_NOR4 (insn2))
                    {
                      adjust = ~adjust;
                      ckcore_insn_debug (("CKCORE: nor r4,r4\n"));
                    }
                  else if (V2_16_IS_BSETI4 (insn2))
                    {
                      int imm = (insn2 & 0x1f);
                      adjust |= (1 << imm);
                      ckcore_insn_debug (("CKCORE: bseti r4, %d\n", imm));
                    }
                  else if (V2_16_IS_BCLRI4 (insn2))
                    {
                      int imm = (insn2 & 0x1f);
                      adjust &= ~(1 << imm);
                      ckcore_insn_debug (("CKCORE: bclri r4, %d\n", imm));
                    }
                  else if (V2_16_IS_LSLI4 (insn2))
                    {
                      int imm = (insn2 & 0x1f);
                      adjust <<= imm;
                      ckcore_insn_debug (("CKCORE: lsli r4,r4, %d\n", imm));
                    }

                  offset += insn_len;
                  insn_len =  csky_get_insn (gdbarch, addr + offset, &insn2);
                };

              ckcore_insn_debug (("CKCORE: done looking for r4 adjusters\n"));

              /* If the next insn adjusts the stack pointer, we keep everything;
                 if not, we scrap it and we've found the end of the prologue. */
              if (V2_IS_SUBU4 (insn2))
                {
                  addr += offset;
                  stacksize += adjust;
                  ckcore_insn_debug (("CKCORE: found stack adjustment of 0x%x"
                                      " bytes.\n", adjust));
                  ckcore_insn_debug (("CKCORE: skipping to new address 0x%x\n",
                                      addr));
                  ckcore_insn_debug (("CKCORE: continuing\n"));
                  continue;
                }

              /* None of these instructions are prologue, so don't touch
                 anything.  */
              ckcore_insn_debug (("CKCORE: no subu sp,sp,r4; NOT altering"
                                 " stacksize.\n"));
              break;
            }
        }   /* End of 'if(insn_len == 4)'.  */
      else
        {
          if (V2_16_IS_SUBI0 (insn))  /* subi.sp sp,disp.  */
            {
              int offset = V2_16_SUBI_IMM (insn);
              ckcore_insn_debug (("CKCORE: got subi r0,%d; continuing\n",
                                 offset));
              stacksize += offset; /* Capacity of creating space in stack.  */
              continue;
            }
          else if (V2_16_IS_STWx0 (insn))   /* stw.16 rz,(sp,disp).  */
            {
              /* Spill register: see note for IS_STM above.  */
              int disp;

              rn = V2_16_ST_VAL_REGNUM (insn);
              disp = V2_16_ST_OFFSET (insn);
              register_offsets[rn] = stacksize - disp;
              ckcore_insn_debug (("CKCORE: r%d saved at offset 0x%x\n",
                                 rn, register_offsets[rn]));
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }
          else if (V2_16_IS_MOV_FP_SP (insn))
            {
              /* SP is saved to FP reg, means code afer prologue may
                 modify SP.  */
              is_fp_saved = 1;
              adjust_fp = stacksize;
              continue;
            }
          else if (V2_16_IS_PUSH (insn)) /* Push for 16_bit.  */
            {
              int offset = 0;
              if (V2_16_IS_PUSH_R15 (insn))
                {
                  stacksize += 4;
                  register_offsets[15] = stacksize;
                  ckcore_insn_debug (("CKCORE: r15 saved at offset 0x%x\n",
                                     register_offsets[15]));
                  offset += 4;
                 }
              if (V2_16_PUSH_LIST1 (insn))
                {
                  int num = V2_16_PUSH_LIST1 (insn);
                  int tmp = 0;
                  stacksize += num * 4;
                  offset += num * 4;
                  ckcore_insn_debug (("CKCORE: push regs_array: r4-r%d\n",
                                     4 + num - 1));
                  for (rn = 4; rn <= 4 + num - 1; rn++)
                    {
                       register_offsets[rn] = stacksize - tmp;
                       ckcore_insn_debug (("CKCORE: r%d saved at 0x%x"
                                           " (offset %d)\n",
                                            rn, register_offsets[rn], offset));
                       tmp += 4;
                    }
                }

              framesize = stacksize;
              ckcore_insn_debug (("CKCORE: continuing\n"));
              continue;
            }  /* End of push for 16_bit.  */
          else if (V2_16_IS_LRW4 (insn) || V2_16_IS_MOVI4 (insn))
            {
              int adjust = 0;
              int offset = 0;
              unsigned int insn2;

              ckcore_insn_debug (("CKCORE: looking at large frame\n"));
              if (V2_16_IS_LRW4 (insn))
                {
                  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
                  int offset = ((insn & 0x300) >> 3) | (insn & 0x1f);
                  int literal_addr = (addr + ( offset << 2)) & 0xfffffffc;
                  adjust = read_memory_unsigned_integer (literal_addr, 4,
                                                         byte_order);
                }
              else    /* V2_16_IS_MOVI4(insn).  */
                adjust = (insn  & 0xff);

              ckcore_insn_debug (("CKCORE: base stacksize=0x%x\n", adjust));

              /* May have zero or more insns which modify r4.  */
              ckcore_insn_debug (("CKCORE: looking for r4 adjusters...\n"));
              offset = 2;
              insn_len = csky_get_insn (gdbarch, addr + offset, &insn2);
              while (V2_IS_R4_ADJUSTER (insn2))
                {
                  if (V2_32_IS_ADDI4 (insn2))
                    {
                      int imm = (insn2 & 0xfff) + 1;
                      adjust += imm;
                      ckcore_insn_debug (("CKCORE: addi r4,%d\n", imm));
                    }
                  else if (V2_32_IS_SUBI4 (insn2))
                    {
                      int imm = (insn2 & 0xfff) + 1;
                      adjust -= imm;
                      ckcore_insn_debug (("CKCORE: subi r4,%d\n", imm));
                    }
                  else if (V2_32_IS_NOR4 (insn2))
                    {
                      adjust = ~adjust;
                      ckcore_insn_debug (("CKCORE: nor r4,r4,r4\n"));
                    }
                  else if (V2_32_IS_ROTLI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      int temp = adjust >> (32 - imm);
                      adjust <<= imm;
                      adjust |= temp;
                      ckcore_insn_debug (("CKCORE: rotli r4,r4,%d\n", imm));
                    }
                  else if (V2_32_IS_LISI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      adjust <<= imm;
                      ckcore_insn_debug (("CKCORE: lsli r4,r4,%d\n", imm));
                    }
                  else if (V2_32_IS_BSETI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      adjust |= (1 << imm);
                      ckcore_insn_debug (("CKCORE: bseti r4,r4 %d\n", imm));
                    }
                  else if (V2_32_IS_BCLRI4 (insn2))
                    {
                      int imm = ((insn2 >> 21) & 0x1f);
                      adjust &= ~(1 << imm);
                      ckcore_insn_debug (("CKCORE: bclri r4,r4 %d\n", imm));
                    }
                  else if (V2_32_IS_IXH4 (insn2))
                    {
                      adjust *= 3;
                      ckcore_insn_debug (("CKCORE: ixh r4,r4,r4\n"));
                    }
                  else if (V2_32_IS_IXW4 (insn2))
                    {
                      adjust *= 5;
                      ckcore_insn_debug (("CKCORE: ixw r4,r4,r4\n"));
                    }
                  else if (V2_16_IS_ADDI4 (insn2))
                    {
                      int imm = (insn2 & 0xff) + 1;
                      adjust += imm;
                      ckcore_insn_debug (("CKCORE: addi r4,%d\n", imm));
                    }
                  else if (V2_16_IS_SUBI4 (insn2))
                    {
                      int imm = (insn2 & 0xff) + 1;
                      adjust -= imm;
                      ckcore_insn_debug (("CKCORE: subi r4,%d\n", imm));
                    }
                  else if (V2_16_IS_NOR4 (insn2))
                    {
                      adjust = ~adjust;
                      ckcore_insn_debug (("CKCORE: nor r4,r4\n"));
                    }
                  else if (V2_16_IS_BSETI4 (insn2))
                    {
                      int imm = (insn2 & 0x1f);
                      adjust |= (1 << imm);
                      ckcore_insn_debug (("CKCORE: bseti r4, %d\n", imm));
                    }
                  else if (V2_16_IS_BCLRI4 (insn2))
                    {
                      int imm = (insn2 & 0x1f);
                      adjust &= ~(1 << imm);
                      ckcore_insn_debug (("CKCORE: bclri r4, %d\n", imm));
                    }
                  else if (V2_16_IS_LSLI4 (insn2))
                    {
                      int imm = (insn2 & 0x1f);
                      adjust <<= imm;
                      ckcore_insn_debug (("CKCORE: lsli r4,r4, %d\n", imm));
                    }

                  offset += insn_len;
                  insn_len = csky_get_insn (gdbarch, addr + offset, &insn2);
                };

              ckcore_insn_debug (("CKCORE: done looking for r4 adjusters\n"));

              /* If the next insn adjusts the stack pointer, we keep
                 everything; if not, we scrap it and we've found the end
                 of the prologue.  */
              if (V2_IS_SUBU4 (insn2))
                {
                  addr += offset;
                  stacksize += adjust;
                  ckcore_insn_debug (("CKCORE: found stack adjustment of 0x%x"
                                      " bytes.\n", adjust));
                  ckcore_insn_debug (("CKCORE: skipping to new address 0x%x\n",
                                     addr));
                  ckcore_insn_debug (("CKCORE: continuing\n"));
                  continue;
                }

              /* None of these instructions are prologue, so don't touch
                 anything.  */
              ckcore_insn_debug (("CKCORE: no subu sp,r4;  NOT altering"
                                  " stacksize.\n"));
              break;
            }
        }

      /* This is not a prologue insn, so stop here.  */
      ckcore_insn_debug (("CKCORE: insn is not a prologue insn"
                          " -- ending scan\n"));
      break;

    }

  if (this_cache)
    {
      CORE_ADDR unwound_fp;
      enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
      this_cache->framesize = framesize;

      if (is_fp_saved)
        {
          this_cache->framereg = CSKY_FP_REGNUM;
          unwound_fp = get_frame_register_unsigned (this_frame,
                                                    this_cache->framereg);
          this_cache->prev_sp = unwound_fp + adjust_fp;
        }
      else
        {
          this_cache->framereg = CSKY_SP_REGNUM;
          unwound_fp = get_frame_register_unsigned (this_frame,
                                                    this_cache->framereg);
          this_cache->prev_sp = unwound_fp + stacksize;
        }

      /* Note where saved registers are stored. The offsets in REGISTER_OFFSETS
         are computed relative to the top of the frame. */
      for (rn = 0; rn < CSKY_NUM_GREGS_v2; rn++)
        {
          if (register_offsets[rn] >= 0)
            {
              CORE_ADDR rn_value;
              this_cache->saved_regs[rn].addr =
                this_cache->prev_sp - register_offsets[rn];
              rn_value = read_memory_unsigned_integer
                           (this_cache->saved_regs[rn].addr,
                            4, byte_order);
              ckcore_insn_debug (("Saved register %s stored at 0x%08x,"
                                  " value=0x%08x\n",
                                  csky_register_names_v2[rn],
                                  this_cache->saved_regs[rn].addr,
                                  rn_value));
            }
        }
      if (tdep->lr_type_p == 1)
        {
          /* rte || epc .  */
          this_cache->saved_regs[CSKY_PC_REGNUM] =
            this_cache->saved_regs[CSKY_EPC_REGNUM];
        }
      else if (tdep->lr_type_p == 2)
        {
          /* rfi || fpc .  */
          this_cache->saved_regs[CSKY_PC_REGNUM] =
            this_cache->saved_regs[CSKY_FPC_REGNUM];
        }
      else
        {
          this_cache->saved_regs[CSKY_PC_REGNUM] =
            this_cache->saved_regs[CSKY_LR_REGNUM];
        }

    } /* End of if (this_cache).  */

  return addr;
}
#endif /* CSKYGDB_CONFIG_ABIV2 */


static CORE_ADDR
csky_scan_prologue (struct gdbarch *gdbarch,
                    CORE_ADDR start, CORE_ADDR limit,
                    CORE_ADDR end,
                    struct frame_info *this_frame,
                    struct csky_unwind_cache *this_cache)
{
#ifndef CSKYGDB_CONFIG_ABIV2
  if (csky_get_insn_version(gdbarch) == CSKY_INSN_V1)
    {
      return csky_analyze_prologue_v1 (gdbarch, start, limit, end,
                                       this_frame, this_cache);
    }
  else
    {
      return csky_analyze_prologue_v2p (gdbarch, start, limit, end,
                                        this_frame, this_cache);
    }
#else    /* CSKYGDB_CONFIG_ABIV2 */
  return csky_analyze_prologue_v2 (gdbarch, start, limit, end,
                                   this_frame, this_cache);
#endif   /* CSKYGDB_CONFIG_ABIV2 */
}


/* Function: skip_prologue
   Find end of function prologue.  */

#define DEFAULT_SEARCH_LIMIT 128

static CORE_ADDR
csky_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;
  LONGEST return_value;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  /* See what the symbol table says.  */

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end <= func_end)
        {
          func_end = sal.end;
        }
      else
        {
          /* Either there's no line info, or the line after the prologue is
             after the end of the function.  In this case, there probably
             isn't a prologue.  */
          func_end = min (func_end, func_addr + DEFAULT_SEARCH_LIMIT);
        }
    }
  else
    func_end = pc + DEFAULT_SEARCH_LIMIT;

  /* If pc's location is not readable, just quit.  */
  if (!safe_read_memory_integer (pc, 4, byte_order, &return_value))
    return pc;

  /* Find the end of prologue.  */
  return csky_scan_prologue (gdbarch, pc, func_end, func_end, NULL, NULL);
}


/* Get the length of insn from PCPTR.  */

static void
csky_breakpoint_from_pc_self (struct gdbarch *gdbarch,
                              CORE_ADDR *pcptr,
                              int *lenptr)
{
#ifndef CSKYGDB_CONFIG_ABIV2
  if (csky_get_insn_version(gdbarch) == CSKY_INSN_V1)
    {
      *lenptr = 2;
    }
  else
    {
      if (csky_pc_is_csky16 (gdbarch, *pcptr, 0))
        {
          *lenptr = 2;
        }
      else
        {
          /* Set two bkpt16 in.  */
          *lenptr = 4;
        }
    }
#else    /* CSKYGDB_CONFIG_ABIV2 */
  if (csky_pc_is_csky16 (gdbarch, *pcptr, 1))
    {
      *lenptr = 2;
    }
  else
    {
      /* Set two bkpt16 in.  */
      *lenptr = 4;
    }
#endif    /* CSKYGDB_CONFIG_ABIV2 */
}

/* This function implements gdbarch_breakpoint_from_pc.  It uses the program
   counter value to determine whether a 16- or 32-bit breakpoint should be used.
   It returns a pointer to a string of bytes that encode a breakpoint
   instruction, stores the length of the string to *lenptr, and adjusts pc (if
   necessary) to point to the actual memory location where the breakpoint
   should be inserted.  */

static const gdb_byte *
csky_breakpoint_from_pc (struct gdbarch *gdbarch,
                         CORE_ADDR *pcptr,
                         int *lenptr)
{
  static gdb_byte csky_16_breakpoint[] = { 0, 0 };
  static gdb_byte csky_32_breakpoint[] = { 0, 0, 0, 0 };

  csky_breakpoint_from_pc_self (gdbarch, pcptr, lenptr);

  if (*lenptr == 4)
    return csky_32_breakpoint;
  else
    return csky_16_breakpoint;
}


/* Insert csky software breakpoint, here, all breakpoint should generate
   insn "st.w". When the bkpt address can't be divided 4, extra memory
   will be read and saved.  */

static int
csky_memory_insert_breakpoint (struct gdbarch *gdbarch,
                               struct bp_target_info *bp_tgt)
{
  int val;
  const unsigned char *bp;
  struct cleanup *cleanups;
  gdb_byte bp_write_record1[] = { 0, 0, 0, 0 };
  gdb_byte bp_write_record2[] = { 0, 0, 0, 0 };
  gdb_byte bp_record[] = { 0, 0, 0, 0 };

  /* For odd bp_address testing.  */
  if (bp_tgt->reqstd_address % 2)
    warning (_("Breakpoint address of 0x%x is an odd address,please check!\n"),
             (unsigned int)bp_tgt->reqstd_address);
  /* Avoid gdb read memory from breakpoint shadows.  */
  cleanups = make_show_memory_breakpoints_cleanup (1);

  /* Determine appropriate breakpoint contents and size for this address.  */
  csky_breakpoint_from_pc_self (gdbarch, &bp_tgt->reqstd_address,
                                &bp_tgt->placed_size);

  /* Save the memory contents.  */
  bp_tgt->shadow_len = bp_tgt->placed_size;

  /* Fill bp_tgt->placed_address.  */
  bp_tgt->placed_address = bp_tgt->reqstd_address;

  /* Diff for 16 or 32.  */
  if (bp_tgt->placed_size == 2)
    {
      if ((bp_tgt->reqstd_address % 4) == 0)
        {
          /* The last 2 represents two gdb_bytes.  */
          val = target_read_memory (bp_tgt->reqstd_address,
                                    bp_tgt->shadow_contents, 2);
          if (val)
            return val;
          val = target_read_memory (bp_tgt->reqstd_address + 2, bp_record, 2);
          if (val)
            return val;
          /* Write the breakpoint.  */
          bp_write_record1[2] = bp_record[0];
          bp_write_record1[3] = bp_record[1];
          bp = bp_write_record1;
          val = target_write_raw_memory (bp_tgt->reqstd_address, bp,
                                     CSKY_WR_BKPT_MODE);
        }
      else
        {
          /* The last 2 represents two gdb_bytes  */
          val = target_read_memory (bp_tgt->reqstd_address,
                                    bp_tgt->shadow_contents, 2);
          if (val)
            return val;
          val = target_read_memory (bp_tgt->reqstd_address - 2, bp_record, 2);
          if (val)
            return val;
          /* Write the breakpoint.  */
          bp_write_record1[0] = bp_record[0];
          bp_write_record1[1] = bp_record[1];
          bp = bp_write_record1;
          val = target_write_raw_memory (bp_tgt->reqstd_address - 2, bp,
                                     CSKY_WR_BKPT_MODE);
        }
    }
  else
    {
      if ((bp_tgt->placed_address % 4) == 0)
        {
          val = target_read_memory (bp_tgt->reqstd_address,
                                    bp_tgt->shadow_contents,
                                    CSKY_WR_BKPT_MODE);
          if (val)
             return val;
          /* Write the breakpoint.  */
          bp = bp_write_record1;
          val = target_write_raw_memory (bp_tgt->reqstd_address, bp,
                                     CSKY_WR_BKPT_MODE);
        }
      else
        {
          val = target_read_memory (bp_tgt->reqstd_address,
                                    bp_tgt->shadow_contents,
                                    CSKY_WR_BKPT_MODE);
          if (val)
            return val;
          /* The last 2 represents two gdb_bytes.  */
          val = target_read_memory (bp_tgt->reqstd_address - 2, bp_record, 2);
          if (val)
            return val;
          val = target_read_memory (bp_tgt->reqstd_address + 4,
                                    bp_record + 2, 2);
          if (val)
            return val;
          bp_write_record1[0] = bp_record[0];
          bp_write_record1[1] = bp_record[1];
          bp_write_record2[2] = bp_record[2];
          bp_write_record2[3] = bp_record[3];
          /* Write the breakpoint.  */
          bp = bp_write_record1;
          val = target_write_raw_memory (bp_tgt->reqstd_address - 2, bp,
                                     CSKY_WR_BKPT_MODE);
          if (val)
            return val;
          bp = bp_write_record2;
          val = target_write_raw_memory (bp_tgt->reqstd_address + 2, bp,
                                     CSKY_WR_BKPT_MODE);
        }
    }
  do_cleanups (cleanups);
  return val;
}

/* Restore the breakpoint shadow_contents to the target.  */

static int
csky_memory_remove_breakpoint (struct gdbarch *gdbarch,
                               struct bp_target_info *bp_tgt)
{
  int val;
  gdb_byte bp_record[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  /* Diff for shadow_len 2 or 4.  */
  if (bp_tgt->shadow_len == 2)
    {
      /* Diff for address keep write memory with word.  */
      if ((bp_tgt->reqstd_address % 4) == 0)
        {
          /* The last 2 represents two gdb_bytes.  */
          val = target_read_memory (bp_tgt->reqstd_address + 2,
                                    bp_record + 2, 2);
          if (val)
            return val;
          bp_record[0] = bp_tgt->shadow_contents[0];
          bp_record[1] = bp_tgt->shadow_contents[1];
          return target_write_raw_memory (bp_tgt->reqstd_address, bp_record,
                                      CSKY_WR_BKPT_MODE);
        }
      else
        {
          val = target_read_memory (bp_tgt->reqstd_address - 2,  bp_record, 2);
          if (val)
            return val;
          bp_record[2] = bp_tgt->shadow_contents[0];
          bp_record[3] = bp_tgt->shadow_contents[1];
          return target_write_raw_memory (bp_tgt->reqstd_address - 2, bp_record,
                                      CSKY_WR_BKPT_MODE);
        }
    }
  else
    {
      /* Diff for address keep write memory with word.  */
      if ((bp_tgt->placed_address % 4)==0)
        {
          return target_write_raw_memory (bp_tgt->reqstd_address,
                                      bp_tgt->shadow_contents,
                                      CSKY_WR_BKPT_MODE);
        }
      else
        {
          /* The last 2 represents two gdb_bytes.  */
          val = target_read_memory (bp_tgt->reqstd_address - 2,  bp_record, 2);
          if (val)
            return val;
          val = target_read_memory (bp_tgt->reqstd_address + 4,  bp_record+6, 2);
          if (val)
            return val;
          bp_record[2] = bp_tgt->shadow_contents[0];
          bp_record[3] = bp_tgt->shadow_contents[1];
          bp_record[4] = bp_tgt->shadow_contents[2];
          bp_record[5] = bp_tgt->shadow_contents[3];
          return target_write_raw_memory (bp_tgt->reqstd_address - 2, bp_record,
                                      CSKY_WR_BKPT_MODE * 2);
        }
    }
}


#ifndef CSKYGDB_CONFIG_ABIV2
/* Get link register type in abiv1.  */

static void
csky_analyze_lr_type_v1 (struct gdbarch *gdbarch,
                         CORE_ADDR start_pc, CORE_ADDR end_pc)
{
  CORE_ADDR addr;
  unsigned int insn, rn;
  struct gdbarch_tdep * tdep= gdbarch_tdep (gdbarch);
  for (addr = start_pc; addr < end_pc; addr += 2)
    {
      csky_get_insn (gdbarch, addr, &insn);
      if (V1_IS_MFCR_EPSR (insn) || V1_IS_MFCR_EPC (insn) || V1_IS_RTE (insn))
        {
          tdep->lr_type_p = 1;  /* ld = epc.  */
          break;
        }
      else if (V1_IS_MFCR_FPSR (insn) || V1_IS_MFCR_FPC (insn)
               || V1_IS_RFI (insn))
        {
          tdep->lr_type_p = 2;  /* ld = fpc.  */
          break;
        }
      else if (V1_IS_JMP (insn) || V1_IS_BR (insn) || V1_IS_JMPI (insn))
        {
          tdep->lr_type_p = 0;  /* lr = r15.  */
          break;
        }
    }
}

#else /* CSKYGDB_CONFIG_ABIV2 */
/* Get link register in abiv2.  */

static void
csky_analyze_lr_type_v2 (struct gdbarch *gdbarch,
                         CORE_ADDR start_pc, CORE_ADDR end_pc)
{
  CORE_ADDR addr;
  unsigned int insn, rn ,insn_len;
  insn_len = 2;
  struct gdbarch_tdep * tdep= gdbarch_tdep (gdbarch);
  for (addr = start_pc; addr < end_pc; addr += insn_len)
    {
      insn_len = csky_get_insn (gdbarch, addr, &insn);
      if (insn_len == 4) /* If 32 bits.  */
        {
          if (V2_32_IS_MFCR_EPSR (insn) || V2_32_IS_MFCR_EPC (insn)
              || V2_32_IS_RTE (insn))
            {
              tdep->lr_type_p = 1;  /* lr = epc.  */
              break;
            }
        }
      else if (V2_32_IS_MFCR_FPSR (insn) || V2_32_IS_MFCR_FPC (insn)
               || V2_32_IS_RFI (insn))
        {
          tdep->lr_type_p = 2;  /* lr = fpc.  */
          break;
        }
      else if (V2_32_IS_JMP (insn) || V2_32_IS_BR (insn)
               || V2_32_IS_JMPIX (insn) || V2_32_IS_JMPI (insn))
        {
          tdep->lr_type_p = 0;  /* lr = r15.  */
        }
      else  /* 16 bits.  */
        {
          if (V2_16_IS_JMP (insn) || V2_16_IS_BR (insn) || V2_16_IS_JMPIX (insn))
            tdep->lr_type_p = 0;  /* lr = r15.  */
        }
    }
}
#endif    /* CSKYGDB_CONFIG_ABIV2 */

/* Find link register.  */

static void
csky_scan_lr_type (struct gdbarch *gdbarch,
                   CORE_ADDR start_pc, CORE_ADDR end_pc)
{
#ifndef CSKYGDB_CONFIG_ABIV2
  csky_analyze_lr_type_v1 (gdbarch, start_pc, end_pc);
#else
  csky_analyze_lr_type_v2 (gdbarch, start_pc, end_pc);
#endif
}

/* Heuristic unwinder.  */

static struct csky_unwind_cache *
csky_frame_unwind_cache (struct frame_info *this_frame)
{
  CORE_ADDR prologue_start, prologue_end, func_end, prev_pc, block_addr;
  struct csky_unwind_cache *cache;
  const struct block *bl;
  unsigned long func_size = 0;
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  unsigned int sp_regnum = CSKY_SP_REGNUM;
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->lr_type_p = 0;  /* Default lr = r15.  */

  cache = FRAME_OBSTACK_ZALLOC (struct csky_unwind_cache);
  cache->saved_regs = trad_frame_alloc_saved_regs (this_frame);

  /* Assume there is no frame until proven otherwise.  */
  cache->framereg = sp_regnum;

  cache->framesize = 0;

  prev_pc = get_frame_pc (this_frame);
  block_addr = get_frame_address_in_block (this_frame);
  if (find_pc_partial_function (block_addr, NULL, &prologue_start, &func_end)
       == 0)
  {
    /* FIXME: i don't know how to do with it here.  */
    return cache;
  }
  /* ------------------- For get symbol from prologue_start---------------  */
  bl = block_for_pc (prologue_start);
  if (bl != NULL)
    {
      func_size = bl->endaddr - bl->startaddr;
    }
  else
    {
      struct bound_minimal_symbol msymbol =
               lookup_minimal_symbol_by_pc (prologue_start);
      if (msymbol.minsym != NULL)
        {
          func_size = MSYMBOL_SIZE (msymbol.minsym);
        }
    }

  /* FUNC_SIZE = 0 : the function of the symbol is a disassemble func,
     we should check the lr_type,using tdep->lr_tdep as flag,which used
     in analyze prologue
     FUNC_SIZE != 0: the func of the symbnol is not disassemble func,
     do nothing here!  */
  /* "(func_size==0||func_type != FUNC)"? HOW to get func_type.  */
  if (func_size == 0 )
    csky_scan_lr_type (gdbarch, prologue_start, func_end);

  prologue_end = func_end;

  /* FIXME : prologue_end is enough.  */
  prologue_end = min (prologue_end, prev_pc);

  /* Analyze the function prologue.  */
  csky_scan_prologue (gdbarch, prologue_start, prologue_end,
                      func_end, this_frame, cache);

  /* gdbarch_sp_regnum contains the value and not the address.  */
  trad_frame_set_value (cache->saved_regs,
                        sp_regnum,
                        cache->prev_sp);
  return cache;
}


static CORE_ADDR
csky_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, CSKY_PC_REGNUM);
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */

static void
csky_frame_this_id (struct frame_info *this_frame,
                    void **this_prologue_cache, struct frame_id *this_id)
{
  struct csky_unwind_cache *cache;
  struct frame_id id;

  if (*this_prologue_cache == NULL)
    *this_prologue_cache = csky_frame_unwind_cache (this_frame);
  cache = (struct csky_unwind_cache *) *this_prologue_cache;

  /* This marks the outermost frame.  */
  if (cache->prev_sp == 0)
    return;

  id = frame_id_build (cache->prev_sp, get_frame_func (this_frame));
  (*this_id) = id;
}

static struct value *
csky_frame_prev_register (struct frame_info *this_frame,
                          void **this_prologue_cache, int regnum)
{
  struct csky_unwind_cache *cache;

  if (*this_prologue_cache == NULL)
    *this_prologue_cache = csky_frame_unwind_cache (this_frame);
  cache = (struct csky_unwind_cache *) *this_prologue_cache;

  return trad_frame_get_prev_register (this_frame, cache->saved_regs, regnum);
}

static const struct frame_unwind csky_unwind_cache = {
  NORMAL_FRAME,
  default_frame_unwind_stop_reason,
  csky_frame_this_id,
  csky_frame_prev_register,
  NULL,
  default_frame_sniffer,
  NULL,
  NULL
};

static CORE_ADDR
csky_frame_base_address (struct frame_info *this_frame, void **this_cache)
{
  struct csky_unwind_cache *cache;

  if (*this_cache == NULL)
    *this_cache = csky_frame_unwind_cache (this_frame);
  cache = (struct csky_unwind_cache *) *this_cache;

  return cache->prev_sp - cache->framesize;
}

static const struct frame_base csky_frame_base = {
  &csky_unwind_cache,
  csky_frame_base_address,
  csky_frame_base_address,
  csky_frame_base_address
};

/* Assuming THIS_FRAME is a dummy, return the frame ID of that dummy
   frame.  The frame ID's base needs to match the TOS value saved by
   save_dummy_frame_tos(), and the PC match the dummy frame's breakpoint.  */

static struct frame_id
csky_dummy_id (struct gdbarch *gdbarch, struct frame_info *this_frame)
{
  unsigned int sp_regnum = CSKY_SP_REGNUM;

  CORE_ADDR sp = get_frame_register_unsigned (this_frame, sp_regnum);
  return frame_id_build (sp, get_frame_pc (this_frame));
}


static struct value *
csky_dwarf2_prev_register (struct frame_info *this_frame, void **this_cache,
                           int regnum)
{
  struct csky_unwind_cache *cache = NULL;
  cache = csky_frame_unwind_cache(this_frame);
  return trad_frame_get_prev_register (this_frame, cache->saved_regs, regnum);
}

static void
csky_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
                            struct dwarf2_frame_state_reg *reg,
                            struct frame_info *this_frame)
{
  /* FIXME: read all registers from prologue, because cfa info is not
     enough to do.  */
  reg->how = DWARF2_FRAME_REG_FN;
  reg->loc.fn = csky_dwarf2_prev_register;
}

/* Different ck gdb have different relative root.  */

static char * csky_gdb_name[3][2]={
#ifndef CSKYGDB_CONFIG_ABIV2
  {"csky-linux-gdb",      "../csky-linux/"},
  {"csky-elf-gdb",        "../csky-elf/"},
  {"csky-uclinux-gdb",    "../csky-uclinux/"},
#else
  {"csky-abiv2-linux-gdb","../csky-abiv2-linux/"},
  {"csky-abiv2-elf-gdb",  "../csky-abiv2-elf/"},
  {"csky-abiv2-uclinux-gdb" "../csky-abiv2-uclinux/"}
#endif
};

static void
csky_init_gdb_sysroot ()
{
  char *p, *exe_name; /* CK gdb's name.  */
  int i, count;

  /* Get ck gdb' insatll sysroot, if it is "/home/install/",
     then csky_default_sysroot is "/home/install/../csky-lib/".  */
  char csky_default_sysroot[256] = {0};
#ifdef __MINGW32__
  if (!GetModuleFileNameA (NULL, csky_default_sysroot, 256))
    {
      warning ("can't get csky_default_sysroot\n");
    }
  p = csky_default_sysroot;
  while (*p != 0)
    {
      p ++;
    }
  while (*p != '\\')
    {
      p --;
    }
#else /* not __MINGW32__ */
  count = readlink ("/proc/self/exe", (char *)csky_default_sysroot, 256);
  if (count <= 0 || count > 256)
    {
      warning ("can't get csky_default_sysroot\n");
    }

  csky_default_sysroot[count] = '\0';
  p = csky_default_sysroot + count;
  while (*(p) != '/' && p >= csky_default_sysroot)
    {
      p--;
    }  /* Now p point to the the last '/'.  */
#endif /* not __MINGW32__  */
  exe_name = p + 1;

  /* Check out which gdb being used, chose its lib path.  */
  for(i = 0; i < 3; i++)
    {
      if ((strcmp (exe_name, csky_gdb_name[i][0]) == 0))
        {
          /* Cut out ck gdb name, only save the path.  */
          *(p+1) = '\0';

          strcat (csky_default_sysroot, csky_gdb_name[i][1]);
          gdb_sysroot = (char*) malloc (256);
          strcpy (gdb_sysroot, csky_default_sysroot);
          break;
        }
    }
}


/* Create two new csky register groups.  */

static void
csky_init_reggroup ()
{
  cr_reggroup = reggroup_new ("cr", USER_REGGROUP);
  fr_reggroup = reggroup_new ("fr", USER_REGGROUP);
  vr_reggroup = reggroup_new ("vr", USER_REGGROUP);
  mmu_reggroup = reggroup_new ("mmu", USER_REGGROUP);
#ifdef CSKYGDB_CONFIG_ABIV2  /* ABIV2 HAS PROFILING REG */
  prof_reggroup = reggroup_new ("profiling", USER_REGGROUP);
#endif /* CSKYGDB_CONFIG_ABIV2 */
}

/* Add register groups into reggroup list. */

static void
csky_add_reggroups (struct gdbarch *gdbarch)
{
  reggroup_add (gdbarch, all_reggroup);
  reggroup_add (gdbarch, general_reggroup);

  reggroup_add (gdbarch, cr_reggroup);
  reggroup_add (gdbarch, fr_reggroup);
  reggroup_add (gdbarch, vr_reggroup);
  reggroup_add (gdbarch, mmu_reggroup);
#ifdef CSKYGDB_CONFIG_ABIV2   /* ABIV2 HAS PROFILING REG */
  reggroup_add (gdbarch, prof_reggroup);
#endif /* CSKYGDB_CONFIG_ABIV2 */
}

/* Return the groups that a CSKY register can be categorised into.  */

static int
csky_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
                          struct reggroup *reggroup)
{
  int raw_p;

  if (gdbarch_register_name (gdbarch, regnum) == NULL
      || gdbarch_register_name (gdbarch, regnum)[0] == '\0')
    return 0;
  if (reggroup == all_reggroup)
    return 1;

  raw_p = regnum < gdbarch_num_regs (gdbarch);
  if (reggroup == save_reggroup || reggroup == restore_reggroup)
    return raw_p;
#ifndef CSKYGDB_CONFIG_ABIV2   /* FOR ABIV1 */
  if (csky_get_insn_version (gdbarch) == CSKY_INSN_V2P)
    {
      if (((regnum >= CSKY_R0_REGNUM)
           && (regnum <= CSKY_R0_REGNUM + 31))
          && (reggroup == general_reggroup))
        return 1;
      if (((regnum == CSKY_PC_REGNUM)
           ||((regnum >= CSKY_CR0_REGNUM)
              && (regnum <= CSKY_CR0_REGNUM + 30)))
          && (reggroup == cr_reggroup))
        return 2;
      if ((((regnum >= CSKY_VR0_REGNUM) && (regnum <= CSKY_VR0_REGNUM + 15))
           || ((regnum >= CSKY_VCR0_REGNUM)
               && (regnum <= CSKY_VCR0_REGNUM + 2)))
          && (reggroup == vr_reggroup))
        return 3;
      if (((regnum >= CSKY_MMU_REGNUM) && (regnum <= CSKY_MMU_REGNUM + 8))
          && (reggroup == mmu_reggroup))
        return 4;
      if (((regnum >= CSKY_PROFCR_REGNUM)
           && (regnum <= CSKY_PROFCR_REGNUM + 48))
          && (reggroup == prof_reggroup))
        return 5;
      if ((((regnum >= CSKY_FR0_REGNUMV2)
            && (regnum <= CSKY_FR0_REGNUMV2 + 15))
           || ((regnum >= CSKY_VCR0_REGNUM)
               && (regnum <= CSKY_VCR0_REGNUM + 2)))
          && (reggroup == fr_reggroup))
        return 6;
    }
  else  /* CSKY_INSN_V1  */
    {
      if (((regnum >= CSKY_R0_REGNUM) && (regnum <= CSKY_R0_REGNUM + 15))
          && (reggroup == general_reggroup))
        return 1;
      if (((regnum == CSKY_PC_REGNUM)
           || ((regnum >= CSKY_CR0_REGNUM)
               && (regnum <= CSKY_CR0_REGNUM + 30)))
          && (reggroup == cr_reggroup))
        return 2;
      if ((((regnum >= CSKY_VR0_REGNUM) && (regnum <= CSKY_VR0_REGNUM + 31))
           || ((regnum >= CSKY_VCR0_REGNUM)
               && (regnum <= CSKY_VCR0_REGNUM + 6)))
          && ((reggroup == vr_reggroup) || (reggroup == fr_reggroup)))
      return 3;
      if (reggroup == prof_reggroup)
        /* CSKY_INSN_V1 have no profiling register.  */
        return 0;
      if (((regnum >= CSKY_MMU_REGNUM) && (regnum <= CSKY_MMU_REGNUM + 8))
          && (reggroup == mmu_reggroup))
        return 4;
    }
#else    /* CSKYGDB_CONFIG_ABIV2 */
  if (((regnum >= CSKY_R0_REGNUM) && (regnum <= CSKY_R0_REGNUM + 31))
      && (reggroup == general_reggroup))
    return 1;
  if (((regnum == CSKY_PC_REGNUM)
       || ((regnum >= CSKY_CR0_REGNUM) && (regnum <= CSKY_CR0_REGNUM + 30)))
      && (reggroup == cr_reggroup))
    return 2;
  if ((((regnum >= CSKY_VR0_REGNUM) && (regnum <= CSKY_VR0_REGNUM + 15))
       || ((regnum >= CSKY_VCR0_REGNUM) && (regnum <= CSKY_VCR0_REGNUM + 2)))
      && (reggroup == vr_reggroup))
    return 3;
  if (((regnum >= CSKY_MMU_REGNUM) && (regnum <= CSKY_MMU_REGNUM + 8))
      && (reggroup == mmu_reggroup))
    return 4;
  if (((regnum >= CSKY_PROFCR_REGNUM) && (regnum <= CSKY_PROFCR_REGNUM + 48))
      && (reggroup == prof_reggroup))
    return 5;
  if ((((regnum >= CSKY_FR0_REGNUMV2) && (regnum <= CSKY_FR0_REGNUMV2 + 15))
       || ((regnum >= CSKY_VCR0_REGNUM) && (regnum <= CSKY_VCR0_REGNUM + 2)))
      && (reggroup == fr_reggroup))
    return 6;
#endif    /* CSKYGDB_CONFIG_ABIV2 */
  return 0;
}

/* CSKY_DO_REGISTERS_INFO(): called by "info register [args]" command.
   Override interface for command: info register.  */

static void
csky_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file,
                           struct frame_info *frame, int regnum, int all)
{
  /* Call default print_registers_info function.  */
  default_print_registers_info (gdbarch, file, frame, regnum, all);

  /* For command: info register.  */
  if (regnum == -1 && all == 0)
    {
      default_print_registers_info (gdbarch, file, frame, CSKY_PC_REGNUM, 0);
      default_print_registers_info (gdbarch, file, frame, CSKY_EPC_REGNUM, 0);
      default_print_registers_info (gdbarch, file, frame, CSKY_CR0_REGNUM, 0);
      default_print_registers_info (gdbarch, file, frame, CSKY_EPSR_REGNUM, 0);
    }
  return;
}


/* The init the csky_selected_register_p table, init status
   is all registers cared.  */

static void
csky_init_selected_register_p (void)
{
  int i;
  for (i = 0; i < CSKY_NUM_REGS; i++)
    csky_selected_register_p[i] = 1;
  return;
}


/* Functions for CSKY CORE FILE debug.  */

#ifndef CSKYGDB_CONFIG_ABIV2        /* FOR ABIV1 */

/* General regset pc, r1, r0, psr, r2-r15 for CK610.  */
#define SIZEOF_CSKY_GREGSET_V1 18*4
/* Float regset fsr fesr, fr0-fr31 for CK610.  */
#define SIZEOF_CSKY_FREGSET_V1 34*4

#endif /* not CSKYGDB_CONFIG_ABIV2 */

/* General regset pc, r1, r0, psr, r2-r31 for CK810.  */
#define SIZEOF_CSKY_GREGSET_V2 34*4
/* Float regset fesr fsr fr0-fr31 for CK810.  */
#define SIZEOF_CSKY_FREGSET_V2 34*4

#ifndef CSKYGDB_CONFIG_ABIV2        /* FOR ABIV1 */
/* Offset mapping table from core_section to regcache of general
   registers for ck610.  */
static int csky_gregset_offset_v1[] =
{
  72,  1,  0, 89,  2,  /* pc, r1, r0, psr, r2.  */
   3,  4,  5,  6,  7,  /* r3 ~ r15.  */
   8,  9, 10, 11, 12,
  13, 14, 15
};

/* Offset mapping table from core_section to regcache of float
   registers for ck610.  */
static int csky_fregset_offset_v1[] =
{
  121,122, 24, 25, 26,    /* cp1.cr0, cp1.cr1, cp1.gr0 ~ cp1.gr2.  */
   27, 28, 29, 30, 31,    /* cp1.gr3 ~ cp1.gr31.  */
   32, 33, 34, 35, 36,
   37, 38, 39, 40, 41,
   42, 43, 44, 45, 46,
   47, 48, 49, 50, 51,
   52, 53, 54, 55
};
#endif /* not CSKYGDB_CONFIG_ABIV2 */

/* Offset mapping table from core_section to regcache of general
   registers for ck810.  */
static int csky_gregset_offset_v2[] =
{
  72,  1,  0, 89,  2,  /* pc, r1, r0, psr, r2.  */
   3,  4,  5,  6,  7,  /* r3 ~ r32.  */
   8,  9, 10, 11, 12,
  13, 14, 15, 16, 17,
  18, 19, 20, 21, 22,
  23, 24, 25, 26, 27,
  28, 29, 30, 31
};

/* Offset mapping table from core_section to regcache of float
   registers for ck810.  */
static int csky_fregset_offset_v2[] =
{
  122, 123, 40, 41, 42,     /* fcr, fesr, fr0 ~ fr2.  */
   43,  44, 45, 46, 47,     /* fr3 ~ fr15.  */
   48,  49, 50, 51, 52,
   53,  54, 55
};


#ifndef CSKYGDB_CONFIG_ABIV2     /* FOR ABIV1 */
/* This func is to  get the regset of general register's
   core_section for ck610.  */

static void
csky_supply_gregset_v1 (const struct regset *regset,
                        struct regcache *regcache, int regnum,
                        const void *regs, size_t len)
{
  int i,gregset_num;
  const gdb_byte *gregs = (const gdb_byte *)regs;

  gdb_assert (len >= SIZEOF_CSKY_GREGSET_V1);
  gregset_num = ARRAY_SIZE (csky_gregset_offset_v1);

  for (i = 0; i < gregset_num; i++)
    {
      if ((regnum == csky_gregset_offset_v1[i] || regnum == -1)
          && csky_gregset_offset_v1[i] != -1)
        regcache_raw_supply (regcache, csky_gregset_offset_v1[i],
                             gregs + 4 * i);
    }
}

/* This func is to  get the regset of float register
   core_section for ck610.  */

void
csky_supply_fregset_v1 (const struct regset *regset,
                        struct regcache *regcache, int regnum,
                        const void *regs, size_t len)
{
  int i;
  struct gdbarch *gdbarch;
  int offset = 0;
  const gdb_byte *fregs = (const gdb_byte *)regs;
  int fregset_num = ARRAY_SIZE (csky_fregset_offset_v1);

  gdbarch = get_regcache_arch (regcache);
  gdb_assert (len >= SIZEOF_CSKY_FREGSET_V1);

  for (i = 0; i < fregset_num; i++)
    {
      if ((regnum == csky_fregset_offset_v1[i] || regnum == -1)
          && csky_fregset_offset_v1[i] != -1)
        {
          int num = csky_fregset_offset_v1[i];
          offset += register_size (gdbarch, num);
          regcache_raw_supply (regcache, csky_fregset_offset_v1[i],
                               fregs + offset);
        }
    }
}

static const struct regset csky_regset_general_v1 =
{
  NULL,
  csky_supply_gregset_v1,
  NULL
};

static const struct regset csky_regset_float_v1 =
{
  NULL,
  csky_supply_fregset_v1,
  NULL
};

#endif /* ifndef CSKYGDB_CONFIG_ABIV2 */

/* This func is to  get the regset of general register's
   core_section for ck810.  */

static void
csky_supply_gregset_v2 (const struct regset *regset,
                        struct regcache *regcache, int regnum,
                        const void *regs, size_t len)
{
  int i, gregset_num;
  const gdb_byte *gregs = (const gdb_byte *)regs ;

  gdb_assert (len >= SIZEOF_CSKY_GREGSET_V2);
  gregset_num = ARRAY_SIZE (csky_gregset_offset_v2);

  for (i = 0; i < gregset_num; i++)
    {
      if ((regnum == csky_gregset_offset_v2[i] || regnum == -1)
          && csky_gregset_offset_v2[i] != -1)
        regcache_raw_supply (regcache, csky_gregset_offset_v2[i],
                             gregs + 4 * i);
    }
}

/* This func is to  get the regset of float register
   core_section for ck810.  */

void
csky_supply_fregset_v2 (const struct regset *regset,
                        struct regcache *regcache, int regnum,
                        const void *regs, size_t len)
{
  int i, offset;
  struct gdbarch *gdbarch;
  const gdb_byte *fregs = (const gdb_byte *)regs;
  int fregset_num = ARRAY_SIZE (csky_fregset_offset_v2);

  gdbarch = get_regcache_arch (regcache);
  offset = 0;

  gdb_assert (len >= SIZEOF_CSKY_FREGSET_V2);
  for (i = 0; i < fregset_num; i++)
    {
      if ((regnum == csky_fregset_offset_v2[i] || regnum == -1)
          && csky_fregset_offset_v2[i] != -1)
        {
          int num = csky_fregset_offset_v2[i];
          offset += register_size (gdbarch, num);
          regcache_raw_supply (regcache, csky_fregset_offset_v2[i],
                               fregs + offset);
        }
    }
}


static const struct regset csky_regset_general_v2 =
{
  NULL,
  csky_supply_gregset_v2,
  NULL
};

static const struct regset csky_regset_float_v2 =
{
  NULL,
  csky_supply_fregset_v2,
  NULL
};

static void
csky_linux_iterate_over_regset_sections (struct gdbarch *gdbarch,
                                        iterate_over_regset_sections_cb *cb,
                                        void *cb_data,
                                        const struct regcache *regcache)
{
#ifndef CSKYGDB_CONFIG_ABIV2
  cb (".reg", sizeof (csky_gregset_offset_v1), &csky_regset_general_v1,
      NULL, cb_data);
  cb (".reg2", sizeof (csky_fregset_offset_v1), &csky_regset_float_v1,
      NULL, cb_data);
#else
  cb (".reg", sizeof (csky_gregset_offset_v2), &csky_regset_general_v2,
      NULL, cb_data);
  cb (".reg2", sizeof (csky_fregset_offset_v2), &csky_regset_float_v2,
      NULL, cb_data);
#endif
}

/* When file command is exed, this function will be exed.
   Check abi version between CK gdb and debugged file.  */

static void
csky_check_file_abi (const char* filename)
{
  bfd * abfd = symfile_bfd_open (filename);
  unsigned int e_flags = elf_elfheader (abfd)->e_flags;
#ifndef CSKYGDB_CONFIG_ABIV2
  if ((e_flags & CSKY_ABI_MASK) == CSKY_ABI_V2)
#else
  if ((e_flags & CSKY_ABI_MASK) != CSKY_ABI_V2)
#endif
    error ("Fail to start debugging : file's abi is"
           " conflict with current gdb.\n");
}


/* Initialize the current architecture based on INFO.  If possible,
   re-use an architecture from ARCHES, which is a list of
   architectures already created during this debugging session.

   Called e.g. at program startup, when reading a core file, and when
   reading a binary file.  */

static struct gdbarch *
csky_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  static bfd_arch_info_type bfd_csky_arch_t;

  /* Analysis info.target_desc.  */
  int num_pseudo_regs = 0;
  int num_regs = 0;

  /* "file" command interface transplantation.  */
  deprecated_exec_file_display_hook = csky_check_file_abi;
#ifndef CSKYGDB_CONFIG_ABIV2
  if (info.abfd)
    {
     const struct bfd_arch_info *bfd_arch_info =
                         bfd_lookup_arch (bfd_arch_csky,
                                          elf_elfheader(info.abfd)->e_flags);
      if (bfd_arch_info)
        {
          info.bfd_arch_info = bfd_arch_info;
        }
      else
        {
          memcpy (&bfd_csky_arch_t, info.bfd_arch_info,
                  sizeof (bfd_csky_arch_t));
          bfd_csky_arch_t.mach = elf_elfheader (info.abfd)->e_flags;
          info.bfd_arch_info = &bfd_csky_arch_t;
        }
    }
  else /* No file info, default value is CK510.  */
    {
      memcpy (&bfd_csky_arch_t, info.bfd_arch_info, sizeof (bfd_csky_arch_t));
      bfd_csky_arch_t.mach = CSKY_ARCH_510;
      info.bfd_arch_info = &bfd_csky_arch_t;
    }
#else
  memcpy (&bfd_csky_arch_t, info.bfd_arch_info, sizeof (bfd_csky_arch_t));
  bfd_csky_arch_t.mach = CSKY_ARCH_810 | CSKY_ABI_V2;
  info.bfd_arch_info = &bfd_csky_arch_t;
#endif

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Allocate space for the new architecture.  */
  tdep = XCNEW (struct gdbarch_tdep);
  tdep->lr_type_p = 0;
  tdep->return_reg = CSKY_RET_REGNUM;
  tdep->selected_registers = csky_selected_register_p;
  gdbarch = gdbarch_alloc (&info, tdep);

  set_gdbarch_read_pc (gdbarch, csky_read_pc);
  set_gdbarch_write_pc (gdbarch, csky_write_pc);
  set_gdbarch_unwind_sp (gdbarch, csky_unwind_sp);

  set_gdbarch_num_regs (gdbarch, CSKY_NUM_REGS);
  set_gdbarch_pc_regnum (gdbarch, CSKY_PC_REGNUM);
  set_gdbarch_sp_regnum (gdbarch, CSKY_SP_REGNUM);
  set_gdbarch_register_name (gdbarch, csky_register_name);
  set_gdbarch_register_type (gdbarch, csky_register_type);

  set_gdbarch_print_registers_info (gdbarch, csky_print_registers_info);

  /* Add csky register groups.  */
  csky_add_reggroups (gdbarch);
  set_gdbarch_register_reggroup_p (gdbarch, csky_register_reggroup_p);

  set_gdbarch_push_dummy_call (gdbarch, csky_push_dummy_call);
  set_gdbarch_return_value (gdbarch, csky_return_value);

  set_gdbarch_skip_prologue (gdbarch, csky_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_from_pc (gdbarch, csky_breakpoint_from_pc);

  set_gdbarch_memory_insert_breakpoint (gdbarch,
                                        csky_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch,
                                        csky_memory_remove_breakpoint);

  set_gdbarch_frame_align (gdbarch, csky_frame_align);

  frame_base_set_default (gdbarch, &csky_frame_base);

  /* Methods for saving / extracting a dummy frame's ID.  The ID's
     stack address must match the SP value returned by
     PUSH_DUMMY_CALL, and saved by generic_save_dummy_frame_tos.  */
  set_gdbarch_dummy_id (gdbarch, csky_dummy_id);

  /* Return the unwound PC value.  */
  set_gdbarch_unwind_pc (gdbarch, csky_unwind_pc);

  set_gdbarch_print_insn (gdbarch, print_insn_csky);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  /* Hook in the default unwinders.  */
  dwarf2_append_unwinders (gdbarch);
  frame_unwind_append_unwinder (gdbarch, &csky_unwind_cache);
  dwarf2_frame_set_init_reg (gdbarch, csky_dwarf2_frame_init_reg);

  /* Support simple overlay manager.  */
  set_gdbarch_overlay_update (gdbarch, simple_overlay_update);

  /* Support csky core file. */
  set_gdbarch_iterate_over_regset_sections
            (gdbarch, csky_linux_iterate_over_regset_sections);
  /* Support embeded dynamic library debugging. */
  set_solib_svr4_fetch_link_map_offsets (gdbarch,
                                         svr4_ilp32_fetch_link_map_offsets);
  return gdbarch;
}

void
_initialize_csky_tdep (void)
{

  register_gdbarch_init (bfd_arch_csky, csky_gdbarch_init);

  csky_init_reggroup ();
  csky_init_gdb_sysroot ();
  /* Initialize the selected_register_p table.  */
  csky_init_selected_register_p ();
}
