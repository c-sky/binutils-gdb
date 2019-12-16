/* Native-dependent code for GNU/Linux on CSKY processors.

   Copyright (C) 2012-2016 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "regcache.h"
#include "linux-nat.h"

#include "nat/gdb_ptrace.h"

#include <sys/procfs.h>

#include <asm/ptrace.h>
#include <elf.h>
#include <linux/version.h>
#include "csky-tdep.h"

/* Defines ps_err_e, struct ps_prochandle.  */
#include "gdb_proc_service.h"

/* Fill GDB's register array with the general-purpose register values
   in *GREGSETP.  */

void
supply_gregset (struct regcache* regcache,
		const elf_gregset_t *gregsetp)
{
  int i;
  int base = 0;
  const struct pt_regs *regset = (const struct pt_regs *)gregsetp;

  regcache_raw_supply (regcache, CSKY_R15_REGNUM, &regset->lr);
  regcache_raw_supply (regcache, CSKY_PC_REGNUM, &regset->pc);
  regcache_raw_supply (regcache, CSKY_PSR_REGNUM, &regset->sr);
  regcache_raw_supply (regcache, CSKY_SP_REGNUM, &regset->usp);
 
  regcache_raw_supply (regcache, CSKY_R0_REGNUM, &regset->a0);
  regcache_raw_supply (regcache, CSKY_R0_REGNUM + 1, &regset->a1);
  regcache_raw_supply (regcache, CSKY_R0_REGNUM + 2, &regset->a2);
  regcache_raw_supply (regcache, CSKY_R0_REGNUM + 3, &regset->a3);

  base = CSKY_R0_REGNUM + 4;

  for (i = 0; i < 10; i++)
    regcache_raw_supply (regcache, base + i, &regset->regs[i]);

  base = CSKY_R0_REGNUM + 16;
  for (i = 0; i < 16; i++)
    regcache_raw_supply (regcache, base + i, &regset->exregs[i]);

  regcache_raw_supply (regcache, CSKY_HI_REGNUM, &regset->rhi);
  regcache_raw_supply (regcache, CSKY_LO_REGNUM, &regset->rlo);
}

/* Fill registers in *GREGSETPS with the values in GDB's
   register array.  */

void
fill_gregset (const struct regcache* regcache,
	      elf_gregset_t *gregsetp, int regno)
{
  int i;
  int base = 0;
  struct pt_regs *regset = (struct pt_regs *)gregsetp;

  regcache_raw_collect (regcache, CSKY_R15_REGNUM,  &regset->lr);
  regcache_raw_collect (regcache, CSKY_PC_REGNUM, &regset->pc);
  regcache_raw_collect (regcache, CSKY_PSR_REGNUM, &regset->sr);
  regcache_raw_collect (regcache, CSKY_SP_REGNUM, &regset->usp);

  regcache_raw_collect (regcache, CSKY_R0_REGNUM, &regset->a0);
  regcache_raw_collect (regcache, CSKY_R0_REGNUM + 1, &regset->a1);
  regcache_raw_collect (regcache, CSKY_R0_REGNUM + 2, &regset->a2);
  regcache_raw_collect (regcache, CSKY_R0_REGNUM + 3, &regset->a3);

  base = CSKY_R0_REGNUM + 4;

  for (i = 0; i < 10; i++)
    regcache_raw_collect (regcache, base + i,   &regset->regs[i]);

  base = CSKY_R0_REGNUM + 16;
  for (i = 0; i < 16; i++)
    regcache_raw_collect (regcache, base + i,   &regset->exregs[i]);

  regcache_raw_collect (regcache, CSKY_HI_REGNUM, &regset->rhi);
  regcache_raw_collect (regcache, CSKY_LO_REGNUM, &regset->rlo);
}

/* Transfering floating-point registers between GDB, inferiors and cores.  */

/* Fill GDB's register array with the floating-point register values in
   *FPREGSETP.  */

void
supply_fpregset (struct regcache *regcache,
		 const elf_fpregset_t *fpregsetp)
{
  int i, base;
  const struct user_fp *regset = (const struct user_fp *)fpregsetp;

  base = CSKY_VR0_REGNUM;

  for (i = 0; i < 16; i++)
    regcache_raw_supply (regcache, base + i, &regset->vr[i * 4]);

  base = CSKY_FR0_REGNUMV2;

  for (i = 0; i < 16; i++)
    regcache_raw_supply (regcache, base + i, &regset->vr[i * 4]);
  regcache_raw_supply (regcache, 121, &regset->fcr);
  regcache_raw_supply (regcache, 122, &regset->fesr);
  regcache_raw_supply (regcache, 123, &regset->fid);
}

/* Fill register REGNO (if it is a floating-point register) in
   *FPREGSETP with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_fpregset (const struct regcache *regcache,
	       elf_fpregset_t *fpregsetp, int regno)
{
  int i, base;
  struct user_fp *regset = (struct user_fp *)fpregsetp;

  base = CSKY_VR0_REGNUM;

  for (i = 0; i < 16; i++)
    regcache_raw_collect (regcache, base + i, &regset->vr[i * 4]);
  regcache_raw_collect (regcache, 121, &regset->fcr);
  regcache_raw_collect (regcache, 122, &regset->fesr);
  regcache_raw_collect (regcache, 123, &regset->fid);
}

static void
fetch_regs (struct target_ops *ops, struct regcache *regcache, int regnum)
{
  int tid;

  tid = ptid_get_lwp (inferior_ptid);
  if (tid == 0)
    tid = ptid_get_pid (inferior_ptid);

  {
    struct pt_regs regs;
    struct iovec iov;

    iov.iov_base = &regs;
    iov.iov_len = sizeof (regs);

    if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iov) < 0)
      perror_with_name (_("Couldn't get registers"));

    supply_gregset (regcache, (const elf_gregset_t *)&regs);
  }
}

static void
store_regs (struct target_ops *ops, struct regcache *regcache, int regnum)
{
  int tid;

  tid = ptid_get_lwp (inferior_ptid);
  if (tid == 0)
    tid = ptid_get_pid (inferior_ptid);

  {
    struct pt_regs regs;  /* Size is 152.  */
    struct iovec iov;

    iov.iov_base = &regs;
    iov.iov_len = sizeof (regs);

    if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iov) < 0)
       perror_with_name (_("Couldn't get registers"));

    fill_gregset (regcache, (elf_gregset_t *)&regs, regnum);

    if (ptrace (PTRACE_SETREGSET, tid, NT_PRSTATUS, &iov) < 0)
      perror_with_name (_("Couldn't write registers"));
  }
}

static void
fetch_fpregs (struct target_ops *ops, struct regcache *regcache, int regno)
{
  int tid;

  tid = ptid_get_lwp (inferior_ptid);
  if (tid == 0)
    tid = ptid_get_pid (inferior_ptid);

  {
    struct user_fp regs;
    struct iovec iov;

    iov.iov_base = &regs;
    iov.iov_len = sizeof (regs);

    if (ptrace (PTRACE_GETREGSET, tid, NT_FPREGSET, &iov) < 0)
      perror_with_name (_("Couldn't get registers"));

    supply_fpregset (regcache, (const elf_fpregset_t *)&regs);
  }
}

static void
store_fpregs (struct target_ops *ops, struct regcache *regcache, int regno)
{
  int tid;

  tid = ptid_get_lwp (inferior_ptid);
  if (tid == 0)
    tid = ptid_get_pid (inferior_ptid);

  {
    struct user_fp regs;
    struct iovec iov;

    iov.iov_base = &regs;
    iov.iov_len = sizeof (regs);

    if (ptrace (PTRACE_GETREGSET, tid, NT_FPREGSET, &iov) < 0)
       perror_with_name (_("Couldn't get registers"));

    fill_fpregset (regcache, (elf_fpregset_t *)&regs, regno);

    if (ptrace (PTRACE_SETREGSET, tid, NT_FPREGSET, &iov) < 0)
      perror_with_name (_("Couldn't write registers"));
  }
}

/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers.  */

static void
fetch_inferior_registers (struct target_ops *ops,
			  struct regcache *regcache, int regnum)
{
  fetch_regs (ops, regcache, regnum);
  fetch_fpregs (ops, regcache, regnum);
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers.  */

static void
store_inferior_registers (struct target_ops *ops,
			  struct regcache *regcache, int regnum)
{
  store_regs (ops, regcache, regnum);
  store_fpregs (ops, regcache, regnum);
}


/* Fetch the thread-local storage pointer for libthread_db.  */

ps_err_e
ps_get_thread_area (struct ps_prochandle *ph,
                    lwpid_t lwpid, int idx, void **base)
{
  struct pt_regs regset;
  if (ptrace (PTRACE_GETREGSET, lwpid,
             (PTRACE_TYPE_ARG3) (long) NT_PRSTATUS, &regset) != 0)
    return PS_ERR;

  *base = (void *) regset.tls;

  /* IDX is the bias from the thread pointer to the beginning of the
     thread descriptor.  It has to be subtracted due to implementation
     quirks in libthread_db.  */
  *base = (void *) ((char *)*base - idx);

  return PS_OK;
}

extern initialize_file_ftype _initialize_tile_linux_nat;

void
_initialize_tile_linux_nat (void)
{
  struct target_ops *t;

  /* Fill in the generic GNU/Linux methods.  */
  t = linux_target ();

  /* Add our register access methods.  */
  t->to_fetch_registers = fetch_inferior_registers;
  t->to_store_registers = store_inferior_registers;

  /* Register the target.  */
  linux_nat_add_target (t);
}
