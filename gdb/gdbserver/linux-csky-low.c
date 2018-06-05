/* GNU/Linux/CSKY specific low level interface, for the remote server for GDB.
   Copyright (C) 1995-2016 Free Software Foundation, Inc.

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

#include "server.h"
#include "linux-low.h"
#include "regcache.h"
#include <sys/ptrace.h>
#include "gdb_proc_service.h"
#include <linux/version.h>
#include <asm/ptrace.h>
#include <elf.h>

#if 0
#define csky_debug_info(args) do { printf args; } while (0)
#else
#define csky_debug_info(args) do { } while (0)
#endif

/* Defined in auto-generated file cskyv1-linux.c.  */
void init_registers_cskyv1_linux (void);
extern const struct target_desc *tdesc_cskyv1_linux;

/* Defined in auto-generated file cskyv2-linux.c.  */
void init_registers_cskyv2_linux (void);
extern const struct target_desc *tdesc_cskyv2_linux;

#ifdef PTRACE_GET_THREAD_AREA
#undef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA  25
#else  /* not PTRACE_GET_THREAD_AREA.  */
#define PTRACE_GET_THREAD_AREA  25
#endif /* not PTRACE_GET_THREAD_AREA.  */

#ifdef __CSKYABIV2__
  #define CSKY_GREG_NUMBER   32
  /* Shoule the same with regoff[] in file : ptrace.c.  */
  #define CSKY_GREG_SIZE    144
  #define CSKY_FREG_NUMBER   16
  #define CSKY_FREG_LEN       8
  #define CSKY_FREG_SIZE    140   /* 16 * 8 + 3 * 4 bytes.  */
  #define csky_num_regs     128
#else /* not __CSKYABIV2__ */
  #define CSKY_GREG_NUMBER   16
  /* Shoule the same with regoff[] in file : ptrace.c.  */
  #define CSKY_GREG_SIZE    144
  #define CSKY_FREG_NUMBER   32
  #define CSKY_FREG_LEN       4
  #define CSKY_FREG_SIZE    140   /* 32 * 4 + 3 * 4 bytes.  */
  #ifndef __CSKY_LINUX_2_6_27_55__
    #define csky_num_regs   126
  #else  /* __CSKY_LINUX_2_6_27_55__ */
    #define csky_num_regs    90
  #endif /* __CSKY_LINUX_2_6_27_55__ */
#endif /* not __CSKYABIV2__ */

#if ((LINUX_VERSION_CODE >> 16) >= 4)
#define CSKY_USING_PT_RGESET 1
#define CSKY_KERNEL_CODE_BTHAN_4X
#endif

/* Return the ptrace "address" of register REGNO.  */
#ifndef __CSKY_LINUX_2_6_27_55__
static int csky_regmap[] = {
   0*4,  1*4,  2*4,  3*4,  4*4,  5*4,  6*4,  7*4,
   8*4,  9*4, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4,
#ifdef __CSKYABIV2__  /* CSKY insn v2 */
  16*4, 17*4, 18*4, 19*4, 20*4, 21*4, 22*4, 23*4,
  24*4, 25*4, 26*4, 27*4, 28*4, 29*4, 30*4, 31*4,
    -1,   -1,   -1,   -1, 34*4, 35*4,   -1,   -1,
  40*4, 42*4, 44*4, 46*4, 48*4, 50*4, 52*4, 54*4, /* Fr0 ~ fr15, 64bit  */
  56*4, 58*4, 60*4, 62*4, 64*4, 66*4, 68*4, 70*4,
  72*4, 76*4, 80*4, 84*4, 88*4, 92*4, 96*4,100*4, /* Vr0 ~ vr15, 128bit  */
 104*4,108*4,112*4,116*4,120*4,124*4,128*4,132*4,
#else  /* not __CSKYABIV2__ */
    -1,  -1,    -1,   -1, 34*4, 35*4,   -1,   -1,
  40*4, 41*4, 42*4, 43*4, 44*4, 45*4, 46*4, 47*4,
  48*4, 49*4, 50*4, 51*4, 52*4, 53*4, 54*4, 55*4,
  56*4, 57*4, 58*4, 59*4, 60*4, 61*4, 62*4, 63*4,
  64*4, 65*4, 66*4, 67*4, 68*4, 69*4, 70*4, 71*4,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
#endif /* not __CSKYABIV2__ */
  33*4,  /* pc  */
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  32*4,   -1,   -1,   -1,   -1,   -1,   -1,   -1, /* PSR  */
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
#ifdef __CSKYABIV2__
  73*4, 72*4, 74*4,   -1,   -1,   -1, 14*4,  /* Fcr, fid, fesr, usp  */
#else  /* not __CSKYABIV2__ */
    -1, 72*4, 73*4,   -1, 74*4,   -1,   -1,  /* Fcr, fsr, fesr  */
#endif /* not __CSKYABIV2__ */
};
#else /* __CSKY_LINUX_2_6_27_55__ */

static int csky_regmap[] = {
   0*4,  1*4,  2*4,  3*4,  4*4,  5*4,  6*4,  7*4,
   8*4,  9*4, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4,
    -1,  -1,    -1,   -1, 18*4, 19*4,   -1,   -1,
  20*4, 21*4, 22*4, 23*4, 24*4,  25*4,  26*4,  27*4,
  28*4, 29*4, 30*4, 31*4, 32*4,  33*4,  34*4,  35*4,
  36*4, 37*4, 38*4, 39*4, 40*4,  41*4,  42*4,  43*4,
  44*4, 45*4, 46*4, 47*4, 48*4,  49*4,  50*4,  51*4,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  17*4,  /* PC  */
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  16*4,   -1,   -1,   -1,   -1,   -1,   -1,   -1, /* PSR  */
};
#endif /* __CSKY_LINUX_2_6_27_55__ */

/* CSKY software breakpoint instruction code.  */
/* When ABIV2 and Kernel code version behind v4.x,
   illegal insn 0x1464 is a software bkpt trigger.
   When an illegal insn exception happens, the case
   that insn at EPC is 0x1464 will be regnized as SIGTRAP.  */
static unsigned short csky_breakpoint_bkpt_2 = 0x0000;
static unsigned int csky_breakpoint_bkpt_4 = 0x00000000;
#ifdef __CSKYABIV2__
/* Just used in abiv2.  */
static unsigned short csky_breakpoint_illegal_2_v2 = 0x1464;
static unsigned int csky_breakpoint_illegal_4_v2 = 0x14641464;
#else /* not __CSKYABIV2__ */
/* Just in abiv1.  */
static unsigned short csky_breakpoint_trap1_v1 = 0x0009;
#endif

static int
csky_cannot_fetch_register (int regno)
{
  if (csky_regmap[regno] == -1)
    return 1;

  return 0;
}

static int
csky_cannot_store_register (int regno)
{
  if (csky_regmap[regno] == -1)
    return 1;

  return 0;
}

/* Get the value of pc register.  */

static CORE_ADDR
csky_get_pc (struct regcache *regcache)
{
  unsigned long pc;
  collect_register_by_name (regcache, "pc", &pc);
  csky_debug_info (("FUNC: csky_get_pc, pc = 0x%x\n", (int)pc));
  return pc;
}

/* Set pc register.  */

static void
csky_set_pc (struct regcache *regcache, CORE_ADDR pc)
{
  unsigned long new_pc = pc;
  supply_register_by_name (regcache, "pc", &new_pc);
  csky_debug_info (("FUNC: csky_set_pc, pc = 0x%x\n", (int)new_pc));
}

static void
csky_arch_setup ()
{
#ifndef __CSKYABIV2__
  current_process ()->tdesc = tdesc_cskyv1_linux;
#else
  current_process ()->tdesc = tdesc_cskyv2_linux;
#endif

  return;
}

static struct usrregs_info csky_usrregs_info =
{
  csky_num_regs,
  csky_regmap,
};

/* Fetch the thread-local storage pointer for libthread_db.  */

ps_err_e
ps_get_thread_area (struct ps_prochandle *ph,
                    lwpid_t lwpid, int idx, void **base)
{
#ifndef CSKY_USING_PT_RGESET
  if (ptrace (PTRACE_GET_THREAD_AREA, lwpid, NULL, base) != 0)
    return PS_ERR;
#else
  struct pt_regs regset;
  if (ptrace (PTRACE_GETREGSET, lwpid,
             (PTRACE_TYPE_ARG3) (long) NT_PRSTATUS, &regset) != 0)
    return PS_ERR;

  *base = (void *) regset.tls;
#endif

  /* IDX is the bias from the thread pointer to the beginning of the
     thread descriptor.  It has to be subtracted due to implementation
     quirks in libthread_db.  */
  *base = (void *) ((char *)*base - idx);

  return PS_OK;
}

#ifdef HAVE_PTRACE_GETREGS

static void
csky_fill_gregset (struct regcache *regcache, void *buf)
{
  int i;

  csky_debug_info (("FUNC: csky_fill_gregset\n"));
  /* Insn v1: r0 ~ r15
     insn v2: r0 ~ r32, hi , lo.  */
  for (i = 0; i < CSKY_GREG_NUMBER; i++)
    {
      if (csky_regmap[i] != -1)
        {
          collect_register (regcache, i, ((char*) buf) + csky_regmap[i]);
          csky_debug_info (("\tcollect r%d = 0x%x\n", i,
                           *(int*)(((char*) buf) + csky_regmap[i])));
        }
    }

  /* For PC & PSR.  */
  collect_register_by_name (regcache, "pc", (char *) buf + 33 * 4);
  collect_register_by_name (regcache, "psr", (char *) buf + 32 * 4);
  csky_debug_info (("\tcollect pc = 0x%x, psr = 0x%x\n",
                    *(int*)(char *) buf + 33 * 4,
                    *(int*)((char *) buf + 32 * 4)));

#ifdef __CSKYABIV2__
  collect_register_by_name (regcache, "hi", (char *) buf + 34 * 4);
  collect_register_by_name (regcache, "lo", (char *) buf + 35 * 4);
#endif

}

static void
csky_store_gregset (struct regcache *regcache, const void *buf)
{
  int i;
  char zerobuf[4];

  memset (zerobuf, 0, 4);
  for (i = 0; i < CSKY_GREG_NUMBER; i++)
    {
      if (csky_regmap[i] != -1)
        {
          supply_register (regcache, i, ((char *) buf) + csky_regmap[i]);
          csky_debug_info (("\tsupply r%d = 0x%x\n", i,
                            *(int*)(((char*) buf) + csky_regmap[i])));
        }
      else
        {
          supply_register (regcache, i, zerobuf);
        }
    }

  /* For PC & PSR  */
  supply_register_by_name (regcache, "pc", (char *) buf + 33 * 4);
  supply_register_by_name (regcache, "psr", (char *) buf + 32 * 4);
  csky_debug_info (("\tsupply pc = 0x%x, psr = 0x%x\n",
                    *(int*)(char *) buf + 33 * 4,
                    *(int*)((char *) buf + 32 * 4)));

#ifdef __CSKYABIV2__
  supply_register_by_name (regcache, "hi", (char *) buf + 34 * 4);
  supply_register_by_name (regcache, "lo", (char *) buf + 35 * 4);
#endif

}

static void
csky_fill_fpregset (struct regcache *regcache, void *buf)
{
  int i, base;
  void * p_buf = buf;
  /* Adjust buf to fill float general regs.  */
  const static int buf_adjust = 12;

#ifdef __CSKYABIV2__
  collect_register_by_name (regcache, "fcr", (char *)p_buf);
  collect_register_by_name (regcache, "fesr", (char *) p_buf + 4);
  collect_register_by_name (regcache, "fid", (char *) p_buf + 8);
  base = find_regno (regcache->tdesc, "fr0");
#else
  collect_register_by_name (regcache, "cp1cr1", (char *) p_buf);    /* FCR */
  collect_register_by_name (regcache, "cp1cr2", (char *) p_buf + 4);/* FSR */
  collect_register_by_name (regcache, "cp1cr4", (char *) p_buf + 8);/* FESR */
  base = find_regno (regcache->tdesc, "cp1gr0");
#endif

  for (i = 0; i < CSKY_FREG_NUMBER; i++)
    {
      collect_register (regcache, base + i,
                        (char *) p_buf + buf_adjust + i * CSKY_FREG_LEN);
    }
}

static void
csky_store_fpregset (struct regcache *regcache, const void *buf)
{
  int i, base;
  const void * p_buf = buf;
  /* Adjust buf to fill float general regs.  */
  const static int buf_adjust = 12;

#ifdef __CSKYABIV2__
  supply_register_by_name (regcache, "fcr", (char *) p_buf);
  supply_register_by_name (regcache, "fesr", (char *) p_buf + 4);
  supply_register_by_name (regcache, "fid", (char *) p_buf + 8);
  base = find_regno (regcache->tdesc, "fr0");
#else
  supply_register_by_name (regcache, "cp1cr1", (char *) p_buf); /* FCR */
  supply_register_by_name (regcache, "cp1cr2", (char *) p_buf + 4); /* FSR */
  supply_register_by_name (regcache, "cp1cr4", (char *) p_buf + 8); /* FESR */
  base = find_regno (regcache->tdesc, "cp1gr0");
#endif

  for (i = 0; i < CSKY_FREG_NUMBER; i++)
    {
      supply_register (regcache, base + i,
                       (char *) p_buf + buf_adjust + i * CSKY_FREG_LEN);
    }
}

#ifdef CSKY_USING_PT_RGESET /* Gdbserver will use PTRACE_GET/SET_RGESET.  */
static void
csky_fill_pt_gregset (struct regcache *regcache, void *buf)
{
  int i, base;
  struct pt_regs *regset = (struct pt_regs *)buf;

  collect_register_by_name (regcache, "r15",  &regset->lr);
  collect_register_by_name (regcache, "pc", &regset->pc);
  collect_register_by_name (regcache, "psr", &regset->sr);
#ifndef __CSKYABIV2__
  collect_register_by_name (regcache, "r0", &regset->usp);
#else   /* __CSKYABIV2__ */
  collect_register_by_name (regcache, "r14", &regset->usp);
#endif  /* __CSKYABIV2__ */

#ifndef __CSKYABIV2__
  collect_register_by_name (regcache, "r2", &regset->a0);
  collect_register_by_name (regcache, "r3", &regset->a1);
  collect_register_by_name (regcache, "r4", &regset->a2);
  collect_register_by_name (regcache, "r5", &regset->a3);

  collect_register_by_name (regcache, "r6",  &regset->regs[0]);
  collect_register_by_name (regcache, "r7",  &regset->regs[1]);
  collect_register_by_name (regcache, "r8",  &regset->regs[2]);
  collect_register_by_name (regcache, "r9",  &regset->regs[3]);
  collect_register_by_name (regcache, "r10", &regset->regs[4]);
  collect_register_by_name (regcache, "r11", &regset->regs[5]);
  collect_register_by_name (regcache, "r12", &regset->regs[6]);
  collect_register_by_name (regcache, "r13", &regset->regs[7]);
  collect_register_by_name (regcache, "r14", &regset->regs[8]);
  collect_register_by_name (regcache, "r1",  &regset->regs[9]);
#else  /* __CSKYABIV2__ */

  collect_register_by_name (regcache, "r0", &regset->a0);
  collect_register_by_name (regcache, "r1", &regset->a1);
  collect_register_by_name (regcache, "r2", &regset->a2);
  collect_register_by_name (regcache, "r3", &regset->a3);

  base = find_regno (regcache->tdesc, "r4");

  for (i = 0; i < 10; i++)
    collect_register (regcache, base + i,   &regset->regs[i]);

  base = find_regno (regcache->tdesc, "r16");
  for (i = 0; i < 16; i++)
    collect_register (regcache, base + i,   &regset->exregs[i]);

  collect_register_by_name (regcache, "hi", &regset->rhi);
  collect_register_by_name (regcache, "lo", &regset->rlo);
#endif /* __CSKYABIV2__ */
}

static void
csky_store_pt_gregset (struct regcache *regcache, const void *buf)
{
  int i, base;
  const struct pt_regs *regset = (const struct pt_regs *) buf;

  supply_register_by_name (regcache, "r15",  &regset->lr);
  supply_register_by_name (regcache, "pc", &regset->pc);
  supply_register_by_name (regcache, "psr", &regset->sr);
#ifndef __CSKYABIV2__
  supply_register_by_name (regcache, "r0", &regset->usp);
#else   /* __CSKYABIV2__ */
  supply_register_by_name (regcache, "r14", &regset->usp);
#endif  /* __CSKYABIV2__ */

#ifndef __CSKYABIV2__
  supply_register_by_name (regcache, "r2", &regset->a0);
  supply_register_by_name (regcache, "r3", &regset->a1);
  supply_register_by_name (regcache, "r4", &regset->a2);
  supply_register_by_name (regcache, "r5", &regset->a3);

  supply_register_by_name (regcache, "r6",  &regset->regs[0]);
  supply_register_by_name (regcache, "r7",  &regset->regs[1]);
  supply_register_by_name (regcache, "r8",  &regset->regs[2]);
  supply_register_by_name (regcache, "r9",  &regset->regs[3]);
  supply_register_by_name (regcache, "r10", &regset->regs[4]);
  supply_register_by_name (regcache, "r11", &regset->regs[5]);
  supply_register_by_name (regcache, "r12", &regset->regs[6]);
  supply_register_by_name (regcache, "r13", &regset->regs[7]);
  supply_register_by_name (regcache, "r14", &regset->regs[8]);
  supply_register_by_name (regcache, "r1",  &regset->regs[9]);
#else  /* __CSKYABIV2__ */
  supply_register_by_name (regcache, "r0", &regset->a0);
  supply_register_by_name (regcache, "r1", &regset->a1);
  supply_register_by_name (regcache, "r2", &regset->a2);
  supply_register_by_name (regcache, "r3", &regset->a3);

  base = find_regno (regcache->tdesc, "r4");

  for (i = 0; i < 10; i++)
    supply_register (regcache, base + i,   &regset->regs[i]);

  base = find_regno (regcache->tdesc, "r16");
  for (i = 0; i < 16; i++)
    supply_register (regcache, base + i,   &regset->exregs[i]);

  supply_register_by_name (regcache, "hi", &regset->rhi);
  supply_register_by_name (regcache, "lo", &regset->rlo);
#endif /* __CSKYABIV2__ */
}

static void
csky_fill_pt_vrregset (struct regcache *regcache, void *buf)
{
#ifdef    __CSKYABIV2__
  int i, base;
  struct user_fp *regset = (struct user_fp *)buf;

  base = find_regno (regcache->tdesc, "vr0");

  for (i = 0; i < 16; i++)
    collect_register (regcache, base + i, &regset->vr[i * 4]);
  collect_register_by_name (regcache, "fcr", &regset->fcr);
  collect_register_by_name (regcache, "fesr", &regset->fesr);
  collect_register_by_name (regcache, "fid", &regset->fid);
#endif /* __CSKYABIV2__ */
}

static void
csky_store_pt_vrregset (struct regcache *regcache, const void *buf)
{
#ifdef    __CSKYABIV2__
  int i, base;
  const struct user_fp *regset = (const struct user_fp *)buf;

  base = find_regno (regcache->tdesc, "vr0");

  for (i = 0; i < 16; i++)
    supply_register (regcache, base + i, &regset->vr[i * 4]);

  base = find_regno (regcache->tdesc, "fr0");

  for (i = 0; i < 16; i++)
    supply_register (regcache, base + i, &regset->vr[i * 4 + 2]);
  supply_register_by_name (regcache, "fcr", &regset->fcr);
  supply_register_by_name (regcache, "fesr", &regset->fesr);
  supply_register_by_name (regcache, "fid", &regset->fid);
#endif /* __CSKYABIV2__ */
}

#endif  /* CSKY_USING_PT_RGESET */
#endif  /* HAVE_PTRACE_GETREGS */

struct regset_info csky_regsets[] = {
#ifdef HAVE_PTRACE_GETREGS
#if (defined CSKY_USING_PT_RGESET) && (defined __CSKYABIV2__)
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_PRSTATUS, sizeof(struct pt_regs),
    GENERAL_REGS, csky_fill_pt_gregset, csky_store_pt_gregset},

  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_FPREGSET, sizeof(struct user_fp),
    FP_REGS, csky_fill_pt_vrregset, csky_store_pt_vrregset},
#else  /* not (CSKY_USING_PT_RGESET &&  __CSKYABIV2__) */
  { PTRACE_GETREGS, PTRACE_SETREGS, 0, CSKY_GREG_SIZE, GENERAL_REGS,
    csky_fill_gregset, csky_store_gregset },

  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, 0, CSKY_FREG_SIZE, FP_REGS,
    csky_fill_fpregset, csky_store_fpregset },
#endif /* not (CSKY_USING_PT_RGESET &&  __CSKYABIV2__) */
#endif /* HAVE_PTRACE_GETREGS */
  NULL_REGSET
};

static struct regsets_info csky_regsets_info =
{
  csky_regsets, /* Regsets  */
  0,            /* Num_regsets  */
  NULL,         /* Disabled_regsets  */
};


static struct regs_info regs_info =
{
  NULL, /* FIXME: what's this  */
  &csky_usrregs_info,
  &csky_regsets_info
};

static const struct regs_info *
csky_regs_info (void)
{
  return &regs_info;  
}

/* Implementation of linux_target_ops method "sw_breakpoint_from_kind".  */

static const gdb_byte *
csky_sw_breakpoint_from_kind (int kind, int *size)
{
#ifdef __CSKYABIV2__
#ifdef CSKY_KERNEL_CODE_BTHAN_4X
  if (kind == 4)
    {
      *size = 4;
      return (const gdb_byte *) &csky_breakpoint_illegal_4_v2;
    }
  else
    {
      *size = 2;
      return (const gdb_byte *) &csky_breakpoint_illegal_2_v2;
    }
#else  /* not CSKY_KERNEL_CODE_BTHAN_4X */
  if (kind == 4)
    {
      *size = 4;
      return (const gdb_byte *) &csky_breakpoint_bkpt_4;
    }
  else
    {
      *size = 2;
      return (const gdb_byte *) &csky_breakpoint_bkpt_2;
    }
#endif /* not CSKY_KERNEL_CODE_BTHAN_4X */

#else  /* not __CSKYABIV2__ */
#ifdef CSKY_KERNEL_CODE_BTHAN_4X
  *size = 2;
  return (const gdb_byte *) &csky_breakpoint_trap1_v1;
#else  /* not CSKY_KERNEL_CODE_BTHAN_4X */
  *size = 2;
  return (const gdb_byte *) &csky_breakpoint_bkpt_2;
#endif /* not CSKY_KERNEL_CODE_BTHAN_4X */
#endif /* not __CSKYABIV2__ */

}

static int
csky_supports_z_point_type (char z_type)
{
  /* FIXME: hardware breakpoint support ??  */
  if (z_type == Z_PACKET_SW_BP)
    return 1;

  return 0;
}

static int
csky_breakpoint_at (CORE_ADDR pc)
{
  unsigned int insn;

  /* Here just read 2 bytes, as csky_breakpoint_illegal_4_v2
     is double of csky_breakpoint_illegal_2_v2, csky_breakpoint_bkpt_4
     is double of csky_breakpoint_bkpt_2. Others are 2 bytes bkpt.  */
  (*the_target->read_memory) (pc, (unsigned char *) &insn, 2);

#ifdef __CSKYABIV2__
#ifdef CSKY_KERNEL_CODE_BTHAN_4X
  if (insn == csky_breakpoint_illegal_2_v2)
    return 1;
#else  /* not CSKY_KERNEL_CODE_BTHAN_4X */
  if (insn == csky_breakpoint_bkpt_2)
    return 1;
#endif /* not CSKY_KERNEL_CODE_BTHAN_4X */

#else  /* not __CSKYABIV2__ */
#ifdef CSKY_KERNEL_CODE_BTHAN_4X
  if (insn == csky_breakpoint_trap1_v1)
    return 1;
#else  /* not CSKY_KERNEL_CODE_BTHAN_4X */
  if (ret == csky_breakpoint_bkpt_2)
    return 1
#endif /* not CSKY_KERNEL_CODE_BTHAN_4X */
#endif /* not __CSKYABIV2__ */

  return 0;
}

/* Support for hardware single step.  */

static int
csky_supports_hardware_single_step (void)
{
  return 1;
}

struct linux_target_ops the_low_target = {
  csky_arch_setup,
  csky_regs_info,
  csky_cannot_fetch_register,
  csky_cannot_store_register,
  NULL,  /* FIXME: ??  */
  csky_get_pc,
  csky_set_pc,
  NULL,  /* FIXME: ??  */
  csky_sw_breakpoint_from_kind,
  NULL,  /* FIXME: ??  */
  0,
  csky_breakpoint_at,
  csky_supports_z_point_type, /* FIXME: support later ??  */
  NULL,  /* insert hw/watch breakpoint.  */
  NULL,  /* remov hw/watch breakpoint.  */
  NULL,  /* stopped_by_watchpoint  */
  NULL,  /* stopped_data_address  */
  NULL,  /* collect_ptrace_register  */
  NULL,  /* supply_ptrace_register  */
  NULL,  /* siginfo_fixup  */
  NULL,  /* new_process  */
  NULL,  /* new_thread  */
  NULL,  /* new_fork  */
  NULL,  /* prepare_to_resume  */
  NULL,  /* process_qsupported  */
  NULL,  /* supports_tracepoints  */
  NULL,  /* get_thread_area  */
  NULL,  /* install_fast_tracepoint_jump_pad  */
  NULL,  /* emit_ops  */
  NULL,  /* get_min_fast_tracepoint_insn_len  */
  NULL,  /* supports_range_stepping  */
  NULL,  /* breakpoint_kind_from_current_state  */
  csky_supports_hardware_single_step,
  NULL,  /* get_syscall_trapinfo  */
  NULL,  /* get_ipa_tdesc_idx  */
};

void
initialize_low_arch (void)
{  
#ifdef __CSKYABIV2__
  init_registers_cskyv2_linux ();
#else  /* not __CSKYABIV2__ */
  init_registers_cskyv1_linux ();
#endif /* not __CSKYABIV2__ */
  initialize_regsets_info (&csky_regsets_info);
}
