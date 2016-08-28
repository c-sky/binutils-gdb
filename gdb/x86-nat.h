/* Native-dependent code for x86 (i386 and x86-64).

   Low level functions to implement Oeprating System specific
   code to manipulate x86 debug registers.

   Copyright (C) 2009-2016 Free Software Foundation, Inc.

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

#ifndef X86_NAT_H
#define X86_NAT_H 1

#include "breakpoint.h"
#include "nat/x86-dregs.h"
#include "target.h"

/* Hardware-assisted breakpoints and watchpoints.  */

/* Use this function to set x86_dr_low debug_register_length field
   rather than setting it directly to check that the length is only
   set once.  It also enables the 'maint set/show show-debug-regs' 
   command.  */

extern void x86_set_debug_register_length (int len);

/* Use this function to reset the x86-nat.c debug register state.  */

extern void x86_cleanup_dregs (void);

/* Called whenever GDB is no longer debugging process PID.  It deletes
   data structures that keep track of debug register state.  */

extern void x86_forget_process (pid_t pid);

extern int x86_can_use_hw_breakpoint (enum bptype type, int cnt, int othertype);
extern int x86_region_ok_for_hw_watchpoint (CORE_ADDR addr, int len);
extern int x86_stopped_by_watchpoint ();
extern int x86_stopped_data_address (CORE_ADDR *addr_p);
extern int x86_insert_watchpoint (CORE_ADDR addr, int len,
			   enum target_hw_bp_type type,
			   struct expression *cond);
extern int x86_remove_watchpoint (CORE_ADDR addr, int len,
			   enum target_hw_bp_type type,
			   struct expression *cond);
extern int x86_insert_hw_breakpoint (struct gdbarch *gdbarch,
			      struct bp_target_info *bp_tgt);
extern int x86_remove_hw_breakpoint (struct gdbarch *gdbarch,
				     struct bp_target_info *bp_tgt);

/* Convenience template used to add x86 watchpoints support to a
   target.  */

template <class T>
struct x86_nat_target : public T
{
  /* Hook in the x86 hardware watchpoints/breakpoints support.  */

  bool have_continuable_watchpoint () OVERRIDE
  { return true; }

  int can_use_hw_breakpoint (enum bptype type, int cnt, int othertype) OVERRIDE
  { return x86_can_use_hw_breakpoint (type, cnt, othertype); }

  int region_ok_for_hw_watchpoint (CORE_ADDR addr, int len) OVERRIDE
  { return x86_region_ok_for_hw_watchpoint (addr, len); }

  int insert_watchpoint (CORE_ADDR addr, int len,
			 enum target_hw_bp_type type,
			 struct expression *cond) OVERRIDE
  { return x86_insert_watchpoint (addr, len, type, cond); }

  int remove_watchpoint (CORE_ADDR addr, int len,
			 enum target_hw_bp_type type,
			 struct expression *cond) OVERRIDE
  { return x86_remove_watchpoint (addr, len, type, cond); }

  int insert_hw_breakpoint (struct gdbarch *gdbarch,
			    struct bp_target_info *bp_tgt) OVERRIDE
  { return x86_insert_hw_breakpoint (gdbarch, bp_tgt); }

  int remove_hw_breakpoint (struct gdbarch *gdbarch,
			    struct bp_target_info *bp_tgt) OVERRIDE
  { return x86_remove_hw_breakpoint (gdbarch, bp_tgt); }

  int stopped_by_watchpoint () OVERRIDE
  { return x86_stopped_by_watchpoint (); }

  int stopped_data_address (CORE_ADDR *addr_p) OVERRIDE
  { return x86_stopped_data_address (addr_p); }
};


#endif /* X86_NAT_H */
