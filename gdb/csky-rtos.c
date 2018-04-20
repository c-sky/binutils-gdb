/* csky-rtos.c
   This file extends struct target_ops csky_ops to support the
   multi-tasks debugging; This file only implement the framework
   of csky multi-tasks debugging, for the specified implement,
   please put it into csky-${RTOS_NAME}-nat.c.  */
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
#include <stdlib.h>
#include "csky-rtos.h"

/* The index value in rtos_init_tables.  */
#define ECOS_INIT_TABLE_INDEX           0
#define UCOSIII_INIT_TABLE_INDEX        1
#define NUTTX_INIT_TABLE_INDEX          2
#define FREERTOS_INIT_TABLE_INDEX       3
#define RHINO_INIT_TABLE_INDEX          4
#define ZEPHYR_INIT_TABLE_INDEX         5

/************ Start Implementation for rtos eCos support *******************
 * 1. RTOS_TCB ecos_tcb
 * 2. struct target_ops ecos_ops
 * 3. eocs_open ()
 * 4. RTOS_EVENT ecos_event
 */

/* ***********  1. RTOS_TCB ecos tcb ************************  */

/* --------------- csky_ecos_reg_offset_table -------------------------  */
static int csky_ecos_reg_offset_table[] = {
/* General register 0~15: 0 ~ 15.  */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
     -1,   0x2*4,   0x3*4,   0x4*4,   0x5*4,   0x6*4,   0x7*4,   0x8*4,
   0x9*4,  0xa*4,   0xb*4,   0xc*4,   0xd*4,   0xe*4,   0xf*4,   0x10*4,
  /* dsp hilo register: 97, 98.  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       -1,      -1,   -1,    -1,

  /* FPU reigster: 24~55.  */
  /*
  "cp1.gr0", "cp1.gr1", "cp1.gr2",  "cp1.gr3",
  "cp1.gr4",  "cp1.gr5",  "cp1.gr6",  "cp1.gr7",
  "cp1.gr8", "cp1.gr9", "cp1.gr10", "cp1.gr11",
  "cp1.gr12", "cp1.gr13", "cp1.gr14", "cp1.gr15",
  "cp1.gr16","cp1.gr17","cp1.gr18", "cp1.gr19",
  "cp1.gr10", "cp1.gr21", "cp1.gr22", "cp1.gr23",
  "cp1.gr24","cp1.gr25","cp1.gr26", "cp1.gr27",
  "cp1.gr28", "cp1.gr29", "cp1.gr30", "cp1.gr31",
  */
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,

  /* hole */
  /*
  "",  "",  "",  "",  "",  "",  "",  "",
  "",  "",  "",  "",  "",  "",  "",  "",
  */
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,

/* ****** Above all must according to compiler for debug info ********  */
  /*
  "all",   "gr",   "ar",   "cr",   "",    "",    "",    "",
  "cp0",   "cp1",  "",     "",     "",    "",    "",    "",
  "",      "",     "",     "",     "",    "",    "",    "cp15",
  */

  /* pc : 72 : 64  */
  /*
  "pc",
  */
  0x0*4,

  /* Optional register(ar) : 73~88 :  16 ~ 31  */
  /*
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  -1,      -1,    -1,     -1,      -1,      -1,     -1,    -1,
  -1,      -1,    -1,     -1,      -1,      -1,     -1,    -1,

  /* Control registers (cr) : 89~120 : 32 ~ 63  */
  /*
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "",
  */
  0x1*4,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* FPC control register: 0x100 & (32 ~ 38)  */
  /*
  "cp1.cr0","cp1.cr1","cp1.cr2","cp1.cr3","cp1.cr4","cp1.cr5","cp1.cr6",
  */
  -1,        -1,       -1,       -1,       -1,        -1,      -1,

  /* MMU control register: 0xf00 & (32 ~ 40)  */
  /*
  "cp15.cr0",  "cp15.cr1",  "cp15.cr2",  "cp15.cr3",
  "cp15.cr4",  "cp15.cr5",  "cp15.cr6",  "cp15.cr7",
  "cp15.cr8",  "cp15.cr9",  "cp15.cr10", "cp15.cr11",
  "cp15.cr12", "cp15.cr13", "cp15.cr14", "cp15.cr15",
  "cp15.cr16", "cp15.cr29", "cp15.cr30", "cp15.cr31"
  */
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1
};

/* ----------------- RTOS_TCB_DES table in RTOS_TCB -------------------   */ 
/* Ecos 3 special fields definitions.  */
/* RTOS_FIELD_DES for thread_list field.  */
static const  char* ecos_thread_list_offset_table[1] =
{
  "& Cyg_Thread::thread_list"
};
static int ecos_thread_list_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_thread_list =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_thread_list_offset_table,
  ecos_thread_list_offset_cache,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for current thread field.  */
static const char* ecos_current_thread_offset_table[1] =
{
  "& Cyg_Scheduler_Base::current_thread"
};
static int ecos_current_thread_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_current_thread =
{
  "current_thread",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_current_thread_offset_table,
  ecos_current_thread_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for current thread field.  */
static const char* ecos_list_next_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->list_next"
};
static int ecos_list_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_list_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_list_next_offset_table,
  ecos_list_next_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/*
 * Ecos 5 basic fields definition
 */
/* RTOS_FIELD_DES for thread id field.  */
static const char* ecos_id_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->timer->thread"
};
static int ecos_id_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_id =
{
  "id",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_id_offset_table,
  ecos_id_offset_cache,
  NULL,
  NULL /* Use default output field method.  */
};

/* RTOS_FIELD_DES for stack_ptr field.  */
static const char* ecos_stack_ptr_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->stack_ptr"
};
static int ecos_stack_ptr_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_stack_ptr =
{
  "stack_ptr",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_stack_ptr_offset_table,
  ecos_stack_ptr_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for entry_base field.  */
static const char* ecos_entry_base_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->entry_point"
};
static int ecos_entry_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_entry_base =
{
  "entry_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_entry_base_offset_table,
  ecos_entry_base_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for tcb_base field.  */
static const char* ecos_tcb_base_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->timer->thread"
};
static int ecos_tcb_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_tcb_base =
{
  "tcb_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_tcb_base_offset_table,
  ecos_tcb_base_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for task_name field.  */
static const char* ecos_task_name_offset_table[2] =
{
  "& ((Cyg_Thread*)0)->name",
  "0"
};
static int ecos_task_name_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES ecos_field_task_name =
{
  "task_name",
  String,
  4,
  NULL,
  1,
  2,
  ecos_task_name_offset_table,
  ecos_task_name_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/*
 * Ecos_extend_field definition
 */
/* RTOS_FIELD_DES for priority field.  */
static const char* ecos_priority_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->priority"
};
static int ecos_priority_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_priority =
{
  "priority",
  Integer,
  4,
  NULL,
  1,
  1,
  ecos_priority_offset_table,
  ecos_priority_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for state field.  */
static char * ecos_state_int2String[] =
{
  "RUNNING",
  "SLEEPING",
  "COUNTSLEEP",
  " ",
  "SUSPENDED",
  " "," "," ",
  "CREATING",
  " "," ", " ", " ", " ", " "," ",
  "EXITED"
};

static const char *
ecos_state_int2String_Transfer (int index)
{
  int str_num
    = sizeof (ecos_state_int2String) / sizeof (ecos_state_int2String[0]);
  return ecos_state_int2String[index % str_num];
}

static const char* ecos_state_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->state"
};
static int ecos_state_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_state =
{
  "state",
  IntToString,
  4,
  ecos_state_int2String_Transfer,
  1,
  1,
  ecos_state_offset_table,
  ecos_state_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_size.  */
static const char* ecos_stack_size_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->stack_size"
};
static int ecos_stack_size_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_stack_size =
{
  "stack_size",
  Integer,
  4,
  NULL,
  1,
  1,
  ecos_stack_size_offset_table,
  ecos_stack_size_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for current_pc.  */

static int
ecos_parse_current_pc (CORE_ADDR tcb_base,
                       struct rtos_field_des* itself,
                       RTOS_FIELD *val)
{
  CORE_ADDR thread_id;
  struct regcache *regcache;
  ptid_t ptid;

  /* Get thread_id first.  */
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* STACK SIZE  */
  if (itself->offset_cache[0] == -1) /* Not parsed.  */
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 0)),
                                   (itself->offset_cache + 0)) < 0)
        {
          return -1;
        }
    }

  thread_id
    = read_memory_unsigned_integer (tcb_base + itself->offset_cache[0],
                                    4, byte_order);

  ptid = ptid_build (rtos_ops.target_ptid.pid, 0, thread_id);
  regcache = get_thread_regcache (ptid);
  val->coreaddr = regcache_read_pc (regcache);

  return 0;
}

static const char* ecos_current_pc_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->timer->thread"
};
static int ecos_current_pc_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_current_pc =
{
  "current_pc",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  ecos_current_pc_offset_table,
  ecos_current_pc_offset_cache,
  ecos_parse_current_pc,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_high.  */

static int
ecos_parse_stack_high (CORE_ADDR tcb_base,
                       struct rtos_field_des* itself,
                       RTOS_FIELD *val)
{
  CORE_ADDR stack_high;
  CORE_ADDR stack_size;
  CORE_ADDR stack_base;

  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* STACK SIZE.  */
  if (itself->offset_cache[0] == -1)  /* Not parsed.  */
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 0)),
                                   (itself->offset_cache + 0)) < 0)
        {
          return -1;
        }
    }

  stack_size
    = read_memory_unsigned_integer (tcb_base + itself->offset_cache[0],
                                    4, byte_order);

  /* Stack base.  */
  if (itself->offset_cache[1] == -1) /* Not parsed.  */
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 1)),
                                   (itself->offset_cache + 1)) < 0)
        {
          return -1;
        }
    }

  stack_base
    = read_memory_unsigned_integer (tcb_base + itself->offset_cache[1],
                                    4, byte_order);

  /* Get stack high.  */
  stack_high = stack_base + stack_size;
  val->coreaddr = stack_high;
  return 0;
}

static const char* ecos_stack_high_offset_table[2] =
{
  "& ((Cyg_Thread*)0)->stack_size",
  "& ((Cyg_Thread*)0)->stack_base"
};
static int ecos_stack_high_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES ecos_field_stack_high =
{
  "stack_high",
  CoreAddr,
  4,
  NULL,
  0,
  2,
  ecos_stack_high_offset_table,
  ecos_stack_high_offset_cache,
  ecos_parse_stack_high,
  NULL /* Use default ouput method.  */
};

/* RTOS_TCB definition for stack_base.  */
static const char* ecos_stack_base_offset_table[1] =
{
  "& ((Cyg_Thread*)0)->stack_base"
};
static int ecos_stack_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ecos_field_stack_base =
{
  "stack_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ecos_stack_base_offset_table,
  ecos_stack_base_offset_cache,
  NULL,
  NULL  /* Use default output method.  */
};

/* RTOS_TCB definition for ecos.  */
#define ECOS_EXTEND_FIELD_NUM 6
static RTOS_FIELD_DES ecos_tcb_extend_table[ECOS_EXTEND_FIELD_NUM];

static RTOS_FIELD_DES*
init_ecos_tcb_extend_table ()
{
  ecos_tcb_extend_table[0] = ecos_field_priority;
  ecos_tcb_extend_table[1] = ecos_field_state;
  ecos_tcb_extend_table[2] = ecos_field_stack_size;
  ecos_tcb_extend_table[3] = ecos_field_current_pc;
  ecos_tcb_extend_table[4] = ecos_field_stack_high;
  ecos_tcb_extend_table[5] = ecos_field_stack_base;
  return ecos_tcb_extend_table;
}

/* Fields needed for "info mthreads *" commands.  */

/* Fields needed for "info mthreads list"  */
static unsigned int ecos_i_mthreads_list_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  5, /* Name  */
};

/* Fields needed for "info mthreads ID"  */
static unsigned int ecos_i_mthread_one_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  RTOS_BASIC_FIELD_NUM + 4, /* Current_pc  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack size  */
  5, /* Name  */
};

/* Fields needed for "info mthreads stack all"  */
static unsigned int ecos_i_mthreads_stack_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 6, /* Stack_base  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack_high  */
  2, /* Stack_ptr  */
  5, /* Name  */
};

/* ---------- funcitons defined in RTOS_TCB -----------------------  */
/*
 *  The read/write register handler
 *  stack_ptr: current stack pointer of this task
 *  regno: the number of register
 *  val: the value to be read/written
 *  return 0: the reg is in the memory pointed by stack_ptr,
 *  else reg is in CPU
 */
/*

void
csky_ecos_fetch_registers (CORE_ADDR stack_ptr,
                           int regno,
                           unsigned int* val)
{
  // FIXME I DON'T KNOW WHAT TO DO
  return;
}

void
csky_ecos_store_registers (CORE_ADDR stack_ptr,
                           int regno,
                           unsigned int val)
{
  // FIXME I DON'T KNOW WHAT TO DO
  return;
}
*/

/* To check if this thread_id is valid.  */

static int
csky_ecos_is_valid_task_id (RTOS_FIELD task_id)
{
  return ((task_id.IntVal != 0) && (task_id.IntVal & 7) == 0);
}

/* To check if this reg in task's stack.  */

static int
csky_ecos_is_regnum_in_task_list (RTOS_TCB* rtos_des, ptid_t ptid, int regno)
{
  /* Only r1-r15, psr, pc stored in task'stack.  */
  if (rtos_des->rtos_reg_offset_table[regno] != -1)
    {
      return 1;
    }
  return 0;
}

static RTOS_TCB ecos_tcb;

static void
init_ecos_tcb ()
{
  /* 3 special fields.  */
  ecos_tcb.task_list_count = 1;
  {
    ecos_tcb.rtos_special_table[0][0] = ecos_field_thread_list;
    ecos_tcb.rtos_special_table[0][1] = ecos_field_current_thread;
    ecos_tcb.rtos_special_table[0][2] = ecos_field_list_next;
  }

  /* 5 basic fields.  */
  {
    ecos_tcb.rtos_tcb_table[0] = ecos_field_id;
    ecos_tcb.rtos_tcb_table[1] = ecos_field_stack_ptr;
    ecos_tcb.rtos_tcb_table[2] = ecos_field_entry_base;
    ecos_tcb.rtos_tcb_table[3] = ecos_field_tcb_base;
    ecos_tcb.rtos_tcb_table[4] = ecos_field_task_name;
  }

  /* Extend field number.  */
  ecos_tcb.extend_table_num = ECOS_EXTEND_FIELD_NUM;
  ecos_tcb.rtos_tcb_extend_table = init_ecos_tcb_extend_table ();

  /* For "info mthreads commands"  */
  ecos_tcb.i_mthreads_list = ecos_i_mthreads_list_table;
  ecos_tcb.i_mthreads_list_size = sizeof (ecos_i_mthreads_list_table)
                                    / sizeof (ecos_i_mthreads_list_table[0]);
  ecos_tcb.i_mthreads_stack = ecos_i_mthreads_stack_table;
  ecos_tcb.i_mthreads_stack_size = sizeof (ecos_i_mthreads_stack_table)
                                     / sizeof (ecos_i_mthreads_stack_table[0]);
  ecos_tcb.i_mthread_one = ecos_i_mthread_one_table;
  ecos_tcb.i_mthread_one_size = sizeof (ecos_i_mthread_one_table)
                                  / sizeof (ecos_i_mthread_one_table[0]);

  /* Ecos read/write register handler.  */
  ecos_tcb.rtos_reg_offset_table = csky_ecos_reg_offset_table;
  ecos_tcb.to_get_register_base_address = NULL;
  ecos_tcb.to_fetch_registers =  NULL;
  ecos_tcb.to_store_registers =  NULL;

  /* Ecos check thread_id valid.  */
  ecos_tcb.IS_VALID_TASK = csky_ecos_is_valid_task_id;

  /* Ecos check regno in task's stack.  */
  ecos_tcb.is_regnum_in_task_list = csky_ecos_is_regnum_in_task_list;
}

/* ************ 2. target_ops ecos_ops ****************  */
static struct target_ops ecos_ops;

/* ************ 3. ecos_open () *********************  */
static void ecos_open (const char * name, int from_tty);

/* Open for ecos system.  */

static void
ecos_open (const char * name, int from_tty)
{
  rtos_ops.current_ops = rtos_init_tables[ECOS_INIT_TABLE_INDEX].ops;
  rtos_ops.rtos_des = rtos_init_tables[ECOS_INIT_TABLE_INDEX].rtos_tcb_des;
  rtos_ops.event_des = rtos_init_tables[ECOS_INIT_TABLE_INDEX].rtos_event_des;
  common_open (name, from_tty);
}

#define ECOS_NAME_NUM 2
static const char * ecos_names[ECOS_NAME_NUM] =
{
  "eCos",
  "ecos"
};

/* *********** 4. ecos_event **********************  */
/* Ecos does not support events check.  */

/* ****************** end of implementation of ecos ***************  */

/*
 * ********** Start Implementation for rtos ucosiii support *********
 * 1. RTOS_TCB ucosiii_tcb
 * 2. struct target_ops ucosiii_ops
 * 3. ucosiii_open ()
 */

/* ***********  1. RTOS_TCB ucosiii_tcb ***********************  */

/* ------------ csky_ucosiii_reg_offset_table -----------------  */

static int csky_ucosiii_reg_offset_table[] = {
/* General register 0~15: 0 ~ 15   */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
#ifndef CSKYGDB_CONFIG_ABIV2
   -1,     0x2*4,   0x3*4,   0x4*4,   0x5*4,   0x6*4,   0x7*4,   0x8*4,
   0x9*4,  0xa*4,   0xb*4,   0xc*4,   0xd*4,   0xe*4,   0xf*4,   0x10*4,
#else
   0x0*4,  0x1*4,   0x2*4,   0x3*4,   0x4*4,   0x5*4,   0x6*4,   0x7*4,
   0x8*4,  0x9*4,   0xa*4,   0xb*4,   0xc*4,   0xd*4,   -1,      0xe*4,
#endif
  /* dsp hilo register: 97, 98  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       -1,      -1,   -1,    -1,

  /* FPU reigster: 24~55  */
  /*
  "cp1.gr0",  "cp1.gr1",  "cp1.gr2",  "cp1.gr3",
  "cp1.gr4",  "cp1.gr5",  "cp1.gr6",  "cp1.gr7",
  "cp1.gr8",  "cp1.gr9",  "cp1.gr10", "cp1.gr11",
  "cp1.gr12", "cp1.gr13", "cp1.gr14", "cp1.gr15",
  "cp1.gr16", "cp1.gr17", "cp1.gr18", "cp1.gr19",
  "cp1.gr10", "cp1.gr21", "cp1.gr22", "cp1.gr23",
  "cp1.gr24", "cp1.gr25", "cp1.gr26", "cp1.gr27",
  "cp1.gr28", "cp1.gr29", "cp1.gr30", "cp1.gr31",
  */
   -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1,

  /* hole  */
  /*
  "",  "",  "",  "",  "",  "",  "",  "",
  "",  "",  "",  "",  "",  "",  "",  "",
  */
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,

/* ****** Above all must according to compiler for debug info *********  */

  /*
  "all",   "gr",   "ar",   "cr",   "",    "",    "",    "",
  "cp0",   "cp1",  "",     "",     "",    "",    "",    "",
  "",      "",     "",     "",     "",    "",    "",    "cp15",
  */

  /* pc : 72 : 64  */
  /*
  "pc",
  */
#ifndef CSKYGDB_CONFIG_ABIV2
  0x0*4,
#else
  0x10*4,
#endif

  /* Optional register(ar) : 73~88 :  16 ~ 31  */
  /*
  "ar0",  "ar1", "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* Control registers (cr) : 89~120 : 32 ~ 63.  */
  /*
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "",
  */
#ifndef CSKYGDB_CONFIG_ABIV2
  0x1*4,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
#else
  0xf*4,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
#endif
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* FPC control register: 0x100 & (32 ~ 38)  */
  /*
  "cp1.cr0", "cp1.cr1", "cp1.cr2", "cp1.cr3", "cp1.cr4", "cp1.cr5", "cp1.cr6",
  */
  -1,        -1,        -1,        -1,        -1,        -1,        -1,

  /* MMU control register: 0xf00 & (32 ~ 40)  */
  /*
  "cp15.cr0",  "cp15.cr1",  "cp15.cr2",  "cp15.cr3",
  "cp15.cr4",  "cp15.cr5",  "cp15.cr6",  "cp15.cr7",
  "cp15.cr8",  "cp15.cr9",  "cp15.cr10", "cp15.cr11",
  "cp15.cr12", "cp15.cr13", "cp15.cr14", "cp15.cr15",
  "cp15.cr16", "cp15.cr29", "cp15.cr30", "cp15.cr31"
  */
  -1,    -1,    -1,     -1,
  -1,    -1,    -1,     -1,
  -1,    -1,    -1,     -1,
  -1,    -1,    -1,     -1,
  -1,    -1,    -1,     -1
};

/* -------------- RTOS_TCB_DES table in RTOS_TCB ----------------  */
/*
 * Ucosiii 3 special fields definitions
 */

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* ucosiii_thread_list_offset_table[1] =
{
  "& OSTaskDbgListPtr"
};
static int ucosiii_thread_list_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_thread_list =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_thread_list_offset_table,
  ucosiii_thread_list_offset_cache,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for current thread field.  */
static const char* ucosiii_current_thread_offset_table[1] =
{
  "& OSTCBCurPtr"
};
static int ucosiii_current_thread_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_current_thread =
{
  "current_thread",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_current_thread_offset_table,
  ucosiii_current_thread_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field.  */
static const char* ucosiii_list_next_offset_table[1] =
{
  "& ((struct os_tcb*)0)->DbgNextPtr"
};
static int ucosiii_list_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_list_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_list_next_offset_table,
  ucosiii_list_next_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* Ucosiii 5 basic fields definition.  */

/* RTOS_FIELD_DES for thread id field.  */

static int
ucosiii_parse_thread_id (CORE_ADDR tcb_base,
                         struct rtos_field_des* itself,
                         RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}

static RTOS_FIELD_DES ucosiii_field_id =
{
  "id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  ucosiii_parse_thread_id,
  NULL /* Use default output field method.  */
};

/* RTOS_FIELD_DES for stack_ptr field.  */
static const char* ucosiii_stack_ptr_offset_table[1] =
{
  "& ((struct os_tcb*)0)->StkPtr"
};
static int ucosiii_stack_ptr_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_stack_ptr =
{
  "stack_ptr",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_stack_ptr_offset_table,
  ucosiii_stack_ptr_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for entry_base field.  */
static const char* ucosiii_entry_base_offset_table[1] =
{
  "& ((struct os_tcb*)0)->TaskEntryAddr"
};
static int ucosiii_entry_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_entry_base =
{
  "entry_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_entry_base_offset_table,
  ucosiii_entry_base_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for tcb_base field.  */

static int
ucosiii_parse_tcb_base (CORE_ADDR tcb_base,
                        struct rtos_field_des* itself,
                        RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}

static RTOS_FIELD_DES ucosiii_field_tcb_base =
{
  "tcb_base",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  ucosiii_parse_tcb_base,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for task_name field.  */
static const char* ucosiii_task_name_offset_table[2] =
{
  "& ((struct os_tcb*)0)->NamePtr",
  "0"
};
static int ucosiii_task_name_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES ucosiii_field_task_name =
{
  "task_name",
  String,
  4,
  NULL,
  1,
  2,
  ucosiii_task_name_offset_table,
  ucosiii_task_name_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* Ucosiii_extend_field definition.  */

/* RTOS_FIELD_DES for priority field.  */
static const char* ucosiii_priority_offset_table[1] =
{
  "& ((struct os_tcb*)0)->Prio"
};
static int ucosiii_priority_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_priority =
{
  "priority",
  Integer,
  1,
  NULL,
  1,
  1,
  ucosiii_priority_offset_table,
  ucosiii_priority_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for state field.  */
static char * ucosiii_state_int2String[] =
{
  "RDY",                        /* 0  */ 
  "DLY",                        /* 1  */
  "PEND",                       /* 2  */
  "PEND_TIMEOUT",               /* 3  */
  "SUSPENDED",                  /* 4  */
  "DLY_SUSPENDED",              /* 5  */
  "PEND_SUSPENDED",             /* 6  */
  "PEND_TIMEOUT_SUSPENDED",     /* 7  */
  "DEL"                         /* 8  */
};

static const char *
ucosiii_state_int2String_Transfer (int index)
{
  int str_num;
  str_num = sizeof (ucosiii_state_int2String)
              / sizeof (ucosiii_state_int2String[0]);
  if (index < (str_num - 1))
    {
      return ucosiii_state_int2String[index];
    }
  else
    {
      return ucosiii_state_int2String[str_num -1];
    }
}
static const char* ucosiii_state_offset_table[1] =
{
  "& ((struct os_tcb*)0)->TaskState"
};
static int ucosiii_state_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_state =
{
  "state",
  IntToString,
  1,
  ucosiii_state_int2String_Transfer,
  1,
  1,
  ucosiii_state_offset_table,
  ucosiii_state_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_size.  */
static const char* ucosiii_stack_size_offset_table[1] =
{
  "& ((struct os_tcb*)0)->StkSize"
};
static int ucosiii_stack_size_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_stack_size =
{
  "stack_size",
  Integer,
  4,
  NULL,
  1,
  1,
  ucosiii_stack_size_offset_table,
  ucosiii_stack_size_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for current_pc.  */

static int
ucosiii_parse_current_pc (CORE_ADDR tcb_base,
                          struct rtos_field_des* itself,
                          RTOS_FIELD *val)
{
  CORE_ADDR thread_id;
  struct regcache *regcache;
  ptid_t ptid;

  thread_id = tcb_base;

  ptid = ptid_build (rtos_ops.target_ptid.pid, 0, thread_id);
  regcache = get_thread_regcache (ptid);
  val->coreaddr = regcache_read_pc (regcache);

  return 0;
}

static RTOS_FIELD_DES ucosiii_field_current_pc =
{
  "current_pc",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  ucosiii_parse_current_pc,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_high.  */

static int
ucosiii_parse_stack_high (CORE_ADDR tcb_base,
                          struct rtos_field_des* itself,
                          RTOS_FIELD *val)
{
  CORE_ADDR stack_high;
  CORE_ADDR stack_size;
  CORE_ADDR stack_base;

  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* STACK SIZE  */
  if (itself->offset_cache[0] == -1) /* Not parsed.  */
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 0)),
                                    (itself->offset_cache + 0)) < 0)
        {
          return -1;
        }
    }

  stack_size
     = read_memory_unsigned_integer (tcb_base + itself->offset_cache[0],
                                     4, byte_order);

  /* Stack base  */
  if (itself->offset_cache[1] == -1) /* Not parsed.  */
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 1)),
                                   (itself->offset_cache + 1)) < 0)
        {
          return -1;
        }
    }

  stack_base
    = read_memory_unsigned_integer (tcb_base + itself->offset_cache[1],
                                    4, byte_order);

  /* Get stack high.  */
  stack_high = stack_base + stack_size;
  val->coreaddr = stack_high;
  return 0;
}

static const char* ucosiii_stack_high_offset_table[2] =
{
  "& ((struct os_tcb*)0)->StkSize",
  "& ((struct os_tcb*)0)->StkBasePtr"
};
static int ucosiii_stack_high_offset_cache[2] = {-1,-1};
static RTOS_FIELD_DES ucosiii_field_stack_high =
{
  "stack_high",
  CoreAddr,
  4,
  NULL,
  0,
  2,
  ucosiii_stack_high_offset_table,
  ucosiii_stack_high_offset_cache,
  ucosiii_parse_stack_high,
  NULL /* Use default ouput method.  */
};

/* RTOS_TCB definition for stack_base.  */
static const char* ucosiii_stack_base_offset_table[1] =
{
  "& ((struct os_tcb*)0)->StkBasePtr"
};
static int ucosiii_stack_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_stack_base =
{
  "stack_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_stack_base_offset_table,
  ucosiii_stack_base_offset_cache,
  NULL,
  NULL  /* Use default output method.  */
};

/* RTOS_TCB definition for ucosiii.  */
#define UCOSIII_EXTEND_FIELD_NUM 6
static RTOS_FIELD_DES ucosiii_tcb_extend_table[UCOSIII_EXTEND_FIELD_NUM];

static RTOS_FIELD_DES*
init_ucosiii_tcb_extend_table ()
{

  ucosiii_tcb_extend_table[0] = ucosiii_field_priority;
  ucosiii_tcb_extend_table[1] = ucosiii_field_state;
  ucosiii_tcb_extend_table[2] = ucosiii_field_stack_size;
  ucosiii_tcb_extend_table[3] = ucosiii_field_current_pc;
  ucosiii_tcb_extend_table[4] = ucosiii_field_stack_high;
  ucosiii_tcb_extend_table[5] = ucosiii_field_stack_base;
  return ucosiii_tcb_extend_table;
}

/* Fields needed for "info mthreads *" commands.  */

/* Fields needed for "info mthreads list"  */
static unsigned int ucosiii_i_mthreads_list_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  5, /* Name  */
};

/* Fields needed for "info mthreads ID"  */
static unsigned int ucosiii_i_mthread_one_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  RTOS_BASIC_FIELD_NUM + 4, /* Current_pc  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack size  */
  5, /* Name  */
};

/* Fields needed for "info mthreads stack all"  */
static unsigned int ucosiii_i_mthreads_stack_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 6, /* Stack_base  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack_high  */
  2, /* Stack_ptr  */
  5, /* Name  */
};

/* ---------- funcitons defined in RTOS_TCB ------------------  */

static int
csky_ucosiii_is_valid_task_id (RTOS_FIELD task_id)
{
  return 1;
}

/* To check if this reg in task's stack.  */

static int
csky_ucosiii_is_regnum_in_task_list (RTOS_TCB* rtos_des,
                                     ptid_t ptid, int regno)
{
  /* Only r1-r15, psr, pc stored in task'stack.  */
  if (rtos_des->rtos_reg_offset_table[regno] != -1)
    {
      return 1;
    }
  return 0;
}

static RTOS_TCB ucosiii_tcb;

static void 
init_ucosiii_tcb ()
{
  /* 3 special fields  */
  ucosiii_tcb.task_list_count = 1;
  {
    ucosiii_tcb.rtos_special_table[0][0] = ucosiii_field_thread_list;
    ucosiii_tcb.rtos_special_table[0][1] = ucosiii_field_current_thread;
    ucosiii_tcb.rtos_special_table[0][2] = ucosiii_field_list_next;
  }

  /* 5 basic fields  */
  {
    ucosiii_tcb.rtos_tcb_table[0] = ucosiii_field_id;
    ucosiii_tcb.rtos_tcb_table[1] = ucosiii_field_stack_ptr;
    ucosiii_tcb.rtos_tcb_table[2] = ucosiii_field_entry_base;
    ucosiii_tcb.rtos_tcb_table[3] = ucosiii_field_tcb_base;
    ucosiii_tcb.rtos_tcb_table[4] = ucosiii_field_task_name;
  }

  /* Extend field number.  */
  ucosiii_tcb.extend_table_num = UCOSIII_EXTEND_FIELD_NUM;
  ucosiii_tcb.rtos_tcb_extend_table = init_ucosiii_tcb_extend_table ();

  /* For "info mthreads commands"  */
  ucosiii_tcb.i_mthreads_list = ucosiii_i_mthreads_list_table;
  ucosiii_tcb.i_mthreads_list_size
    = sizeof (ucosiii_i_mthreads_list_table)
        / sizeof (ucosiii_i_mthreads_list_table[0]);
  ucosiii_tcb.i_mthreads_stack = ucosiii_i_mthreads_stack_table;
  ucosiii_tcb.i_mthreads_stack_size
    = sizeof (ucosiii_i_mthreads_stack_table)
        / sizeof (ucosiii_i_mthreads_stack_table[0]);
  ucosiii_tcb.i_mthread_one = ucosiii_i_mthread_one_table;
  ucosiii_tcb.i_mthread_one_size
    = sizeof (ucosiii_i_mthread_one_table)
        / sizeof (ucosiii_i_mthread_one_table[0]);


  /* Ucosiii read/write register handler.  */
  ucosiii_tcb.rtos_reg_offset_table = csky_ucosiii_reg_offset_table;
  ucosiii_tcb.to_get_register_base_address = NULL;
  ucosiii_tcb.to_fetch_registers =  NULL;
  ucosiii_tcb.to_store_registers =  NULL;

  /* Ucosiii check thread_id valid.  */
  ucosiii_tcb.IS_VALID_TASK = csky_ucosiii_is_valid_task_id;

  /* Ucosiii check regno in task's stack.  */
  ucosiii_tcb.is_regnum_in_task_list = csky_ucosiii_is_regnum_in_task_list;
}

/* ************ 2. target_ops ucosiii_ops ************************  */
static struct target_ops ucosiii_ops;

/* ************ 3. ucosiii_open () *********************  */
static void ucosiii_open (const char * name, int from_tty);

/* Open for ucosiii system.  */

static void 
ucosiii_open (const char * name, int from_tty)
{
  rtos_ops.current_ops = rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].ops;
  rtos_ops.rtos_des = rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].rtos_tcb_des;
  rtos_ops.event_des
    = rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].rtos_event_des;
  common_open (name, from_tty);
}

#define UCOSIII_NAME_NUM 3
static const char * ucosiii_names[UCOSIII_NAME_NUM] =
{
  "uCosiii",
  "uCosIII",
  "ucosiii"
};

/* ********** 4. ucosiii_event **********************  */
/* Functions used for event info check command.  */

static int 
ucosiii_parse_event_id (CORE_ADDR event_base,
                        struct rtos_field_des* itself,
                        RTOS_FIELD *val)
{
  val->coreaddr = event_base;
  return 0;
}

static char type_name[5];

static const char *
ucosiii_event_type_IntToString_Transfer (int index)
{
  int i;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  for (i =0 ;i < 5; i++)
    {
      type_name[i] = 0;
    }
  if (byte_order == BFD_ENDIAN_BIG)
    {
      type_name[0] = (char)((index >> 24) & 0xff);
      type_name[1] = (char)((index >> 16) & 0xff);
      type_name[2] = (char)((index >> 8) & 0xff);
      type_name[3] = (char)((index >> 0) & 0xff);
    }
  else if (byte_order == BFD_ENDIAN_LITTLE)
    {
      type_name[3] = (char)((index >> 24) & 0xff);
      type_name[2] = (char)((index >> 16) & 0xff);
      type_name[1] = (char)((index >> 8) & 0xff);
      type_name[0] = (char)((index >> 0) & 0xff);
    }
  else
    {
      warning ("BFD_ENDIAN_UNKNOWN.");
    }
  return type_name;
}
static char * pendlist_offset_table[3]=
{
  "& ((struct  os_pend_list*)0)->HeadPtr",
  "& ((struct  os_pend_data*)0)->TCBPtr",
  "& ((struct  os_pend_data*)0)->NextPtr"
};

static int pendlist_offset_cache[3]={-1,-1,-1};

static void 
ucosiii_event_pendlist_field_ouput (struct rtos_field_des* itself,
                                    RTOS_FIELD *val, int from_tty)
{
  int i;
  CORE_ADDR headptr,tmp;
  CORE_ADDR thread_id;
  ptid_t ptid;
  RTOS_TCB_COMMON *tp = NULL;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());
  struct ui_out *uiout = current_uiout;

  ui_out_text (uiout, "pendlist:");

  if (val->undef == 0)
    {
      /* No pendlist of this event.  */
      return;
    }

  for (i = 0; i < 3; i++)
    {
      if (pendlist_offset_cache[i] == -1)
        {
          if (csky_rtos_symbol2offset (pendlist_offset_table[i],
                                       &pendlist_offset_cache[i]) < 0)
            {
              /* Error info ???  */
              return;
            }
        }
    }
  headptr = read_memory_unsigned_integer ((CORE_ADDR)val->undef
                                           + pendlist_offset_cache[0],
                                          4, byte_order);

  tmp = headptr;
  if (headptr != 0)
    {
      ui_out_text (uiout,"\n\tid     task_name");
    }
  else
    {
      ui_out_text (uiout,"\n\tnull.");
      return;
    }

  do
    {
      if (tmp == 0)
        {
          return;
        }
      thread_id
        = read_memory_unsigned_integer (tmp + pendlist_offset_cache[1],
                                        4, byte_order);
      ptid = ptid_build (rtos_ops.target_ptid.pid, 0, thread_id);
      tp = find_rtos_task_info (ptid);
      if (tp != NULL)
        {
          ui_out_text (uiout,"\n\t");
          /* Output thread info : id + name.  */
          csky_rtos_output_field_default
            (&(rtos_ops.rtos_des->rtos_tcb_table[0]),
             &(tp->rtos_basic_table[0]),
             from_tty);
          csky_rtos_output_field_default
            (&(rtos_ops.rtos_des->rtos_tcb_table[4]),
             &(tp->rtos_basic_table[4]),
             from_tty);
        }
      tmp = read_memory_unsigned_integer (tmp+pendlist_offset_cache[2],
                                          4, byte_order);
    }
  while ((tmp != headptr) && (tmp != 0));
  return ;
}

static int
ucosiii_parse_event_pendlist (CORE_ADDR event_base,
                              struct rtos_field_des* itself,
                              RTOS_FIELD *val)
{
  if (itself->offset_cache[0] == -1)
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 0)),
                                   (itself->offset_cache + 0)) < 0)
        {
          return -1;
        }
    }

  val->undef = (void *)(event_base + itself->offset_cache[0]);
  return 0;
}


/* Ucosiii_event includes:
   (1) rtos_event_special_table
   (2) rtos_event_info_table   */

/* ----------(1) rtos_event_special_table--------------------  */
/* RTOS_FIELD_DES for flag event_base.  */
static const char* ucosiii_flag_event_base_offset_table[1] =
{
  "& OSFlagDbgListPtr"
};
static int ucosiii_flag_event_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_flag_event_base =
{
  "event_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_flag_event_base_offset_table,
  ucosiii_flag_event_base_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for flag event_next.  */
static const char* ucosiii_flag_event_next_offset_table[1] =
{
  "& ((struct  os_flag_grp*)0)->DbgNextPtr"
};
static int ucosiii_flag_event_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_flag_event_next =
{
  "event_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_flag_event_next_offset_table,
  ucosiii_flag_event_next_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for mutex event_base.  */
static const char* ucosiii_mutex_event_base_offset_table[1] =
{
  "& OSMutexDbgListPtr"
};
static int ucosiii_mutex_event_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_mutex_event_base =
{
  "event_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_mutex_event_base_offset_table,
  ucosiii_mutex_event_base_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for mutex event_next.  */
static const char* ucosiii_mutex_event_next_offset_table[1] =
{
  "& ((struct  os_mutex*)0)->DbgNextPtr"
};
static int ucosiii_mutex_event_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_mutex_event_next =
{
  "event_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_mutex_event_next_offset_table,
  ucosiii_mutex_event_next_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for message event_base.  */
static const char* ucosiii_message_event_base_offset_table[1] =
{
  "& OSQDbgListPtr"
};
static int ucosiii_message_event_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_message_event_base =
{
  "event_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_message_event_base_offset_table,
  ucosiii_message_event_base_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for message event_next.  */
static const char* ucosiii_message_event_next_offset_table[1] =
{
  "& ((struct  os_q*)0)->DbgNextPtr"
};
static int ucosiii_message_event_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_message_event_next =
{
  "event_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_message_event_next_offset_table,
  ucosiii_message_event_next_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for sema event_base.  */
static const char* ucosiii_sema_event_base_offset_table[1] =
{
  "& OSSemDbgListPtr"
};
static int ucosiii_sema_event_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_sema_event_base =
{
  "event_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_sema_event_base_offset_table,
  ucosiii_sema_event_base_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for sema event_next.  */
static const char* ucosiii_sema_event_next_offset_table[1] =
{
  "& ((struct  os_sem*)0)->DbgNextPtr"
};
static int ucosiii_sema_event_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_sema_event_next =
{
  "event_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  ucosiii_sema_event_next_offset_table,
  ucosiii_sema_event_next_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* ---------- (2) rtos_event_info_table------------------  */

/* --------for flag event----------------  */

/* RTOS_FIELD_DES for flag_event_id.  */
static RTOS_FIELD_DES ucosiii_field_flag_event_id =
{
  "event_id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  ucosiii_parse_event_id,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for flag_event_type.  */
static const char* ucosiii_flag_event_type_offset_table[1] =
{
  "& ((struct  os_flag_grp*)0)->Type"
};
static int ucosiii_flag_event_type_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_flag_event_type =
{
  "event_type",
  IntToString,
  4,
  ucosiii_event_type_IntToString_Transfer,
  1,
  1,
  ucosiii_flag_event_type_offset_table,
  ucosiii_flag_event_type_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for flag_event_name.  */
static const char* ucosiii_flag_event_name_offset_table[2] =
{
  "& ((struct  os_flag_grp*)0)->NamePtr",
  "0"
};
static int ucosiii_flag_event_name_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES ucosiii_field_flag_event_name =
{
  "event_name",
  String,
  4,
  NULL,
  1,
  2,
  ucosiii_flag_event_name_offset_table,
  ucosiii_flag_event_name_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for flag_event_pendlist.  */
static const char* ucosiii_flag_event_pendlist_offset_table[1] =
{
  "& (*(struct  os_flag_grp*)0)->PendList"
};
static int ucosiii_flag_event_pendlist_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_flag_event_pendlist =
{
  "event_pendlist",
  Undefine,
  4,
  NULL,
  0,
  1,
  ucosiii_flag_event_pendlist_offset_table,
  ucosiii_flag_event_pendlist_offset_cache,
  ucosiii_parse_event_pendlist,
  ucosiii_event_pendlist_field_ouput  /* Use default ouput method.  */
};

/* For mutex event.  */

/* RTOS_FIELD_DES for mutex_event_id.  */
static RTOS_FIELD_DES ucosiii_field_mutex_event_id =
{
  "event_id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  ucosiii_parse_event_id,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for mutex_event_type.  */
static const char* ucosiii_mutex_event_type_offset_table[1] =
{
  "& ((struct  os_mutex*)0)->Type"
};
static int ucosiii_mutex_event_type_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_mutex_event_type =
{
  "event_type",
  IntToString,
  4,
  ucosiii_event_type_IntToString_Transfer,
  1,
  1,
  ucosiii_mutex_event_type_offset_table,
  ucosiii_mutex_event_type_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for mutex_event_name.  */
static const char* ucosiii_mutex_event_name_offset_table[2] =
{
  "& ((struct  os_mutex*)0)->NamePtr",
  "0"
};
static int ucosiii_mutex_event_name_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES ucosiii_field_mutex_event_name =
{
  "event_name",
  String,
  4,
  NULL,
  1,
  2,
  ucosiii_mutex_event_name_offset_table,
  ucosiii_mutex_event_name_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for mutex_event_pendlist.  */
static const char* ucosiii_mutex_event_pendlist_offset_table[1] =
{
  "& (*(struct  os_mutex*)0)->PendList"
};
static int ucosiii_mutex_event_pendlist_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_mutex_event_pendlist =
{
  "event_pendlist",
  Undefine,
  4,
  NULL,
  0,
  1,
  ucosiii_mutex_event_pendlist_offset_table,
  ucosiii_mutex_event_pendlist_offset_cache,
  ucosiii_parse_event_pendlist,
  ucosiii_event_pendlist_field_ouput
};

/* --------For message event----------------  */

/* RTOS_FIELD_DES for message_event_id.  */
static RTOS_FIELD_DES ucosiii_field_message_event_id =
{
  "event_id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  ucosiii_parse_event_id,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for message_event_type.  */
static const char* ucosiii_message_event_type_offset_table[1] =
{
  "& ((struct  os_q*)0)->Type"
};
static int ucosiii_message_event_type_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_message_event_type =
{
  "event_type",
  IntToString,
  4,
  ucosiii_event_type_IntToString_Transfer,
  1,
  1,
  ucosiii_message_event_type_offset_table,
  ucosiii_message_event_type_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for message_event_name.  */
static const char* ucosiii_message_event_name_offset_table[2] =
{
  "& ((struct  os_q*)0)->NamePtr",
  "0"
};
static int ucosiii_message_event_name_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES ucosiii_field_message_event_name =
{
  "event_name",
  String,
  4,
  NULL,
  1,
  2,
  ucosiii_message_event_name_offset_table,
  ucosiii_message_event_name_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for message_event_pendlist.  */
static const char* ucosiii_message_event_pendlist_offset_table[1] =
{
  "& (*(struct  os_q*)0)->PendList"
};
static int ucosiii_message_event_pendlist_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_message_event_pendlist =
{
  "event_pendlist",
  Undefine,
  4,
  NULL,
  0,
  1,
  ucosiii_message_event_pendlist_offset_table,
  ucosiii_message_event_pendlist_offset_cache,
  ucosiii_parse_event_pendlist,
  ucosiii_event_pendlist_field_ouput
};

/* For sema event.  */

/* RTOS_FIELD_DES for sema_event_id.  */
static RTOS_FIELD_DES ucosiii_field_sema_event_id =
{
  "event_id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  ucosiii_parse_event_id,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for sema_event_type.  */
static const char* ucosiii_sema_event_type_offset_table[1] =
{
  "& ((struct  os_sem*)0)->Type"
};
static int ucosiii_sema_event_type_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_sema_event_type =
{
  "event_type",
  IntToString,
  4,
  ucosiii_event_type_IntToString_Transfer,
  1,
  1,
  ucosiii_sema_event_type_offset_table,
  ucosiii_sema_event_type_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for sema_event_name.  */
static const char* ucosiii_sema_event_name_offset_table[2] =
{
  "& ((struct  os_sem*)0)->NamePtr",
  "0"
};
static int ucosiii_sema_event_name_offset_cache[2] = {-1,  -1};
static RTOS_FIELD_DES ucosiii_field_sema_event_name =
{
  "event_name",
  String,
  4,
  NULL,
  1,
  2,
  ucosiii_sema_event_name_offset_table,
  ucosiii_sema_event_name_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for sema_event_pendlist.  */
static const char* ucosiii_sema_event_pendlist_offset_table[1] =
{
  "& (*(struct  os_sem*)0)->PendList"
};
static int ucosiii_sema_event_pendlist_offset_cache[1] = {-1};
static RTOS_FIELD_DES ucosiii_field_sema_event_pendlist =
{
  "event_pendlist",
  Undefine,
  4,
  NULL,
  0,
  1,
  ucosiii_sema_event_pendlist_offset_table,
  ucosiii_sema_event_pendlist_offset_cache,
  ucosiii_parse_event_pendlist,
  ucosiii_event_pendlist_field_ouput
};

static RTOS_EVENT ucosiii_event;

static void
init_ucosiii_event ()
{
  ucosiii_event.rtos_event_special_table[0][0]
    = ucosiii_field_flag_event_base;
  ucosiii_event.rtos_event_special_table[0][1]
    = ucosiii_field_flag_event_next;
  ucosiii_event.rtos_event_special_table[1][0]
    = ucosiii_field_mutex_event_base;
  ucosiii_event.rtos_event_special_table[1][1]
    = ucosiii_field_mutex_event_next;
  ucosiii_event.rtos_event_special_table[2][0]
    = ucosiii_field_message_event_base;
  ucosiii_event.rtos_event_special_table[2][1]
    = ucosiii_field_message_event_next;
  ucosiii_event.rtos_event_special_table[3][0]
    = ucosiii_field_sema_event_base;
  ucosiii_event.rtos_event_special_table[3][1]
    = ucosiii_field_sema_event_next;

  ucosiii_event.rtos_event_info_table[0][0]
    = ucosiii_field_flag_event_id;
  ucosiii_event.rtos_event_info_table[0][1]
    = ucosiii_field_flag_event_type;
  ucosiii_event.rtos_event_info_table[0][2]
    = ucosiii_field_flag_event_name;
  ucosiii_event.rtos_event_info_table[0][3]
    = ucosiii_field_flag_event_pendlist;

  ucosiii_event.rtos_event_info_table[1][0]
    = ucosiii_field_mutex_event_id;
  ucosiii_event.rtos_event_info_table[1][1]
    = ucosiii_field_mutex_event_type;
  ucosiii_event.rtos_event_info_table[1][2]
    = ucosiii_field_mutex_event_name;
  ucosiii_event.rtos_event_info_table[1][3]
    = ucosiii_field_mutex_event_pendlist;

  ucosiii_event.rtos_event_info_table[2][0]
    = ucosiii_field_message_event_id;
  ucosiii_event.rtos_event_info_table[2][1]
    = ucosiii_field_message_event_type;
  ucosiii_event.rtos_event_info_table[2][2]
    = ucosiii_field_message_event_name;
  ucosiii_event.rtos_event_info_table[2][3]
    = ucosiii_field_message_event_pendlist;

  ucosiii_event.rtos_event_info_table[3][0]
    = ucosiii_field_sema_event_id;
  ucosiii_event.rtos_event_info_table[3][1]
    = ucosiii_field_sema_event_type;
  ucosiii_event.rtos_event_info_table[3][2]
    = ucosiii_field_sema_event_name;
  ucosiii_event.rtos_event_info_table[3][3]
    = ucosiii_field_sema_event_pendlist;
}

/* ************** End of implementation of ucosiii ****************  */

/* ********** Start Implementation for rtos nuttx support ************
   1. RTOS_TCB nuttx_tcb
   2. struct target_ops nuttx_ops
   3. nuttx_open()
   4. RTOS_EVENT nuttx_event.
 *******************************************************************  */

/* ***********  1. RTOS_TCB nuttx tcb ************************  */

/* --------------- csky_nuttx_reg_offset_table ---------------  */

static int csky_nuttx_reg_offset_table[] = {
/* General register 0~15: 0 ~ 15  */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
   0x0*4, 0x1*4,  0x2*4,  0x3*4,  0x4*4,  0x5*4,  0x6*4,  0x7*4,
   0x8*4, 0x9*4,  0xa*4,  0xb*4,  0xc*4,  0xd*4,  0xe*4,  0xf*4,
  /* dsp hilo register: 97, 98  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       -1,      -1,   -1,    -1,

  /* FPU reigster: 24~55  */
  /*
  "cp1.gr0",  "cp1.gr1",  "cp1.gr2",  "cp1.gr3",
  "cp1.gr4",  "cp1.gr5",  "cp1.gr6",  "cp1.gr7",
  "cp1.gr8",  "cp1.gr9",  "cp1.gr10", "cp1.gr11",
  "cp1.gr12", "cp1.gr13", "cp1.gr14", "cp1.gr15",
  "cp1.gr16", "cp1.gr17", "cp1.gr18", "cp1.gr19",
  "cp1.gr10", "cp1.gr21", "cp1.gr22", "cp1.gr23",
  "cp1.gr24", "cp1.gr25", "cp1.gr26", "cp1.gr27",
  "cp1.gr28", "cp1.gr29", "cp1.gr30", "cp1.gr31",
  */
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,

  /* hole  */
  /*
  "",      "",    "",     "",     "",    "",   "",    "",
  "",      "",    "",     "",     "",    "",   "",    "",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

/* ****** Above all must according to compiler for debug info ******  */

  /*
  "all",   "gr",   "ar",   "cr",   "",    "",    "",    "",
  "cp0",   "cp1",  "",     "",     "",    "",    "",    "",
  "",      "",     "",     "",     "",    "",    "",    "cp15",
  */

  /* PC : 72 : 64  */
  /*
  "pc",
  */
  0x11*4,

  /* Optional register(ar) : 73~88 :  16 ~ 31  */
  /*
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* Control registers (cr) : 89~120 : 32 ~ 63  */
  /*
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "",
  */
  0x10*4,     -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* FPC control register: 0x100 & (32 ~ 38)  */
  /*
  "cp1.cr0","cp1.cr1","cp1.cr2","cp1.cr3","cp1.cr4","cp1.cr5","cp1.cr6",
  */
  -1, -1, -1, -1, -1, -1, -1,

  /* MMU control register: 0xf00 & (32 ~ 40)  */
  /*
  "cp15.cr0",  "cp15.cr1",  "cp15.cr2",  "cp15.cr3",
  "cp15.cr4",  "cp15.cr5",  "cp15.cr6",  "cp15.cr7",
  "cp15.cr8",  "cp15.cr9",  "cp15.cr10", "cp15.cr11",
  "cp15.cr12", "cp15.cr13", "cp15.cr14", "cp15.cr15",
  "cp15.cr16", "cp15.cr29", "cp15.cr30", "cp15.cr31"
  */
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1
};

/* ------------- RTOS_TCB_DES table in RTOS_TCB -------------  */

/* Nuttx 3 special fields definitions.  */

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* nuttx_thread_list_offset_table[1] =
{
  "&(g_readytorun.head)"
};
static int nuttx_thread_list_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_thread_list =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_thread_list_offset_table,
  nuttx_thread_list_offset_cache,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for current thread field.  */
static const char* nuttx_current_thread_offset_table[1] =
{
  "&(g_readytorun.head)"
};
static int nuttx_current_thread_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_current_thread =
{
  "current_thread",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_current_thread_offset_table,
  nuttx_current_thread_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field.  */
static const char* nuttx_list_next_offset_table[1] =
{
  "& ((struct tcb_s*)0)->flink"
};
static int nuttx_list_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_list_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_list_next_offset_table,
  nuttx_list_next_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* nuttx_thread_list_offset_table1[1] =
{
  "&(g_pendingtasks.head)"
};
static int nuttx_thread_list_offset_cache1[1] = {-1};
static RTOS_FIELD_DES nuttx_field_thread_list1 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_thread_list_offset_table1,
  nuttx_thread_list_offset_cache1,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* nuttx_thread_list_offset_table2[1] =
{
  "&(g_inactivetasks.head)"
};
static int nuttx_thread_list_offset_cache2[1] = {-1};
static RTOS_FIELD_DES nuttx_field_thread_list2 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_thread_list_offset_table2,
  nuttx_thread_list_offset_cache2,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* nuttx_thread_list_offset_table3[1] =
{
  "&(g_waitingforsemaphore.head)"
};
static int nuttx_thread_list_offset_cache3[1] = {-1};
static RTOS_FIELD_DES nuttx_field_thread_list3 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_thread_list_offset_table3,
  nuttx_thread_list_offset_cache3,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* nuttx_thread_list_offset_table4[1] =
{
  "&(g_waitingforsignal.head)"
};
static int nuttx_thread_list_offset_cache4[1] = {-1};
static RTOS_FIELD_DES nuttx_field_thread_list4 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_thread_list_offset_table4,
  nuttx_thread_list_offset_cache4,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* nuttx_thread_list_offset_table5[1] =
{
  "&(g_waitingformqnotempty.head)"
};
static int nuttx_thread_list_offset_cache5[1] = {-1};
static RTOS_FIELD_DES nuttx_field_thread_list5 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_thread_list_offset_table5,
  nuttx_thread_list_offset_cache5,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* nuttx_thread_list_offset_table6[1] =
{
  "&(g_waitingformqnotfull.head)"
};
static int nuttx_thread_list_offset_cache6[1] = {-1};
static RTOS_FIELD_DES nuttx_field_thread_list6 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_thread_list_offset_table6,
  nuttx_thread_list_offset_cache6,
  NULL,
  NULL  /* No output method for this field.  */
};

/* Nuttx 5 basic fields definition.  */

/* RTOS_FIELD_DES for thread id field.  */

static int
nuttx_parse_thread_id (CORE_ADDR tcb_base,
                       struct rtos_field_des* itself,
                       RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}

static RTOS_FIELD_DES nuttx_field_internal_id =
{
  "id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  nuttx_parse_thread_id,
  NULL /* Use default output field method.  */
};

/* RTOS_FIELD_DES for stack_ptr field.  */
#ifndef CSKYGDB_CONFIG_ABIV2
static const char* nuttx_stack_ptr_offset_table[1] =
{
  "& ((struct tcb_s*)0)->xcp.regs[0]"
};
#else
static const char* nuttx_stack_ptr_offset_table[1] =
{
  "& ((struct tcb_s*)0)->xcp.regs[14]"
};
#endif

static int nuttx_stack_ptr_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_stack_ptr =
{
  "stack_ptr",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_stack_ptr_offset_table,
  nuttx_stack_ptr_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for entry_base field.  */
static const char* nuttx_entry_base_offset_table[1] =
{
  "& ((struct tcb_s*)0)->entry.main"
};
static int nuttx_entry_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_entry_base =
{
  "entry_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_entry_base_offset_table,
  nuttx_entry_base_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for tcb_base field.  */

static int
nuttx_parse_tcb_base (CORE_ADDR tcb_base,
                      struct rtos_field_des* itself,
                      RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}

static RTOS_FIELD_DES nuttx_field_tcb_base =
{
  "tcb_base",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  nuttx_parse_tcb_base,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for task_name field.  */
static const char* nuttx_task_name_offset_table[1] =
{
  "& ((struct tcb_s*)0)->name"
};
static int nuttx_task_name_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_task_name =
{
  "task_name",
  String,
  4,
  NULL,
  1,
  1,
  nuttx_task_name_offset_table,
  nuttx_task_name_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* Nuttx_extend_field definition.  */

/* RTOS_FIELD_DES for priority field.  */
static const char* nuttx_priority_offset_table[1] =
{
  "& ((struct tcb_s*)0)->sched_priority"
};
static int nuttx_priority_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_priority =
{
  "priority",
  Integer,
  1,
  NULL,
  1,
  1,
  nuttx_priority_offset_table,
  nuttx_priority_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for state field.  */

/* The task state array is a strange "bitmap" of
   reasons to sleep. Thus "running" is zero, and
   you can test for combinations of others with
   simple bit tests.  */

static const char *nuttx_state_int2String[] =
{
  "running",      /* 0  */
  "sleeping",     /* 1  */
  "disk sleep",   /* 2  */
  "stopped",      /* 4  */
  "tracing stop", /* 8  */
  "zombie",       /* 16  */
  "dead"          /* 32  */
};

static const char *
nuttx_state_int2String_Transfer (int index)
{
  int bitmap = 0;
  int str_num
    = sizeof (nuttx_state_int2String) / sizeof (nuttx_state_int2String[0]);

  while (bitmap < str_num)
    {
      if ((index >> bitmap) & 0x1)
        {
	  return nuttx_state_int2String[bitmap];
	}
      bitmap++;
    }
  return "unknown";
}

static const char* nuttx_state_offset_table[1] =
{
  "& ((struct tcb_s*)0)->task_state"
};
static int nuttx_state_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_state =
{
  "state",
  IntToString,
  1,
  nuttx_state_int2String_Transfer,
  1,
  1,
  nuttx_state_offset_table,
  nuttx_state_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_size.  */
static const char* nuttx_stack_size_offset_table[1] =
{
  "& ((struct tcb_s*)0)->adj_stack_size"
};
static int nuttx_stack_size_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_stack_size =
{
  "stack_size",
  Integer,
  4,
  NULL,
  1,
  1,
  nuttx_stack_size_offset_table,
  nuttx_stack_size_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for current_pc.  */
static const char* nuttx_current_pc_offset_table[1] =
{
  "& ((struct tcb_s*)0)->xcp.regs[17]"
};
static int nuttx_current_pc_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_current_pc =
{
  "current_pc",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_current_pc_offset_table,
  nuttx_current_pc_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_high.  */
static const char* nuttx_stack_high_offset_table[1] =
{
  "& ((struct tcb_s*)0)->adj_stack_ptr"
};
static int nuttx_stack_high_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_stack_high =
{
  "stack_high",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  nuttx_stack_high_offset_table,
  nuttx_stack_high_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_TCB definition for error number.  */
static const char* nuttx_errno_offset_table[1] =
{
  "& ((struct tcb_s*)0)->pterrno"
};
static int nuttx_errno_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_error_number =
{
  "errno",
  Integer,
  4,
  NULL,
  1,
  1,
  nuttx_errno_offset_table,
  nuttx_errno_offset_cache,
  NULL,
  NULL  /* Use default output method.  */
};

/* RTOS_TCB definition for sem.  */

/* Output sem info by the given sem address.  */

static char *
nuttx_sem_info_Transfer (CORE_ADDR sem_addr)
{
  /* The longest output is "0xffffffff <-65534>"  */
  int str_len;
  char * sem_output = (char*) malloc (32);

  if (sem_addr == 0x0)
    {
      str_len = sprintf (sem_output, "0x0<null>");
    }
  else
    {
      int value = 0;
      enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());
      value = read_memory_integer (sem_addr, 2, byte_order);
      str_len = sprintf (sem_output, "0x%x<%d>", sem_addr, value);
    }
  sem_output[str_len % 32] = '\0';
  return sem_output;
}

static int
nuttx_parse_sem_info (CORE_ADDR tcb_base,
                      struct rtos_field_des* itself,
                      RTOS_FIELD *val)
{
  CORE_ADDR sem_addr;
  static int offset = -1;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  if (offset == -1)
    {
      csky_rtos_symbol2offset ("& ((struct tcb_s*)0)->waitsem", &offset);
    }
  sem_addr = read_memory_unsigned_integer ((CORE_ADDR)(tcb_base + offset),
                                            4, byte_order);
  val->string = nuttx_sem_info_Transfer (sem_addr);
  return 0;
}

static const char* nuttx_sem_info_offset_table[1] =
{
  "& ((struct tcb_s*)0)->waitsem"
};
static int nuttx_sem_info_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_sem_info =
{
  "semaddr<semcount>",
  String,
  1,
  NULL,
  0,
  1,
  NULL,
  NULL,
  nuttx_parse_sem_info,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for thread id field.  */
static const char* nuttx_id_offset_table[1] =
{
  "& ((struct tcb_s*)0)->pid"
};
static int nuttx_id_offset_cache[1] = {-1};
static RTOS_FIELD_DES nuttx_field_id =
{
  "kernel_id",
  Integer,
  2,
  NULL,
  1,
  1,
  nuttx_id_offset_table,
  nuttx_id_offset_cache,
  NULL,
  NULL /* Use default output field method.  */
};

/* RTOS_TCB definition for ecos.  */
#define NUTTX_EXTEND_FIELD_NUM 8
static RTOS_FIELD_DES nuttx_tcb_extend_table[NUTTX_EXTEND_FIELD_NUM];

static RTOS_FIELD_DES* 
init_nuttx_tcb_extend_table ()
{
  nuttx_tcb_extend_table[0] = nuttx_field_priority;
  nuttx_tcb_extend_table[1] = nuttx_field_state;
  nuttx_tcb_extend_table[2] = nuttx_field_stack_size;
  nuttx_tcb_extend_table[3] = nuttx_field_current_pc;
  nuttx_tcb_extend_table[4] = nuttx_field_stack_high;
  nuttx_tcb_extend_table[5] = nuttx_field_error_number;
  nuttx_tcb_extend_table[6] = nuttx_field_sem_info;
  nuttx_tcb_extend_table[7] = nuttx_field_id;
  return nuttx_tcb_extend_table;
}

/* Fields needed for "info mthreads *" commands.  */

/* Fields needed for "info mthreads list"  */
static unsigned int nuttx_i_mthreads_list_table[] =
{
  1, /* Internal id used for GDB(tcb_base)  */
  RTOS_BASIC_FIELD_NUM + 8, /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  RTOS_BASIC_FIELD_NUM + 6, /* Error number  */
  RTOS_BASIC_FIELD_NUM + 7, /* Sem info  */
  5, /* Name  */
};

/* Fields needed for "info mthreads ID"  */
static unsigned int nuttx_i_mthread_one_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  RTOS_BASIC_FIELD_NUM + 4, /* Current_pc  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack size  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack high  */
  RTOS_BASIC_FIELD_NUM + 6, /* Error number  */
  RTOS_BASIC_FIELD_NUM + 7, /* Sem info  */
  5, /* Name  */
};

/* Fields needed for "info mthreads stack all"  */
static unsigned int nuttx_i_mthreads_stack_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack size  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack high  */
  2, /* Stack_ptr  */
  5, /* Name  */
};

/* ---------- Funcitons defined in RTOS_TCB ----------------  */

/* To check if this thread_id is valid.  */

static int
csky_nuttx_is_valid_task_id (RTOS_FIELD task_id)
{
  return ((task_id.IntVal != 0) && (task_id.IntVal & 3) == 0);
}

/* To check if this reg in task's stack.  */

static int
csky_nuttx_is_regnum_in_task_list (RTOS_TCB* rtos_des, ptid_t ptid, int regno)
{
  if (rtos_des->rtos_reg_offset_table[regno] != -1)
    {
      return 1;
    }
  return 0;
}

/* Nuttx registers are save in a global block, here get the base address
   used to get the offset.  */

/* RTOS_FIELD_DES for thread id field.  */

static CORE_ADDR
nuttx_get_register_base_address (CORE_ADDR tcb_base, int regno)
{
  static int offset = -1;
  if (offset == -1)
    {
      /* Here can not use csky_rtos_symbol2offset
         which will cause recursive call...,
         then stack overflow!
         csky_rtos_symbol2offset("& ((struct tcb_s*)0)->xcp.regs[0]", &offset);
         How to solve ?  FIXME
         The way to solve is use the cache to get the offset.  */
#ifndef CSKYGDB_CONFIG_ABIV2
      offset = nuttx_stack_ptr_offset_cache[0];
#else
      offset = nuttx_stack_ptr_offset_cache[0] - (14 * 4);
#endif
    }
  return tcb_base + offset;
}

static RTOS_TCB nuttx_tcb;

static void
init_nuttx_tcb ()
{
  /* 3 special fields  */
  nuttx_tcb.task_list_count = 7;
  {
    /* g_readytorun  */
    nuttx_tcb.rtos_special_table[0][0] = nuttx_field_thread_list;
    nuttx_tcb.rtos_special_table[0][1] = nuttx_field_current_thread;
    nuttx_tcb.rtos_special_table[0][2] = nuttx_field_list_next;

    /* g_pendingtasks  */
    nuttx_tcb.rtos_special_table[1][0] = nuttx_field_thread_list1;
    nuttx_tcb.rtos_special_table[1][1] = nuttx_field_current_thread;
    nuttx_tcb.rtos_special_table[1][2] = nuttx_field_list_next;

    /* g_inactivetasks  */
    nuttx_tcb.rtos_special_table[2][0] = nuttx_field_thread_list2;
    nuttx_tcb.rtos_special_table[2][1] = nuttx_field_current_thread;
    nuttx_tcb.rtos_special_table[2][2] = nuttx_field_list_next;

    /* g_waitingforsemaphore  */
    nuttx_tcb.rtos_special_table[3][0] = nuttx_field_thread_list3;
    nuttx_tcb.rtos_special_table[3][1] = nuttx_field_current_thread;
    nuttx_tcb.rtos_special_table[3][2] = nuttx_field_list_next;

    /* g_waitingforsignal  */
    nuttx_tcb.rtos_special_table[4][0] = nuttx_field_thread_list4;
    nuttx_tcb.rtos_special_table[4][1] = nuttx_field_current_thread;
    nuttx_tcb.rtos_special_table[4][2] = nuttx_field_list_next;

    /* g_waitingformqnotempty  */
    nuttx_tcb.rtos_special_table[5][0] = nuttx_field_thread_list5;
    nuttx_tcb.rtos_special_table[5][1] = nuttx_field_current_thread;
    nuttx_tcb.rtos_special_table[5][2] = nuttx_field_list_next;

    /* g_waitingformqnotfull  */
    nuttx_tcb.rtos_special_table[6][0] = nuttx_field_thread_list6;
    nuttx_tcb.rtos_special_table[6][1] = nuttx_field_current_thread;
    nuttx_tcb.rtos_special_table[6][2] = nuttx_field_list_next;
  }

  /* 5 basic fields  */
  {
    nuttx_tcb.rtos_tcb_table[0] = nuttx_field_internal_id;
    nuttx_tcb.rtos_tcb_table[1] = nuttx_field_stack_ptr;
    nuttx_tcb.rtos_tcb_table[2] = nuttx_field_entry_base;
    nuttx_tcb.rtos_tcb_table[3] = nuttx_field_tcb_base;
    nuttx_tcb.rtos_tcb_table[4] = nuttx_field_task_name;
  }

  /* Extend field number.  */
  nuttx_tcb.extend_table_num = NUTTX_EXTEND_FIELD_NUM;
  nuttx_tcb.rtos_tcb_extend_table = init_nuttx_tcb_extend_table ();

  /* For "info mthreads commands"  */
  nuttx_tcb.i_mthreads_list = nuttx_i_mthreads_list_table;
  nuttx_tcb.i_mthreads_list_size
    = sizeof (nuttx_i_mthreads_list_table)
        / sizeof (nuttx_i_mthreads_list_table[0]);
  nuttx_tcb.i_mthreads_stack = nuttx_i_mthreads_stack_table;
  nuttx_tcb.i_mthreads_stack_size
    = sizeof (nuttx_i_mthreads_stack_table)
        / sizeof (nuttx_i_mthreads_stack_table[0]);
  nuttx_tcb.i_mthread_one = nuttx_i_mthread_one_table;
  nuttx_tcb.i_mthread_one_size
    = sizeof (nuttx_i_mthread_one_table)
        / sizeof (nuttx_i_mthread_one_table[0]);

  /* Nuttx read/write register handler.  */
  nuttx_tcb.rtos_reg_offset_table = csky_nuttx_reg_offset_table;
  nuttx_tcb.to_get_register_base_address = nuttx_get_register_base_address;
  nuttx_tcb.to_fetch_registers =  NULL;
  nuttx_tcb.to_store_registers =  NULL;

  /* Nuttx check thread_id valid.  */
  nuttx_tcb.IS_VALID_TASK = csky_nuttx_is_valid_task_id;

  /* Check regno in task's stack.  */
  nuttx_tcb.is_regnum_in_task_list = csky_nuttx_is_regnum_in_task_list;
}

/* ************ 2. target_ops nuttx_ops **************  */
static struct target_ops nuttx_ops;

/* ************ 3. nuttx_open() **********************  */

static void nuttx_open (const char * name, int from_tty);

/* Open for nuttx system.  */

static void
nuttx_open (const char * name, int from_tty)
{
  rtos_ops.current_ops = rtos_init_tables[NUTTX_INIT_TABLE_INDEX].ops;
  rtos_ops.rtos_des = rtos_init_tables[NUTTX_INIT_TABLE_INDEX].rtos_tcb_des;
  rtos_ops.event_des = rtos_init_tables[NUTTX_INIT_TABLE_INDEX].rtos_event_des;
  common_open (name, from_tty);
}

#define NUTTX_NAME_NUM 3
static const char * nuttx_names[NUTTX_NAME_NUM] =
{
  "nuttx",
  "NUTTX",
  "NuttX"
};

/* *********** 4. nuttx_event **********************  */
/* Nuttx does not support events check.  */

/* ********************** End of implementation of ecos ****************  */


/* ********** Start Implementation for rtos FreeRTOS support **********
   1. RTOS_TCB freertos_tcb
   2. struct target_ops freertos_ops
   3. freertos_open()
   4. RTOS_EVENT freertos_event
 ***********************************************************************  */

/* ***********  1. RTOS_TCB freertos tcb ************************  */

/* ------------- csky_freertos_reg_offset_table -----------------  */
static int csky_freertos_reg_offset_table[] = {
  /* General register 0~15: 0 ~ 15.  */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
#ifdef CSKYGDB_CONFIG_ABIV2
  0x0*4, 0x1*4,  0x2*4,  0x3*4,  0x4*4,  0x5*4,  0x6*4,  0x7*4,
  0x8*4, 0x9*4,  0xa*4,  0xb*4,  0xc*4,  0xd*4,  -1,     0xe*4,
#else  /* not CSKYGDB_CONFIG_ABIV2 */
  -1,    0x2*4,  0x3*4,  0x4*4,  0x5*4,  0x6*4,  0x7*4,  0x8*4,
  0x9*4, 0xa*4,  0xb*4,  0xc*4,  0xd*4,  0xe*4,  0xf*4,  0x10*4,
#endif /* CSKYGDB_CONFIG_ABIV2 */
  /* dsp hilo register: 97, 98.  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       -1,      -1,   -1,    -1,

  /* FPU reigster: 24 ~ 55.  */
  /*
  "cp1.gr0",  "cp1.gr1",  "cp1.gr2",  "cp1.gr3",
  "cp1.gr4",  "cp1.gr5",  "cp1.gr6",  "cp1.gr7",
  "cp1.gr8",  "cp1.gr9",  "cp1.gr10", "cp1.gr11",
  "cp1.gr12", "cp1.gr13", "cp1.gr14", "cp1.gr15",
  "cp1.gr16", "cp1.gr17", "cp1.gr18", "cp1.gr19",
  "cp1.gr10", "cp1.gr21", "cp1.gr22", "cp1.gr23",
  "cp1.gr24", "cp1.gr25", "cp1.gr26", "cp1.gr27",
  "cp1.gr28", "cp1.gr29", "cp1.gr30", "cp1.gr31",
  */
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,

  /* Hole  */
  /*
  "",      "",    "",     "",     "",    "",   "",    "",
  "",      "",    "",     "",     "",    "",   "",    "",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* ****** Above all must according to compiler for debug info ********  */

  /*
  "all",   "gr",   "ar",   "cr",   "",    "",    "",    "",
  "cp0",   "cp1",  "",     "",     "",    "",    "",    "",
  "",      "",     "",     "",     "",    "",    "",    "cp15",
  */

  /* PC : 72 : 64 */
  /*
  "pc",
  */
#ifdef CSKYGDB_CONFIG_ABIV2
  0x10*4,
#else  /* not CSKYGDB_CONFIG_ABIV2 */
  0x0*4
#endif /* not CSKYGDB_CONFIG_ABIV2 */

  /* Optional register(ar) : 73~88 :  16 ~ 31  */
  /*
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* Control registers (cr) : 89~120 : 32 ~ 63  */
  /*
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "",
  */
#ifdef CSKYGDB_CONFIG_ABIV2
  0xf*4,     -1,    -1,     -1,    -1,    -1,   -1,    -1,
#else  /* not CSKYGDB_CONFIG_ABIV2 */
  0x1*4,     -1,    -1,     -1,    -1,    -1,   -1,    -1,
#endif /* not CSKYGDB_CONFIG_ABIV2 */
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* FPC control register: 0x100 & (32 ~ 38)  */
  /*
  "cp1.cr0","cp1.cr1","cp1.cr2","cp1.cr3","cp1.cr4","cp1.cr5","cp1.cr6",
  */
  -1, -1, -1, -1, -1, -1, -1,

  /* MMU control register: 0xf00 & (32 ~ 40) */
  /*
  "cp15.cr0",  "cp15.cr1",  "cp15.cr2",  "cp15.cr3",
  "cp15.cr4",  "cp15.cr5",  "cp15.cr6",  "cp15.cr7",
  "cp15.cr8",  "cp15.cr9",  "cp15.cr10", "cp15.cr11",
  "cp15.cr12", "cp15.cr13", "cp15.cr14", "cp15.cr15",
  "cp15.cr16", "cp15.cr29", "cp15.cr30", "cp15.cr31"
  */
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1
};


/* ----------------- RTOS_TCB_DES table in RTOS_TCB -----------------  */

/* Freertos 8 special fields definitions.  */

/* Function declared.  */
static int
freertos_parse_stack_ptr (CORE_ADDR tcb_base,
                          struct rtos_field_des* itself,
                          RTOS_FIELD *val);

/* RTOS_FIELD_DES for thread_list field.  */

/* Get first task info from pxReadyTasksLists[configMax_PRIORITIES].  */
#define TaskListSize 20
static int freertos_cur_priority = 0;
static int list0_cur_end = 0;
static int list0_cur_item_addr = 0;
static int list0_item_addr_start = 0;
static int readytasklist_addr = 0;

static int
freertos_parse_readytasklist_first (CORE_ADDR tcb_base,
                                    struct rtos_field_des * itself,
                                    RTOS_FIELD * val)
{
  int i;
  char str_tmp[256];
  int list0_cur_item_addr_p = 0;
  int task_num_in_list_addr = 0;
  int task_num_in_list = 0;
  int cur_listitem_addr_start = 0;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Get addr of pxReadyTasksLists.  */
  if (itself->offset_cache[0] == -1)
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 0)),
                                    (itself->offset_cache + 0)) < 0)
        return -1;
      readytasklist_addr = itself->offset_cache[0];
    }

  /* Get val of marco "configMAX_PRIORITIES"  */
  if (itself->offset_cache[1] == -1)
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 1)),
                                   (itself->offset_cache + 1)) < 0)
        return -1;
      freertos_cur_priority = itself->offset_cache[1];
    }

  freertos_cur_priority = itself->offset_cache[1];

  /* Get first ready task tcb.  */
  while (1)
    {
      sprintf (str_tmp, "& ((*(List_t *)%ld)->uxNumberOfItems)",
               readytasklist_addr
               + TaskListSize * (freertos_cur_priority - 1));
      if (csky_rtos_symbol2offset (str_tmp, &task_num_in_list_addr) < 0)
        {
          return -1;
        }
      task_num_in_list = read_memory_unsigned_integer (task_num_in_list_addr,
                                                       4, byte_order);
      if (task_num_in_list == 0)
        {
          /* List end, return.  */
          /* FIXME: msg: no task find ???  */
          if (freertos_cur_priority == 1)
            {
              freertos_cur_priority = 0;
              return -1;
            }
          /* Scan the next priority list.  */
          freertos_cur_priority -= 1;
          task_num_in_list_addr = 0;
          task_num_in_list = 0;
          continue;
        }

      /* Get addr of "xListEnd"  */
      sprintf (str_tmp, "&((*(List_t *) %d)->xListEnd )",
               readytasklist_addr
               + TaskListSize * (freertos_cur_priority - 1));
      if (csky_rtos_symbol2offset (str_tmp, &list0_cur_end) < 0)
        {
          return -1;
        }

      /* Get addr of current list "pxIndex"  */
      sprintf (str_tmp, "& ((*(List_t *) %d)->pxIndex)",
               readytasklist_addr
               + TaskListSize * (freertos_cur_priority - 1));
      if (csky_rtos_symbol2offset (str_tmp, &list0_cur_item_addr_p) < 0)
        {
          return -1;
        }
      cur_listitem_addr_start
        = read_memory_unsigned_integer (list0_cur_item_addr_p, 4, byte_order);
      list0_cur_item_addr = cur_listitem_addr_start;

      while (1)
        {
          if (list0_cur_end != list0_cur_item_addr)
            {
              int owner_addr = 0;
              int owner_val = 0;
              sprintf (str_tmp, " &((*(ListItem_t *) %d)->pvOwner)",
                       list0_cur_item_addr);
              if (csky_rtos_symbol2offset (str_tmp, &owner_addr) < 0)
                {
                   return -1;
                }
              owner_val = read_memory_unsigned_integer (owner_addr,
                                                        4, byte_order);
              if (owner_val != 0)
                {
                  val->coreaddr = owner_val;
                  list0_item_addr_start = list0_cur_item_addr;
                  return 0;
                }
            }

          sprintf (str_tmp, "& (*(ListItem_t *) %d)->pxNext",
                   list0_cur_item_addr);
          if (csky_rtos_symbol2offset (str_tmp, &list0_cur_item_addr_p) < 0)
            {
              return -1;
            }
          list0_cur_item_addr
            = read_memory_unsigned_integer (list0_cur_item_addr_p,
                                            4, byte_order);

          if (cur_listitem_addr_start == list0_cur_item_addr)
            {
              /* FIXME: msg no task find in cur list when uxNumberOfItems
                 is not zero.  */
              break;
            }
        }
      /* Some vars init for continue.  */
      /* List end, return.  */
      /* FIXME: msg: no task find ???  */
      if (freertos_cur_priority == 1)
        {
          freertos_cur_priority = 0;
          return -1;
        }
      /* Scan the next priority list.  */
      freertos_cur_priority -= 1;
      task_num_in_list_addr = 0;
      task_num_in_list = 0;
    }
  return -1;
}

static const  char* freertos_thread_list_offset_table[2] =
{
  "& pxReadyTasksLists",
  "sizeof(pxReadyTasksLists)/sizeof(List_t)"
};
static int freertos_thread_list_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES freertos_field_thread_list =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  0,
  2,
  freertos_thread_list_offset_table,
  freertos_thread_list_offset_cache,
  freertos_parse_readytasklist_first,
  NULL  /* No output method for this field.  */
};


/* RTOS_FIELD_DES for current thread field.  */
/* FIXME: addrs of thread_list and current thread field is not the "same"  */
static const char* freertos_current_thread_offset_table[1] =
{
  "&pxCurrentTCB"
};
static int freertos_current_thread_offset_cache[1] = {-1};
static RTOS_FIELD_DES freertos_field_current_thread =
{
  "current_thread",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  freertos_current_thread_offset_table,
  freertos_current_thread_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field(for ready lists).  */

static int
freertos_parse_readytasklist_next (CORE_ADDR tcb_base,
                                   struct rtos_field_des *itself,
                                   RTOS_FIELD *val)
{
  char str_tmp[256];
  int task_list_changed = 0;
  int task_num_in_list_addr = 0;
  unsigned int task_num_in_list = 0;
  int list0_cur_item_addr_p = 0;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  if (freertos_cur_priority == 0)
    {
      /* FIXME: msg ???  */
      val->coreaddr = 0;
      return 0;
    }

  while (1)
    {
      while (1)
        {
          sprintf (str_tmp, "& (*(ListItem_t *) %d)->pxNext",
                   list0_cur_item_addr);
          if (csky_rtos_symbol2offset (str_tmp, &list0_cur_item_addr_p) < 0)
            {
              return -1;
            }
          list0_cur_item_addr
            = read_memory_unsigned_integer (list0_cur_item_addr_p,
                                            4, byte_order);
          if (list0_cur_item_addr == list0_item_addr_start)
            break;
          if (list0_cur_item_addr != list0_cur_end)
            {
              int owner_addr = 0;
              int owner_val = 0;
              sprintf (str_tmp, " &(*(ListItem_t *) %d)->pvOwner",
                       list0_cur_item_addr);
              if (csky_rtos_symbol2offset (str_tmp, &owner_addr) < 0)
                {
                   return -1;
                }
              owner_val = read_memory_unsigned_integer (owner_addr,
                                                        4, byte_order);
              if (owner_val != 0)
                {
                  val->coreaddr = owner_val;
                  if (task_list_changed)
                    list0_item_addr_start = list0_cur_item_addr;
                  return 0;
                }
            }
        }
      if (freertos_cur_priority == 1)
        {
          /* FIXME: msg no extra task find.  */
          freertos_cur_priority = 0;
          val->coreaddr = 0;
          return 0;
        }
      /* Scan next ready list.  */
      freertos_cur_priority -= 1;
      while (1)
        {
          sprintf (str_tmp, "& ((*(List_t *)%d)->uxNumberOfItems)",
                   readytasklist_addr
                   + TaskListSize * (freertos_cur_priority - 1));
          if (csky_rtos_symbol2offset (str_tmp, &task_num_in_list_addr) < 0)
            {
              return -1;
            }
          task_num_in_list
            = read_memory_unsigned_integer (task_num_in_list_addr,
                                            4, byte_order);
          if (task_num_in_list == 0)
            {
              /* List end, return.  */
              /* FIXME: msg: no task find ???  */
              if (freertos_cur_priority == 1)
                {
                  freertos_cur_priority = 0;
                  val->coreaddr = 0;
                  return 0;
                }
              /* Scan the next priority list.  */
              freertos_cur_priority -= 1;
              task_num_in_list_addr = 0;
              task_num_in_list = 0;
              continue;
            }
          /* Get addr of "xListEnd"  */
          sprintf (str_tmp, "&((*(List_t *) %d)->xListEnd )",
                   readytasklist_addr
                   + TaskListSize * (freertos_cur_priority - 1));
          if (csky_rtos_symbol2offset (str_tmp, &list0_cur_end) < 0)
            {
              return -1;
            }

          /* Get addr of current list "pxIndex"  */
          sprintf (str_tmp, "& ((*(List_t *) %d)->pxIndex)",
                   readytasklist_addr
                   + TaskListSize * (freertos_cur_priority - 1));
          if (csky_rtos_symbol2offset (str_tmp, &list0_cur_item_addr_p) < 0)
            {
              return -1;
            }
          list0_cur_item_addr
            = read_memory_unsigned_integer (list0_cur_item_addr_p,
                                            4, byte_order);
          task_list_changed = 1;
          break;
        }
    }
}

static RTOS_FIELD_DES freertos_field_list_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  freertos_parse_readytasklist_next,
  NULL /* No output method for this field.  */
};


/* ***************************************************************
 *
 *  Get first tasks info from TaskList_n
 *  n from 1 to 5
 *  n=1 : xDelayedTaskList1 = *pxDelayedTaskList
 *  n=2 : xDelayedTaskList2 = *pxOverflowDelayedTaskList
 *  n=3 : xPendingReadyList
 *  n=4 : xTasksWaitingTermination
 *  n=5 : xSuspendedTaskList
 *
 *  1~3 must exist, 4, 5 are configured by target
 *  0 is not used
 *
 * **************************************************************  */

int listn_cur_end[6] = {0};
int listn_cur_item_addr[6] = {0};
int listn_item_addr_start[6] = {0};
int tasklistn_addr[6] = {0};

/* Function for get first task in lists.  */
static int
freertos_parse_tasklist_n_first_common (CORE_ADDR tcb_base,
                                        struct rtos_field_des *itself,
                                        RTOS_FIELD *val,
                                        int tasklist_n)
{
  char str_tmp[256];
  int task_num_in_list_addr = 0;
  unsigned int task_num_in_list = 0;
  int cur_listitem_addr_start = 0;
  int listn_cur_item_addr_p = 0;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Get addr of pxReadyTasksLists.  */
  if (itself->offset_cache[0] == -1)
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 0)),
                                   &(itself->offset_cache[0])) < 0)
        return -1;
    }
  tasklistn_addr[tasklist_n] = itself->offset_cache[0];

  sprintf (str_tmp, "& ((*(List_t *)%d)->uxNumberOfItems)",
           tasklistn_addr[tasklist_n]);
  if (csky_rtos_symbol2offset(str_tmp, &task_num_in_list_addr) < 0)
    {
      return -1;
    }

  task_num_in_list = read_memory_unsigned_integer (task_num_in_list_addr,
                                                   4, byte_order);
  if (task_num_in_list == 0)
    {
      /* FIXME: no task find, msg ???  */
      val->coreaddr = 0;
      return 0;
    }

  /* Get addr of "xListEnd"  */
  sprintf (str_tmp, "&((*(List_t *) %d)->xListEnd )",
           tasklistn_addr[tasklist_n]);
  if (csky_rtos_symbol2offset (str_tmp, &listn_cur_end[tasklist_n]) < 0)
    {
      return -1;
    }

  /* Get addr of current list "pxIndex"  */
  sprintf (str_tmp, "& ((*(List_t *) %d)->pxIndex)",
           tasklistn_addr[tasklist_n]);
  if (csky_rtos_symbol2offset (str_tmp, &listn_cur_item_addr_p) < 0)
    {
      return -1;
    }
  listn_cur_item_addr[tasklist_n]
    = read_memory_unsigned_integer (listn_cur_item_addr_p,
                                    4, byte_order);
  cur_listitem_addr_start = listn_cur_item_addr[tasklist_n];

  while (1)
    {
      if (listn_cur_end[tasklist_n] != listn_cur_item_addr[tasklist_n])
        {
          int owner_addr = 0;
          int owner_val = 0;
          sprintf (str_tmp, " &(*(ListItem_t *) %d)->pvOwner",
                   listn_cur_item_addr[tasklist_n]);
          if (csky_rtos_symbol2offset (str_tmp, &owner_addr) < 0)
            {
               return -1;
            }
          owner_val = read_memory_unsigned_integer (owner_addr,
                                                    4, byte_order);
          if (owner_val != 0)
            {
              val->coreaddr = owner_val;
              listn_item_addr_start[tasklist_n]
                = listn_cur_item_addr[tasklist_n];
              return 0;
            }
        }
      sprintf (str_tmp, "& (*(ListItem_t *) %d)->pxNext",
               listn_cur_item_addr[tasklist_n]);
      if (csky_rtos_symbol2offset (str_tmp, &listn_cur_item_addr_p) < 0)
        {
          return -1;
        }
      listn_cur_item_addr[tasklist_n]
        = read_memory_unsigned_integer (listn_cur_item_addr_p,
                                        4, byte_order);

      if (cur_listitem_addr_start == listn_cur_item_addr[tasklist_n])
        {
          /* FIXME: msg no task find in cur list when uxNumberOfItems
             is not zero  */
          return -1;
        }
    }
}

/* Function for get next task info from list_n.  */

static int
freertos_parse_tasklist_n_next_common (CORE_ADDR tcb_base,
                                       struct rtos_field_des *itself,
                                       RTOS_FIELD *val,
                                       int tasklist_n)
{
  char str_tmp[256];
  CORE_ADDR task_num_in_list_addr = 0;
  unsigned int task_num_in_list = 0;
  int listn_cur_item_addr_p = 0;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  while (1)
    {
      sprintf (str_tmp, "& (*(ListItem_t *) %d)->pxNext",
               listn_cur_item_addr[tasklist_n]);
      if (csky_rtos_symbol2offset (str_tmp,
                                   &listn_cur_item_addr_p) < 0)
        {
          return -1;
        }
      listn_cur_item_addr[tasklist_n]
         = read_memory_unsigned_integer (listn_cur_item_addr_p,
                                         4, byte_order);

      if (listn_cur_item_addr[tasklist_n]
            == listn_item_addr_start[tasklist_n])
        {
          /* FIXME: no next task find in this list, msg ?  */
          val->coreaddr = 0;
          return 0;
        }
      if (listn_cur_item_addr[tasklist_n] != listn_cur_end[tasklist_n])
        {
          int owner_addr = 0;
          int owner_val = 0;
          sprintf (str_tmp, " &(*(ListItem_t *) %d)->pvOwner",
                   listn_cur_item_addr[tasklist_n]);
          if (csky_rtos_symbol2offset (str_tmp, &owner_addr) < 0)
            {
               return -1;
            }
          owner_val = read_memory_unsigned_integer (owner_addr,
                                                    4, byte_order);
          if (owner_val != 0)
            {
              val->coreaddr = owner_val;
              return 0;
            }
        }
    }
}


/* RTOS_FIELD_DES for thread_list field.  */

/* Get first task info from xDelayedTaskList1.  */

static int
freertos_parse_delayedtasklist1_first (CORE_ADDR tcb_base,
                                       struct rtos_field_des *itself,
                                       RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_first_common (tcb_base,
                                                 itself,
                                                 val,
                                                 1);/* xDelayedTaskList1  */
}

static const char* freertos_thread_list_offset_table1[1] =
{
  "& xDelayedTaskList1"
};
static int freertos_thread_list_offset_cache1[1] = {-1};
static RTOS_FIELD_DES freertos_field_thread_list1 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  freertos_thread_list_offset_table1,
  freertos_thread_list_offset_cache1,
  freertos_parse_delayedtasklist1_first,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field(for delayed list1).  */

static int
freertos_parse_delayedtasklist1_next (CORE_ADDR tcb_base,
                                      struct rtos_field_des *itself,
                                      RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_next_common (tcb_base,
                                                itself,
                                                val,
                                                1);/* xDelayedTaskList1  */
}

static RTOS_FIELD_DES freertos_field_list1_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  NULL,
  NULL,
  freertos_parse_delayedtasklist1_next,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for thread_list field.  */

/* Get first task info from xDelayedTaskList2.  */

static int
freertos_parse_delayedtasklist2_first (CORE_ADDR tcb_base,
                                       struct rtos_field_des *itself,
                                       RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_first_common (tcb_base,
                                                 itself,
                                                 val,
                                                 2);/* xDelayedTaskList2  */
}

static const  char* freertos_thread_list_offset_table2[1] =
{
  "& xDelayedTaskList2"
};
static int freertos_thread_list_offset_cache2[1] = {-1};
static RTOS_FIELD_DES freertos_field_thread_list2 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  freertos_thread_list_offset_table2,
  freertos_thread_list_offset_cache2,
  freertos_parse_delayedtasklist2_first,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field(for delayed list2).  */

static int
freertos_parse_delayedtasklist2_next (CORE_ADDR tcb_base,
                                      struct rtos_field_des *itself,
                                      RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_next_common (tcb_base,
                                                itself,
                                                val,
                                                2);/* xDelayedTaskList2  */
}

static RTOS_FIELD_DES freertos_field_list2_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  NULL,
  NULL,
  freertos_parse_delayedtasklist2_next,
  NULL // No output method for this field
};


/* RTOS_FIELD_DES for thread_list field.  */

/* Get first task info from xPendingReadyList.  */

static int
freertos_parse_pendingreadylist_first (CORE_ADDR tcb_base,
                                       struct rtos_field_des *itself,
                                       RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_first_common (tcb_base,
                                                 itself,
                                                 val,
                                                 3);/* xPendingReadyList  */
}
static const  char* freertos_thread_list_offset_table3[1] =
{
  "& xPendingReadyList"
};
static int freertos_thread_list_offset_cache3[1] = {-1};
static RTOS_FIELD_DES freertos_field_thread_list3 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  freertos_thread_list_offset_table3,
  freertos_thread_list_offset_cache3,
  freertos_parse_pendingreadylist_first,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field(for xPendingReadyList).  */

static int
freertos_parse_pendingreadylist_next (CORE_ADDR tcb_base,
                                      struct rtos_field_des *itself,
                                      RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_next_common (tcb_base,
                                                itself,
                                                val,
                                                3);/* xPendingReadyList  */
}

static RTOS_FIELD_DES freertos_field_list3_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  NULL,
  NULL,
  freertos_parse_pendingreadylist_next,
  NULL /* No output method for this field.  */
};


/* RTOS_FIELD_DES for thread_list field.  */

/* Get first task info from xTasksWaitingTermination.  */

static int
freertos_parse_taskswaitingtermination_first (CORE_ADDR tcb_base,
                                              struct rtos_field_des *itself,
                                              RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_first_common (tcb_base,
                                                 itself,
                                                 val,
                                             /* xTasksWaitingTermination  */
                                                 4);
}

static const  char* freertos_thread_list_offset_table4[1] =
{
  "& xTasksWaitingTermination"
};
static int freertos_thread_list_offset_cache4[1] = {-1};
static RTOS_FIELD_DES freertos_field_thread_list4 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  freertos_thread_list_offset_table4,
  freertos_thread_list_offset_cache4,
  freertos_parse_taskswaitingtermination_first,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field(for xTasksWaitingTermination).  */

static int
freertos_parse_taskswaitingtermination_next (CORE_ADDR tcb_base,
                                             struct rtos_field_des *itself,
                                             RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_next_common (tcb_base,
                                                itself,
                                                val,
                                            /* xTasksWaitingTermination  */
                                                4);
}

static RTOS_FIELD_DES freertos_field_list4_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  NULL,
  NULL,
  freertos_parse_taskswaitingtermination_next,
  NULL /* No output method for this field.  */
};


/* RTOS_FIELD_DES for thread_list field.  */

/* Get first task info from xSuspendedTaskList.  */

static int
freertos_parse_suspendedtasklist_first (CORE_ADDR tcb_base,
                                        struct rtos_field_des *itself,
                                        RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_first_common (tcb_base,
                                                 itself,
                                                 val,
                                                /* xSuspendedTaskList  */
                                                 5);
}

static const  char* freertos_thread_list_offset_table5[1] =
{
  "& xSuspendedTaskList"
};
static int freertos_thread_list_offset_cache5[1] = {-1};
static RTOS_FIELD_DES freertos_field_thread_list5 =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  freertos_thread_list_offset_table5,
  freertos_thread_list_offset_cache5,
  freertos_parse_suspendedtasklist_first,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field(for xSuspendedTaskList).  */

static int
freertos_parse_suspendedtasklist_next (CORE_ADDR tcb_base,
                                       struct rtos_field_des *itself,
                                       RTOS_FIELD *val)
{
  return freertos_parse_tasklist_n_next_common (tcb_base,
                                                itself,
                                                val,
                                                5);/* xSuspendedTaskList  */
}

static RTOS_FIELD_DES freertos_field_list5_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  NULL,
  NULL,
  freertos_parse_suspendedtasklist_next,
  NULL /* No output method for this field.  */
};


/* FreeRTOS 5 basic fields defination.  */

/* RTOS_FIELD_DES for thread id field.  */

static int
freertos_parse_thread_id (CORE_ADDR tcb_base,
                          struct rtos_field_des* itself,
                          RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}
static RTOS_FIELD_DES freertos_field_internal_id =
{
  "id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  freertos_parse_thread_id,
  NULL /* Use default output field method.  */
};

/* RTOS_FIELD_DES for stack_ptr field.  */

static int
freertos_parse_stack_ptr (CORE_ADDR tcb_base,
                          struct rtos_field_des* itself,
                          RTOS_FIELD *val)
{
  int cur_tcb_addr;
  int cur_tcb_addr_p;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  if (csky_rtos_symbol2offset (((char *)*freertos_current_thread_offset_table),
                               &cur_tcb_addr_p) < 0)
    {
      return -1;
    }
  cur_tcb_addr = read_memory_unsigned_integer (cur_tcb_addr_p,
                                               4, byte_order);

  if (tcb_base == cur_tcb_addr)
    {
      struct regcache *regcache;
      ptid_t ptid;

      ptid = ptid_build (rtos_ops.target_ptid.pid, 0, tcb_base);
      regcache = get_thread_regcache (ptid);
      regcache_raw_read (regcache, CSKY_SP_REGNUM,
                        (gdb_byte *)&(val->coreaddr));
      return 0;
    }
  else
    {
      char str_tmp[256];
      int cur_sp_addr_p;

      sprintf (str_tmp, "& (*(TCB_t *)%d)->pxTopOfStack", tcb_base);
      if (csky_rtos_symbol2offset (str_tmp, &cur_sp_addr_p) < 0)
        {
          return -1;
        }
      val->coreaddr = read_memory_unsigned_integer (cur_sp_addr_p,
                                                    4, byte_order);
      return 0;
    }
}

static RTOS_FIELD_DES freertos_field_stack_ptr =
{
  "stack_ptr",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  freertos_parse_stack_ptr,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for entry_base field.  */
static const char* freertos_entry_base_offset_table[1] =
{
  "((TCB_t *)0)->pxTopOfStack"
};
static int freertos_entry_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES freertos_field_entry_base =
{
  "entry_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  freertos_entry_base_offset_table,
  freertos_entry_base_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for tcb_base field.  */

static int
freertos_parse_tcb_base (CORE_ADDR tcb_base,
                         struct rtos_field_des* itself,
                         RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}
static RTOS_FIELD_DES freertos_field_tcb_base =
{
  "tcb_base",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  freertos_parse_tcb_base,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for task_name field.  */
static const char* freertos_task_name_offset_table[1] =
{
  "& ((TCB_t *)0)->pcTaskName"
};
static int freertos_task_name_offset_cache[1] = {-1};
static RTOS_FIELD_DES freertos_field_task_name =
{
  "task_name",
  String,
  4,
  NULL,
  1,
  1,
  freertos_task_name_offset_table,
  freertos_task_name_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for uxPriority field.  */
static const char* freertos_task_priority_offset_table[1] =
{
  "& ((TCB_t *)0)->uxPriority"
};
static int freertos_task_priority_offset_cache[1] = {-1};
static RTOS_FIELD_DES freertos_field_priority =
{
  "priority",
  Integer,
  4,
  NULL,
  1,
  1,
  freertos_task_priority_offset_table,
  freertos_task_priority_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for State field.  */

static int
freertos_task_parse_state_field (CORE_ADDR tcb_base,
                                 struct rtos_field_des* itself,
                                 RTOS_FIELD *val)
{
  char str_tmp[256];
  int i = 0;
  int pvowner_addr = 0;
  int pvowner_addr_p = 0;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Get TCB_t->xGenericListItem->pvOwner.  */
  sprintf (str_tmp, "& (*(TCB_t *)%d)->xGenericListItem->pvContainer",
           tcb_base);
  if (csky_rtos_symbol2offset (str_tmp, &pvowner_addr_p) < 0)
    {
      return -1;
    }
  pvowner_addr = read_memory_unsigned_integer (pvowner_addr_p,
                                               4, byte_order);

  /* Get addr of pxReadyTaskLists.  */
  if (freertos_thread_list_offset_cache[0] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(freertos_thread_list_offset_table + 0)),
             (freertos_thread_list_offset_cache + 0)) < 0)
        return -1;
    }
  if (freertos_thread_list_offset_cache[1] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(freertos_thread_list_offset_table + 1)),
             (freertos_thread_list_offset_cache + 1)) < 0)
        return -1;
    }
  for (i = 0; i < freertos_thread_list_offset_cache[1]; i++)
     {
       if (pvowner_addr == (freertos_thread_list_offset_cache[0]
                            + TaskListSize * i))
         {
           val->IntVal = 'R';
           return 0;
         }
     }

  /* Get addr of xDelayedTaskList1.  */
  if (freertos_thread_list_offset_cache1[0] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(freertos_thread_list_offset_table1 + 0)),
             (freertos_thread_list_offset_cache1 + 0)) < 0)
        return -1;
    }
  if (pvowner_addr == freertos_thread_list_offset_cache1[0])
    {
      val->IntVal = 'B';
      return 0;
    }

  /* Get addr of xDelayedTaskList2.  */
  if (freertos_thread_list_offset_cache2[0] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(freertos_thread_list_offset_table2 + 0)),
             (freertos_thread_list_offset_cache2 + 0)) < 0)
        return -1;
    }
  if (pvowner_addr == freertos_thread_list_offset_cache2[0])
    {
      val->IntVal = 'B';
      return 0;
    }

  /* Get addr of xPendingReadyList.  */
  if (freertos_thread_list_offset_cache3[0] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(freertos_thread_list_offset_table3 + 0)),
             (freertos_thread_list_offset_cache3 + 0)) < 0)
         return -1;
    }
  if (pvowner_addr == freertos_thread_list_offset_cache3[0])
    {
      val->IntVal = 'R';
      return 0;
    }

  /* Get addr of xTasksWaitingTermination.  */
  if (freertos_thread_list_offset_cache4[0] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(freertos_thread_list_offset_table4 + 0)),
             (freertos_thread_list_offset_cache4 + 0)) < 0)
         return -1;
    }
  if (pvowner_addr == freertos_thread_list_offset_cache4[0])
    {
      val->IntVal = 'D';
      return 0;
    }

  /* Get addr of xSuspendedTaskList.  */
  if (freertos_thread_list_offset_cache5[0] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(freertos_thread_list_offset_table5 + 0)),
             (freertos_thread_list_offset_cache5 + 0)) < 0)
        return -1;
    }
  if (pvowner_addr == freertos_thread_list_offset_cache5[0])
    {
      val->IntVal = 'S';
      return 0;
    }

  val->IntVal = 'U';
  return 0;
}

static const char *freertos_state_int2String[] =
{
  "ready",
  "blocked",
  "suspended",
  "deleted",
  "unknown"
};

static const char *
freertos_state_int2String_Transfer (int index)
{
  if (index == 'R')
    return freertos_state_int2String[0];
  else if (index == 'B')
    return freertos_state_int2String[1];
  else if (index == 'S')
    return freertos_state_int2String[2];
  else if (index == 'D')
    return freertos_state_int2String[3];
  else
    return freertos_state_int2String[4];
}

static RTOS_FIELD_DES freertos_field_state =
{
  "state",
  IntToString,
  1,
  freertos_state_int2String_Transfer,
  0,
  0,
  NULL,
  NULL,
  freertos_task_parse_state_field,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for uxTCBNumber field.  */
static const char* freertos_task_tcbnumber_offset_table[1] =
{
  "& ((TCB_t *)0)->uxTCBNumber"
};
static int freertos_task_tcbnumber_offset_cache[1] = {-1};
static RTOS_FIELD_DES freertos_field_tcbnumber =
{
  "tcbnumber",
  Integer,
  4,
  NULL,
  1,
  1,
  freertos_task_tcbnumber_offset_table,
  freertos_task_tcbnumber_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for current pc field.  */

static int
freertos_parse_current_pc (CORE_ADDR tcb_base,
                           struct rtos_field_des* itself,
                           RTOS_FIELD *val)
{
  struct regcache *regcache;
  ptid_t ptid;

  ptid = ptid_build (rtos_ops.target_ptid.pid, 0, tcb_base);
  regcache = get_thread_regcache (ptid);
  val->coreaddr = regcache_read_pc (regcache);

  return 0;
}
static RTOS_FIELD_DES freertos_field_current_pc =
{
  "current_pc",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  NULL,
  NULL,
  freertos_parse_current_pc,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack base field.  */
static const char * freertos_stack_base_offset_table[1] =
{
  "& ((TCB_t *)0)->pxStack"
};
static int freertos_stack_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES freertos_field_stack_base =
{
  "stack_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  freertos_stack_base_offset_table,
  freertos_stack_base_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack left field.  */

static int
freertos_parse_stack_left (CORE_ADDR tcb_base,
                           struct rtos_field_des *itself,
                           RTOS_FIELD *val)
{
  char str_tmp[256];
  int stack_base;
  int stack_base_addr;
  RTOS_FIELD cur_sp;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Get SP.  */
  if (freertos_parse_stack_ptr (tcb_base, NULL, &cur_sp))
    {
      return -1;
    }

  /* Get stack base.  */
  sprintf (str_tmp, "& (*(TCB_t *)%d)->pxStack", tcb_base);
  if (csky_rtos_symbol2offset (str_tmp, &stack_base_addr) < 0)
    {
      return -1;
    }
  stack_base = read_memory_unsigned_integer (stack_base_addr,
                                             4, byte_order);

  val->IntVal = cur_sp.coreaddr - stack_base;
  return 0;
}

static RTOS_FIELD_DES freertos_field_stack_left =
{
  "stack_left",
  Integer,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  freertos_parse_stack_left,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for notify state field.  */
static const char *freertos_notify_state_int2String[] =
{
  "eNotWaitingNotification",
  "eWaitingNotification",
  "eNotified",
  "unknown"
};

static const char *
freertos_notify_state_int2String_Transfer (int index)
{
  if (index >=0 || index <= 2)
    return freertos_notify_state_int2String[index];
  else
    return freertos_notify_state_int2String[3];
}

static const char * freertos_notify_state_offset_table[1] =
{
  "& ((TCB_t *)0)->ulNotifiedValue"
};
static int freertos_notify_state_offset_cache[1] = {-1};
static RTOS_FIELD_DES freertos_field_notify_state =
{
  "notifystate",
  IntToString,
  4,
  freertos_notify_state_int2String_Transfer,
  1,
  1,
  freertos_notify_state_offset_table,
  freertos_notify_state_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_TCB definition for FreeRTOS.  */
#define FREERTOS_EXTEND_FIELD_NUM  7
static RTOS_FIELD_DES freertos_tcb_extend_table[NUTTX_EXTEND_FIELD_NUM];

static RTOS_FIELD_DES*
init_freertos_tcb_extend_table ()
{
  freertos_tcb_extend_table[0] = freertos_field_priority;
  freertos_tcb_extend_table[1] = freertos_field_state;
  freertos_tcb_extend_table[2] = freertos_field_tcbnumber;
  freertos_tcb_extend_table[3] = freertos_field_current_pc;
  freertos_tcb_extend_table[4] = freertos_field_stack_base;
  freertos_tcb_extend_table[5] = freertos_field_stack_left;
  freertos_tcb_extend_table[6] = freertos_field_notify_state;
  return freertos_tcb_extend_table;
}

/* Fields needed for "info mthreads *" commands.  */

/* Fields needed for "info mthreads list"  */
static unsigned int freertos_i_mthreads_list_table[] =
{
  1, /* Internal id used for GDB(tcb_base)  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  /* Without macro configUSE_TRACE_FACILITY, there will
     no uxTCBNumber in TCB_t.  */
  /*  RTOS_BASIC_FIELD_NUM + 3, Tcb number  */
  5, /* Name  */
};

/* Fields needed for "info mthreads ID"  */
static unsigned int freertos_i_mthread_one_table[] =
{
  1, /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  /* RTOS_BASIC_FIELD_NUM + 3,  Tcbnumber  */
  RTOS_BASIC_FIELD_NUM + 4, /* Current pc  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack base  */
  RTOS_BASIC_FIELD_NUM + 6, /* Stack size  */
  RTOS_BASIC_FIELD_NUM + 7, /* Notify state  */
  5, /* Name  */
};

/* Fields needed for "info mthreads stack all"  */
static unsigned int freertos_i_mthreads_stack_table[] =
{
  1, /* Id  */
  2, /* Stack_ptr  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack base  */
  RTOS_BASIC_FIELD_NUM + 6, /* Stack size  */
  5, /* Name  */
};

/* ---------- Funcitons defined in RTOS_TCB ------------------------  */

/* To check if this thread_id is valid.  */

static int
csky_freertos_is_valid_task_id (RTOS_FIELD task_id)
{
  return (task_id.IntVal != 0 && ((task_id.IntVal % 2) == 0));
}

/* To check if this reg in task's stack.  */

static int
csky_freertos_is_regnum_in_task_list (RTOS_TCB* rtos_des,
                                      ptid_t ptid,
                                      int regno)
{
  if (rtos_des->rtos_reg_offset_table[regno] != -1)
    {
      return 1;
    }
  return 0;
}


static RTOS_TCB freertos_tcb;

static void
init_freertos_tcb ()
{
  /* 8 tasks lists.  */
  freertos_tcb.task_list_count = 6;
  {
    freertos_tcb.rtos_special_table[0][0] = freertos_field_thread_list;
    freertos_tcb.rtos_special_table[0][1] = freertos_field_current_thread;
    freertos_tcb.rtos_special_table[0][2] = freertos_field_list_next;

    freertos_tcb.rtos_special_table[1][0] = freertos_field_thread_list1;
    freertos_tcb.rtos_special_table[1][1] = freertos_field_current_thread;
    freertos_tcb.rtos_special_table[1][2] = freertos_field_list1_next;

    freertos_tcb.rtos_special_table[2][0] = freertos_field_thread_list2;
    freertos_tcb.rtos_special_table[2][1] = freertos_field_current_thread;
    freertos_tcb.rtos_special_table[2][2] = freertos_field_list2_next;

    freertos_tcb.rtos_special_table[3][0] = freertos_field_thread_list3;
    freertos_tcb.rtos_special_table[3][1] = freertos_field_current_thread;
    freertos_tcb.rtos_special_table[3][2] = freertos_field_list3_next;

    freertos_tcb.rtos_special_table[4][0] = freertos_field_thread_list4;
    freertos_tcb.rtos_special_table[4][1] = freertos_field_current_thread;
    freertos_tcb.rtos_special_table[4][2] = freertos_field_list4_next;

    freertos_tcb.rtos_special_table[5][0] = freertos_field_thread_list5;
    freertos_tcb.rtos_special_table[5][1] = freertos_field_current_thread;
    freertos_tcb.rtos_special_table[5][2] = freertos_field_list5_next;
  }

  /* 5 basic fields.  */
  {
    freertos_tcb.rtos_tcb_table[0] = freertos_field_internal_id;
    freertos_tcb.rtos_tcb_table[1] = freertos_field_stack_ptr;
    freertos_tcb.rtos_tcb_table[2] = freertos_field_entry_base;
    freertos_tcb.rtos_tcb_table[3] = freertos_field_tcb_base;
    freertos_tcb.rtos_tcb_table[4] = freertos_field_task_name;
  }

  /* Extend field number.  */
  freertos_tcb.extend_table_num = FREERTOS_EXTEND_FIELD_NUM;
  freertos_tcb.rtos_tcb_extend_table = init_freertos_tcb_extend_table ();

  /* For "info mthreads commands"  */
  freertos_tcb.i_mthreads_list = freertos_i_mthreads_list_table;
  freertos_tcb.i_mthreads_list_size
    = sizeof (freertos_i_mthreads_list_table)
        / sizeof (freertos_i_mthreads_list_table[0]);

  freertos_tcb.i_mthreads_stack = freertos_i_mthreads_stack_table;
  freertos_tcb.i_mthreads_stack_size
    = sizeof (freertos_i_mthreads_stack_table)
        / sizeof (freertos_i_mthreads_stack_table[0]);
  freertos_tcb.i_mthread_one = freertos_i_mthread_one_table;
  freertos_tcb.i_mthread_one_size
    = sizeof (freertos_i_mthread_one_table)
        / sizeof (freertos_i_mthread_one_table[0]);

  /* Freertos read/write rigister handler.  */
  freertos_tcb.rtos_reg_offset_table = csky_freertos_reg_offset_table;
  freertos_tcb.to_get_register_base_address = NULL;
  freertos_tcb.to_fetch_registers =  NULL;
  freertos_tcb.to_store_registers =  NULL;

  /* Freertos check thread_id valid.  */
  freertos_tcb.IS_VALID_TASK = csky_freertos_is_valid_task_id;

  /* Check regno in task's stack.  */
  freertos_tcb.is_regnum_in_task_list = csky_freertos_is_regnum_in_task_list;
}

/* ************ 2. target_ops freertos_ops ************************  */
static struct target_ops freertos_ops;

/* ************ 3. freeros_open() *********************  */
static void freertos_open (const char * name, int from_tty);

/* Open for freertos system.  */
static void
freertos_open (const char * name, int from_tty)
{
  rtos_ops.current_ops
    = rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].ops;
  rtos_ops.rtos_des
    = rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].rtos_tcb_des;
  rtos_ops.event_des
    = rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].rtos_event_des;
  common_open (name, from_tty);
}

#define FREERTOS_NAME_NUM 3
static const char * freertos_names[FREERTOS_NAME_NUM] =
{
  "freertos",
  "FreeRTOS",
  "FREERTOS"
};


/* Start Implementation for rtos rhino support
  1. RTOS_TCB rhino_tcb
  2. struct target_ops rhino_ops
  3. rhino_open() */

/* ***********  1. RTOS_TCB rhino_tcb **********************  */

/* --------------- csky_rhino_reg_offset_table ----------------------  */
static int csky_rhino_reg_offset_table[] = {
  /* General register 0~15: 0 ~ 15.  */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
#ifndef CSKYGDB_CONFIG_ABIV2
  -1,     0x2*4,   0x3*4,   0x4*4,   0x5*4,   0x6*4,   0x7*4,   0x8*4,
  0x9*4,  0xa*4,   0xb*4,   0xc*4,   0xd*4,   0xe*4,   0xf*4,   0x10*4,
#else   /* CSKYGDB_CONFIG_ABIV2 */
  0x0*4,  0x1*4,   0x2*4,   0x3*4,   0x4*4,   0x5*4,   0x6*4,   0x7*4,
  0x8*4,  0x9*4,   0xa*4,   0xb*4,   0xc*4,   0xd*4,   -1,      0xe*4,
#endif  /* CSKYGDB_CONFIG_ABIV2 */
  /* Dsp hilo register: 97, 98  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       -1,      -1,   -1,    -1,

  /* FPU reigster: 24~55  */
  /*
  "cp1.gr0",  "cp1.gr1",  "cp1.gr2",  "cp1.gr3",
  "cp1.gr4",  "cp1.gr5",  "cp1.gr6",  "cp1.gr7",
  "cp1.gr8",  "cp1.gr9",  "cp1.gr10", "cp1.gr11",
  "cp1.gr12", "cp1.gr13", "cp1.gr14", "cp1.gr15",
  "cp1.gr16", "cp1.gr17", "cp1.gr18", "cp1.gr19",
  "cp1.gr10", "cp1.gr21", "cp1.gr22", "cp1.gr23",
  "cp1.gr24", "cp1.gr25", "cp1.gr26", "cp1.gr27",
  "cp1.gr28", "cp1.gr29", "cp1.gr30", "cp1.gr31",
  */
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,

  /* Hole  */
  /*
  "",      "",    "",     "",     "",    "",   "",    "",
  "",      "",    "",     "",     "",    "",   "",    "",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* PC : 72 : 64  */
  /*
  "pc",
  */
#ifndef CSKYGDB_CONFIG_ABIV2
  0x0*4,
#else
  0x10*4,
#endif
  /* Optional register(ar) : 73~88 :  16 ~ 31  */
  /*
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* Control registers (cr) : 89~120 : 32 ~ 63  */
  /*
  "psr",   "vbr", "epsr", "fpsr", "epc",   "fpc",  "ss0",  "ss1",
  "ss2",   "ss3",  "ss4",  "gcr", "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30",     "",
  */
#ifndef CSKYGDB_CONFIG_ABIV2
  0x1*4,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
#else
  0xf*4,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
#endif
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* FPC control register: 0x100 & (32 ~ 38)  */
  /*
  "cp1.cr0","cp1.cr1","cp1.cr2","cp1.cr3","cp1.cr4","cp1.cr5","cp1.cr6",
  */
  -1, -1, -1, -1, -1, -1, -1,

  /* MMU control register: 0xf00 & (32 ~ 40)  */
  /*
  "cp15.cr0",  "cp15.cr1",  "cp15.cr2",  "cp15.cr3",
  "cp15.cr4",  "cp15.cr5",  "cp15.cr6",  "cp15.cr7",
  "cp15.cr8",  "cp15.cr9",  "cp15.cr10", "cp15.cr11",
  "cp15.cr12", "cp15.cr13", "cp15.cr14", "cp15.cr15",
  "cp15.cr16", "cp15.cr29", "cp15.cr30", "cp15.cr31"
  */
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1
};

/* -------------- RTOS_TCB_DES table in RTOS_TCB ---------------------  */
/* Rhino special fields definitions.   */
static unsigned int rhino_cur_klist_addr = 0;

/* RTOS_FIELD_DES for thread_list field.  */

static int
rhino_parse_thread_list (CORE_ADDR tcb_base,
                         struct rtos_field_des *itself,
                         RTOS_FIELD *val)
{
  int i;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  for (i = 0; i < 2; i++)
    {
      if (itself->offset_cache[i] == -1)
        {
          if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + i)),
                                       &(itself->offset_cache[i])) < 0)
            return -1;
        }
    }
  rhino_cur_klist_addr
    = read_memory_unsigned_integer (itself->offset_cache[0], 4, byte_order);

  val->coreaddr = rhino_cur_klist_addr - itself->offset_cache[1];

  return 0;
}

static const  char* rhino_thread_list_offset_table[2] =
{
  "& g_kobj_list.task_head",
  "& ((ktask_t *)0)->task_stats_item"
};
static int rhino_thread_list_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES rhino_field_thread_list =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  0,
  2,
  rhino_thread_list_offset_table,
  rhino_thread_list_offset_cache,
  rhino_parse_thread_list,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for current thread field.  */
static const char* rhino_current_thread_offset_table[2] =
{
  "& g_active_task",
  "0"
};
static int rhino_current_thread_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES rhino_field_current_thread =
{
  "current_thread",
  CoreAddr,
  4,
  NULL,
  1,
  2,
  rhino_current_thread_offset_table,
  rhino_current_thread_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field.  */

static int
rhino_parse_thread_list_next (CORE_ADDR tcb_base,
                              struct rtos_field_des *itself,
                              RTOS_FIELD *val)
{
  int klist_addr;
  char str_tmp[256];
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  sprintf (str_tmp, "& ((klist_t *)%d)->next", rhino_cur_klist_addr);
  if (csky_rtos_symbol2offset (str_tmp, &klist_addr) < 0)
    {
      return -1;
    }

  klist_addr
    = read_memory_unsigned_integer (klist_addr, 4, byte_order);

  if (rhino_thread_list_offset_cache[0] == -1)
    {
      if (csky_rtos_symbol2offset
           (((char *)*(rhino_thread_list_offset_table + 0)),
            &(rhino_thread_list_offset_cache[0])) < 0)
        return -1;
    }
  if (klist_addr == rhino_thread_list_offset_cache[0])
    {
      val->coreaddr = 0;
      return 0;
    }

  if (rhino_thread_list_offset_cache[1] == -1)
    {
      if (csky_rtos_symbol2offset
            (((char *)*(rhino_thread_list_offset_table + 1)),
             &(rhino_thread_list_offset_cache[1])) < 0)
        return -1;
    }

  val->coreaddr = klist_addr - rhino_thread_list_offset_cache[1];

  rhino_cur_klist_addr = klist_addr;

  return 0;
}

static const char* rhino_list_next_offset_table[1] =
{
  "((klist_t *)0)->next"
};
static int rhino_list_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES rhino_field_list_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  0,
  1,
  rhino_list_next_offset_table,
  rhino_list_next_offset_cache,
  rhino_parse_thread_list_next,
  NULL /* No output method for this field.  */
};

/* Rhino 5 basic fields definition.  */

/* RTOS_FIELD_DES for thread id field.  */

static int
rhino_parse_thread_id (CORE_ADDR tcb_base,
                       struct rtos_field_des* itself,
                       RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}

static RTOS_FIELD_DES rhino_field_id =
{
  "id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  rhino_parse_thread_id,
  NULL /* use default output field method.  */
};

static const char* rhino_stack_ptr_offset_table[1] =
{
  "((ktask_t *)0)->task_stack"
};
static int rhino_stack_ptr_offset_cache[1] = {-1};
static RTOS_FIELD_DES rhino_field_stack_ptr =
{
  "stack_ptr",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  rhino_stack_ptr_offset_table,
  rhino_stack_ptr_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for entry_base field.  */
/* Read saved PC.  */
static const char* rhino_entry_base_offset_table[1] =
{
  "((ktask_t *)0)->task_stack"
};
static int rhino_entry_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES rhino_field_entry_base =
{
  "entry_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  rhino_entry_base_offset_table,
  rhino_entry_base_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for tcb_base field.  */

static int
rhino_parse_tcb_base (CORE_ADDR tcb_base,
                      struct rtos_field_des* itself,
                      RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}
static RTOS_FIELD_DES rhino_field_tcb_base =
{
  "tcb_base",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  rhino_parse_tcb_base,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for task_name field.  */
static const char* rhino_task_name_offset_table[2] =
{
  "& ((ktask_t *)0)->task_name",
  "0"
};
static int rhino_task_name_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES rhino_field_task_name =
{
  "task_name",
  String,
  4,
  NULL,
  1,
  2,
  rhino_task_name_offset_table,
  rhino_task_name_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* Rhino_extend_field definition.  */
/* RTOS_FIELD_DES for priority field.  */
static const char* rhino_priority_offset_table[1] =
{
  "& ((ktask_t *)0)->prio"
};
static int rhino_priority_offset_cache[1] = {-1};
static RTOS_FIELD_DES rhino_field_priority =
{
  "priority",
  Integer,
  1,
  NULL,
  1,
  1,
  rhino_priority_offset_table,
  rhino_priority_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

const static char * rhino_state_int2String[] =
{
  "K_SEED",             /* 0  */
  "K_RDY",              /* 1  */
  "K_PEND",             /* 2  */
  "K_SUSPENDED",        /* 3  */
  "K_PEDN_SUSPENDED",   /* 4  */
  "K_SLEEP",            /* 5  */
  "K_SLEEP_SUSPENDED",  /* 6  */
  "K_DELETED"           /* 7  */
};

const static char *
rhino_state_int2String_Transfer (int index)
{
  int str_num;
  str_num
    = sizeof (rhino_state_int2String) / sizeof (rhino_state_int2String[0]);
  if (index < str_num-1)
    {
      return rhino_state_int2String[index];
    }
  else
    {
      return rhino_state_int2String[str_num -1];
    }
}

static const char* rhino_state_offset_table[1] =
{
  "& ((ktask_t *)0)->task_state"
};
static int rhino_state_offset_cache[1] = {-1};
static RTOS_FIELD_DES rhino_field_state =
{
  "state",
  IntToString,
  1,
  rhino_state_int2String_Transfer,
  1,
  1,
  rhino_state_offset_table,
  rhino_state_offset_cache,
  NULL,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_size.  */
static const char* rhino_stack_size_offset_table[1] =
{
  "& ((ktask_t *)0)->stack_size"
};
static int rhino_stack_size_offset_cache[1] = {-1};
static RTOS_FIELD_DES rhino_field_stack_size =
{
  "stack_size",
  Integer,
  4,
  NULL,
  1,
  1,
  rhino_stack_size_offset_table,
  rhino_stack_size_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for current_pc.  */
static int
rhino_parse_current_pc (CORE_ADDR tcb_base,
                        struct rtos_field_des* itself,
                        RTOS_FIELD *val)
{
  CORE_ADDR thread_id;
  struct regcache *regcache;
  ptid_t ptid;

  thread_id = tcb_base;

  ptid = ptid_build (rtos_ops.target_ptid.pid, 0, thread_id);
  regcache = get_thread_regcache (ptid);
  val->coreaddr = regcache_read_pc (regcache);

  return 0;
}

static RTOS_FIELD_DES rhino_field_current_pc =
{
  "current_pc",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  rhino_parse_current_pc,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_base.  */
static const char* rhino_stack_base_offset_table[1] =
{
  "& ((ktask_t *)0)->task_stack_base"
};
static int rhino_stack_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES rhino_field_stack_base =
{
  "stack_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  rhino_stack_base_offset_table,
  rhino_stack_base_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_TCB definition for rhino.  */
#define RHINO_EXTEND_FIELD_NUM 5
static RTOS_FIELD_DES rhino_tcb_extend_table[UCOSIII_EXTEND_FIELD_NUM];

static RTOS_FIELD_DES*
init_rhino_tcb_extend_table ()
{
  rhino_tcb_extend_table[0] = rhino_field_priority;
  rhino_tcb_extend_table[1] = rhino_field_state;
  rhino_tcb_extend_table[2] = rhino_field_stack_size;
  rhino_tcb_extend_table[3] = rhino_field_current_pc;
  rhino_tcb_extend_table[4] = rhino_field_stack_base;
  return rhino_tcb_extend_table;
}

/* Fields needed for "info mthreads *" commands.  */

/* Fields needed for "info mthreads list".  */
static unsigned int rhino_i_mthreads_list_table[] =
{
  1,                        /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  5,                        /* Name  */
};

/* Fields needed for "info mthreads ID".  */
static unsigned int rhino_i_mthread_one_table[] =
{
  1,                        /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  RTOS_BASIC_FIELD_NUM + 4, /* Current_pc  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack size  */
  5,                        /* Name  */
};

/* Fields needed for "info mthreads stack all".  */
static unsigned int rhino_i_mthreads_stack_table[] =
{
  1,                        /* Id  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack_base  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack_size  */
  2,                        /* Stack_ptr  */
  5,                        /* Name  */
};

/* ---------- Funcitons defined in RTOS_TCB ---------------  */

static int
csky_rhino_is_valid_task_id (RTOS_FIELD task_id)
{
  return 1;
}

/* To check if this reg in task's stack.  */

static int
csky_rhino_is_regnum_in_task_list (RTOS_TCB* rtos_des,
                                   ptid_t ptid,
                                   int regno)
{
  if (rtos_des->rtos_reg_offset_table[regno] != -1)
    {
      return 1;
    }
  return 0;
}

#define RHINO_NAME_NUM 2
static const char * rhino_names[RHINO_NAME_NUM] =
{
  "rhino",
  "RHINO"
};

static RTOS_TCB rhino_tcb;

static void
init_rhino_tcb ()
{
  /* 3 special fields.  */
  rhino_tcb.task_list_count = 1;
  {
    rhino_tcb.rtos_special_table[0][0] = rhino_field_thread_list;
    rhino_tcb.rtos_special_table[0][1] = rhino_field_current_thread;
    rhino_tcb.rtos_special_table[0][2] = rhino_field_list_next;
  }
  /* 5 basic fields  */
  {
    rhino_tcb.rtos_tcb_table[0] = rhino_field_id;
    rhino_tcb.rtos_tcb_table[1] = rhino_field_stack_ptr;
    rhino_tcb.rtos_tcb_table[2] = rhino_field_entry_base;
    rhino_tcb.rtos_tcb_table[3] = rhino_field_tcb_base;
    rhino_tcb.rtos_tcb_table[4] = rhino_field_task_name;
  }
  /* Extend field number.  */
  rhino_tcb.extend_table_num = RHINO_EXTEND_FIELD_NUM;
  rhino_tcb.rtos_tcb_extend_table = init_rhino_tcb_extend_table ();
  /* For "info mthreads commands".  */
  rhino_tcb.i_mthreads_list = rhino_i_mthreads_list_table;
  rhino_tcb.i_mthreads_list_size
    = sizeof (rhino_i_mthreads_list_table)
        / sizeof (rhino_i_mthreads_list_table[0]);
  rhino_tcb.i_mthreads_stack = rhino_i_mthreads_stack_table;
  rhino_tcb.i_mthreads_stack_size
    = sizeof (rhino_i_mthreads_stack_table)
        / sizeof (rhino_i_mthreads_stack_table[0]);
  rhino_tcb.i_mthread_one = rhino_i_mthread_one_table;
  rhino_tcb.i_mthread_one_size
     = sizeof (rhino_i_mthread_one_table)
         /sizeof (rhino_i_mthread_one_table[0]);


  /* Rhino read/write register handler.  */
  rhino_tcb.rtos_reg_offset_table = csky_rhino_reg_offset_table;
  rhino_tcb.to_get_register_base_address = NULL;
  rhino_tcb.to_fetch_registers =  NULL;
  rhino_tcb.to_store_registers =  NULL;

  /* Rhino check thread_id valid.  */
  rhino_tcb.IS_VALID_TASK = csky_rhino_is_valid_task_id;

  /* Rhino check regno in task's stack.  */
  rhino_tcb.is_regnum_in_task_list = csky_rhino_is_regnum_in_task_list;
}

/* ************ 2. target_ops rhino_ops ************************  */
static struct target_ops rhino_ops;

/* ************ 3. rhino_open () **********************/
static void rhino_open (char * name, int from_tty);

/* Open for rhino system.  */

static void
rhino_open (const char * name, int from_tty)
{
  rtos_ops.current_ops
    = rtos_init_tables[RHINO_INIT_TABLE_INDEX].ops;
  rtos_ops.rtos_des
    = rtos_init_tables[RHINO_INIT_TABLE_INDEX].rtos_tcb_des;
  rtos_ops.event_des
    = rtos_init_tables[RHINO_INIT_TABLE_INDEX].rtos_event_des;
  common_open (name, from_tty);
}


/* ********** Start Implementation for rtos zephyr support ***************
   1. RTOS_TCB zephyr_tcb
   2. struct target_ops zephyr_ops
   3. zephyr_open().  */

/* ***********  1. RTOS_TCB zephyr_tcb ************************  */

/* --------------- csky_zephyr_reg_offset_table ---------------  */
static int csky_zephyr_reg_offset_table[] = {
  /* General register 0~15: 0 ~ 15  */
  /*
  "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
  */
  0x0*4, 0x1*4,  0x2*4,   0x3*4,  0x0*4,  0x1*4,  0x2*4,  0x3*4,
  0x4*4, 0x5*4,  0x6*4,   0x7*4,  0x4*4,  0x5*4,  0x8*4,  0x6*4,
  /* Dsp hilo register: 97, 98  */
  /*
  "",      "",    "",     "",     "hi",    "lo",   "",    "",
  */
  -1,      -1,    -1,     -1,       -1,      -1,   -1,    -1,

  /* FPU reigster: 24~55.  */
  /*
  "cp1.gr0",  "cp1.gr1",  "cp1.gr2",  "cp1.gr3",
  "cp1.gr4",  "cp1.gr5",  "cp1.gr6",  "cp1.gr7",
  "cp1.gr8",  "cp1.gr9",  "cp1.gr10", "cp1.gr11",
  "cp1.gr12", "cp1.gr13", "cp1.gr14", "cp1.gr15",
  "cp1.gr16", "cp1.gr17", "cp1.gr18", "cp1.gr19",
  "cp1.gr10", "cp1.gr21", "cp1.gr22", "cp1.gr23",
  "cp1.gr24", "cp1.gr25", "cp1.gr26", "cp1.gr27",
  "cp1.gr28", "cp1.gr29", "cp1.gr30", "cp1.gr31",
  */
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,
   -1, -1, -1, -1,

  /* Hole.  */
  /*
  "",      "",    "",     "",     "",    "",   "",    "",
  "",      "",    "",     "",     "",    "",   "",    "",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /******* Above all must according to compiler for debug info **********/

  //"all",   "gr",   "ar",   "cr",   "",    "",    "",    "",
  //"cp0",   "cp1",  "",     "",     "",    "",    "",    "",
  //"",      "",     "",     "",     "",    "",    "",    "cp15",

  /* PC : 72 : 64  */
  /*
  "pc",
  */
  0x9*4,

  /* Optional register(ar) : 73~88 :  16 ~ 31  */
  /*
  "ar0",  "ar1",  "ar2",  "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
  "ar8",  "ar9", "ar10", "ar11", "ar12", "ar13", "ar14", "ar15",
  */
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
  -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* Control registers (cr) : 89~120 : 32 ~ 63  */
  /*
  "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
  "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
  "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
  "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "",
  */
  0xa*4,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,
     -1,      -1,    -1,     -1,    -1,    -1,   -1,    -1,

  /* FPC control register: 0x100 & (32 ~ 38)  */
  /*
  "cp1.cr0","cp1.cr1","cp1.cr2","cp1.cr3","cp1.cr4","cp1.cr5","cp1.cr6",
  */
  -1, -1, -1, -1, -1, -1, -1,

  /* MMU control register: 0xf00 & (32 ~ 40)  */
  /*
  "cp15.cr0", "cp15.cr1", "cp15.cr2", "cp15.cr3",
  "cp15.cr4", "cp15.cr5", "cp15.cr6", "cp15.cr7",
  "cp15.cr8", "cp15.cr9", "cp15.cr10","cp15.cr11",
  "cp15.cr12","cp15.cr13","cp15.cr14","cp15.cr15",
  "cp15.cr16","cp15.cr29","cp15.cr30","cp15.cr31"
  */
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1,
  -1,      -1,    -1,     -1
};


/* ----------------- RTOS_TCB_DES table in RTOS_TCB ------------------  */

static int
zephyr_parse_current_pc (CORE_ADDR tcb_base,
                         struct rtos_field_des* itself,
                         RTOS_FIELD *val);
CORE_ADDR
zephyr_get_register_base_address (CORE_ADDR tcb_base, int regno);

/* Zephyr special fields definitions.  */

/* RTOS_FIELD_DES for thread_list field.  */
static const  char* zephyr_thread_list_offset_table[1] =
{
  "& _kernel.threads"
};
static int zephyr_thread_list_offset_cache[1] = {-1};
static RTOS_FIELD_DES zephyr_field_thread_list =
{
  "thread_list",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  zephyr_thread_list_offset_table,
  zephyr_thread_list_offset_cache,
  NULL,
  NULL  /* No output method for this field.  */
};

/* RTOS_FIELD_DES for current thread field.  */

static int
zephyr_parse_current_thread (CORE_ADDR tcb_base,
                             struct rtos_field_des* itself,
                             RTOS_FIELD *val)
{
   int i;
   char *str_tmp = "& ((struct k_thread *)0)->caller_saved";
   enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

   for (i = 0; i < 3; i++)
     {
       if (itself->offset_cache[i] == -1)// not parsed
         {
           if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + i)),
                                        (itself->offset_cache + i)) < 0)
             {
               return -1;
             }
        }
     }

   val->coreaddr = read_memory_unsigned_integer (itself->offset_cache[0],
                                                 4, byte_order);

    return 0;
}

static const char* zephyr_current_thread_offset_table[3] =
{
  "& _kernel.current",
  "& ((struct k_thread *)0)->caller_saved",
  "& ((struct k_thread *)0)->callee_saved"
};
static int zephyr_current_thread_offset_cache[3] = {-1, -1, -1};
static RTOS_FIELD_DES zephyr_field_current_thread =
{
  "current_thread",
  CoreAddr,
  4,
  NULL,
  0,
  3,
  zephyr_current_thread_offset_table,
  zephyr_current_thread_offset_cache,
  zephyr_parse_current_thread,
  NULL /* No output method for this field.  */
};

/* RTOS_FIELD_DES for next thread field.  */
static const char* zephyr_list_next_offset_table[1] =
{
  "& ((struct k_thread *)0)->next_thread"
};
static int zephyr_list_next_offset_cache[1] = {-1};
static RTOS_FIELD_DES zephyr_field_list_next =
{
  "list_next",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  zephyr_list_next_offset_table,
  zephyr_list_next_offset_cache,
  NULL,
  NULL /* No output method for this field.  */
};

/* Zephyr 5 basic fields definition.  */

/* RTOS_FIELD_DES for thread id field.  */

static int
zephyr_parse_thread_id (CORE_ADDR tcb_base,
                        struct rtos_field_des* itself,
                        RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}

static RTOS_FIELD_DES zephyr_field_id =
{
  "id",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  zephyr_parse_thread_id,
  NULL /* Use default output field method.  */
};

/* RTOS_FIELD_DES for stack_ptr field.  */
static int
zephyr_parse_stack_ptr (CORE_ADDR tcb_base,
                        struct rtos_field_des* itself,
                        RTOS_FIELD *val)
{
  unsigned int cur_tcb_addr;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  if (itself->offset_cache[0] == -1)
    {
      if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 0)),
                                   &(itself->offset_cache[0])) < 0)
         return -1;
    }

  cur_tcb_addr = read_memory_unsigned_integer (itself->offset_cache[0],
                                               4, byte_order);

  if (tcb_base == cur_tcb_addr)
    {
      struct regcache *regcache;
      ptid_t ptid;

      ptid = ptid_build (rtos_ops.target_ptid.pid, 0, tcb_base);
      regcache = get_thread_regcache (ptid);
      regcache_raw_read (regcache,
                         CSKY_SP_REGNUM,
                         (gdb_byte *)&(val->coreaddr));
      return 0;
    }
  else
    {
      unsigned int sp_addr;
      if (itself->offset_cache[1] == -1)
        {
          if (csky_rtos_symbol2offset (((char *)*(itself->offset_table + 1)),
                                       &(itself->offset_cache[1])) < 0)
            return -1;
        }
      sp_addr = tcb_base + itself->offset_cache[1];
      val->coreaddr = read_memory_unsigned_integer (sp_addr, 4, byte_order);
      return 0;
    }
}

static const char* zephyr_stack_ptr_offset_table[2] =
{
  "& _kernel.current",
  "& (*(struct k_thread *)0)->callee_saved->sp"
};
static int zephyr_stack_ptr_offset_cache[2] = {-1, -1};
static RTOS_FIELD_DES zephyr_field_stack_ptr =
{
  "stack_ptr",
  CoreAddr,
  4,
  NULL,
  0,
  2,
  zephyr_stack_ptr_offset_table,
  zephyr_stack_ptr_offset_cache,
  zephyr_parse_stack_ptr,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for entry_base field.  */

static int
zephyr_parse_entry_base (CORE_ADDR tcb_base,
                         struct rtos_field_des* itself,
                         RTOS_FIELD *val)
{
  char str_tmp[256];
  int entry_addr_p;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  sprintf (str_tmp, "& (*(struct k_thread *)%d)->entry->pEntry",
           tcb_base);
  if (csky_rtos_symbol2offset (str_tmp, &entry_addr_p) < 0)
    return -1;
  val->coreaddr = read_memory_unsigned_integer (entry_addr_p, 4, byte_order);
  return 0;
}
static RTOS_FIELD_DES zephyr_field_entry_base =
{
  "entry_base",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  zephyr_parse_entry_base,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for tcb_base field.  */

static int
zephyr_parse_tcb_base (CORE_ADDR tcb_base,
                       struct rtos_field_des* itself,
                       RTOS_FIELD *val)
{
  val->coreaddr = tcb_base;
  return 0;
}

static RTOS_FIELD_DES zephyr_field_tcb_base =
{
  "tcb_base",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  zephyr_parse_tcb_base,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for task_name field.  */

static int
zephyr_parse_task_name (CORE_ADDR tcb_base,
                        struct rtos_field_des* itself,
                        RTOS_FIELD *val)
{
  int ret;
  RTOS_FIELD val_tmp;
  unsigned int pc;
  unsigned int register_save_base;
  struct symbol *msymbol = NULL;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  val->string =  (char *) malloc (TASK_NAME_MAX);
  memset (val->string, 0, TASK_NAME_MAX);

  register_save_base
    = zephyr_get_register_base_address (tcb_base, CSKY_PC_REGNUM);

  pc = read_memory_unsigned_integer
         (register_save_base + csky_zephyr_reg_offset_table[CSKY_PC_REGNUM],
          4, byte_order);
  msymbol = find_pc_function (pc);
  if (msymbol)
    {
      strcpy (val->string, msymbol->ginfo.name);
    }
  else
    {
      strcpy (val->string, "null");
    }
  return 0;

}

static RTOS_FIELD_DES zephyr_field_task_name =
{
  "task_name",
  String,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  zephyr_parse_task_name,
  NULL /* Use default ouput method.  */
};


/* Zephyr_extend_field definition.  */

/* RTOS_FIELD_DES for priority field.  */

static int
zephyr_parse_task_priority (CORE_ADDR tcb_base,
                            struct rtos_field_des* itself,
                            RTOS_FIELD *val)
{
  int prio_addr_p;
  char str_tmp[256];
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  sprintf (str_tmp, "& (*(struct k_thread *)%d)->base->prio", tcb_base);
  if (csky_rtos_symbol2offset (str_tmp, &prio_addr_p) < 0)
    return -1;

  val->IntVal = read_memory_unsigned_integer (prio_addr_p, 1, byte_order);
  return 0;
}

static RTOS_FIELD_DES zephyr_field_priority =
{
  "priority",
  Integer,
  1,
  NULL,
  0,
  0,
  NULL,
  NULL,
  zephyr_parse_task_priority,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for state field.  */

const static char * zephyr_state_int2String[] =
{
  "_THREAD_DUMMY",     /* (1 << 0)  */
  "_THREAD_PENDING",   /* (1 << 1)  */
  "_THREAD_RESTART",   /* (1 << 2)  */
  "_THREAD_DEAD",      /* (1 << 3)  */
  "_THREAD_SUSPENDED", /* (1 << 4)  */
  "_THREAD_POLLING"    /* (1 << 5)  */
};

const static char *
zephyr_state_int2String_Transfer (int index)
{
  int str_num;
  int i;

  str_num = sizeof (zephyr_state_int2String)
              / sizeof (zephyr_state_int2String[0]);

  for (i = 0; i < str_num; i++)
    {
      if ((1 << i) == index)
        break;
    }

  if (i < str_num-1)
    {
      return zephyr_state_int2String[i];
    }
  else
    {
      return "UNKNOWN";
    }
}

static int
zephyr_parse_thread_state (CORE_ADDR tcb_base,
                           struct rtos_field_des* itself,
                           RTOS_FIELD *val)
{
  char str_tmp[256];
  int state_addr_p;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  sprintf (str_tmp,
           "& ((*(struct k_thread *)%d)->base.thread_state)",
           tcb_base);
  if (csky_rtos_symbol2offset (str_tmp, &state_addr_p) < 0)
    return -1;

  val->IntVal = read_memory_unsigned_integer (state_addr_p, 1, byte_order);
  return 0;
}

static RTOS_FIELD_DES zephyr_field_state =
{
  "state",
  IntToString,
  1,
  zephyr_state_int2String_Transfer,
  0,
  0,
  NULL,
  NULL,
  zephyr_parse_thread_state,
  NULL /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_size.  */

static const char* zephyr_stack_size_offset_table[1] =
{
  "& (*(struct k_thread *)0)->stack_info.size"
};
static int zephyr_stack_size_offset_cache[1] = {-1};
static RTOS_FIELD_DES zephyr_field_stack_size =
{
  "stack_size",
  Integer,
  4,
  NULL,
  1,
  1,
  zephyr_stack_size_offset_table,
  zephyr_stack_size_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for stack_base.  */
static const char* zephyr_stack_base_offset_table[1] =
{
  "& (*(struct k_thread *)0)->stack_info.start"
};
static int zephyr_stack_base_offset_cache[1] = {-1};
static RTOS_FIELD_DES zephyr_field_stack_base =
{
  "stack_base",
  CoreAddr,
  4,
  NULL,
  1,
  1,
  zephyr_stack_base_offset_table,
  zephyr_stack_base_offset_cache,
  NULL,
  NULL  /* Use default ouput method.  */
};

/* RTOS_FIELD_DES for current_pc.  */

static int
zephyr_parse_current_pc (CORE_ADDR tcb_base,
                         struct rtos_field_des* itself,
                         RTOS_FIELD *val)
{
  CORE_ADDR thread_id;
  struct regcache *regcache;
  ptid_t ptid;

  thread_id = tcb_base;

  ptid = ptid_build (rtos_ops.target_ptid.pid, 0, thread_id);
  regcache = get_thread_regcache (ptid);
  val->coreaddr = regcache_read_pc (regcache);

  return 0;
}

static RTOS_FIELD_DES zephyr_field_current_pc =
{
  "current_pc",
  CoreAddr,
  4,
  NULL,
  0,
  0,
  NULL,
  NULL,
  zephyr_parse_current_pc,
  NULL /* Use default ouput method.  */
};

/* RTOS_TCB definition for zephyr.  */
#define ZEPHYR_EXTEND_FIELD_NUM 5
static RTOS_FIELD_DES zephyr_tcb_extend_table[ZEPHYR_EXTEND_FIELD_NUM];

static RTOS_FIELD_DES*
init_zephyr_tcb_extend_table ()
{

  zephyr_tcb_extend_table[0] = zephyr_field_priority;
  zephyr_tcb_extend_table[1] = zephyr_field_state;
  zephyr_tcb_extend_table[2] = zephyr_field_stack_size;
  zephyr_tcb_extend_table[3] = zephyr_field_current_pc;
  zephyr_tcb_extend_table[4] = zephyr_field_stack_base;
  return zephyr_tcb_extend_table;
}

/* Fields needed for "info mthreads *" commands.  */

/* Fields needed for "info mthreads list".  */
static unsigned int zephyr_i_mthreads_list_table[] =
{
  1,                        /* Internal Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  5,                        /* Name  */
};

/* Fields needed for "info mthreads ID"  */
static unsigned int zephyr_i_mthread_one_table[] =
{
  1,                        /* Id  */
  RTOS_BASIC_FIELD_NUM + 1, /* Priority  */
  RTOS_BASIC_FIELD_NUM + 2, /* State  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack_size  */
  RTOS_BASIC_FIELD_NUM + 4, /* Current_pc  */
  5,                        /* Name  */
};

/* Fields needed for "info mthreads stack all"  */
static unsigned int zephyr_i_mthreads_stack_table[] =
{
  1,                        /* Id  */
  RTOS_BASIC_FIELD_NUM + 5, /* Stack_base  */
  2,                        /* Stack_ptr  */
  RTOS_BASIC_FIELD_NUM + 3, /* Stack_size  */
  5,                        /* Name  */
};

/* ---------- Funcitons defined in RTOS_TCB ------------  */

/* To check if this thread_id is valid.  */

static int
csky_zephyr_is_valid_task_id (RTOS_FIELD task_id)
{
  return 1;
}

/* To check if this reg in task's stack.  */

static int
csky_zephyr_is_regnum_in_task_list (RTOS_TCB* rtos_des,
                                    ptid_t ptid,
                                    int regno)
{
  if (rtos_des->rtos_reg_offset_table[regno] != -1)
    {
      return 1;
    }
  return 0;
}

/* To read zephyr register from thread caller and callee.  */
#define ZEPHYR_CALLER_SAVE_REGISTER_NUM 7
#define ZEPHYR_CALLEE_SAVE_REGISTER_NUM 11
static const int
zephyr_caller_saved_register_num[ZEPHYR_CALLER_SAVE_REGISTER_NUM] =
{0, 1, 2, 3, 12, 13, 15};

static const int
zephyr_callee_saved_register_num[ZEPHYR_CALLEE_SAVE_REGISTER_NUM] =
{4, 5, 6, 7, 8, 9, 10, 11, 14, 72, 89};

CORE_ADDR
zephyr_get_register_base_address (CORE_ADDR tcb_base, int regno)
{
  int i;
  for (i = 0; i < ZEPHYR_CALLER_SAVE_REGISTER_NUM; i++)
    {
      if (regno == zephyr_caller_saved_register_num[i])
        break;
    }
  if (i < ZEPHYR_CALLER_SAVE_REGISTER_NUM)
    {
      return tcb_base + zephyr_current_thread_offset_cache[1];
    }

  for (i = 0; i < ZEPHYR_CALLEE_SAVE_REGISTER_NUM; i++)
    {
      if (regno == zephyr_callee_saved_register_num[i])
        break;
    }
  if (i < ZEPHYR_CALLEE_SAVE_REGISTER_NUM)
    {
      return tcb_base + zephyr_current_thread_offset_cache[2];
    }
  return -1;
}


#define ZEPHYR_NAME_NUM 2
static const char * zephyr_names[ZEPHYR_NAME_NUM] =
{
  "zephyr",
  "ZEPHYR"
};

static RTOS_TCB zephyr_tcb;

static void
init_zephyr_tcb ()
{
  /* 3 special fields.  */
  zephyr_tcb.task_list_count = 1;
  {
    zephyr_tcb.rtos_special_table[0][0] = zephyr_field_thread_list;
    zephyr_tcb.rtos_special_table[0][1] = zephyr_field_current_thread;
    zephyr_tcb.rtos_special_table[0][2] = zephyr_field_list_next;
  }
  /* 5 basic fields.  */
  {
    zephyr_tcb.rtos_tcb_table[0] = zephyr_field_id;
    zephyr_tcb.rtos_tcb_table[1] = zephyr_field_stack_ptr;
    zephyr_tcb.rtos_tcb_table[2] = zephyr_field_entry_base;
    zephyr_tcb.rtos_tcb_table[3] = zephyr_field_tcb_base;
    zephyr_tcb.rtos_tcb_table[4] = zephyr_field_task_name;
  }
  /* Extend field number.  */
  zephyr_tcb.extend_table_num = ZEPHYR_EXTEND_FIELD_NUM;
  zephyr_tcb.rtos_tcb_extend_table = init_zephyr_tcb_extend_table();
  /* For "info mthreads commands".  */
  zephyr_tcb.i_mthreads_list = zephyr_i_mthreads_list_table;
  zephyr_tcb.i_mthreads_list_size
    = sizeof (zephyr_i_mthreads_list_table)
        / sizeof (zephyr_i_mthreads_list_table[0]);
  zephyr_tcb.i_mthreads_stack = zephyr_i_mthreads_stack_table;
  zephyr_tcb.i_mthreads_stack_size
    = sizeof (zephyr_i_mthreads_stack_table)
        /sizeof(zephyr_i_mthreads_stack_table[0]);
  zephyr_tcb.i_mthread_one = zephyr_i_mthread_one_table;
  zephyr_tcb.i_mthread_one_size
    = sizeof (zephyr_i_mthread_one_table)
        / sizeof (zephyr_i_mthread_one_table[0]);


  /* Zephyr read/write register handler.  */
  zephyr_tcb.rtos_reg_offset_table = csky_zephyr_reg_offset_table;
  zephyr_tcb.to_get_register_base_address = zephyr_get_register_base_address;
  zephyr_tcb.to_fetch_registers = NULL;
  zephyr_tcb.to_store_registers = NULL;

  /* Zephyr check thread_id valid.  */
  zephyr_tcb.IS_VALID_TASK = csky_zephyr_is_valid_task_id;

  /* Zephyr check regno in task's stack.  */
  zephyr_tcb.is_regnum_in_task_list = csky_zephyr_is_regnum_in_task_list;
}

/* ************ 2. target_ops zephyr_ops ************************  */
static struct target_ops zephyr_ops;

/* ************ 3. zephyr_open () *********************  */
static void zephyr_open (char * name, int from_tty);

/* Open for zephyr system.  */

static void
zephyr_open (const char * name, int from_tty)
{
  rtos_ops.current_ops
    = rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].ops;
  rtos_ops.rtos_des
    = rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].rtos_tcb_des;
  rtos_ops.event_des
    = rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].rtos_event_des;
  common_open (name, from_tty);
}





/* ************** common init table for all RTOS ********************  */

RTOS_INIT_TABLE rtos_init_tables[CSKY_RTOS_NUM];

void
initialize_rtos_init_tables ()
{
  /* Define ECOS_INIT_TABLE_INDEX     0  */
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].ops = &ecos_ops;
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].to_open = ecos_open;
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].names = ecos_names;
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].name_num = ECOS_NAME_NUM;
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].rtos_tcb_des = &ecos_tcb;
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].init_rtos_tcb = init_ecos_tcb;

  /* Ecos not support event check.  */
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].rtos_event_des = NULL;
  rtos_init_tables[ECOS_INIT_TABLE_INDEX].init_rtos_event = NULL;

  /* Define UCOSIII_INIT_TABLE_INDEX  1  */
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].ops = &ucosiii_ops;
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].to_open = ucosiii_open;
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].names = ucosiii_names;
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].name_num = UCOSIII_NAME_NUM;

  /* Rtos_tcb init.  */
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].rtos_tcb_des = &ucosiii_tcb;
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].init_rtos_tcb = init_ucosiii_tcb;

  /* Rtos_event init.  */
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].rtos_event_des = &ucosiii_event;
  rtos_init_tables[UCOSIII_INIT_TABLE_INDEX].init_rtos_event
    = init_ucosiii_event;

  /* Define NUTTX_INIT_TABLE_INDEX     2  */
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].ops = &nuttx_ops;
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].to_open = nuttx_open;
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].names = nuttx_names;
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].name_num = NUTTX_NAME_NUM;
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].rtos_tcb_des = &nuttx_tcb;
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].init_rtos_tcb = init_nuttx_tcb;

  /* Do not support event check.  */
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].rtos_event_des = NULL;
  rtos_init_tables[NUTTX_INIT_TABLE_INDEX].init_rtos_event = NULL;


  /* Define FREERTOS_INIT_TABLE_INDEX     3  */
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].ops = &freertos_ops;
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].to_open = freertos_open;
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].names = freertos_names;
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].name_num = FREERTOS_NAME_NUM;
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].rtos_tcb_des = &freertos_tcb;
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].init_rtos_tcb = init_freertos_tcb;
  /* Do not support event check.  */
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].rtos_event_des = NULL;
  rtos_init_tables[FREERTOS_INIT_TABLE_INDEX].init_rtos_event = NULL;

  /* Define RHINO_INIT_TABLE_INDEX    4  */
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].ops = &rhino_ops;
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].to_open = rhino_open;
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].names = rhino_names;
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].name_num = RHINO_NAME_NUM;
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].rtos_tcb_des = &rhino_tcb;
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].init_rtos_tcb = init_rhino_tcb;
  /* Do not support event check.  */
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].rtos_event_des = NULL;
  rtos_init_tables[RHINO_INIT_TABLE_INDEX].init_rtos_event = NULL;

  /* Define ZEPHYR_INIT_TABLE_INDEX    5  */
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].ops = &zephyr_ops;
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].to_open = zephyr_open;
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].names = zephyr_names;
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].name_num =ZEPHYR_NAME_NUM;
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].rtos_tcb_des = &zephyr_tcb;
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].init_rtos_tcb = init_zephyr_tcb;
  /* Do not support event check.  */
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].rtos_event_des = NULL;
  rtos_init_tables[ZEPHYR_INIT_TABLE_INDEX].init_rtos_event = NULL;
}

