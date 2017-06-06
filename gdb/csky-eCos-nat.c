/* Multi-process/thread control defs for GDB, the GNU debugger.
   Copyright (C) 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1997, 1998, 1999,
   2000, 2007, 2008, 2009, 2010 Free Software Foundation, Inc.
   Contributed by Lynx Real-Time Systems, Inc.  Los Gatos, CA.

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
#include "value.h"
#include "expression.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "serial.h"
#include "target.h"
#include "exceptions.h"
#include "regcache.h"
#include <ctype.h>
#include "csky-tdep.h"
#include "gdbthread.h"
#include "breakpoint.h"
#include "arch-utils.h"
#include "csky-djp.h"
#include "cli/cli-cmds.h"
#include "csky-kernel.h"
#include <stdlib.h>

/* Source file for csky eCos multi-threads/tasks debug.  */

extern ptid_t remote_csky_ptid;

/* Struction organization for interface from kernel_ops. 
   See file: csky-kernel.h.  */

static void eCos_init_thread_info (int intensity);
static int  eCos_update_thread_info (ptid_t *inferior_ptid);
static void eCos_fetch_registers (ptid_t ptid, int regno, unsigned int *val);
static void eCos_store_register (ptid_t ptid, int regno, unsigned int val);
static int  eCos_thread_alive (ptid_t ptid);
static void eCos_pid_to_str (ptid_t ptid, char* buf);
static void eCos_command_implement (char* args, int from_tty);

struct kernel_ops eCos_kernel_ops = 
{
  {42000, 0, 42000},
  eCos_init_thread_info,
  eCos_update_thread_info,
  eCos_fetch_registers,
  eCos_store_register,
  eCos_thread_alive,
  eCos_pid_to_str,
  eCos_command_implement
};

/* Data Struction about multi-threads implementation.  */

struct Cyg_HardwareThread
{
  unsigned int stack_base;
  unsigned int stack_size;
  unsigned int stack_limit;
  unsigned int stack_ptr;
  /* Thread's stack depth.  */
  unsigned int lowest;
  CORE_ADDR entry_point;
  /* FUN_NAME corresponding to entry_point.  */
  char* fun_name;
  unsigned int entry_data;
};

struct Cyg_SchedThread
{
  CORE_ADDR next;
  CORE_ADDR prev;
  int priority;
  unsigned int timeslice_count;
  CORE_ADDR queue;
  int mutex_count;
  int original_priority;
  int priority_inherited;
};

/* This structure is never used.  */

struct exception_control
{
  CORE_ADDR exception_handler;
  unsigned int exception_data;
};

struct timer
{
  CORE_ADDR next;
  CORE_ADDR prev;
  CORE_ADDR counter;
  CORE_ADDR alarm;
  unsigned int data;
  unsigned long long trigger;  /* 64bits.  */
  unsigned long long interval;
  int enabled;

  /* Address for current thread info, also used as ptid.tid.  */
  CORE_ADDR thread_id;
};

/* ECOS_THREAD_INFO used to recieve the threads' info from the csky target,
   so it should be consistented with the eCos thread struct.  */

typedef struct eCos_thread_info
{
  /* Store all C-sky registers' value.  */
  unsigned int csky_registers[144];

  /* 1: Valid Value; 0:Invalid Value.  */
  unsigned int csky_registers_dirty[144];

  /* Provides hardware abstractions.  */
  struct Cyg_HardwareThread *cyg_hthread;

  /* Provides scheduling abstractions.  */
  struct Cyg_SchedThread *cyg_sthread;

  /* Process state, should be enum datatype.  */
  unsigned int state;

  /* Puspend counts.  */
  unsigned int suspend_count;

  /* Wakeup counts.  */
  unsigned int wakeup_count;

  /* Info for proecss waiting.  */
  unsigned int wait_info;

  unsigned short unique_id; /* 16bits.  */

  struct exception_control *exception_con;

  struct timer *t;

  /* Reason for sleep,should be enum datatype.  */
  unsigned int sleep_reason;

  /* Reason for wakeup,should be enum datatype.  */
  unsigned int wake_reason;

  unsigned int thread_data[6];

  unsigned int thread_data_map;

  /* Thread name.  */
  char* name;

  /* For the eCos_Thread_Info chain.  */
  struct eCos_thread_info *next;

  /* Thread info list head address.  */
  CORE_ADDR thread_list;

  /* Flag for refresh_eCos_thread_list()
     if accessed = 1: this member is accessed
     else, member is not accessed.  */
  unsigned int accessed;
} eCos_thread_info;

/* ECOS_THEAD_LIST used to store the threads' info from the csky target.  */
static eCos_thread_info *eCos_thread_list = NULL;

/* Record last current thread ptid.  */
static ptid_t new_current_thread_ptid;

/* Thread info state descrption.  */
struct thread_id_record
{
  struct thread_id_record *next;
  
  /* Record every ptid state.
     0: this ptid exists in previous eCos_thread_list, but not in this.
     1: this ptid exists in both previous eCos_thread_list and this.
     2: this ptid is a new one.  */
  int thread_id_flag;

  /* All threads' ptid in GDB thread_list.  */
  ptid_t ptid;
};

/* Record all the threads, in GDB thread_list.  */
static struct thread_id_record *thread_id_list = NULL;

/* Variable in eCos thread info, these expressions are used to
   count the offset from Cyg_Thread::thread_list.  */
#define THREAD_LIST_ADDR           0
#define CURRENT_THREAD_ADDR        1
#define STACK_BASE_OFFSET          2
#define STACK_PTR_OFFSET           3
#define ENTRY_POINT_OFFSET         4
#define UNIQUE_ID_OFFSET           5
#define THREAD_ID_OFFSET           6
#define THREAD_NAME_OFFSET         7
#define LIST_NEXT_OFFSET           8
#define PRIORITY_OFFSET            9
#define STATE_OFFSET              10
#define SUSPEND_COUNT_OFFSET      11
#define WAKE_COUNT_OFFSET         12
#define WAIT_INFO_OFFSET          13
#define SLEEP_REASON_OFFSET       14
#define WAKE_REASON_OFFSET        15
#define MUTEX_COUNT_OFFSET        16
#define PRIO_INHERIT_OFFSET       17
#define PRIO_ORIGINAL_OFFSET      18
#define STACK_SIZE_OFFSET         19
#define ENTRY_DATA_OFFSET         20

static const char* eCos_thread_info_name[] =
{
  "& Cyg_Thread::thread_list",
  "& Cyg_Scheduler_Base::current_thread",
  "& ((Cyg_Thread*)0)->stack_base",
  "& ((Cyg_Thread*)0)->stack_ptr",
  "& ((Cyg_Thread*)0)->entry_point",
  "& ((Cyg_Thread*)0)->unique_id",
  "& ((Cyg_Thread*)0)->timer->thread",
  "& ((Cyg_Thread*)0)->name",
  "& ((Cyg_Thread*)0)->list_next",
  "& ((Cyg_Thread*)0)->priority",
  "& ((Cyg_Thread*)0)->state",
  "& ((Cyg_Thread*)0)->suspend_count",
  "& ((Cyg_Thread*)0)->wakeup_count",
  "& ((Cyg_Thread*)0)->wait_info",
  "& ((Cyg_Thread*)0)->sleep_reason",
  "& ((Cyg_Thread*)0)->wake_reason",
  "& ((Cyg_Thread*)0)->mutex_count",
  "& ((Cyg_Thread*)0)->priority_inherited",
  "& ((Cyg_Thread*)0)->original_priority",
  "& ((Cyg_Thread*)0)->stack_size",
  "& ((Cyg_Thread*)0)->entry_data"
};

/* Record the offset of each element in array eCos_thread_info_name[]
   If eCos_thread_info_elements_offset[i] = -1, need to read elf file.
   else use this directly.  */
static long int eCos_thread_info_elements_offset[] =
{
/* THREAD_LIST_ADDR, CURRENT_THREAD_ADDR, STACK_BASE_OFFSET  */
       -1,               -1,                 -1,
/* STACK_PTR_OFFSET, ENTRY_POINT_OFFSET, UNIQUE_ID_OFFSET  */
       -1,               -1,                 -1,
/* THREAD_ID_OFFSET, THREAD_NAME_OFFSET, LIST_NEXT_OFFSET  */
       -1,               -1,                 -1,
/* PRIORITY_OFFSET, STATE_OFFSET, SUSPEND_COUNT_OFFSET  */
       -1,               -1,                 -1,
/* WAKE_COUNT_OFFSET, WAIT_INFO_OFFSET, SLEEP_REASON_OFFSET  */
       -1,               -1,                 -1,
/* WAKE_REASON_OFFSET, MUTEX_COUNT_OFFSET, PRIO_INHERIT_OFFSET  */
       -1,               -1,                 -1,
/* PRIO_ORIGINAL_OFFSET, STACK_SIZE_OFFSET, ENTRY_DATA_OFFSET  */
       -1,               -1,                 -1
};

/* Get the CORE_ADDR according to expression.  */

static int eCos_get_label_value (CORE_ADDR *val, int index);

/* Refresh the eCos thread list.  */

static void refresh_eCos_thread_list (void);

/* Get the eCos_thread_info struct by the ptid specified.  */

static eCos_thread_info* find_eCos_thread_info(ptid_t);

/* Transfer the ECOS_THREAD_INFO struct into the
   THREAD_INFO struct for GDB.  */

static void csky_thread_info_xfer (struct thread_info*, eCos_thread_info*);

/* Read current_thread_info in csky-target-board.  */

static int refresh_eCos_current_thread (void);

/* Cache thread info in GDB thread_list.  */

static void thread_id_list_cache (void);

/* Update thread_id_list according to eCos_thread_list.  */

static void refresh_thread_id_list (void);


/* To check whether the kernel ops is eCos_kernel_ops or not
   return: 0 -> failure: target kernel ops should not be eCos kernel_ops
           1 -> success: target kernel ops is eCos kernel_ops.  */

int
is_eCos_kernel_ops (void)
{
  /* FIXME: The way we check is very limited
     FIXME: Should we check, or return 1 directory ?
  if (!eCos_get_label_value (NULL, THREAD_LIST_ADDR))
    {
      warning ("Programe is not eCos multi-thread.");
      return 0;
    }  */
  return 1;
}

static void
eCos_pid_to_str (ptid_t ptid, char* buf)
{
  eCos_thread_info *t;

  t = find_eCos_thread_info (ptid);

  if (t)
    {
      snprintf (buf, 64 * sizeof (char*), "Thread 0x%x <%s>", 
		(unsigned int)ptid.tid, t->name);
    }
  return;  
}

static int
eCos_thread_alive (ptid_t ptid)
{
  eCos_thread_info *t;

  t = find_eCos_thread_info (ptid);

  if (t || ptid_equal (ptid, eCos_kernel_ops.csky_target_ptid))
    return 1;

  return 0; 
}

/* Initial eCos_thread_list. 
   Here we do not simply deal with eCos_thread_list by free() function.
   Because many members in eCos_thread_info is once effective, permanent,
   We can only make each time different variables cleared.
   It will improve debugging speed for us to use free() as little as possible.
   INTENSITY: initial flag. when 1: initial eCos_thread_list totally by free()
                            when 0: use free() as little as possible.  */

static void
eCos_init_thread_info (int intensity)
{
  eCos_thread_info*  t = eCos_thread_list;

  if (intensity)
    { 
      while (t != NULL)
        {
          eCos_thread_info*  p = t;
          t = p->next;
          if (p->cyg_hthread)
            free (p->cyg_hthread);
          if (p->t)
            free (p->t);
          if (p->name)
            free (p->name);
          if (p->cyg_sthread)
            free (p->cyg_sthread);
          if (p->exception_con)
            free (p->exception_con);

          free (p);
        }
      eCos_thread_list = NULL;
    }
  else /* INTENSITY == 0, make each time different variables cleared.  */
    {
      while (t != NULL)
        {
          int i;
          eCos_thread_info*  p = t;
          t = p->next;

          if (p->cyg_hthread)
            {
              p->cyg_hthread->stack_ptr = 0;
              p->cyg_hthread->lowest = 0;
            }
          if (p->t)
            {
              p->t->data = 0;
              p->t->trigger = 0;
              p->t->interval = 0;
              p->t->enabled = 0;
            }
          if (p->cyg_sthread)
            {
              /* FIXME ???  */
              p->cyg_sthread->priority = 0;
              p->cyg_sthread->timeslice_count = 0;
              p->cyg_sthread->mutex_count = 0;
              p->cyg_sthread->original_priority = 0;
              p->cyg_sthread->priority_inherited = 0;
            }
          if (p->exception_con)
            {
              /* FIXME???  */
              p->exception_con->exception_data = 0;
            }

          for (i = 0; i < 144; i++)
            {
              p->csky_registers_dirty[i] = 0;
            }
          for (i = 0; i < 6; i++)
            {
              p->thread_data[i] = 0;
            }
          p->state = 0;
          p->suspend_count = 0;
          p->wakeup_count = 0;
          p->wait_info = 0;
          p->unique_id = 0;
          p->sleep_reason = 0;
          p->wake_reason = 0;
          p->thread_data_map = 0;
          p->accessed = 0;
        }
    }

  return;
}

/* Get the CORE_ADDR according to expression.
   EXPRESSION: the name of expressoin.
   VAL: the CORE_ADDR you want to get.
   INDEX: the index of the member in eCos_thread_info_elements_offset[]
   return: 0 -> failure: there is no expression in your elf file.
           1 -> success: VAL is valid.  */

static int
eCos_get_label_value (CORE_ADDR *val, int index)
{
  struct expression *expr;
  struct value *value;
  struct cleanup *old_chain = 0;
  struct gdb_exception exception = exception_none;

  if (eCos_thread_info_elements_offset[index] >= 0)
    {
      /* Offset in buffer is valid,no need to resume.  */
      if (val)
        *val = eCos_thread_info_elements_offset[index];
    
      return 1;
    }
  /* Step1: make sure EXPRESSION exist in executable file.  */
  TRY
    {
      parse_expression ((const char *)eCos_thread_info_name[index]);
    }
  CATCH (ex, RETURN_MASK_ALL)
    {
      exception = ex;
    }

  if (exception.reason != 0)
    {
      return 0;
    }

  /* Step2: get the val by expression.  */
  if (val)
    {
      expr = parse_expression (eCos_thread_info_name[index]);
      old_chain = make_cleanup (free_current_contents, &expr);
      value = evaluate_expression (expr);
      do_cleanups (old_chain);
      /* Take target byte_order in consideration.  */
      *val = extract_unsigned_integer (value_contents_raw (value), 
		4, gdbarch_byte_order (get_current_arch ()));
      /* Enable offset in buffer.  */
      eCos_thread_info_elements_offset[index] = *val;
    } 
  return 1;
}

#define IS_VALID_THREAD_ID(addr)  \
      ( ((addr) != 0) \
       && (((addr) & 7) == 0) \
        )

/* Read current_thread_info in csky-target-board.  */

static int
refresh_eCos_current_thread (void)
{
  enum bfd_endian byte_order;
  CORE_ADDR Current_Thread_Addr, Current_Thread_VAL;
  CORE_ADDR cyg_thread_list_addr, cyg_thread_list;

  eCos_get_label_value (&Current_Thread_Addr, CURRENT_THREAD_ADDR);
  eCos_get_label_value (&cyg_thread_list_addr, THREAD_LIST_ADDR);

  byte_order = gdbarch_byte_order (get_current_arch ());
  Current_Thread_VAL = read_memory_unsigned_integer (
			Current_Thread_Addr, 4, byte_order);
  cyg_thread_list = read_memory_unsigned_integer (
			cyg_thread_list_addr, 4, byte_order);

  if (IS_VALID_THREAD_ID(Current_Thread_VAL)
      && IS_VALID_THREAD_ID(cyg_thread_list))
    {
      new_current_thread_ptid = ptid_build (remote_csky_ptid.pid,
                                            0,
                                            Current_Thread_VAL);
      return 0;
    }

  return -1;
}

/* Cache thread info in GDB thread_list.  */

static void
thread_id_list_cache (void)
{
  struct thread_info *tmp_thread_list =
               any_thread_of_process (remote_csky_ptid.pid);
  struct thread_id_record *tmp_thread_id_list = thread_id_list;

  /* Step1: initial thread_id_list.  */
  while (tmp_thread_id_list)
    {
      struct thread_id_record *p;
      p = tmp_thread_id_list;
      tmp_thread_id_list = tmp_thread_id_list->next;
      free (p);
    }
  thread_id_list = NULL;

  /* Step2: iterate thread_info,and malloc space to build thread_id_list.  */ 
  for ( ; tmp_thread_list != NULL; tmp_thread_list = tmp_thread_list->next)
    {
      tmp_thread_id_list = (thread_id_record*) malloc (
                                sizeof (struct thread_id_record));
      tmp_thread_id_list->ptid = tmp_thread_list->ptid;
      tmp_thread_id_list->thread_id_flag = 0;
      tmp_thread_id_list->next = thread_id_list;
      thread_id_list = tmp_thread_id_list; 
    }

  return;
}

/* Find proper thread_id in thread_id_list.  */

static struct
thread_id_record* find_thread_id_record (ptid_t ptid)
{
  struct thread_id_record *t;
  for (t = thread_id_list; t != NULL; t = t->next)
    {
      if (ptid_equal (t->ptid, ptid))
        return t;
    }
  return NULL;
}

/* Update thread_id_list according to eCos_thread_list.  */

static void
refresh_thread_id_list (void)
{
  eCos_thread_info *p;
  struct thread_id_record *t;
  
  for (p = eCos_thread_list; p != NULL; p = p->next)
    {
      /* accessed == 1: thread is in csky-target-board now.  */
      if (p->accessed == 1)
        {
          t = find_thread_id_record (ptid_build (remote_csky_ptid.pid,
                                                 0,
                                                 p->t->thread_id));
          if (t)
            {
              /* This one exists both pre and current eCos_thread_list.  */
              t->thread_id_flag = 1;
            }
          else
            {
              /* This thread_id is a new one.  */
              t = (thread_id_record*) malloc (
                            sizeof (struct thread_id_record));
              t->ptid = ptid_build (remote_csky_ptid.pid, 0, p->t->thread_id);
              t->thread_id_flag = 2;
              t->next = thread_id_list;
              thread_id_list = t;
            }
        }
    }
  return;
}

#define IS_VALID_THREAD_ID(addr)  \
      ( ((addr) != 0) \
       && (((addr) & 7) == 0) \
        )

/* Refresh eCos_thread_list.
   initialize the exist one first,and then 
   create a new one.  */

static void
refresh_eCos_thread_list (void)
{
  /* Thread list addr of csky-target-board.  */
  CORE_ADDR Cyg_Thread_List_Base;
  struct timer *time_tmp;
  struct Cyg_HardwareThread *hthread_tmp;
  unsigned int thread_id, stack_ptr,stack_base, entry_point;
  eCos_thread_info *tmp = NULL;
  eCos_thread_info *tmpprev = NULL;
  /* For each member in eCos_thread_info.  */
  CORE_ADDR thread_list, thread_name, offset = 0;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Initial eCos_thread_list.  */
  eCos_init_thread_info (0);
  
  /* Get Cyg_Thread_List_Base from csky-target-board.  */
  eCos_get_label_value (&offset, THREAD_LIST_ADDR);
  Cyg_Thread_List_Base = read_memory_unsigned_integer (offset,
                                                       4,
                                                       byte_order);
  thread_list = Cyg_Thread_List_Base;

  /* Read Cyg_Thread::thread_list,and refresh eCos_thread_list. 
     Here we read thread_id first,
     if thread_id exist in eCos_thread_list, we refresh this
     eCos_thread_info, else we malloc() an eCos_thread_info,
     and add it to eCos_thread_list.  */
  do
    {  
      char name[65]; /* Thread name should not longer than 65bytes.  */
      unsigned int k, i = 0, j = 0;

      for (k = 0; k < 65; k++)
        {
          name[k] = '\0';
        }
      eCos_get_label_value (&offset, THREAD_ID_OFFSET);
      thread_id = read_memory_unsigned_integer (thread_list + offset,
                                                4,
                                                byte_order);

      /* Find eCos_thread_info in eCos_thread_list by ptid.  */
      tmp = find_eCos_thread_info (ptid_build (remote_csky_ptid.pid,
                                               0,
                                               thread_id));
    
      if (tmp == NULL)
        {
          /* No eCos_thread_info in eCos_thread_list,
             we should malloc() a new one.  */
          tmp = (eCos_thread_info*) malloc (sizeof (eCos_thread_info));
          memset (tmp, 0, sizeof (eCos_thread_info));
        }

      if (tmp->cyg_hthread == NULL)
        {
          hthread_tmp = (Cyg_HardwareThread*) malloc (
                            sizeof (struct Cyg_HardwareThread));
          memset (hthread_tmp, 0, sizeof (struct Cyg_HardwareThread));
          tmp->cyg_hthread = hthread_tmp;
        }
    
      if (tmp->cyg_hthread->stack_base == 0)
        {
          eCos_get_label_value (&offset, STACK_BASE_OFFSET);
          stack_base = read_memory_unsigned_integer (thread_list + offset,
                                                     4,
                                                     byte_order);
          tmp->cyg_hthread->stack_base = stack_base;
        }

      if (tmp->cyg_hthread->stack_ptr == 0)
        {
          eCos_get_label_value (&offset, STACK_PTR_OFFSET);
          stack_ptr = read_memory_unsigned_integer (thread_list + offset,
                                                    4,
                                                    byte_order);
          tmp->cyg_hthread->stack_ptr = stack_ptr;
        }

      if (tmp->cyg_hthread->entry_point == 0)
        {
          eCos_get_label_value (&offset, ENTRY_POINT_OFFSET);
          entry_point = read_memory_unsigned_integer (thread_list + offset,
                                                      4,
                                                      byte_order);
          tmp->cyg_hthread->entry_point = entry_point;
          find_pc_partial_function (entry_point,
                                    (const char **)
                                      &tmp->cyg_hthread->fun_name,
                                    NULL, NULL);
        }

      if (tmp->name == NULL)
        {
          eCos_get_label_value (&offset, THREAD_NAME_OFFSET);
          thread_name = read_memory_unsigned_integer (thread_list + offset,
                                                      4,
                                                      byte_order);

          if (thread_name)
            {
              name[0] = read_memory_unsigned_integer (thread_name + i,
                                                      1,
                                                      byte_order);
              while (name[i] != '\0' && i < 65)
                {
                  i++;
                  name[i] = read_memory_unsigned_integer (thread_name + i,
                                                          1,
                                                          byte_order);
                }
   
            }

          tmp->name = (char *) malloc (sizeof (char) * (i + 1));
          for (j = 0; j < i; j++)
             {
               *(tmp->name + j) = name[j];
             }
          *(tmp->name + i) = '\0';
        }
    
      if (tmp->t == NULL)
        {
          time_tmp = (timer *) malloc (sizeof (struct timer));
          memset(time_tmp, 0, sizeof (struct timer));
          tmp->t = time_tmp;
          tmp->t->thread_id = thread_id;
        }
    
      /* Counte the value of SP for each thread. FIXME.  */
      tmp->csky_registers[CSKY_SP_REGNUM] = tmp->cyg_hthread->stack_ptr
                                            + (18 * 4);
      tmp->csky_registers_dirty[CSKY_SP_REGNUM] = 1;

      /* Now put the *tmp into the eCos_thread_list. 
         if tmp->thread_list == 0, thus, tmp is a new one,
         else, eCos_thread_info existed in eCos_thread_list.  */
      if (tmp->thread_list == 0)
        {
          tmp->thread_list = Cyg_Thread_List_Base;
          tmp->next = eCos_thread_list;
          eCos_thread_list = tmp;
        }

      /* Set accessed flag.  */
      tmp->accessed = 1;

      eCos_get_label_value (&offset, LIST_NEXT_OFFSET);
      thread_list = read_memory_unsigned_integer (thread_list + offset,
                                                  4,
                                                  byte_order);

    }
    /* Finish iterating the loop.  */
  while (Cyg_Thread_List_Base != thread_list);

  return;
}

static void 
csky_thread_info_xfer (struct thread_info* d_ops, eCos_thread_info* s_ops)
{
  d_ops->control.step_frame_id.stack_status = FID_STACK_VALID;
  d_ops->control.step_frame_id.code_addr_p = 1;
  d_ops->control.step_frame_id.special_addr_p = 0;
  d_ops->control.step_frame_id.stack_addr = s_ops->cyg_hthread->stack_base;
  d_ops->control.step_frame_id.code_addr = s_ops->cyg_hthread->entry_point;
 
  d_ops->control.step_stack_frame_id = d_ops->control.step_frame_id;
}

static eCos_thread_info*
find_eCos_thread_info (ptid_t ptid)
{
  eCos_thread_info *tp;
  for (tp = eCos_thread_list; tp != NULL; tp = tp->next)
    {
      if (ptid.tid == tp->t->thread_id)
        return tp;
    }
  return NULL;
}

static void
eCos_fetch_registers (ptid_t ptid, int regno, unsigned int* val)
{
  /* If the reg_dirty is valid,we get the register value in the buffer,
     else we should fetch the memory.  */
  
  eCos_thread_info *tp = find_eCos_thread_info (ptid);
  
  if (tp->csky_registers_dirty[regno])
    {
      *val = tp->csky_registers[regno];
      return;
    }
  else
    { 
      enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());
      /* r1-r15.  */
      if ((regno >= 1) && (regno <= 15) && regno != CSKY_SP_REGNUM)
        {
          tp->csky_registers[regno] = read_memory_unsigned_integer (
                                  tp->cyg_hthread->stack_ptr + 4 * (1 + regno),
                                  4,
                                  byte_order);
          *val = tp->csky_registers[regno];
          /* EnValid the register buffer.  */
          tp->csky_registers_dirty[regno] = 1;
        }
      else if (regno == 89)  /* For PSR.  */
        {
          tp->csky_registers[regno] = read_memory_unsigned_integer (
                                  tp->cyg_hthread->stack_ptr + 4,
                                  4, byte_order);
          *val = tp->csky_registers[regno];
          /* EnValid the register buffer.  */
          tp->csky_registers_dirty[regno] = 1;
        }
      else if(regno == 72)  /* For PC  */
        {
          tp->csky_registers[regno] = read_memory_unsigned_integer (
                                  tp->cyg_hthread->stack_ptr,
                                  4, byte_order);
          *val = tp->csky_registers[regno];
          /* EnValid the register buffer.  */
          tp->csky_registers_dirty[regno] = 1;
        }
      else  /* Invalid regno.  */
        {
          *val = 0;
        }
    }
  return;
}

/* In eCos Multi-threads, valid regno: 1-15(r1-r15); 89(psr);72(pc)
   if regno is not valid, we do nothing, else we store value to memory and
   we should update the csky_registers_dirty at the same time.  */

static void
eCos_store_register (ptid_t ptid, int regno, unsigned int val)
{
  eCos_thread_info *tp = find_eCos_thread_info (ptid);

  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());
  /* r1-r15.  */
  if ((regno >= 1) && (regno <= 15) && regno != CSKY_SP_REGNUM)
    {
      write_memory_unsigned_integer (
                        tp->cyg_hthread->stack_ptr + 4 * (1 + regno),
                        4,
                        byte_order,
                        val);
      tp->csky_registers[regno] = val;
      /* EnValid the register buffer.  */
      tp->csky_registers_dirty[regno] = 1;
    }
  else if (regno == 89)  /* For PSR.  */
    {
      write_memory_unsigned_integer (tp->cyg_hthread->stack_ptr + 4,
                                     4, byte_order, val);
      tp->csky_registers[regno] = val;
      /* EnValid the register buffer.  */
      tp->csky_registers_dirty[regno] = 1;
    }
  else if(regno == 72)  /* For PC  */
    {
      write_memory_unsigned_integer (tp->cyg_hthread->stack_ptr,
                                     4, byte_order, val);
      tp->csky_registers[regno] = val;
      /* EnValid the register buffer.  */
      tp->csky_registers_dirty[regno] = 1;
    }
  else  /* Invalid regno.  */
    {
      /* Do nothing here.  */
    }
  return;
}

static int
eCos_update_thread_info (ptid_t *inferior_ptid)
{
  /* refresh the eCos_thread_list & updata thread_list.  */
  eCos_thread_info *t;
  struct thread_info *p;
  struct thread_id_record *tmp;
  
  if (refresh_eCos_current_thread () < 0)
    {
      /* return negetive: no multi-threads in csky-target-board now.  */
      return NO_INIT_THREAD;
    }
  /* There is no need to refresh eCos_thread_list each time.
     We should check whether refresh_eCos_thread_list() or not.  */
  if (ptid_equal (eCos_kernel_ops.csky_target_ptid, new_current_thread_ptid))
    {
      /* Current_thread_ptid in csky-target-board has not changed,
         So, here we can return directly.  */
       return CURRENT_THREAD_UNCHANGED;
    }

  eCos_kernel_ops.csky_target_ptid = new_current_thread_ptid;

  /* Cache the thread_list of the GDB.  */
  thread_id_list_cache ();

  /* Initialize eCos_thread_list, update it according to target info.  */
  refresh_eCos_thread_list ();

  /* Update thread_id_list.  */
  refresh_thread_id_list ();
 
  /* Update thread_list in GDB. */
  for (tmp = thread_id_list; tmp != NULL; tmp = tmp->next)
    {
      if (ptid_equal (tmp->ptid, eCos_kernel_ops.csky_target_ptid))
        continue; /* Deal with inferior_ptid at last.  */
    
      switch (tmp->thread_id_flag)
        {
        case 0: /* Exist in pre but not current.  */
          delete_thread_silent (tmp->ptid);
          break;
        case 1: /* Exist in both pre and current.  */
          break;
        case 2: /* A new one.  */
          p = add_thread_silent (tmp->ptid);
          t = find_eCos_thread_info (tmp->ptid);
          csky_thread_info_xfer (p, t);
          break;
        /* There should not be thread_id which thread_id_flag is any other.  */
        default: ;
        }
    }

  for (tmp = thread_id_list; tmp != NULL; tmp = tmp->next)
    {
      if (ptid_equal (tmp->ptid, eCos_kernel_ops.csky_target_ptid))
        {
          if (tmp->thread_id_flag == 2)
            {
              p = add_thread_silent (tmp->ptid); 
              /* t = find_eCos_thread_info(tmp->ptid);
                 csky_thread_info_xfer(p, t);  */
            }
          break;
        }
    }

  *inferior_ptid = eCos_kernel_ops.csky_target_ptid;
  registers_changed_ptid (*inferior_ptid);

  return CURRENT_THREAD_CHANGED;
}


/* ------------multi-threads commands implementation---------------  */

static const char* thread_state[] = {
  ":RUNNING",
  ":SLEEPING",
  "2",
  "3",
  ":SUSPENDED"
};

static void eCos_threads_list_command (char* args, int from_tty);
static void eCos_thread_info_command (char* args, int from_tty);
static void eCos_threads_stack_command (char* args, int from_tty);
static void eCos_stack_all_command (char* args, int from_tty);
static void eCos_stack_depth_command (char* args, int from_tty);
static void eCos_single_stack_depth_handler (ptid_t ptid);
static unsigned long long 
eCos_get_lowest_sp (unsigned long long start, unsigned long long end);

/* Implement some commands which multi-threads support.
   For eCos module, we support following commands:
   1. info mthreads list
   2. info mthreads ID   // (exp: ID = 0x8100402c)
   3. info mthreads stack all
   4. info mthreads stack [ID]  // if no [ID], list all threads' history stack.
   ARGS: each command's parameter.
   FROM_TTY:  */

static void
eCos_command_implement (char* args, int from_tty)
{
  const char LIST[]  = "list";
  const char ID[]    = "0x";
  const char STACK[] = "stack ";

  /* This command need args.  */
  if (args == NULL)
    {
      error ("Command:\"info mthreads\" need arguments.\nTry\"help"
             " info mthreads\" for more information.");
    }
  
  /* Analysis parameters and determine handler function.  */
  if (strncmp (args, LIST, (sizeof (LIST) - 1)) == 0 
      && (args[strlen(LIST)] == ' ' || args[strlen(LIST)] == '\0'))
    {
      eCos_threads_list_command (&(args[strlen(LIST)]), from_tty);
      return;
    }
  else if (strncmp (args, ID, (sizeof (ID) - 1)) == 0)
    {
      eCos_thread_info_command (args, from_tty);
      return;
    }
  else if (strncmp (args, STACK, (sizeof(STACK) - 1)) == 0)
    {
      eCos_threads_stack_command (&(args[strlen(STACK)]), from_tty);
      return;
    }
  
  /* Invalid parameter.  */
  printf_filtered ("Illegal args,try \"help info mthreads\"for"
                   " more information.\n");
  return;
}

/* List all threads in csky-target-board.
   ARGS will be ignored.  */

static void
eCos_threads_list_command (char* args, int from_tty)
{
  eCos_thread_info *tmp;
  struct Cyg_SchedThread *tmp_sthread;
  struct cleanup *cleanup_threads_list;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());
  struct ui_out *uiout = current_uiout;

  /* This commands need no args.  */
  if (*args != '\0')
    {
      printf_filtered ("info mthreads list command omit all args.\n");
    }

  /* Organize threads list according to eCos_thread_list.  */
  cleanup_threads_list = 
           make_cleanup_ui_out_tuple_begin_end (uiout, "threadslist");
  for (tmp = eCos_thread_list; tmp != NULL; tmp = tmp->next)
    {
      if (tmp->accessed == 1)
        {
          CORE_ADDR offset;
          struct cleanup *cleanup_frame =
                 make_cleanup_ui_out_tuple_begin_end (uiout, "frame");
          ui_out_text (uiout, "#");
          ui_out_field_core_addr (uiout, "id",
                                  get_current_arch (), tmp->t->thread_id);
          ui_out_text (uiout, " ");
  
          if (tmp->cyg_sthread == NULL)
            {
              tmp_sthread = (Cyg_SchedThread*) malloc (
                                            sizeof (struct Cyg_SchedThread));
              memset (tmp_sthread, 0, sizeof (struct Cyg_SchedThread));
              tmp->cyg_sthread = tmp_sthread;
            }
          if (tmp->cyg_sthread->priority == 0)
            {
              int priority;
              eCos_get_label_value (&offset, PRIORITY_OFFSET);
              priority = read_memory_unsigned_integer (
                            tmp->t->thread_id + offset, 4, byte_order);
              tmp->cyg_sthread->priority = priority;
            }
          ui_out_field_int (uiout, "priority", tmp->cyg_sthread->priority);
          ui_out_text (uiout, " ");

          if (tmp->state == 0)
            {
              int state;
              eCos_get_label_value (&offset, STATE_OFFSET);
              state = read_memory_unsigned_integer (
                         tmp->t->thread_id + offset, 4, byte_order);
              tmp->state = state;
            }
          ui_out_field_int (uiout, "state", tmp->state);
          ui_out_text (uiout, thread_state[tmp->state]);
          ui_out_text (uiout, " ");

          if (tmp->cyg_hthread->entry_point == 0)
            {
              int entry_ptr;
              eCos_get_label_value(&offset, ENTRY_POINT_OFFSET);
              entry_ptr = read_memory_unsigned_integer (
                             tmp->t->thread_id + offset, 4, byte_order);
              tmp->cyg_hthread->entry_point = entry_ptr;
            }
          ui_out_field_core_addr (uiout, "entry_ptr",
                      get_current_arch (), tmp->cyg_hthread->entry_point);
          ui_out_text (uiout, " ");

          ui_out_field_string (uiout, "name", tmp->name);
          ui_out_text (uiout, "\n");
          do_cleanups (cleanup_frame);
        }
    }
  do_cleanups (cleanup_threads_list);

  return;
}

/* List a single thread detailed info.
   ARGS: the chosen thread.  */

static void
eCos_thread_info_command (char* args, int from_tty)
{
  eCos_thread_info *tmp;
  struct Cyg_SchedThread *cyg_sthread;
  CORE_ADDR thread_id, offset;
  struct cleanup *cleanup_thread_info;
  struct ui_out *uiout = current_uiout;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());
  static const char* cyg_reason[] = {":Cyg_Thread::NONE",":Cyg_Thread::WAIT",
            ":Cyg_Thread::DELAY",":Cyg_Thread::TIMEOUT",":Cyg_Thread::BREAK",
            ":Cyg_Thread::DESTRUCT",":Cyg_Thread::EXIT",":Cyg_Thread::DONE"};

  /* Analysis args, get ID and assign to thread_id.  */
  thread_id = strtoll (args, NULL, 16);
  tmp = find_eCos_thread_info (ptid_build (remote_csky_ptid.pid,
                                           0, thread_id));
  if (tmp == NULL || tmp->accessed == 0)
    {
      printf_filtered ("Thread 0x%x is not alive.\n",
                       (unsigned int)thread_id);
      return;
    }
  
  /* Organize single thread info according eCos_thread_info.  */
  if (tmp->cyg_sthread == NULL)
    {
      cyg_sthread = (Cyg_SchedThread*) malloc (
                                      sizeof (struct Cyg_SchedThread));
      memset (cyg_sthread, 0, sizeof (struct Cyg_SchedThread));
      tmp->cyg_sthread = cyg_sthread;
    }
  if (tmp->cyg_sthread->priority == 0)
    {
      int priority;
      eCos_get_label_value (&offset, PRIORITY_OFFSET);
      priority = read_memory_unsigned_integer (thread_id + offset,
                                               4, byte_order);
      tmp->cyg_sthread->priority = priority;
    }
  if (tmp->cyg_sthread->mutex_count == 0)
    {
      int mutex_count;
      eCos_get_label_value (&offset, MUTEX_COUNT_OFFSET);
      mutex_count = read_memory_unsigned_integer (thread_id + offset,
                                                  4, byte_order);
      tmp->cyg_sthread->mutex_count = mutex_count;
    }
  if (tmp->cyg_sthread->priority_inherited == 0)
    {
      int prio_inherit;
      eCos_get_label_value (&offset, PRIO_INHERIT_OFFSET);
      prio_inherit = read_memory_unsigned_integer (thread_id + offset,
                                                   4, byte_order);
      tmp->cyg_sthread->priority_inherited = prio_inherit;
    }
  if (tmp->cyg_sthread->original_priority == 0)
    {
      int prio_original;
      eCos_get_label_value (&offset, PRIO_ORIGINAL_OFFSET);
      prio_original = read_memory_unsigned_integer (thread_id + offset,
                                                    4, byte_order);
      tmp->cyg_sthread->priority_inherited = prio_original;
    }
  if (tmp->state == 0)
    {
      int state;
      eCos_get_label_value (&offset, STATE_OFFSET);
      state = read_memory_unsigned_integer (thread_id + offset,
                                            4, byte_order);
      tmp->state = state;
    }
  if (tmp->cyg_hthread->entry_point == 0)
    {
      unsigned int entry_ptr;
      eCos_get_label_value (&offset, ENTRY_POINT_OFFSET);
      entry_ptr = read_memory_unsigned_integer (thread_id + offset,
                                                4, byte_order);
      tmp->cyg_hthread->entry_point = entry_ptr;
      /* Find fun_name corresponding to entry_point.  */
      find_pc_partial_function (entry_ptr,
                                (const char **) &tmp->cyg_hthread->fun_name,
                                NULL, NULL);
    } 
  if (tmp->suspend_count == 0)
    {
      int susp_count;
      eCos_get_label_value (&offset, SUSPEND_COUNT_OFFSET);
      susp_count = read_memory_unsigned_integer (thread_id + offset,
                                                 4, byte_order);
      tmp->suspend_count = susp_count;
    }
  if (tmp->wakeup_count == 0)
    {
      int wakeup_count;
      eCos_get_label_value (&offset, WAKE_COUNT_OFFSET);
      wakeup_count = read_memory_unsigned_integer (thread_id + offset,
                                                   4, byte_order);
      tmp->wakeup_count = wakeup_count;
    }
  if (tmp->wait_info == 0)
    {
      int wait_info;
      eCos_get_label_value (&offset, WAIT_INFO_OFFSET);
      wait_info = read_memory_unsigned_integer (thread_id + offset,
                                                4, byte_order);
      tmp->wait_info = wait_info;
    }
  if (tmp->sleep_reason == 0)
    {
      int sleep_reason;
      eCos_get_label_value (&offset, SLEEP_REASON_OFFSET); 
      sleep_reason = read_memory_unsigned_integer (thread_id + offset,
                                                   4, byte_order);
      tmp->sleep_reason = sleep_reason; 
    }
  if (tmp->wake_reason == 0)
    { 
      int wake_reason;
      eCos_get_label_value (&offset, WAKE_REASON_OFFSET);
      wake_reason = read_memory_unsigned_integer (thread_id + offset,
                                                  4, byte_order);
      tmp->wake_reason = wake_reason;  
    }
  if (tmp->cyg_hthread->stack_size == 0)
    {
      int size;
      eCos_get_label_value (&offset, STACK_SIZE_OFFSET);
      size = read_memory_unsigned_integer (thread_id + offset,
                                           4, byte_order);
      tmp->cyg_hthread->stack_size = size;
    }
  if (tmp->cyg_hthread->entry_data == 0)
    {
      int data;
      eCos_get_label_value (&offset, ENTRY_DATA_OFFSET);
      data = read_memory_unsigned_integer (thread_id + offset,
                                           4, byte_order);
      tmp->cyg_hthread->entry_data = data;
    }
  if (tmp->cyg_hthread->stack_ptr == 0)
    {
      unsigned int ptr;
      eCos_get_label_value (&offset, STACK_PTR_OFFSET);
      ptr = read_memory_unsigned_integer (thread_id + offset,
                                          4, byte_order);
      tmp->cyg_hthread->stack_ptr = ptr;
    }

  /* Formatted output.  */
  cleanup_thread_info = make_cleanup_ui_out_tuple_begin_end (uiout,
                                                             "mthreadinfo");
  /* Info of line 1st.  */
  ui_out_text (uiout, "#");
  ui_out_field_core_addr (uiout, "id", get_current_arch (), thread_id);
  ui_out_text (uiout, " ");
  ui_out_field_int (uiout, "priority", tmp->cyg_sthread->priority);
  ui_out_text (uiout, " ");
  ui_out_field_int (uiout, "state", tmp->state);
  ui_out_text (uiout, thread_state[tmp->state]);
  ui_out_text (uiout, " ");
  ui_out_field_core_addr (uiout, "entry_ptr", get_current_arch (),
                          tmp->cyg_hthread->entry_point);
  ui_out_text (uiout, " ");
  ui_out_field_string (uiout, "name", tmp->name);
  ui_out_text (uiout, "\n\n#");
  /* Info of line 2nd.  */
  ui_out_field_int (uiout, "susp_count", tmp->suspend_count);
  ui_out_text (uiout, " ");
  ui_out_field_int (uiout, "wake_count", tmp->wakeup_count);
  ui_out_text (uiout, " ");
  ui_out_field_int (uiout, "wait_info", tmp->wait_info);
  ui_out_text (uiout, " ");
  ui_out_field_int (uiout, "sleep_reason", tmp->sleep_reason);
  /* ui_out_text(uiout, cyg_reason[tmp->sleep_reason]);  */
  ui_out_text (uiout, " ");
  ui_out_field_int (uiout, "wake_reason", tmp->wake_reason);
  /* ui_out_text(uiout, cyg_reason[tmp->wake_reason]);  */
  ui_out_text (uiout, "\n\n#");
  /* Info of line 3rd.  */
  ui_out_field_int (uiout, "mutex_count",
                    tmp->cyg_sthread->mutex_count);
  ui_out_text (uiout, "\t");
  ui_out_field_int (uiout, "prio_inherit",
                    tmp->cyg_sthread->priority_inherited);
  if (tmp->cyg_sthread->priority_inherited)
    ui_out_text (uiout, ":TRUE");
  else
    ui_out_text (uiout, ":FALSE");
  ui_out_text (uiout, "\t");
  ui_out_field_int (uiout, "prio_original",
                    tmp->cyg_sthread->original_priority);
  ui_out_text (uiout, "\n\n#");
  /* Info of line 4th.  */
  ui_out_field_core_addr (uiout, "stack_base", get_current_arch (),
                          tmp->cyg_hthread->stack_base);
  ui_out_text (uiout, " ");
  ui_out_field_core_addr (uiout, "stack_size", get_current_arch (),
                          tmp->cyg_hthread->stack_size);
  ui_out_text (uiout, " ");
  ui_out_field_core_addr (uiout, "stack_ptr", get_current_arch (),
                          tmp->cyg_hthread->stack_ptr);
  ui_out_text (uiout, "\n\n#");
  /* Info of line 5th.  */
  ui_out_field_core_addr (uiout, "entry_ptr2", get_current_arch (),
                          tmp->cyg_hthread->entry_point);
  ui_out_text (uiout, " ");
  ui_out_field_int (uiout, "entry_data",
                    tmp->cyg_hthread->entry_data);
  ui_out_text (uiout, " ");
  ui_out_field_string (uiout, "label", tmp->cyg_hthread->fun_name);
  ui_out_text (uiout, "\n");

  do_cleanups (cleanup_thread_info);

  return;
}

/* List all threads stack info according args.
   ARGS: stack all: all threads stack info.
         stack depth [ID]: if [ID], list one thread's stack depth by ID
                     else list all threads' stack depth.  */

static void
eCos_threads_stack_command (char* args, int from_tty)
{
  char* arg1 = args;
  const char ALL[] = "all";
  const char DEPTH[] = "depth";
  
  /* Analysis parameters and determine handler function.  */
  while (arg1 != NULL)
    {
      if (*arg1 != ' ')
        break;
      arg1++;
    }
  if (strncmp (arg1, ALL, (sizeof(ALL) - 1)) == 0
       && (arg1[strlen(ALL)] == ' ' || arg1[strlen(ALL)] == '\0'))
    {
      eCos_stack_all_command (&(arg1[strlen(ALL)]), from_tty);
      return;
    }
  else if (strncmp (arg1, DEPTH, (sizeof(DEPTH) - 1)) == 0)
    {
      eCos_stack_depth_command (&(arg1[strlen(DEPTH)]), from_tty);
      return;
    }
  printf_filtered ("Command \"info mthreads stack\"has no such \
format,try \"help info mthreads for more information.\"\n");

  return;
}

/* List all threads stack info.
   ARGS will be ignored.  */

static void
eCos_stack_all_command (char* args, int from_tty)
{
  CORE_ADDR offset;
  eCos_thread_info *tmp;
  struct cleanup *cleanup_stack_info;
  struct ui_out *uiout = current_uiout;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* This commands need no args.  */ 
  if (*args != '\0')
    {
      printf_filtered ("info mthreads stack all command omit all args.\n");
    }

  /* Organize threads list according to eCos_thread_list.  */
  cleanup_stack_info =
           make_cleanup_ui_out_tuple_begin_end (uiout, "stackinfo");
  for (tmp = eCos_thread_list; tmp != NULL; tmp = tmp->next)
    {
      if (tmp->accessed == 1)
        {
          CORE_ADDR offset;
          struct cleanup *cleanup_frame =
                  make_cleanup_ui_out_tuple_begin_end (uiout, "frame");
      
          /* Update eCos_thread_list.  */
          if (tmp->cyg_hthread->stack_size == 0)
            {
              unsigned int size;
              eCos_get_label_value (&offset, STACK_SIZE_OFFSET);
              size = read_memory_unsigned_integer (
                        tmp->t->thread_id + offset, 4, byte_order);
              tmp->cyg_hthread->stack_size = size;
            }
          if (tmp->cyg_hthread->stack_ptr == 0)
            {
              unsigned int ptr;
              eCos_get_label_value (&offset, STACK_PTR_OFFSET);
              ptr = read_memory_unsigned_integer (
                       tmp->t->thread_id + offset, 4, byte_order);
              tmp->cyg_hthread->stack_ptr = ptr;
            }
          /* Formatted output.  */
          ui_out_text (uiout, "#");
          ui_out_field_core_addr (uiout, "id", get_current_arch (),
                                  tmp->t->thread_id);
          ui_out_text (uiout, " ");
          ui_out_field_string (uiout, "name", tmp->name);
          ui_out_text (uiout, " ");
          ui_out_field_core_addr (uiout, "low", get_current_arch (),
                                  tmp->cyg_hthread->stack_base);
          ui_out_text (uiout, " ");
          ui_out_field_core_addr (uiout, "size", get_current_arch (),
                                  tmp->cyg_hthread->stack_size);
          ui_out_text (uiout, " ");
          ui_out_field_core_addr (uiout, "sp", get_current_arch (),
                                  tmp->cyg_hthread->stack_ptr);
          ui_out_text (uiout, "\n");
          do_cleanups (cleanup_frame);
        }
    }
  do_cleanups (cleanup_stack_info);

  return;
}

/* List thread(s) depth info.
   if ARGS, list the one, else list all threads' stack depth info.
   ALL_DEPTH: ALL_DEPTH == 1, list all threads' stack depth info.
              ALL_DEPTH == 0, list one thread's stack depth info.  */

static void
eCos_stack_depth_command (char* args, int from_tty)
{
  eCos_thread_info *tmp = NULL;
  if (args == NULL || *args == '\0')
    {
      /* List all threads' stack depth info.  */
      for (tmp = eCos_thread_list; tmp != NULL; tmp = tmp->next)
        {
          eCos_single_stack_depth_handler (ptid_build (remote_csky_ptid.pid,
                                                       0, tmp->t->thread_id));
        }
      return;
    }
  else
    {
      char* arg1 = args;
      unsigned int thread_id;
      /* Analysis args and 
         list one thread's stack depth according to args.  */
      while (arg1 != NULL)
        {
          if (*arg1 != ' ')
            break;
          arg1++;
        }
      thread_id = strtoll (arg1, NULL, 16);
      eCos_single_stack_depth_handler (ptid_build (remote_csky_ptid.pid,
                                                   0, thread_id));
    }

  return;
}

/* Finish analysis of single thread's stack depth
   PTID: specified thread.  */

static void
eCos_single_stack_depth_handler (ptid_t ptid)
{
  eCos_thread_info *tmp;
  CORE_ADDR offset;
  struct cleanup * cleanup_stack_depth;
  struct ui_out *uiout = current_uiout;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  tmp = find_eCos_thread_info (ptid);
  if (tmp != NULL && tmp->accessed == 1)
    {
      if (tmp->cyg_hthread->stack_ptr == 0)
        {
          unsigned int ptr;
          eCos_get_label_value (&offset, STACK_PTR_OFFSET);
          ptr = read_memory_unsigned_integer (tmp->t->thread_id + offset,
                                              4, byte_order);
          tmp->cyg_hthread->stack_ptr = ptr;
        }
      /* Use dichotomy to compute lowest.  */
      tmp->cyg_hthread->lowest = eCos_get_lowest_sp (
                                    tmp->cyg_hthread->stack_ptr,
                                    tmp->cyg_hthread->stack_base);
      /* Formatted output.  */
      cleanup_stack_depth = make_cleanup_ui_out_tuple_begin_end (
                                                    uiout, "stackdepth");
      ui_out_text (uiout, "#");
      ui_out_field_core_addr (uiout, "id", get_current_arch (),
                              tmp->t->thread_id);
      ui_out_text (uiout, "\t");
      ui_out_text (uiout, tmp->name);
      ui_out_text (uiout, "\t");
      ui_out_field_core_addr (uiout, "lowest", get_current_arch (),
                              tmp->cyg_hthread->lowest);
      ui_out_text (uiout, "\n");
      do_cleanups (cleanup_stack_depth);
    }
  else
    {
      printf_filtered ("Thread 0x%x is not alive.\n", ptid.tid);
    }

  return;
}

#define STACK_INIT_VAL    0xcccccccc

/**
 * Get the lowest sp with the use of dichotomy algorithm
 *
 *  CORE_ADDR   |        MEM
 *  ------------|------------------
 *    start  -->|   |            |
 *              |   |stack region|
 *              |   |            |
 *              |   |            |
 *    end    -->|-------------------
 */

static unsigned long long 
eCos_get_lowest_sp (unsigned long long start, unsigned long long end)
{
  int stack_value, stack_value_p;
  unsigned long long lowest = 0;
  CORE_ADDR offset;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Dichotomy algorithm by recursive.  */
  if (start > end + 8)
    {
      lowest = (start + end) / 2;
      lowest = lowest - (lowest & 0x7);
      stack_value = read_memory_unsigned_integer (lowest, 4, byte_order);
      stack_value_p = read_memory_unsigned_integer (lowest - 4, 4,
                                                     byte_order);
      if (stack_value == STACK_INIT_VAL && stack_value_p == STACK_INIT_VAL)
        return eCos_get_lowest_sp (start, lowest);
      else
        return eCos_get_lowest_sp (lowest, end);
    }
  else  /* START <= end + 8.  */
    {
      /* SP is 8bit allign.  */
      lowest = (start + end) / 2;
      lowest = lowest - (lowest & 0x7);
      return lowest;
    }
}
