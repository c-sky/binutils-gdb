/* Csky-rtos.h  */

#ifndef CSKY_RTOS_H_
#define CSKY_RTOS_H_

#define TASK_NAME_MAX	64
#define RTOS_BASIC_FIELD_NUM 5
#define CSKY_RTOS_NUM 6

extern void init_rtos_ops ();
extern struct target_ops* get_csky_ops ();
extern void prepare_csky_ops (struct target_ops* ops);

typedef enum rtos_data_type
{
  /* Int  */
  Integer,

  /* A char*  */
  String,

  /* Display as const char*, but saved as int in the target.  */
  IntToString,

  /* Core_addr  */
  CoreAddr,

  /* Other data type.  */
  Undefine
} RTOS_DATA_TYPE;

typedef union _rtos_field
{
  int IntVal;
  char * string;
  void * undef;
  CORE_ADDR coreaddr;
} RTOS_FIELD;

/* RTOS extends field descriptor.
   This struct will be used to calculate the field.  */

typedef struct rtos_field_des
{
  /* Human readable name of this field.  */
  const char * name;

  /* The data type.  */
  RTOS_DATA_TYPE type;

  /* The size of this data.  */
  unsigned char size;

  /* A transfer method if type is IntToString type.  */

  const char * (*int2StringTransfer) (int index);

  /* flag to indicate whether use default handler or not
      1: use default handler, will ignore to_read_field&to_write_field
      0: use specified read/write handler: to_read_field&to_write_field  */
  int use_default;

  /* If a field is not directly in target TCB, more than once
     transfer should be needed; Num of elememts of offset_table.  */
  int offset_level;

  const char ** offset_table;

  /* Should the same size as offset_table, and init value is -1.  */
  int *offset_cache;

  /* Specified handler extends by user.  */

  int (*to_read_field) (CORE_ADDR , struct rtos_field_des* , RTOS_FIELD *);

  /* Output value of RTOS_FIELD.  */

  void (*to_output_field) (struct rtos_field_des* itself,
                           RTOS_FIELD *val,
                           int from_tty);
} RTOS_FIELD_DES;

/* Required fields when csky multi-tasks debugging.  */

typedef struct rtos_tcb
{
  /* To indicate how many global task list in the RTOS's kernel.  */
  unsigned int task_list_count;

  /* Thread_list, current_thread, list_next

     For some RTOS(nuttx), the there is more than one global
     task list in kernel; we prepare 10 to garantte to get all
     the task list in RTOS. If there is less then 10 in the RTOS,
     just leave it alone, CSKY GDB will do nothing.

     See $rtos_task_list_count for more information.  */
  RTOS_FIELD_DES rtos_special_table[10][3];

  /* Task_id, stack_ptr, entry_base, tcb_base, task_name.  */
  RTOS_FIELD_DES rtos_tcb_table[5];

  /* The number of elements in rtos_tcb_extend_table.  */
  unsigned int extend_table_num;

  /* Table to describe the way to get all extend field.  */
  RTOS_FIELD_DES* rtos_tcb_extend_table;

  /* The following is used to implements all "info mthreads" commands
     The table contains index for rtos_tcb_extend_table which is used
     to get the value.  */
  unsigned int *i_mthreads_list;
  unsigned int i_mthreads_list_size;
  unsigned int *i_mthreads_stack;
  unsigned int i_mthreads_stack_size;
  unsigned int *i_mthread_one;
  unsigned int i_mthread_one_size;

  /* Rtos register offset table.  */
  int *rtos_reg_offset_table;

  /* For some RTOS, the registers are not save in the stack, and they
     are in a globle block, this handler will return the start address
     of this blobal block.

     tcb_base: the tcb_base address of a task.
     regno: the regno for fetching or store
     result is the register base address for the given task.  */

  CORE_ADDR (*to_get_register_base_address) (CORE_ADDR tcb_base, int regno);

  /* The read/write register handler.

     stack_ptr: current stack pointer of this task
     regno    : the number of register
     val      : the value to be read/written
     return 0 : the reg is in the memory pointed by stack_ptr,
                else reg is in CPU. */

  void (*to_fetch_registers) (CORE_ADDR stack_ptr,
                              int regno,
                              unsigned int* val);
  void (*to_store_registers) (CORE_ADDR stack_ptr,
                              int regno,
                              unsigned int val);

  /* To check if this thread_id is valid.  */

  int (* IS_VALID_TASK) (RTOS_FIELD thread_id);

  /* To check if this reg in task's stack.  */

  int (*is_regnum_in_task_list) (struct rtos_tcb*, ptid_t, int regno);
} RTOS_TCB;

typedef struct rtos_event
{
  /* Events : flag, mutex, message, sema
     Special fields : event_base, event_next.  */
  RTOS_FIELD_DES rtos_event_special_table[4][2];

  /* Fields for each event: id, type, name, pending list.  */
  RTOS_FIELD_DES rtos_event_info_table[4][4];
}RTOS_EVENT;

enum RTOS_STATUS
{
  /* No rtos program in target.  */
  NO_RTOS_PROGRAME,

  /* Rtos is not loaded into target.  */
  NOT_INIT_TARGET,

  /* Valid target.  */
  VALID_TARGET
};

/* Rtos_ops used to extends csky_ops(struct target_ops)
   to support csky multi-tasks debugging.  */

typedef struct
{
  /* The current target ops.  */
  struct target_ops* current_ops;

  /* Rtos tcb descriptor.  */
  RTOS_TCB * rtos_des;
  RTOS_EVENT *event_des;

  /* Keep consistent with current thread in the target-board.  */
  ptid_t target_ptid;

  /* Update event info from target.  */

  void (*to_update_event_info) (struct target_ops*,
                                RTOS_EVENT* rtos_event_des);
  /* Do operation below:
     1. security check for the specified target;
     2. check whether target has the corresponding rtos program
     3. check whether target is running the rtos program
     4. update the tasks info from target
     Return RTOS_STATUS to indicate 1. 2. 3.  */

  enum RTOS_STATUS (*to_update_task_info) (struct target_ops*,
                                           RTOS_TCB*,
                                           ptid_t*);

  /* Hook when target ${RTOS} IP:PORT is executed.
     Initialize current kernel ops no matter whatever.  */

  void (*to_open) (struct target_ops*, RTOS_TCB*);

  /* Initialize current kernel ops no matter whatever.  */

  void (*to_close) (struct target_ops*, RTOS_TCB*);

  /* Check if the register is in RTOS_TCB.
     Return : 0: in RTOS_TCB; other: not there.  */

  int (*is_regnum_in_task_list) (RTOS_TCB*, ptid_t, int regno);

  /* To get the register value.
     Return 0: regno is in task list, else not in the task list.  */

  void (*to_fetch_registers) (RTOS_TCB*,
                              ptid_t,
                              int regno,
                              unsigned int* value);

  /* To store the register value.
     Return 0: regno is in task list, else not in the task list.  */

  void (*to_store_registers) (RTOS_TCB*,
                              ptid_t,
                              int regno,
                              ULONGEST value);

  /* Preparation for resume, does user switch the thread ?  */

  void (*to_prepare_resume) (RTOS_TCB*, ptid_t, int step);

  /* Check whether the given ptid in the RTOS_TCB list.
     Return 1: in the list; 0: not in the list.  */

  int (*is_task_in_task_list) (RTOS_TCB*, ptid_t);

  /* Change the PID into the char pointer for GDB to print out.  */

  void (*to_pid_to_str) (RTOS_TCB*, ptid_t ptid, char *buf);

  /* Reset the rtos_tcb list.  */

  void (*to_reset) (RTOS_TCB*);
} RTOS_OPS;

extern RTOS_OPS rtos_ops;

typedef struct
{
  struct target_ops * ops;
  void (*to_open) (const char *, int);

  /* All names which should use corresponding target_ops.  */
  const char ** names;

  /* The number of elemenst in string array names.  */
  unsigned int name_num;

  /* Used to decribe the TCB.  */
  RTOS_TCB* rtos_tcb_des;

  /* Used to decribe the events.  */
  RTOS_EVENT* rtos_event_des;

  /* Init rtos_tcb.  */

  void (*init_rtos_tcb) (void);

  /* Init rtos_event.  */

  void (*init_rtos_event) (void);
} RTOS_INIT_TABLE;

/* Required fields when csky multi-tasks debugging.
   This struct is used to cache all task info in the target.  */

typedef struct rtos_tcb_common
{
  struct rtos_tcb_common * next;
 
  /* Task_id, stack_ptr, entry_base, tcb_base, task_name.  */
  RTOS_FIELD rtos_basic_table[5];

  /* The extends data table, the data type is RTOS_FIELD.  */
  RTOS_FIELD * extend_fields;

  /* Flag for refresh_rtos_task_list().
     If ACCESSED = 1: this member is accessed;
     Else, member is not accessed.  */
  unsigned int accessed;
} RTOS_TCB_COMMON;

/* This struct is used to cache all task event info in the target.  */

typedef struct rtos_event_common
{
  struct rtos_event_common * next;

  /* Id, type, name, pendlist.  */
  RTOS_FIELD rtos_event_info_table[4];

  /* Flag for refresh_rtos_task_list().
     If ACCESSED = 1: this member is accessed
     else, member is not accessed. */
  unsigned int accessed;
} RTOS_EVENT_COMMON;

extern  RTOS_TCB_COMMON* find_rtos_task_info (ptid_t);
extern int csky_rtos_symbol2offset (char *s, int *offset);
extern void common_open (const char * name, int from_tty);
extern void initialize_rtos_init_tables ();
extern RTOS_INIT_TABLE rtos_init_tables[CSKY_RTOS_NUM];
extern int csky_rtos_parse_field_default (CORE_ADDR tcb_base,
                                          RTOS_FIELD_DES *f_des,
                                          RTOS_FIELD * field);
extern void  csky_rtos_output_field_default (RTOS_FIELD_DES* itself,
                                             RTOS_FIELD *val,
                                             int from_tty);
extern void set_is_rtos_ops ();
extern void clear_is_rtos_ops ();
extern int is_rtos_ops ();

#endif /* CSKY_RTOS_H_ */

