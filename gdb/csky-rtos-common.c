/* csky-rtos.c
  This file extends struct target_ops csky_ops to support
  the multi-tasks debugging; This file only implement the
  framework of csky multi-tasks debugging. For the specified
  implement, please put it into csky-rtos.c.  */

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

extern ptid_t remote_csky_ptid;
RTOS_EVENT_COMMON *rtos_event_list = NULL;

/* The task info cache for the target, only record the result
   of every neccesory field.  */

static RTOS_TCB_COMMON *rtos_task_list = NULL;

/* Is rtos ops  */
static int current_ops_is_rtos = 0;

/* -------- Common function for rtos debugging ----------------------  */
void common_open (char * name, int from_tty);
static void rtos_attach (struct target_ops *ops, char * args, int from_tty);

/* ------------- Multi-threads commands implementation --------------  */
static void rtos_info_mthreads_command (char* args, int from_tty);
static void info_mthreads_implement (char* args, int from_tty);
static void threads_list_command (char* args, int from_tty);
static void thread_info_command (char* args, int from_tty);
static void threads_stack_command (char* args, int from_tty);
static void threads_event_command (char* args, int from_tty);
static void stack_depth_command (char* args, int from_tty);
static void stack_all_command (char* args, int from_tty);
static void event_all_command (char* args, int from_tty);
static void event_single_command (char* args, int from_tty);
static void single_stack_depth_handler (ptid_t ptid,
                                        int column_print,
                                        int print_all_depths,
                                        int from_tty);

/* --------------- Functions for fields parse and output -------------  */

/* To parse fields of ecos thread.  */

static int csky_rtos_parse_field (CORE_ADDR tcb_base,
                                  RTOS_FIELD_DES *f_des,
                                  RTOS_FIELD * field);

/* Default method to parse fields of ecos thread.  */

int csky_rtos_parse_field_default (CORE_ADDR tcb_base,
                                   RTOS_FIELD_DES *f_des,
                                   RTOS_FIELD * field);


/* To output field.  */

static void csky_rtos_output_field (RTOS_FIELD_DES* itself,
                                    RTOS_FIELD *val,
                                    int from_tty);
void  csky_rtos_output_field_default (RTOS_FIELD_DES* itself,
                                      RTOS_FIELD *val,
                                      int from_tty);

/* Special fields table : thread_list, current_thread, list_next.
   For some RTOS(nuttx), the there is more than one global
   task list in kernel; We prepare 10 to garantte to get all
   the task list in RTOS; If there is less then 10 in the RTOS,
   just leave it alone, our GDB will do nothing.  */

static RTOS_FIELD special_fields_table[10][3]=
{
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}},
  {{0},{0},{0}}
};
static RTOS_FIELD special_event_info_table[4][2]=
{
  {{0},{0}},
  {{0},{0}},
  {{0},{0}},
  {{0},{0}}
};

/* To represent last resume is step or not.  */
static int is_step_flag = 0;

/* ---------------- Functions defined in RTOS_OPS -------------  */
static enum RTOS_STATUS csky_rtos_update_task_info (struct target_ops*,
                                                    RTOS_TCB*,
                                                    ptid_t*);
static void csky_rtos_update_event_info (struct target_ops*, RTOS_EVENT*);
static void csky_rtos_open (struct target_ops*, RTOS_TCB*);
static void csky_rtos_close (struct target_ops*, RTOS_TCB*);
static int csky_rtos_is_regnum_in_task_list (RTOS_TCB*, ptid_t, int regno);
static void csky_rtos_fetch_registers (RTOS_TCB*,
                                       ptid_t,
                                       int regno,
                                       unsigned int* value);
static void csky_rtos_store_registers (RTOS_TCB*,
                                       ptid_t,
                                       int regno,
                                       ULONGEST value);
static void csky_rtos_prepare_resume (RTOS_TCB*, ptid_t, int step);
static int csky_rtos_is_task_in_task_list (RTOS_TCB*, ptid_t);
static void csky_rtos_pid_to_str (RTOS_TCB*, ptid_t ptid, char *buf);
static void csky_rtos_reset (RTOS_TCB*);

/* To parse fields of ecos thread.
   RTOS_FIELD_DES *f_des: tell how to parse this field
   RTOS_FIELD * field: store the value of this field.  */

static int
csky_rtos_parse_field (CORE_ADDR tcb_base,
                       RTOS_FIELD_DES *f_des,
                       RTOS_FIELD * field)
{
  int ret;
  if (f_des == NULL || field == NULL)
    {
      error ("Field parse error.");
    }
  if (f_des->use_default) /* Use default method.  */
    {
      ret = csky_rtos_parse_field_default (tcb_base, f_des, field);
    }
  else
    {
      if (f_des->to_read_field)
        {
          ret = f_des->to_read_field (tcb_base, f_des, field);
        }
      else
        {
          warning ("This field's read_field method miss,\
 Try default read_field method.");
          ret = csky_rtos_parse_field_default (tcb_base, f_des, field);
        }
    }
  return ret;
}

int
csky_rtos_symbol2offset (char *s, int *offset)
{
  struct expression *expr;
  struct value *value;
  struct cleanup *old_chain = 0;
  struct gdb_exception exception = exception_none;

  if (s == NULL || offset == NULL)
    {
      return -1;
    }

  TRY
    {
      parse_expression ((const char *)s);
    }
  CATCH (ex, RETURN_MASK_ALL)
    {
      exception = ex;
    }

  if (exception.reason != 0)
    {
      printf_filtered ("Fail to parse rtos symbol \"%s\", please check!\n",
                       s);
      return -1;
    }

  expr = parse_expression (s);
  old_chain = make_cleanup (free_current_contents, &expr);
  value = evaluate_expression (expr);
  do_cleanups (old_chain);
  *offset
    = extract_unsigned_integer (value_contents_raw (value),
                                4,
                                gdbarch_byte_order (get_current_arch ()));

  return 0;
}

/* Default method to parse fields of ecos thread.
   RTOS_FIELD_DES *f_des: tell how to parse this field
   RTOS_FIELD * field: store the value of this field.  */

int
csky_rtos_parse_field_default (CORE_ADDR tcb_base,
                               RTOS_FIELD_DES *f_des,
                               RTOS_FIELD * field)
{
  int offset_level;
  int i;
  CORE_ADDR string_base;
  CORE_ADDR final_base;
  CORE_ADDR tmp_addr;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  final_base = tcb_base;

  if (f_des == NULL
      || field == NULL
      || f_des->offset_table == NULL
      || f_des->offset_cache == NULL)
    {
      return -1;
    }
  offset_level = f_des->offset_level;
  /* Get offset of this field in tcb.  */
  for (i = 0; i < offset_level; i++)
    {
      /* May be thread_list or current_thread parse every time.  */
      if (tcb_base == 0)
        {
          if (csky_rtos_symbol2offset
                (((char *) *(f_des->offset_table + i)),
                 (f_des->offset_cache + i)) < 0)
            {
              return -1;
            }

        }
      else
        {
          if (f_des->offset_cache[i] != -1) /* Already parsed.  */
            {
              continue;
            }
          else
            {
              if (csky_rtos_symbol2offset
                    (((char *) *(f_des->offset_table + i)),
                     (f_des->offset_cache + i)) < 0)
                {
                  return -1;
                }
            }
        }
    }

  /* Get value by read memory from tcb_base + offset.  */

  /* Step 1: get the final_base of this field.  */
  for (i = 0; i < offset_level-1; i++)
    {
      final_base
        = read_memory_unsigned_integer (final_base + f_des->offset_cache[i],
                                        f_des->size,
                                        byte_order);
    }

  /* Step 2: get value from final_base + offset.  */
  switch (f_des->type)
    {
    case Integer:
    case IntToString:
      tmp_addr = final_base + f_des->offset_cache[offset_level - 1];
      field->IntVal = read_memory_unsigned_integer (tmp_addr,
                                                    f_des->size,
                                                    byte_order);
      break;
    case CoreAddr:
      tmp_addr = final_base + f_des->offset_cache[offset_level - 1];
      field->coreaddr = read_memory_unsigned_integer (tmp_addr,
                                                      f_des->size,
                                                      byte_order);
      break;
    case String:
      field->string = (char *) malloc (TASK_NAME_MAX);
      memset (field->string, 0, TASK_NAME_MAX);
      string_base = final_base + f_des->offset_cache[offset_level - 1];
      if (string_base)
        {
          i = 0;
          field->string[0] = read_memory_unsigned_integer (string_base + i,
                                                           1,
                                                           byte_order);
          while (field->string[i] != '\0' && i < TASK_NAME_MAX)
            {
              i++;
              field->string[i] = read_memory_unsigned_integer (string_base + i,
                                                               1,
                                                               byte_order);
            }

        }
      break;
    case Undefine:
      /* FIXME I DON'T KNOW WHAT TO DO.  */
      field->undef = (void *) read_memory_unsigned_integer
                       (final_base + f_des->offset_cache[offset_level - 1],
                        f_des->size,
                        byte_order);
      break;
    default: ; /* Nothing to be done.  */
  }
  return 0;
}

/* *************** Output field **************************  */

static void
csky_rtos_output_field (RTOS_FIELD_DES* itself, RTOS_FIELD *val, int from_tty)
{
  if (itself == NULL || val ==NULL)
    {
      error ("Parse field error.");
      return;
    }
  if (itself->to_output_field)
    {
      /* Use field's output method.  */
      itself->to_output_field (itself, val, from_tty);
    }
  else
    {
      /* Use default output method.  */
      csky_rtos_output_field_default (itself, val, from_tty);
    }
  return;
}

/* Default output field method.  */

void
csky_rtos_output_field_default (RTOS_FIELD_DES* itself,
                                RTOS_FIELD *val,
                                int from_tty)
{
  struct ui_out *uiout = current_uiout;

  switch (itself->type)
    {
    case Integer:
       ui_out_field_fmt (uiout, itself->name, "%2d", val->IntVal);
       break;
    case String:
       ui_out_field_string (uiout, itself->name, val->string);
       break;
    case IntToString:
       if (itself->int2StringTransfer)
         {
           ui_out_field_fmt (uiout, itself->name, "%2d:%s",
                             val->IntVal,
                             itself->int2StringTransfer (val->IntVal));
         }
       else
         {
           ui_out_field_fmt (uiout, itself->name, "%2d", val->IntVal);
         }
       break;
    case CoreAddr:
       ui_out_field_core_addr (uiout, itself->name,
                               get_current_arch (), val->coreaddr);
       break;
    case Undefine:
       ui_out_field_core_addr (uiout, itself->name,
                               get_current_arch (), (CORE_ADDR)val->undef);
       break;
    default:; /* Nothing to do.  */
    }
  ui_out_text (uiout, " ");
}

/* ****** functions and data structure needed In updta_task_info ******  */

/* Record last current thread ptid.  */
static ptid_t new_current_thread_ptid;

/* Thread info state description.  */
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

static enum RTOS_STATUS refresh_rtos_current_thread (RTOS_TCB *rtos_des);
static void thread_id_list_cache (void);
static void refresh_rtis_task_list (void);
static void rtos_init_task_list (int intensity);
static void refresh_thread_id_list (void);
static struct thread_id_record* find_thread_id_record (ptid_t ptid);
RTOS_TCB_COMMON* find_rtos_task_info (ptid_t);
static RTOS_EVENT_COMMON* find_rtos_event_info (CORE_ADDR event_id);
static void csky_task_info_xfer (struct thread_info* d_ops,
                                 RTOS_TCB_COMMON* s_ops);
static int is_field_null (RTOS_FIELD_DES f_des, RTOS_FIELD field);

static int
is_field_null (RTOS_FIELD_DES f_des, RTOS_FIELD field)
{
  int ret = 1;
  switch (f_des.type)
    {
    case Integer:
      if (field.IntVal == 0)
        ret = 1;
      else
        ret = 0;
      break;
    case String:
      if (field.string == NULL || *(field.string) == '\0')
        ret = 1;
      else
        ret = 0;
      break;
    case IntToString:
      if (field.IntVal == 0)
        ret = 1;
      else
        ret = 0;
      break;
    case CoreAddr:
      if (field.coreaddr == 0)
        ret = 1;
      else
        ret = 0;
      break;
    case Undefine:
      if (field.undef == 0)
        ret = 1;
      else
        ret = 0;
      break;
    default:; /* Nothing to do.  */
    }
  return ret;
}

static void
csky_task_info_xfer (struct thread_info* d_ops, RTOS_TCB_COMMON* s_ops)
{
  d_ops->control.step_frame_id.code_addr_p = 1;
  d_ops->control.step_frame_id.special_addr_p = 0;
  d_ops->control.step_frame_id.stack_addr
    = s_ops->rtos_basic_table[1].coreaddr;
  d_ops->control.step_frame_id.code_addr
    = s_ops->rtos_basic_table[2].coreaddr;
  d_ops->control.step_stack_frame_id
    = d_ops->control.step_frame_id;
}

static RTOS_EVENT_COMMON*
find_rtos_event_info (CORE_ADDR event_id)
{
  RTOS_EVENT_COMMON* tp;
  for (tp = rtos_event_list; tp != NULL; tp = tp->next)
    {
      if (tp->rtos_event_info_table[0].coreaddr == event_id)
        {
          return tp;
        }
    }
  return NULL;
}

/* Get the RTOS_TCB_COMMON struct by the ptid specified.  */

RTOS_TCB_COMMON*
find_rtos_task_info (ptid_t ptid)
{
  RTOS_TCB_COMMON* tp;
  for (tp = rtos_task_list; tp != NULL; tp = tp->next)
    {
      if (ptid.tid == tp->rtos_basic_table[0].coreaddr)
        return tp;
    }
  return NULL;
}

/* Find proper thread_id in thread_id_list.  */
static struct thread_id_record*
find_thread_id_record (ptid_t ptid)
{
  struct thread_id_record *t;
  for (t = thread_id_list; t != NULL; t = t->next)
    {
      if (ptid_equal (t->ptid, ptid))
        return t;
    }
  return NULL;
}

static enum RTOS_STATUS
refresh_rtos_current_thread (RTOS_TCB *rtos_des)
{
  int ret;
  unsigned int i;
  unsigned int task_list_count = rtos_des->task_list_count;
  enum RTOS_STATUS state = NOT_INIT_TARGET;

  for (i = 0; i < task_list_count; i++)
    {
      /* To parse thread_list.  */
      ret = csky_rtos_parse_field (0,
                                   &(rtos_des->rtos_special_table[i][0]),
                                   &(special_fields_table[i][0]));
      if (ret < 0)
        {
          return NO_RTOS_PROGRAME;
        }
      /* To parse current_thread.  */
      ret = csky_rtos_parse_field (0,
                                   &(rtos_des->rtos_special_table[i][1]),
                                   &(special_fields_table[i][1]));
      if (ret < 0)
        {
          return NO_RTOS_PROGRAME;
        }
      if (rtos_des->IS_VALID_TASK (special_fields_table[i][0])
          && rtos_des->IS_VALID_TASK (special_fields_table[i][1]))
        {
          state = VALID_TARGET;
        }
    }
  /* Current task is always in the first field table.  */
  new_current_thread_ptid = ptid_build (rtos_ops.target_ptid.pid,
                                        0,
                                        special_fields_table[0][1].coreaddr);
  return state;
}

static void
thread_id_list_cache (void)
{
  struct thread_id_record *tmp_thread_id_list = thread_id_list;
  struct thread_info *tmp_thread_list
    = any_thread_of_process (rtos_ops.target_ptid.pid);

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
  for (; tmp_thread_list != NULL; tmp_thread_list = tmp_thread_list->next)
    {
      tmp_thread_id_list = (struct thread_id_record *) malloc
                             (sizeof (struct thread_id_record));
      tmp_thread_id_list->ptid = tmp_thread_list->ptid;
      tmp_thread_id_list->thread_id_flag = 0;
      tmp_thread_id_list->next = thread_id_list;
      thread_id_list = tmp_thread_id_list;
    }
  return;
}

/* Refresh rtos_task_list.
   Initialize the exist one first,and then
   create a new one.  */

static void
refresh_rtos_task_list (void)
{
  CORE_ADDR rtos_thread_list_base;
  RTOS_FIELD task_id;
  int index, i;
  unsigned int task_list_count;
  int new_flag = 0;
  RTOS_TCB_COMMON *tmp = NULL, *tmp_prev = NULL;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Initial rtos_task_list.  */
  rtos_init_task_list (0);
  task_list_count = rtos_ops.rtos_des->task_list_count;
  for (index = 0; index < task_list_count; index ++)
    {
      rtos_thread_list_base = special_fields_table[index][0].coreaddr;
      do
        {
          if (rtos_thread_list_base == 0)
            {
	      /* Continue the next task list.  */
              continue;
            }
          if (csky_rtos_parse_field (rtos_thread_list_base,
                                     &(rtos_ops.rtos_des->rtos_tcb_table[0]),
                                     &task_id) < 0)
            {
              error ("Fail to parse rtos fields.");
            }

          /* Find RTOS_TCB_COMMON from rtos_task_list by ptid.  */
          tmp = find_rtos_task_info (ptid_build (rtos_ops.target_ptid.pid,
                                                 0,
                                                 task_id.coreaddr));
          new_flag = 0;
          if (tmp == NULL)
            {
              tmp = (RTOS_TCB_COMMON* ) malloc (sizeof (RTOS_TCB_COMMON));
              memset (tmp, 0, sizeof (RTOS_TCB_COMMON));
              new_flag = 1;
            }

          /* Task_id  */
          tmp->rtos_basic_table[0].coreaddr = task_id.coreaddr;

          /* Read other basic fields: stack_ptr, entry_base,
             tcb_base, task_name.  */
          for (i = 1; i < RTOS_BASIC_FIELD_NUM; i++)
            {
              /* Field is null ,need read from target.  */
              if (is_field_null (rtos_ops.rtos_des->rtos_tcb_table[i],
                                 tmp->rtos_basic_table[i]))
                {
                  if (csky_rtos_parse_field
                        (rtos_thread_list_base,
                         &(rtos_ops.rtos_des->rtos_tcb_table[i]),
                         &(tmp->rtos_basic_table[i])) < 0)
                    {
                      error ("Fail to parse rtos fields.");
                    }
                }
            }
          /* Set accessed flag.  */
          tmp->accessed = 1;
          if (new_flag)
            {
              /* Insert tmp into rtos_task_list.  */
              tmp->next = rtos_task_list;
              rtos_task_list = tmp;
            }
          /* Get list_next of thread_list.  */
          if (csky_rtos_parse_field
                (rtos_thread_list_base,
                 &(rtos_ops.rtos_des->rtos_special_table[index][2]),
                 &(special_fields_table[index][2])) < 0)
            {
              error ("Fail to parse rtos fields.");
            }
          rtos_thread_list_base = special_fields_table[index][2].coreaddr;
        }
      while (rtos_thread_list_base != special_fields_table[index][0].coreaddr
             && rtos_thread_list_base != 0);
    }
  return; 
}

/* Initial rtos_task_list.
   To improve debugging speed, using free() as little as possible
   intensity:
           0, using free() as little as possible
           1, initialize rtos_task_list totally by free()  */

static void
rtos_init_task_list (int intensity)
{
  int i;
  RTOS_TCB_COMMON *t = rtos_task_list;

  if (intensity)
    {
      while (t != NULL)
        {
          RTOS_TCB_COMMON *p = t;
          t = p->next;
          if (p->rtos_basic_table[4].string)
            {
              free(p->rtos_basic_table[4].string);
            }
          if (p->extend_fields)
            {
              for (i = 0; i < rtos_ops.rtos_des->extend_table_num; i++)
                {
                  if (rtos_ops.rtos_des->rtos_tcb_extend_table[i].type
                      == String)
                    {
                      free (p->extend_fields[i].string);
                    }
                }
              free (p->extend_fields);
            }
          free (p);
        }
      rtos_task_list = NULL;
    }
  else
    {
      while(t != NULL)
        {
          RTOS_TCB_COMMON *p = t;
          t = p->next;
          p->rtos_basic_table[0].coreaddr = 0;
          p->rtos_basic_table[1].coreaddr = 0;
          p->rtos_basic_table[2].coreaddr = 0;
          p->rtos_basic_table[3].coreaddr = 0;
          p->accessed = 0;
         if (p->rtos_basic_table[4].string)
           memset(p->rtos_basic_table[4].string, 0, TASK_NAME_MAX);
         if (p->extend_fields)
           {
             for (i = 0; i < rtos_ops.rtos_des->extend_table_num; i++)
               {
                 if (rtos_ops.rtos_des->rtos_tcb_extend_table[i].type
                     == String)
                   {
                     free (p->extend_fields[i].string);
                   }
               }
             memset (p->extend_fields,
                     0,
                     sizeof (RTOS_FIELD)*rtos_ops.rtos_des->extend_table_num);
           }
        }
    }
}

/* Update thread_id_list according to rtos_task_list.  */

static void
refresh_thread_id_list (void)
{
  RTOS_TCB_COMMON *p;
  struct thread_id_record *t;
  for (p = rtos_task_list; p != NULL; p = p->next)
    {
      /* Accessed == 1: thread is in csky-target-board now.  */
      if (p->accessed == 1)
        {
          t = find_thread_id_record
                (ptid_build (rtos_ops.target_ptid.pid,
                             0,
                             p->rtos_basic_table[0].coreaddr));
          if (t)
            {
              /* This one exists both pre and current rtos_task_list.  */
              t->thread_id_flag = 1;
            }
          else
            {
              /* This thread_id is a new one.  */
              t =(struct thread_id_record *) malloc
                   (sizeof (struct thread_id_record));
              t->ptid = ptid_build (rtos_ops.target_ptid.pid,
                                    0,
                                    p->rtos_basic_table[0].coreaddr);
              t->thread_id_flag = 2;
              t->next = thread_id_list;
              thread_id_list = t;
            }
        }
    }

  return;
}

/* ***The functions defined in RTOS_OPS, implementation of ecos *******  */

/* Update rtos_event_list from target.  */

static void
rtos_init_event_list (int intensity)
{
  RTOS_EVENT_COMMON *t = rtos_event_list;

  if (intensity)
    {
      while (t != NULL)
        {
          RTOS_EVENT_COMMON *p = t;
          t = p->next;
          if (p->rtos_event_info_table[2].string) /* Event name.  */
            {
              free (p->rtos_event_info_table[2].string);
            }
          free (p);
        }
      rtos_event_list = NULL;
    }
  else
    {
      while(t != NULL)
        {
          RTOS_EVENT_COMMON *p = t;
          t = p->next;
          p->rtos_event_info_table[0].coreaddr = 0;
          p->rtos_event_info_table[1].IntVal = 0;
          p->rtos_event_info_table[3].undef = 0;
          p->accessed = 0;
          if (p->rtos_event_info_table[2].string)
            memset (p->rtos_event_info_table[2].string, 0, TASK_NAME_MAX);
        }
    }
  return;
}

static void
csky_rtos_update_event_info (struct target_ops* t_ops,
                             RTOS_EVENT* rtos_event_des)
{
  int i;
  int j;
  RTOS_FIELD event_id;
  CORE_ADDR event_base;
  RTOS_EVENT_COMMON *tmp;
  int new_flag = 0;

  if (rtos_event_des == NULL)
    {
      return;
    }
  rtos_init_event_list (0);

  /* Get event_base.  */
  for (i = 0;i < 4; i++)
    {
      if (csky_rtos_parse_field (0,
            &(rtos_event_des->rtos_event_special_table[i][0]),
            &(special_event_info_table[i][0])) < 0)
        {
          warning ("Fail to parse rtos fields.");
          return;
        }
    }

  /* Get event infos: id type name pendlist.  */
  for (i = 0; i < 4; i++)
    {
      event_base = special_event_info_table[i][0].coreaddr;
      if (event_base == 0)
        {
          /* Do nothing.  */
          continue;
        }
      do
        {
          if (csky_rtos_parse_field (event_base,
                &(rtos_event_des->rtos_event_info_table[i][0]),
                &event_id) < 0)
            {
              error ("Fail to parse rtos fields.");
            }
          new_flag = 0;
          tmp = find_rtos_event_info (event_id.coreaddr);
          if (tmp == NULL)
            {
              int tmp_size = sizeof (RTOS_EVENT_COMMON);
              tmp = (RTOS_EVENT_COMMON*) malloc (tmp_size);
              memset(tmp, 0, tmp_size);
              new_flag = 1;
            }
          tmp->rtos_event_info_table[0].coreaddr = event_id.coreaddr;

          /* Get type, name, pendlist.  */
          for (j = 1; j < 4; j++)
            {
              /* Field is null, need read from target.  */
              if (is_field_null (rtos_event_des->rtos_event_info_table[i][j],
                                 tmp->rtos_event_info_table[j]))
                {
                  if (csky_rtos_parse_field (event_base,
                        &(rtos_event_des->rtos_event_info_table[i][j]),
                        &(tmp->rtos_event_info_table[j])) < 0)
                    {
                      error ("Fail to parse rtos fields.");
                    }
                }
            }
          /* Set accessed flag.  */
          tmp->accessed = 1;
          /* Insert tmp into rtos_task_list.  */
          if (new_flag)
            {
              tmp->next = rtos_event_list;
              rtos_event_list = tmp;
            }

          /* Get event next;  */
          if (csky_rtos_parse_field (event_base,
                &(rtos_event_des->rtos_event_special_table[i][1]),
                &(special_event_info_table[i][1]))<0)
            {
              error ("Fail to parse rtos fields.");
            }
          event_base = special_event_info_table[i][1].coreaddr;
        }
      while (event_base!= special_event_info_table[i][0].coreaddr
             && event_base!=0);
    }
  return;  
}

/* Update rtos_task_list from target, as well as thread_list of gdb.  */

static enum RTOS_STATUS
csky_rtos_update_task_info (struct target_ops* t_ops,
                            RTOS_TCB* rt_tcb,
                            ptid_t* inferior_ptid)
{
  RTOS_TCB_COMMON *t;
  struct thread_info *tp;
  struct thread_id_record *tmp;
  enum RTOS_STATUS ret;

  ret = refresh_rtos_current_thread (rt_tcb);
  if (ret != VALID_TARGET)
    {
      return ret;
    }
  if (is_step_flag)
    {
      /* No need to update thread info.  */
      if (ptid_equal (rtos_ops.target_ptid, new_current_thread_ptid))
        {
          return ret;
        }
    }
  if (new_current_thread_ptid.tid == 0)
    {
      /* May be not initialized, do nothing.  */
      return ret;
    }
  rtos_ops.target_ptid = new_current_thread_ptid;

  /* Cache the thread_list of GDB.  */
  thread_id_list_cache ();

  /* Update rtos_task_list from target.  */
  refresh_rtos_task_list ();

  /* Update thread_id_list.  */
  refresh_thread_id_list ();

  /* Update thread_list in GDB.  */
  for (tmp = thread_id_list; tmp != NULL; tmp = tmp->next)
    {
      if (ptid_equal (tmp->ptid, rtos_ops.target_ptid)
          || ptid_equal (tmp->ptid, remote_csky_ptid)
          || ptid_equal (tmp->ptid, minus_one_ptid))
        continue; /* Deal with inferior_ptid at last.  */
      switch (tmp->thread_id_flag)
        {
        case 0: /* Exist in pre but not current.  */
          delete_thread_silent (tmp->ptid);
          break;
        case 1: /* Exist in both pre and current.  */
          break;
        case 2: /* A new one.  */
          tp = add_thread_silent (tmp->ptid);
          t = find_rtos_task_info (tmp->ptid);
          csky_task_info_xfer (tp, t);
          break;
        default: ; /* There should not be thread_id which
                      thread_id_flag is any other.  */
        }
    }
  /* Deal with current ptid.  */
  delete_thread_silent (rtos_ops.target_ptid);
  tp = add_thread_silent (rtos_ops.target_ptid);

  *inferior_ptid = rtos_ops.target_ptid;
  registers_changed_ptid (*inferior_ptid);
  return ret;
}

static void
csky_rtos_open (struct target_ops* t_ops, RTOS_TCB* rtos_des)
{
  rtos_init_task_list (1);
  rtos_init_event_list (1);
  rtos_ops.target_ptid = remote_csky_ptid;
}

static void
csky_rtos_close (struct target_ops* t_ops, RTOS_TCB* rtos_des)
{
  rtos_init_event_list (1);
  rtos_init_task_list (1);
}

/* To check if regnum in task ptid's stack.  */

static int
csky_rtos_is_regnum_in_task_list (RTOS_TCB* rtos_des, ptid_t ptid, int regno)
{
  RTOS_TCB_COMMON* tp;

  tp = find_rtos_task_info (ptid);
  if (!tp)
    {
      return 0;
    }
  else
    {
      /* Current_ptid, in hardware register.  */
      if (ptid_equal(ptid, rtos_ops.target_ptid))
        {
          return 0;
        }
      else
        {
          return rtos_des->is_regnum_in_task_list (rtos_des, ptid, regno);
        }
    }
}

/* To read register from task ptid's stack.  */

static void
csky_rtos_fetch_registers (RTOS_TCB* rtos_des,
                           ptid_t ptid,
                           int regno,
                           unsigned int* value)
{
  RTOS_TCB_COMMON* tp;

  enum bfd_endian byte_order = selected_byte_order ();

  tp = find_rtos_task_info (ptid);
  if (!tp)
    {
      error ("No task{%d %ld %ld} in target.", ptid.pid, ptid.lwp, ptid.tid);
    }
  if (rtos_des->to_fetch_registers)
    {
      rtos_des->to_fetch_registers (tp->rtos_basic_table[1].coreaddr,
                                    regno,
                                    value);
      return;
    }
  else if (rtos_des->to_get_register_base_address)
    {
      CORE_ADDR base
        = rtos_des->to_get_register_base_address
            (tp->rtos_basic_table[3].coreaddr, regno);
      *value
        = read_memory_unsigned_integer
            (base + rtos_des->rtos_reg_offset_table[regno],
             4,
             byte_order);
    }
  else /* Common register fetch methods.  */
    {
      *value
        = read_memory_unsigned_integer (tp->rtos_basic_table[1].coreaddr
                                + rtos_des->rtos_reg_offset_table[regno],
                                        4,
                                        byte_order);
    }
  return;
}

static void
csky_rtos_store_registers (RTOS_TCB* rtos_des,
                           ptid_t ptid,
                           int regno,
                           ULONGEST value)
{
  RTOS_TCB_COMMON* tp;
  enum bfd_endian byte_order = selected_byte_order ();

  tp = find_rtos_task_info (ptid);
  if (!tp)
    {
       error ("No task{%d %ld %ld} in target.", ptid.pid, ptid.lwp, ptid.tid);
    }
  if (rtos_des->to_store_registers)
    {
      rtos_des->to_store_registers (tp->rtos_basic_table[1].coreaddr,
                                    regno,
                                    value);
      return;
    }
  else if (rtos_des->to_get_register_base_address)
    {
      CORE_ADDR base;
      CORE_ADDR addr;
      base = rtos_des->to_get_register_base_address
               (tp->rtos_basic_table[3].coreaddr, regno);
      addr = base + rtos_des->rtos_reg_offset_table[regno];
      write_memory_unsigned_integer (addr, 4, byte_order, value);
    }
  else
    {
      CORE_ADDR addr;

      addr = tp->rtos_basic_table[1].coreaddr
             + rtos_des->rtos_reg_offset_table[regno];
      write_memory_unsigned_integer (addr, 4, byte_order, value);
    }
  return;
}

/* Some prepare work before csky_resume.  */

static void
csky_rtos_prepare_resume (RTOS_TCB* rtos_des, ptid_t ptid, int step)
{
  is_step_flag = step;
  return;
}

/* To check task ptid is in task_list or not.  */

static int
csky_rtos_is_task_in_task_list (RTOS_TCB* rtos_des, ptid_t ptid)
{
  RTOS_TCB_COMMON * tp;
  if (ptid_equal (ptid, rtos_ops.target_ptid))
    {
      return 1;
    }
  tp = find_rtos_task_info (ptid);
  if (tp)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/* Transfer pid to str.  */

static void
csky_rtos_pid_to_str (RTOS_TCB* rtos_des, ptid_t ptid, char *buf)
{
  RTOS_TCB_COMMON *tp;

  tp = find_rtos_task_info (ptid);
  if (tp)
    {
      snprintf (buf, 64 * sizeof (char*), "Thread 0x%x <%s>",
                (unsigned int)ptid.tid, tp->rtos_basic_table[4].string);
    }
  return;
}

static void
csky_rtos_reset (RTOS_TCB* rtos_des)
{
  rtos_init_event_list (1);
  rtos_init_task_list (1);
}

/* ********** Info mthreads commands In csky rtos debugging ******** */

/* Implement info mthreads commands which multi-tasks support.
   Now support following commands:
   1. info mthreads list
   2. info mthreads ID   // (exp: ID = 0x8100402c)
   3. info mthreads stack all
   4. info mthreads stack [ID] // If no [ID], list all threads' history stack.
   5. info mthreads event all
   6. info mthreads event [ID]
   ARGS: each command's parameter.
   FROM_TTY:  */

static void
info_mthreads_implement (char* args, int from_tty)
{
  const char LIST[] = "list";
  const char *ID[2] ={"0x", "0X"};
  const char STACK[] = "stack ";
  const char EVENT[] = "event";

  /* This command need args.  */
  if (args == NULL)
    {
      error ("Command:\"info mthreads\" need arguments.\nTry\"help "
              "info mthreads\" for more information.");
    }

  /* Analysis parameters and determine handler function.  */
  if (strncmp (args, LIST, (sizeof(LIST) - 1)) == 0
      && (args[strlen(LIST)] == ' ' || args[strlen(LIST)] == '\0'))
    {
      threads_list_command (&(args[strlen(LIST)]), from_tty);
      return;
    }
  else if (strncmp (args, ID[0], 2) == 0
           || strncmp (args, ID[1], 2) == 0)
    {
      thread_info_command (args, from_tty);
      return;
    }
  else if (strncmp (args, STACK, (sizeof(STACK) - 1)) == 0)
    {
      threads_stack_command (&(args[strlen(STACK)]), from_tty);
      return;
    }
  else if (strncmp (args, EVENT, (sizeof(EVENT) - 1)) == 0)
    {
      threads_event_command (&(args[strlen(EVENT)]), from_tty);
      return;
    }
  /* Invalid parameter.  */
  printf_filtered ("Illegal args,try \"help info mthreads\"for"
                   " more information.\n");
  return;
}

/* CSKY multi-threads commands.
   If no current_kernel_ops, this commands will do nothing
   except warning(). This is only an interface function which
   is implemented by current_kernel_ops.
   ARGS: parameter of commands
   FROM_TTY: 1: command from CLI
             0: command from MI   */

static void
rtos_info_mthreads_command (char* args, int from_tty)
{
  struct ui_out *uiout = current_uiout;

  if (rtos_ops.rtos_des && rtos_task_list != NULL)
    {
      info_mthreads_implement (args, from_tty);
      return;
    }
  else if (from_tty)
    {
      printf_filtered ("\"info mthreads\" is a multi-threads' command "
                       "which does not support in single thread debugging.\n"
                       "Try \"help info mthreads\" for more information.\n");
      return;
    }
  else /* Info thread command from MI command.  */
    {
      struct cleanup *cleanup_error;
      cleanup_error
        = make_cleanup_ui_out_tuple_begin_end (uiout, "mthreadError");
      if (rtos_task_list)
        {
          ui_out_field_string (uiout,
                               "error",
                               "Current Debug method does not support"
                               " multi-threads debugging.");
        }
      else
        {
          ui_out_field_string (uiout,
                               "error",
                               "No task info in current state.");
        }

      do_cleanups (cleanup_error);
      return;
    }
}

/* Utils to output the table column for the give table
   @param table the table which contains the fileds
   @param size the table size.  */

static void
rtos_output_list_table_column (unsigned int * table, int size)
{
  int i, index;
  RTOS_FIELD_DES tmp_des;
  struct ui_out *uiout = current_uiout;

  /* Output the table column.  */
  ui_out_text (uiout, "#");
  for (i = 0; i < size; i++)
    {
      index = table[i];
      if (index <= RTOS_BASIC_FIELD_NUM) /* Basic fields definition.  */
        {
          tmp_des = rtos_ops.rtos_des->rtos_tcb_table[index - 1];
        }
      else
        {
          int tmp_index = index - RTOS_BASIC_FIELD_NUM - 1;
          tmp_des = rtos_ops.rtos_des->rtos_tcb_extend_table[tmp_index];
        }
      ui_out_text (uiout, tmp_des.name);
      ui_out_text (uiout, " ");
    }
  ui_out_text (uiout, "\n");
}

/* To parse fields of ecos thread.
   RTOS_FIELD_DES *f_des: tell how to parse this field
   RTOS_FIELD * field: store the value of this field.  */

static void
threads_list_command (char *args, int from_tty)
{
  int i,index;
  RTOS_FIELD *tmp_field;
  RTOS_FIELD_DES tmp_des;
  RTOS_TCB_COMMON *tp;
  struct cleanup *cleanup_threads_list;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());
  struct ui_out *uiout = current_uiout;

  /* This commands need no args.  */
  if (*args != '\0')
    {
      printf_filtered ("info mthreads list command omit all args.\n");
    }

  /* Organize threads list according to rtos_task_list.  */
  cleanup_threads_list
    = make_cleanup_ui_out_tuple_begin_end (uiout, "threadslist");
  /* Output the table column.  */
  rtos_output_list_table_column (rtos_ops.rtos_des->i_mthreads_list,
                                 rtos_ops.rtos_des->i_mthreads_list_size);

  for (tp = rtos_task_list; tp!=NULL; tp = tp->next)
    {
      if (tp->accessed == 1)
        {
          struct cleanup *cleanup_frame
            = make_cleanup_ui_out_tuple_begin_end (uiout, "frame");
          ui_out_text (uiout, "#");
          if (!tp->extend_fields)
            {
              tp->extend_fields = (RTOS_FIELD *) malloc
                (sizeof (RTOS_FIELD) * rtos_ops.rtos_des->extend_table_num);
              memset (tp->extend_fields,
                0,
                sizeof (RTOS_FIELD)*rtos_ops.rtos_des->extend_table_num);
            }

          /* Output fields value of "info mthreads list" command.  */
          for (i = 0; i < rtos_ops.rtos_des->i_mthreads_list_size; i++)
            {
              index = rtos_ops.rtos_des->i_mthreads_list[i];
              /* Basic fields definition.  */
              if (index <= RTOS_BASIC_FIELD_NUM)
                {
                  tmp_field = &(tp->rtos_basic_table[index-1]);
                  tmp_des = rtos_ops.rtos_des->rtos_tcb_table[index-1];
                }
              else
                {
                  int tmp_index = index - RTOS_BASIC_FIELD_NUM - 1;
                  tmp_field = &(tp->extend_fields[tmp_index]);
                  tmp_des
                    = rtos_ops.rtos_des->rtos_tcb_extend_table[tmp_index];
                }
              /* This field not read.  */
              if (is_field_null (tmp_des, *tmp_field))
                {
                  csky_rtos_parse_field (tp->rtos_basic_table[3].coreaddr,
                                         &tmp_des, tmp_field);
                }
              csky_rtos_output_field (&tmp_des, tmp_field, from_tty);
            }
          ui_out_text (uiout, "\n");
          do_cleanups (cleanup_frame);
        }
    }
  do_cleanups (cleanup_threads_list);
}

/* List a single thread detailed info.
   ARGS: the chosen thread.  */

static void
thread_info_command (char* args, int from_tty)
{
  int i,index;
  RTOS_FIELD *tmp_field;
  RTOS_FIELD_DES tmp_des;
  RTOS_TCB_COMMON *tp;
  struct cleanup *cleanup_thread_info;
  CORE_ADDR thread_id;
  struct ui_out *uiout = current_uiout;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* Analysis args, get ID and assign to thread_id.  */
  if ((args[0] == '0') && ((args[1] == 'x') || (args[1] == 'X')))
    thread_id = strtoll (args, NULL, 16);
  else
    thread_id = strtoll (args, NULL, 10);
  tp = find_rtos_task_info (ptid_build (rtos_ops.target_ptid.pid,
                                        0,
                                        thread_id));
  if (tp == NULL || tp->accessed == 0 || thread_id == 0)
    {
      printf_filtered ("Thread 0x%x is not alive.\n", thread_id);
      return;
    }

  if (!tp->extend_fields)
    {
      tp->extend_fields = (RTOS_FIELD *) malloc
        (sizeof (RTOS_FIELD) * rtos_ops.rtos_des->extend_table_num);
      memset (tp->extend_fields,
        0,
        sizeof (RTOS_FIELD) * rtos_ops.rtos_des->extend_table_num);
    }

  cleanup_thread_info
    = make_cleanup_ui_out_tuple_begin_end (uiout, "mthreadinfo");

  rtos_output_list_table_column (rtos_ops.rtos_des->i_mthread_one,
                                 rtos_ops.rtos_des->i_mthread_one_size);
  ui_out_text (uiout, "#");
  /* Output fields value of "info mthreads ID" command.  */
  for (i = 0; i < rtos_ops.rtos_des->i_mthread_one_size; i++)
    {
      index = rtos_ops.rtos_des->i_mthread_one[i];
      /* Basic fields definition.  */
      if (index <= RTOS_BASIC_FIELD_NUM)
        {
          tmp_field = &(tp->rtos_basic_table[index-1]);
          tmp_des = rtos_ops.rtos_des->rtos_tcb_table[index-1];
        }
      else
        {
          int tmp_index = index - RTOS_BASIC_FIELD_NUM -1;
          tmp_field = &(tp->extend_fields[tmp_index]);
          tmp_des = rtos_ops.rtos_des->rtos_tcb_extend_table[tmp_index];
        }

      /* Read field from target , if necessary.  */
      if (is_field_null (tmp_des, *tmp_field))
        {
          csky_rtos_parse_field (tp->rtos_basic_table[3].coreaddr,
                                 &tmp_des,
                                 tmp_field);
        }
      /* Output this field.  */
      csky_rtos_output_field (&tmp_des, tmp_field, from_tty);
    }
  ui_out_text (uiout, "\n");
  do_cleanups (cleanup_thread_info);
}

/* List all event info according args.
   ARGS: event all: all event  info.
         event single [ID]: if [ID], list one event.  */

static void
threads_event_command (char* args, int from_tty)
{
  char* arg1 = args;
  const char ALL[] = "all";
  const char *SINGLE[2] ={"0x", "0X"};
  struct ui_out *uiout = current_uiout;

  /* Analysis parameters and determine handler function.  */
  if (rtos_ops.event_des == NULL)
    {
      struct cleanup *cleanup_error;
      cleanup_error = make_cleanup_ui_out_tuple_begin_end (uiout,
                                                           "mthreadError");
      ui_out_field_string (uiout,
        "error",
        "Current debug method does not support event info checking.");
      do_cleanups (cleanup_error);
      return;
    }
  while (arg1 != NULL)
    {
      if (*arg1 != ' ')
        break;
      arg1++;
    }
  if (strncmp (arg1, ALL, (sizeof(ALL) - 1)) == 0
      && (arg1[strlen(ALL)] == ' ' || arg1[strlen(ALL)] == '\0'))
    {
      event_all_command (&(arg1[strlen(ALL)]), from_tty);
      return;
    }
  else if (strncmp (arg1, SINGLE[0], 2) == 0
           || strncmp (arg1, SINGLE[1], 2) == 0)
    {
      event_single_command (arg1, from_tty);
      return;
    }

  /* Invalid parameter.  */
  printf_filtered ("Illegal args,try \"help info mthreads\"for"
                   " more information.\n");

  return;
}

static void
event_all_command (char* args, int from_tty)
{
  CORE_ADDR offset;
  RTOS_EVENT_COMMON *tp;
  struct cleanup *cleanup_event_list;
  struct ui_out *uiout = current_uiout;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* This commands need no args.  */
  if (*args != '\0')
    {
      printf_filtered ("info mthreads event all command omit all args.\n");
    }
  cleanup_event_list
    = make_cleanup_ui_out_tuple_begin_end (uiout, "eventlist");

  if (rtos_event_list)
    {
      ui_out_text (uiout, "event_base  event_type");
      ui_out_text (uiout, "\n");

      for (tp = rtos_event_list; tp!=NULL; tp = tp->next)
        {
          if (tp->accessed == 1)
            {
              int i;
              RTOS_FIELD_DES tmp_des;
              struct cleanup *cleanup_frame
                = make_cleanup_ui_out_tuple_begin_end (uiout, "frame");
              ui_out_text (uiout, "#");
              for (i = 0; i < 3; i++)
                {
                  tmp_des = rtos_ops.event_des->rtos_event_info_table[0][i];
                  csky_rtos_output_field (&tmp_des,
                                          &(tp->rtos_event_info_table[i]),
                                          from_tty);
                }
              ui_out_text (uiout, "\n");
              do_cleanups (cleanup_frame);
            }
        }
    }
  else
    {
      ui_out_text (uiout, "null\n");
    }
  do_cleanups (cleanup_event_list);
  return;
}

/* List all threads stack info according args.
   ARGS: stack all: all threads stack info.
         stack depth [ID]: if [ID], list one thread's stack depth by ID
                     else list all threads' stack depth.  */

static void
threads_stack_command (char* args, int from_tty)
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
      stack_all_command (&(arg1[strlen(ALL)]), from_tty);
      return;
    }
  else if (strncmp (arg1, DEPTH, (sizeof(DEPTH) - 1)) == 0)
    {
      stack_depth_command (&(arg1[strlen(DEPTH)]), from_tty);
      return;
    }

  printf_filtered ("Command \"info mthreads stack\"has no such "
    "format,try \"help info mthreads for more information.\"\n");

  return;
}

static void
event_single_command (char* args, int from_tty)
{
  int i;
  RTOS_FIELD_DES tmp_des;
  CORE_ADDR event_id;
  struct cleanup *cleanup_event_info;
  char* arg1 = args;
  RTOS_EVENT_COMMON *tp = NULL;
  struct ui_out *uiout = current_uiout;

  if ((args[0] == '0') && ((args[1] == 'x') || args[1] == 'X'))
    event_id = strtoll (arg1, NULL, 16);
  else
    event_id = strtoll (arg1, NULL, 10);

  tp = find_rtos_event_info (event_id);

  if (tp == NULL || tp->accessed == 0 || event_id == 0)
    {
      printf_filtered ("Event 0x%x does not exist.\n", event_id);
      return;
    }
  cleanup_event_info = make_cleanup_ui_out_tuple_begin_end (uiout,
                                                            "eventinfo");
  ui_out_text (uiout, "event_base  event_type");
  ui_out_text (uiout, "\n#");
  for (i = 0; i < 3; i++)
    {
      tmp_des = rtos_ops.event_des->rtos_event_info_table[0][i];
      csky_rtos_output_field (&tmp_des,
                              &(tp->rtos_event_info_table[i]),
                              from_tty);
    }
  ui_out_text (uiout, "\n");
  /* Output pendlist.  */
  tmp_des = rtos_ops.event_des->rtos_event_info_table[0][3];
  csky_rtos_output_field (&tmp_des,
                          &(tp->rtos_event_info_table[3]),
                          from_tty);
  ui_out_text (uiout, "\n");

  do_cleanups (cleanup_event_info);
}

/* List all threads stack info.
   ARGS will be ignored.  */

static void
stack_all_command (char* args, int from_tty)
{
  int i,index;
  RTOS_FIELD *tmp_field;
  RTOS_FIELD_DES tmp_des;
  RTOS_TCB_COMMON *tp;
  CORE_ADDR offset;
  struct cleanup *cleanup_stack_info;
  struct ui_out *uiout = current_uiout;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  /* This commands need no args.  */
  if (*args != '\0')
    {
      printf_filtered ("info mthreads stack all command omit all args.\n");
    }

  cleanup_stack_info = make_cleanup_ui_out_tuple_begin_end (uiout,
                                                            "stackinfo");
  rtos_output_list_table_column (rtos_ops.rtos_des->i_mthreads_stack,
                                 rtos_ops.rtos_des->i_mthreads_stack_size);

  for (tp = rtos_task_list; tp != NULL; tp = tp->next)
    {
      if (tp->accessed == 1)
        {
          CORE_ADDR offset;
          struct cleanup *cleanup_frame =
           make_cleanup_ui_out_tuple_begin_end (uiout, "frame");

          if (!tp->extend_fields)
            {
              tp->extend_fields = (RTOS_FIELD *) malloc 
                (sizeof (RTOS_FIELD) * rtos_ops.rtos_des->extend_table_num);
              memset (tp->extend_fields,
                0,
                sizeof (RTOS_FIELD) * rtos_ops.rtos_des->extend_table_num);
            }

          ui_out_text (uiout, "#");
          /* Output fields value of "info mthreads list" command.  */
          for (i = 0; i < rtos_ops.rtos_des->i_mthreads_stack_size; i++)
            {
              index = rtos_ops.rtos_des->i_mthreads_stack[i];
              /* Basic fields definition.  */
              if (index <= RTOS_BASIC_FIELD_NUM)
                {
                  tmp_field = &(tp->rtos_basic_table[index - 1]);
                  tmp_des = rtos_ops.rtos_des->rtos_tcb_table[index - 1];
                }
              else
                {
                  int tmp_index = index - RTOS_BASIC_FIELD_NUM -1;
                  tmp_field = &(tp->extend_fields[tmp_index]);
                  tmp_des =
                    rtos_ops.rtos_des->rtos_tcb_extend_table[tmp_index];
                }
              if (is_field_null (tmp_des, *tmp_field))
                {
                  csky_rtos_parse_field (tp->rtos_basic_table[3].coreaddr,
                                         &tmp_des,
                                         tmp_field);
                }
              csky_rtos_output_field (&tmp_des, tmp_field, from_tty);
            }
          ui_out_text (uiout, "\n");
          do_cleanups (cleanup_frame);
        }
    }
  do_cleanups (cleanup_stack_info);
  return;
}

/* List thread(s) depth info.
   If ARGS, list the one, else list all threads' stack depth info.
   ALL_DEPTH: ALL_DEPTH == 1, list all threads' stack depth info.
              ALL_DEPTH == 0, list one thread's stack depth info.  */

#define PRINT_ALL_DEPTH 1
#define PRINT_ONE_DEPTH 0

static void
stack_depth_command (char* args, int from_tty)
{
  CORE_ADDR thread_id;
  RTOS_TCB_COMMON *tp = NULL;

  if (args == NULL || *args == '\0')
    {
      /* List all threads' stack depth info.  */
      int column_print = 1;
      for (tp = rtos_task_list; tp!=NULL; tp = tp->next)
        {
          thread_id = tp->rtos_basic_table[0].coreaddr;
          single_stack_depth_handler (ptid_build (rtos_ops.target_ptid.pid,
                                                  0,
                                                  thread_id),
                                      column_print,
                                      PRINT_ALL_DEPTH,
                                      from_tty);
          column_print = 0;
        }
      return;
    }
  else
    {
      char* arg1 = args;
      const char *SINGLE[2] ={"0x", "0X"};
      /* Analysis args and list one thread's stack depth according to args. */
      while (arg1 != NULL)
        {
          if (*arg1 != ' ')
            break;
          arg1++;
        }

      if (strncmp (arg1, SINGLE[0], 2) != 0
          && strncmp (arg1, SINGLE[1], 2) != 0)
        {
          /* Invalid parameter.  */
          printf_filtered ("Illegal args,try \"help info mthreads\"for"
                           " more information.\n");
          return;
        }

      thread_id = strtoll (arg1, NULL, 16);

      single_stack_depth_handler (ptid_build (rtos_ops.target_ptid.pid,
                                              0,
                                              thread_id),
                                  1,
                                  PRINT_ONE_DEPTH,
                                  from_tty);
    }
  return;
}

/* Finish analysis of single thread's stack depth
   PTID: specified thread.  */

static void
single_stack_depth_handler (ptid_t ptid, int column_print,
                            int print_all_depths, int from_tty)
{
  int i,index;
  RTOS_FIELD *tmp_field;
  RTOS_FIELD_DES tmp_des;
  CORE_ADDR offset;
  struct cleanup * cleanup_stack_depth;
  RTOS_TCB_COMMON *tp = NULL;
  struct ui_out *uiout = current_uiout;
  enum bfd_endian byte_order = gdbarch_byte_order (get_current_arch ());

  tp = find_rtos_task_info (ptid);
  if (tp != NULL && tp->accessed == 1)
    {
      if (!tp->extend_fields)
        {
          tp->extend_fields = (RTOS_FIELD *) malloc
            (sizeof (RTOS_FIELD) * rtos_ops.rtos_des->extend_table_num);
          memset (tp->extend_fields,
                  0,
                  sizeof (RTOS_FIELD) * rtos_ops.rtos_des->extend_table_num);
        }

      cleanup_stack_depth = make_cleanup_ui_out_tuple_begin_end (uiout,
                                                                 "stackdepth");
      /* Print column.  */
      if (column_print)
        rtos_output_list_table_column
          (rtos_ops.rtos_des->i_mthreads_stack,
           rtos_ops.rtos_des->i_mthreads_stack_size);

      ui_out_text (uiout, "#");
      /* Output fields value of "info mthreads ID" command.  */
      for (i = 0; i < rtos_ops.rtos_des->i_mthreads_stack_size; i++)
        {
          index = rtos_ops.rtos_des->i_mthreads_stack[i];
          /* Basic fields definition.  */
          if (index <= RTOS_BASIC_FIELD_NUM)
            {
              tmp_field = &(tp->rtos_basic_table[index-1]);
              tmp_des = rtos_ops.rtos_des->rtos_tcb_table[index -1];
            }
          else
            {
              int tmp_index = index - RTOS_BASIC_FIELD_NUM -1;
              tmp_field = &(tp->extend_fields[tmp_index]);
              tmp_des = rtos_ops.rtos_des->rtos_tcb_extend_table[tmp_index];
            }

          /* Read field from target , if necessary.  */
          if (is_field_null (tmp_des, *tmp_field))
            {
              csky_rtos_parse_field (tp->rtos_basic_table[3].coreaddr,
                                     &tmp_des,
                                     tmp_field);
            }
          /* Output this field.  */
          csky_rtos_output_field (&tmp_des, tmp_field, from_tty);
        }
      ui_out_text (uiout, "\n");
      do_cleanups (cleanup_stack_depth);
    }
  else
    {
      if (ptid.tid != 0 || !print_all_depths)
        printf_filtered ("Thread 0x%x is not alive.\n", ptid.tid);
    }

  return;
}

/* Set current ops is rtos.  */

void
set_is_rtos_ops ()
{
  current_ops_is_rtos = 1;
}

/* Clear current ops is rtos.  */

void
clear_is_rtos_ops ()
{
  current_ops_is_rtos = 0;
}


/* Is current ops is rtos, when this function is excuted,
   current_ops_is_rtos will be cleared.  */

int
is_rtos_ops ()
{
  return current_ops_is_rtos;
}

/* The rtos ops to support csky multi-tasks debugging
   Includes all hooks used in remote-csky.c.  */

RTOS_OPS rtos_ops =
{
  NULL,
  NULL,
  NULL,
  {-1, 0, 0},
  csky_rtos_update_event_info,
  csky_rtos_update_task_info,
  csky_rtos_open,
  csky_rtos_close,
  csky_rtos_is_regnum_in_task_list,
  csky_rtos_fetch_registers,
  csky_rtos_store_registers,
  csky_rtos_prepare_resume,
  csky_rtos_is_task_in_task_list,
  csky_rtos_pid_to_str,
  csky_rtos_reset
};

/* Some common functions.  */

void
common_open (const char * name, int from_tty)
{
  /* Do the real open.  */
  struct target_ops* csky_ops = get_csky_ops ();
  set_is_rtos_ops ();
  csky_ops->to_open (name, from_tty);
}

/* The attach method should only implemented in rtos_ops.  */

static void
rtos_attach (struct target_ops *ops, const char * args, int from_tty)
{
  if (args)
    {
      warning ("csky attach command ignore all prameters.\n");
    }

  inferior_appeared (current_inferior (), ptid_get_pid (remote_csky_ptid));
  if (rtos_ops.rtos_des!= NULL)
    {
      enum RTOS_STATUS status
        = rtos_ops.to_update_task_info (rtos_ops.current_ops,
                                        rtos_ops.rtos_des,
                                        &inferior_ptid);

      if ((status == NO_RTOS_PROGRAME) || (status == NOT_INIT_TARGET))
        {
          /* No multi-threads temporary.  */
          inferior_ptid = remote_csky_ptid;
          add_thread_silent (inferior_ptid);
        }
    }
  if (rtos_ops.event_des != NULL)
    {
      rtos_ops.to_update_event_info (rtos_ops.current_ops,
                                     rtos_ops.event_des);
    }

  reinit_frame_cache ();
  registers_changed ();
  stop_pc = regcache_read_pc (get_current_regcache ());
}

/* Initial the corresponding rtos_ops;
   add_target() for all supported names.  */

void
init_rtos_ops ()
{
  int i, j;

  /* Initialize rtos_init_tables.  */
  initialize_rtos_init_tables ();
  rtos_ops.current_ops = get_csky_ops ();
  rtos_ops.rtos_des = NULL;
  rtos_ops.event_des = NULL;

  /* Set all handlers of rtos_ops.  */
  for (i = 0;
       i < sizeof (rtos_init_tables) / sizeof (rtos_init_tables[0]);
       i++)
    {
      RTOS_INIT_TABLE table = rtos_init_tables[i];
      prepare_csky_ops (table.ops);
      /* Init rtos_tcb.  */
      if (table.init_rtos_tcb)
        {
          table.init_rtos_tcb ();
        }
      /* Init rtos_event.  */
      if (table.init_rtos_event)
        {
          table.init_rtos_event ();
        }
      table.ops->to_open = table.to_open;
      table.ops->to_attach = rtos_attach;
      for (j = 0; j < table.name_num; j++)
        {
          table.ops->to_shortname = (char *)table.names[j];
          add_target (table.ops);
        }
    }

  add_info ("mthreads", rtos_info_mthreads_command,
    "multi-threads commands, only support in multi-threads debugging:\n  "
    "info mthreads list:list all threads' info.\n  "
    "info mthreads [ID]:list one thread's detailed info.\n  "
    "info mthreads stack all:list all threads' stack info.\n  "
    "info mthreads stack depth [ID]:list one or all thread(s)"
    " stack depth info.\n  "
    "info mthreads event all:list all os events.\n  "
    "info mthreads event [ID]:list one event info.\n  "
    "All [ID] should be started with \"0x\".");
}

