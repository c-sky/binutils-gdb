/* Header file for csky multi-threads/tasks debug. This is
   an interface file.

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

#ifndef CSKY_KERNEL_H
#define CSKY_KERNEL_H
/* Define kernel_ops interface for multi-threads/tasks debug.
   To make our gdb support multi-threads, the interface should
   be implemented.  */
struct kernel_ops
{
  /* Keep consistent with current thread in the csky-target-board.  */
  ptid_t csky_target_ptid;

  /* Initialize the thread info of specific OS debug target. 
     INTENSITY: for different intensity of initialization.  */
  void (*to_init_thread_info) (int intensity);

  /* Refresh the thread_info of GDB according to local thread info.  */
  int (*to_update_thread_info) (ptid_t *inferior_ptid);

  /* A dummy way to fetch csky registers. 
     In multi-threads' condition, if context is not the current_context 
               of the csky-target, GDB cann't fetch the register.
     So, here a dummy way can solve the problem.
     If the reg_dirty is valid, we get the register value in the buffer,
     else we should fetch the memory.  */
  void (*to_fetch_registers) (ptid_t ptid, int regno, unsigned int *val);

  /* A dummy way to store csky registers. 
     In multi-threads' condition, if context is not the current_context 
               of the csky-target, GDB cann't store the register.
     So, here a dummy way can solve the problem.
     We can store the val to memory.  */
  void (*to_store_registers)(ptid_t ptid, int regno, unsigned int val);

  /* Check whether the thread by the ptid specified is alive or not.  */
  int (*to_thread_alive) (ptid_t ptid);

  /* Change the PID into the char pointer for GDB to print out.  */
  void (*to_pid_to_str) (ptid_t ptid, char *buf);

  /* Implement some commands which multi-threads module support. 
     ARGS: parameter of commands.  */
  void (*to_command_implement) (char* args, int from_tty);
};

enum kernel_ops_sel
{
  DEFAULT = 0,
  ECOS = 1,
  uCOS = 2,
  uCLINUX = 3
};

/* ----------------------------------------------
    reference each module of multi-threads
   ----------------------------------------------  */

/* Kernel_ops level for eCos.  */
extern struct kernel_ops eCos_kernel_ops;

/* Kernel_ops checking function for eCos.  */
int is_eCos_kernel_ops (void);

/* Kernel_ops level for uCOS.  */
extern struct kernel_ops uCOS_kernel_ops;
 
/* Kernel_ops checking function for uCOS.  */
int is_uCOS_kernel_ops (void);

/* Kernel_ops level for uCLinux.  */
extern struct kernel_ops uCLinux_kernel_ops;
 
/* Kernel_ops checking function for uCLinux.  */
int is_uCLinux_kernel_ops (void);

#define NO_KERNEL_OPS  -1
#define NO_INIT_THREAD  -2
#define CURRENT_THREAD_UNCHANGED  0
#define CURRENT_THREAD_CHANGED    1 

#endif /* CSKY_KERNEL_H */
