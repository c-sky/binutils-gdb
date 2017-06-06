/* tc-csky.c -- Assemble for the CSKY
   Copyright (C) 1989-2016 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* CSKY machine specific gas.
   Create by Lifang Xia (lifang_xia@c-sky.com)
   Bugs & suggestions are completely welcome.  This is free software.
   Please help us make it better.  */

#include "as.h"
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include "safe-ctype.h"
#include "subsegs.h"
#include "obstack.h"
#include "libiberty.h"
#include "struc-symbol.h"

#ifdef OBJ_ELF
#include "elf/csky.h"
#include "dw2gencfi.h"
#endif
#include "tc-csky.h"
#include "dwarf2dbg.h"

#define BUILD_AS          1

#define OPCODE_MAX_LEN    20
#define HAS_SUB_OPERAND   0xffffffff

/* This value is just for lrw to distinguish "[]" label.  */
#define NEED_OUTPUT_LITERAL           1

#define IS_EXTERNAL_SYM(sym, sec)     (S_GET_SEGMENT(sym) != sec)
#define IS_SUPPORT_OPCODE16(opcode)   (opcode->isa_flag16 | isa_flag)
#define IS_SUPPORT_OPCODE32(opcode)   (opcode->isa_flag32 | isa_flag)

#define KB                * 1024
#define MB                KB * 1024
#define GB                MB * 1024

/* Add literal pool related macro here.  */
/* 1024 - 1 entry - 2 byte rounding.*/
#define v1_SPANPANIC      (998)
#define v1_SPANCLOSE      (900)
#define v1_SPANEXIT       (600)
#define v2_SPANPANIC      ((1024)-4)
/* 1024 is flrw offset/
   24 is the biggest size for single instruction.
   for lrw16 (3+7, 512 bytes).  */
#define v2_SPANCLOSE      (512-24)
/* for lrw16, 112 average size for a function.  */
#define v2_SPANEXIT       (512-112)
/* For lrw16 (3+7, 512 bytes).  */
#define v2_SPANCLOSE_ELRW (1016-24)
/* For lrw16, 112 average size for a function.  */
#define v2_SPANEXIT_ELRW  (1016-112)
#define MAX_POOL_SIZE     (1024/4)
#define POOL_END_LABEL    ".LE"
#define POOL_START_LABEL  ".LS"

/* v1_relax_table used.  */
/* These are the two types of relaxable instruction */
#define COND_JUMP         1
#define UNCD_JUMP         2
#define COND_JUMP_PIC     3
#define UNCD_JUMP_PIC     4

#define UNDEF_DISP        0
#define DISP12            1
#define DISP32            2
#define UNDEF_WORD_DISP   3

#define C12_LEN           2
/* allow for align: bt/jmpi/.long + align */
#define C32_LEN           10
/* allow for align: bt/subi/stw/bsr/lrw/add/ld/addi/jmp/.long + align */
#define C32_LEN_PIC       24
#define U12_LEN           2
/* allow for align: jmpi/.long + align */
#define U32_LEN           8
/* allow for align: subi/stw/bsr/lrw/add/ld/addi/jmp/.long + align */
#define U32_LEN_PIC       22

#define C(what,length)    (((what) << 2) + (length))
#define UNCD_JUMP_S       (do_pic ? UNCD_JUMP_PIC : UNCD_JUMP)
#define COND_JUMP_S       (do_pic ? COND_JUMP_PIC : COND_JUMP)
#define U32_LEN_S         (do_pic ? U32_LEN_PIC  : U32_LEN)
#define C32_LEN_S         (do_pic ? C32_LEN_PIC  : C32_LEN)

/* v2_relax_table used.  */
#define COND_DISP10_LEN   2   /* bt/bf_16.  */
#define COND_DISP16_LEN   4   /* bt/bf_32.  */

#define UNCD_DISP10_LEN   2   /* br_16.  */
#define UNCD_DISP16_LEN   4   /* br_32.  */
#define UNCD_DISP26_LEN   4   /* br32_old.  */

#define JCOND_DISP10_LEN  2   /* bt/bf_16.  */
#define JCOND_DISP16_LEN  4   /* bt/bf_32.  */
#define JCOND_DISP32_LEN  12  /* !(bt/bf_16)/jmpi 32/.align 2/literal 4.  */
#define JCOND_DISP26_LEN  8   /* bt/bf_32/br_32 old.  */

#define JUNCD_DISP26_LEN  4   /* bt/bf_32 old.  */
#define JUNCD_DISP10_LEN  2   /* br_16.  */
#define JUNCD_DISP16_LEN  4   /* bt/bf_32.  */
#define JUNCD_DISP32_LEN  10  /* jmpi_32/.align 2/literal 4/  CHANGED!.  */
#define JCOMP_DISP26_LEN  8   /* bne_32/br_32 old.  */

#define JCOMP_DISP16_LEN  4   /* bne_32 old.  */
#define JCOMPZ_DISP16_LEN 4   /* bhlz_32.  */
#define JCOMPZ_DISP32_LEN 14  /* bsz_32/jmpi 32/.align 2/literal 4.  */
#define JCOMPZ_DISP26_LEN 8   /* bsz_32/br_32  old.  */
#define JCOMP_DISP32_LEN  14  /* be_32/jmpi_32/.align 2/literal old.  */

#define BSR_DISP10_LEN    2   /* bsr_16.  */
#define BSR_DISP26_LEN    4   /* bsr_32.  */
#define LRW_DISP7_LEN     2   /* lrw16.  */
#define LRW_DISP16_LEN    4   /* lrw32.  */

/* Works declare.  */
bfd_boolean v1_work_lrw (void);
bfd_boolean v1_work_jbsr (void);
bfd_boolean v1_work_fpu_fo (void);
bfd_boolean v1_work_fpu_fo_fc (void);
bfd_boolean v1_work_fpu_write (void);
bfd_boolean v1_work_fpu_read (void);
bfd_boolean v1_work_fpu_writed (void);
bfd_boolean v1_work_fpu_readd (void);
bfd_boolean v2_work_istack (void);
bfd_boolean v2_work_btsti (void);
bfd_boolean v2_work_addi (void);
bfd_boolean v2_work_subi (void);
bfd_boolean v2_work_add_sub (void);
bfd_boolean v2_work_rotlc (void);
bfd_boolean v2_work_bgeni (void);
bfd_boolean v2_work_not (void);
bfd_boolean v2_work_jbtf (void);
bfd_boolean v2_work_jbr (void);
bfd_boolean v2_work_lrw (void);
bfd_boolean v2_work_lrsrsw (void);
bfd_boolean v2_work_jbsr (void);
bfd_boolean v2_work_jsri (void);
bfd_boolean v2_work_movih (void);
bfd_boolean v2_work_ori (void);
/* csky-opc.h must be include after works defines.  */
#include "opcodes/csky-opc.h"
#include "opcode/csky.h"

enum
{
  RELAX_NONE = 0,
  RELAX_OVERFLOW,

  COND_DISP10 = 20,    /* bt/bf_16.  */
  COND_DISP16,    /* bt/bf_32.  */

  UNCD_DISP10,    /* br_16.  */
  UNCD_DISP16,    /* br_32.  */

  JCOND_DISP10,   /* bt/bf_16.  */
  JCOND_DISP16,   /* bt/bf_32.  */
  JCOND_DISP32,   /* !(bt/bf_32)/jmpi + literal.  */

  JUNCD_DISP10,   /* br_16.  */
  JUNCD_DISP16,   /* br_32.  */
  JUNCD_DISP32,   /* jmpi + literal.  */

  JCOMPZ_DISP16,  /* bez/bnez/bhz/blsz/blz/bhsz.  */
  JCOMPZ_DISP32,  /* !(jbez/jbnez/jblsz/jblz/jbhsz) + jmpi + literal.  */

  BSR_DISP26,     /* bsr_32.  */

  LRW_DISP7,      /* lrw16.  */
  LRW2_DISP8,     /* lrw16 ,-mno-bsr16,8 bit offset.  */
  LRW_DISP16,     /* lrw32.  */
};

unsigned int mach_flag = 0;
unsigned int arch_flag = 0;
unsigned int other_flag = 0;
unsigned int isa_flag = 0;

typedef struct stack_size_entry
{
  struct stack_size_entry *next;
  symbolS *function;
  unsigned int stack_size;
}stack_size_entry;

struct _csky_option_table
{
  /* Option name.  */
  const char *name;
  /* The variable will set with value.  */
  int *ver;
  /* The value will be set to ver.  */
  int value;
  /* operation.  */
  int operation;
  /* Help infomation.  */
  const char *help;
};

struct _csky_long_option
{
  /* Option name.  */
  const char *name;
  /* Help infomation.  */
  const char *help;
  /* The function to handle the option.  */
  int (*func)(const char *subopt);
};

struct _csky_arch
{
  const char *name;
  unsigned int _arch_flag;
  unsigned int _bfd_mach_flag;
};

struct _csky_cpu
{
  const char *name;
  unsigned int _mach_flag;
  unsigned int _isa_flag;
  const char *usage;
};

struct _err_info
{
  int num;
  const char *fmt;
};

/* TODO: add error type.  */
enum error_number
{
  /* The followings are errors.  */
  ERROR_CREG_ILLEGAL = 0,
  ERROR_GREG_ILLEGAL,
  ERROR_REG_OVER_RANGE,
  ERROR_802J_REG_OVER_RANGE,
  ERROR_REG_FORMAT,
  ERROR_REG_LIST,
  ERROR_IMM_ILLEGAL,
  ERROR_IMM_OVERFLOW,             /* 5  */
  ERROR_IMM_POWER,
  ERROR_JMPIX_OVER_RANGE,
  ERROR_EXP_CREG,
  ERROR_EXP_GREG,
  ERROR_EXP_CONSTANT,
  ERROR_EXP_EVEN_FREG,
  ERROR_RELOC_ILLEGAL,
  ERROR_MISSING_OPERAND,          /* 10  */
  ERROR_MISSING_COMMA,
  ERROR_MISSING_LBRACHKET,
  ERROR_MISSING_RBRACHKET,
  ERROR_MISSING_LSQUARE_BRACKETS,
  ERROR_MISSING_RSQUARE_BRACKETS, /* 15  */
  ERROR_MISSING_LANGLE_BRACKETS,
  ERROR_MISSING_RANGLE_BRACKETS,
  ERROR_OFFSET_UNALIGNED,
  ERROR_BAD_END,
  ERROR_UNDEFINE,
  ERROR_CPREG_ILLEGAL,           /* 20  */
  ERROR_OPERANDS_ILLEGAL,
  ERROR_OPERANDS_NUMBER,
  ERROR_OPCODE_ILLEGAL,

  /* The followings are warnings.  */
  WARNING_OPTIONS,
  WARNING_IDLY,
  WARNING_BMASKI_TO_MOVI,
  /* Error and warning end.  */
  ERROR_NONE,
};

typedef enum
{
  INSN_OPCODE,
  INSN_OPCODE16F,
  INSN_OPCODE32F,
}inst_flag;

/* macro infomation.  */
struct _csky_macro_info
{
  const char *name;
  /* how many operands : if operands == 5, all of 1,2,3,4 is ok */
  long oprnd_num;
  int isa_flag;
  /* do the work */
  void (*handle_func)(void);
};

struct _csky_insn
{
  /* name of the opcode.  */
  char * name;
  /* output instruction.  */
  unsigned int inst;
  /* pointer for frag.  */
  char * output;
  /* end of instrution.  */
  char * opcode_end;
  /* flag for INSN_OPCODE16F, INSN_OPCODE32F, INSN_OPCODE, INSN_MACRO.  */
  inst_flag flag_force;
  /* operand number.  */
  int number;
  struct _csky_opcode *opcode;
  struct _csky_macro_info *macro;
  /* insn size for check_literal.  */
  unsigned int isize;
  /* max size of insn for relax frag_var.  */
  unsigned int max;
  /* Indicates which element is in _csky_opcode_info op[] array.  */
  int opcode_idx;
  /* the value of each operand in instruction when layout.  */
  int idx;
  int val[MAX_OPRND_NUM];
  struct relax_info
    {
      int max;
      int var;
      int subtype;
    } relax;
  expressionS e;
};

struct tls_addend {
  fragS*  frag;
  offsetT offset;
};

/* Literal pool data structures.  */
struct literal
{
  unsigned short  refcnt;
  unsigned char   ispcrel;
  unsigned char   unused;
  bfd_reloc_code_real_type r_type;
  expressionS     e;
  struct tls_addend tls_addend;
  unsigned char   isdouble;
  uint64_t dbnum;
};

static void csky_idly (void);
static void csky_rolc (void);
static void csky_sxtrb (void);
static void csky_movtf (void);
static void csky_addc64 (void);
static void csky_subc64 (void);
static void csky_or64 (void);
static void csky_xor64 (void);
static void csky_neg (void);
static void csky_rsubi (void);
static void csky_arith (void);
static void csky_decne (void);
static void csky_lrw (void);

static int parse_cpu(const char *str);
static int parse_arch (const char *str);

static enum bfd_reloc_code_real insn_reloc;

struct _errs
{
  int err_num;
  int idx;
  void *arg1;
  void *arg2;
} errs;

/* Error infomation lists.  */
static const struct _err_info err_infos[] =
{
  {ERROR_CREG_ILLEGAL, "Operand %d error: control register is illegal."},
  {ERROR_GREG_ILLEGAL, "Operand %d error: general register is illegal."},
  {ERROR_REG_OVER_RANGE, "Operand %d error: %s register is over range."},
  {ERROR_802J_REG_OVER_RANGE, "Operandr %d register %s out of range (802j only has registers:0-15,23,24,25,30)"},
  {ERROR_REG_FORMAT, "Operand %d error: %s."},
  {ERROR_REG_LIST, "Register list format is illegal."},
  {ERROR_IMM_ILLEGAL, "Operand %d is not an immediate."},
  {ERROR_IMM_OVERFLOW, "Operand %d is overflow."},
  {ERROR_IMM_POWER, "immediate %d is not a power of two"},
  {ERROR_JMPIX_OVER_RANGE, "The second operand must be 16/24/32/40"},
  {ERROR_EXP_CREG, "Operand %d error: control register is expected."},
  {ERROR_EXP_GREG, "Operand %d error: general register is expected."},
  {ERROR_EXP_CONSTANT, "Operand %d error: constant is expected."},
  {ERROR_EXP_EVEN_FREG, "Operand %d error: even float register is expected."},
  {ERROR_RELOC_ILLEGAL, "@%s reloc is not supported"},
  {ERROR_MISSING_OPERAND, "Operand %d is missing."},
  {ERROR_MISSING_COMMA, "Missing ','"},
  {ERROR_MISSING_LBRACHKET, "Missing '('"},
  {ERROR_MISSING_RBRACHKET, "Missing ')'"},
  {ERROR_MISSING_LSQUARE_BRACKETS, "Missing '['"},
  {ERROR_MISSING_RSQUARE_BRACKETS, "Missing ']'"},
  {ERROR_MISSING_LANGLE_BRACKETS, "Missing '<'"},
  {ERROR_MISSING_RANGLE_BRACKETS, "Missing '>'"},
  {ERROR_OFFSET_UNALIGNED, "Operand %d is unaligned. It must be %d aligned!"},
  {ERROR_BAD_END, "Operands mismatch, it has a bad end: %s"},
  {ERROR_UNDEFINE, NULL},
  {ERROR_CPREG_ILLEGAL, "Operand %d illegal, expect a cpreg(cpr0-cpr63)."},
  {ERROR_OPERANDS_ILLEGAL, "Operands mismatch: %s."},
  {ERROR_OPERANDS_NUMBER, "Operands number mismatch, %d operands expected."},
  {ERROR_OPCODE_ILLEGAL, "The instruction is not recognized."},
  {WARNING_OPTIONS, "Option %s is not support in %s."},
  {WARNING_IDLY, "idly %d is encoded to: idly 4 "},
  {WARNING_BMASKI_TO_MOVI, "translating bmaski to movi."},
  {ERROR_NONE, "There is no error."},
};

static int do_pic = 0;            /* for jbr/jbf/jbt relax jmpi reloc.  */
static int do_pff = -1;           /* for insert two br ahead of literals.  */
static int do_force2bsr = -1;     /* for jbsr->bsr.  */
static int do_jsri2bsr = 1;       /* for jsri->bsr.  */
static int do_nolrw = 0;          /* lrw to movih & ori, only for V2.  */
static int do_anchor = 0;         /* lrw to addi, only for V2  */
static int do_long_jump = 0;      /* control wether jbf,jbt,jbr relax to jmpi.  */
static int do_extend_lrw = -1;    /* delete bsr16 in both two options,
                                     add btesti16 ,lrw offset +1 in -melrw.  */
static int do_func_dump = 0;      /* dump literals after every function.  */
static int do_br_dump = 1;        /* work for -mabr/-mno-abr, control the literals dump.  */
static int do_intr_stack = -1;    /* control interrupt stack module, 801&802&803&803S
                                     default open, 807&810, default close.  */
static int do_callgraph_call= 1;  /* control to reserve function call related relocs,
                                     default on. This option hasn't implenment,
                                     we put it here for compatibility.  */
#ifdef INCLUDE_BRANCH_STUB
static int do_use_branchstub = -1;
#else
static int do_use_branchstub = 0;
#endif

/* This table is used to handle options which will be set a value to a veriable.  */
enum
{
  OPTS_OPERATION_ASSIGN = 1,
  OPTS_OPERATION_OR,
};

struct _csky_option_table csky_opts[] =
{
  {"mlittle-endian",          &target_big_endian, 0, OPTS_OPERATION_ASSIGN,
    "-mlittle-endian    \t\tmeans target is little-endian.\n"},
  {"EL",                      &target_big_endian, 0, OPTS_OPERATION_ASSIGN,
    "-EL                \t\tAlias of -mlittle-endian.\n"},
  {"mbig-endian",             &target_big_endian, 1, OPTS_OPERATION_ASSIGN,
    "-mbig-endian       \t\tmeans target is big-endian.\n"},
  {"EB",                      &target_big_endian, 1, OPTS_OPERATION_ASSIGN,
    "-EB                \t\tAlias of -mbig-endian.\n"},
  {"fpic",                    &do_pic,        1, OPTS_OPERATION_ASSIGN, NULL},
  {"pic",                     &do_pic,        1, OPTS_OPERATION_ASSIGN, NULL},
  {"mljump",                  &do_long_jump,  1, OPTS_OPERATION_ASSIGN,
    "-mljump    \t\t\tjbf,jbt,jbr transform to jmpi (def: dis,CK800 only)\n"},
  {"mno-ljump",               &do_long_jump,  0, OPTS_OPERATION_ASSIGN,
    "-mno-ljump    \t\t\tdisable jbf,jbt,jbr transform to jmpi\n\t\t\t\t(def: dis,CK800 only)\n"},
  {"force2bsr",               &do_force2bsr,  1, OPTS_OPERATION_ASSIGN, NULL},
  {"mforce2bsr",              &do_force2bsr,  1, OPTS_OPERATION_ASSIGN,
    "-mforce2bsr/force2bsr   \tenable force jbsr to bsr, takes priority over jsri2bsr\n\t\t\t\t(default: enable)\n"},
  {"no-force2bsr",            &do_force2bsr,  0, OPTS_OPERATION_ASSIGN, NULL},
  {"mno-force2bsr",           &do_force2bsr,  0, OPTS_OPERATION_ASSIGN,
    "-mno-force2bsr/-no-force2bsr \tdisable force jbsr to bsr, takes priority over jsri2bsr\n"},
  {"jsri2bsr",                &do_jsri2bsr,   1, OPTS_OPERATION_ASSIGN, NULL},
  {"mjsri2bsr",               &do_jsri2bsr,   1, OPTS_OPERATION_ASSIGN,
    "-mjsri2bsr/-jsri2bsr    \tenable jsri to bsr transformation (default: enable)\n"},
  {"mno-jsri2bsr",             &do_jsri2bsr,   0, OPTS_OPERATION_ASSIGN,
    "-mno-jsri2bsr    \t\tdisenable jsri to bsr transformation\n"},
  {"no-jsri2bsr",            &do_jsri2bsr,   0, OPTS_OPERATION_ASSIGN,
    "-no-jsri2bsr    \t\tshort name of -mno-jsri2bsr\n"},
  {"mnolrw",                  &do_nolrw,      1, OPTS_OPERATION_ASSIGN, NULL},
  {"mno-lrw",                 &do_nolrw,      1, OPTS_OPERATION_ASSIGN,
    "-mno-lrw    \t\t\timplement lrw as movih + ori, ignored when -manchor\n"},
  {"mliterals-after-func",    &do_func_dump,  1, OPTS_OPERATION_ASSIGN,
    "-mliterals-after-func    \tenable dump literals after every function\n"},
  {"mlaf",                    &do_func_dump,  1, OPTS_OPERATION_ASSIGN,
    "-mlaf    \t\t\tshort name of -mliterals-after-func\n"},
  {"mno-literals-after-func", &do_func_dump,  0, OPTS_OPERATION_ASSIGN,
    "-mno-literals-after-func   \tdisable dump literals after every function\n"},
  {"mno-laf",                 &do_func_dump,  0, OPTS_OPERATION_ASSIGN,
    "-mno-laf    \t\t\tshort name of -mno-literals-after-func\n"},
  {"manchor",                 &do_anchor,     1, OPTS_OPERATION_ASSIGN,
    "-m{no-}anchor    \t\t{dis}able implement lrw as addi rtb/rdb + offset\n"},
  {"mno-anchor",              &do_anchor,     0, OPTS_OPERATION_ASSIGN, NULL},
  {"melrw",                   &do_extend_lrw, 1, OPTS_OPERATION_ASSIGN,
    "-m{no-}elrw    \t\t\t{dis}able extend lrw instruction and\n\t\t\t\t16 bits btesti instruction (def: dis,CK800 only)\n"},
  {"mno-elrw",                &do_extend_lrw, 0, OPTS_OPERATION_ASSIGN, NULL},
  {"mliterals-after-br",      &do_br_dump,    1, OPTS_OPERATION_ASSIGN,
    "-mliterals-after-br    \t\tdump literals after every br instruction\n"},
  {"mlabr",                   &do_br_dump,    1, OPTS_OPERATION_ASSIGN,
    "-mlabr    \t\t\tshort name of -mliterals-after-br\n"},
  {"mno-literals-after-br",   &do_br_dump,    0, OPTS_OPERATION_ASSIGN,
    "-mno-literals-after-br    \tdisable dump literals after every br instruction\n"},
  {"mno-labr",                &do_br_dump,    0, OPTS_OPERATION_ASSIGN,
    "-mno-labr    \t\t\tshort name of -mno-literals-after-br\n"},
  {"mistack",                 &do_intr_stack, 1, OPTS_OPERATION_ASSIGN, NULL},
  {"mno-istack",              &do_intr_stack, 0, OPTS_OPERATION_ASSIGN, NULL},
  {"mcallgraph",              &do_callgraph_call, 1, OPTS_OPERATION_ASSIGN, NULL},
  {"mno-callgraph",           &do_callgraph_call, 0, OPTS_OPERATION_ASSIGN, NULL},
#ifdef INCLUDE_BRANCH_STUB
  {"mbranch-stub",            &do_use_branchstub, 1, OPTS_OPERATION_ASSIGN, NULL},
  {"mno-branch-stub",         &do_use_branchstub, 0, OPTS_OPERATION_ASSIGN, NULL},
#endif
  {"mmp",                     (int *)&other_flag, CSKY_ARCH_MP,    OPTS_OPERATION_OR,
    "-mmp    \t\t\tsupport multiple processor instructions\n"},
  {"mcp",                     (int *)&other_flag, CSKY_ARCH_CP,    OPTS_OPERATION_OR,
    "-mcp    \t\t\tsupport coprocessor instructions\n"},
  {"mdsp",                    (int *)&other_flag, CSKY_ARCH_DSP,   OPTS_OPERATION_OR,
    "-mdsp    \t\t\tsupport csky DSP instructions\n"},
  {"mcache",                  (int *)&other_flag, CSKY_ARCH_CACHE, OPTS_OPERATION_OR,
    "-mcache    \t\t\tsupport cache prefetch instruction\n"},
  {"msecurity",               (int *)&other_flag, CSKY_ARCH_MAC,   OPTS_OPERATION_OR,
    "-mdsp    \t\t\tsupport csky DSP instructions\n"},
  {"mhard-float",             (int *)&other_flag, CSKY_ARCH_FLOAT, OPTS_OPERATION_OR,
    "-mhard-float    \t\tsupport hard float instructions\n"},
  {NULL, NULL, 0, 0, NULL}
};

struct _csky_long_option csky_long_opts[] =
{
  {"mcpu=", "-mcpu=[ck510, ck610, ...] \tset CPU, 510[e]/520/610[ef]/801/802[e]/802P\n\t\t\t\t/803[e]/803P/803s[ef]/810[ef]/810P(default: 610)\n", parse_cpu},
  {"march=", "-march=[ck510, ck610, ...] \tset arch, 510/610/801/802/803/807/810(default: 610)\n", parse_arch},
  {NULL, NULL, NULL}
};

const relax_typeS *md_relax_table = NULL;
struct literal * literal_insn_offset;
static struct literal litpool [MAX_POOL_SIZE];
static struct literal litpool_tls [MAX_POOL_SIZE*4];
static unsigned poolsize = 0;
static unsigned poolnumber = 0;
static unsigned long poolspan = 0;
static unsigned  count_tls = 0;
static unsigned int SPANPANIC;
static unsigned int SPANCLOSE;
static unsigned int SPANEXIT;

static stack_size_entry *all_stack_size_data = NULL;
static stack_size_entry **last_stack_size_data = &all_stack_size_data;

/* Control by ".no_literal_dump N"
 * 1 : don't dump literal pool between insn1 and insnN+1
 * 0 : do nothing. */
static int do_noliteraldump = 0;

/* label for current pool.  */
static symbolS * poolsym;
static char poolname[8];

static bfd_boolean mov_r1_before;
static bfd_boolean mov_r1_after;

const relax_typeS csky_relax_table [] =
{
  /* C-SKY V1 relax table.  */
  {0, 0, 0, 0},                                   /*RELAX_NONE      */
  {0, 0, 0, 0},                                   /*RELAX_OVERFLOW  */
  {0, 0, 0, 0},
  {0, 0, 0, 0},

  /* COND_JUMP */
  {    0,     0, 0,       0 },                    /* UNDEF_DISP */
  { 2048, -2046, C12_LEN, C(COND_JUMP, DISP32) }, /* DISP12 */
  {    0,     0, C32_LEN, 0 },                    /* DISP32 */
  {    0,     0, C32_LEN, 0 },                    /* UNDEF_WORD_DISP */

  /* UNCD_JUMP */
  {    0,     0, 0,       0 },                    /* UNDEF_DISP */
  { 2048, -2046, U12_LEN, C(UNCD_JUMP, DISP32) }, /* DISP12 */
  {    0,     0, U32_LEN, 0 },                    /* DISP32 */
  {    0,     0, U32_LEN, 0 },                    /* UNDEF_WORD_DISP */

  /* COND_JUMP_PIC */
  {    0,     0, 0,           0 },                /* UNDEF_DISP */
  { 2048, -2046, C12_LEN, C(COND_JUMP_PIC, DISP32) }, /* DISP12 */
  {    0,     0, C32_LEN_PIC, 0 },                /* DISP32 */
  {    0,     0, C32_LEN_PIC, 0 },                /* UNDEF_WORD_DISP */

  /* UNCD_JUMP_PIC */
  {    0,     0, 0,           0 },                /* UNDEF_DISP */
  { 2048, -2046, U12_LEN, C(UNCD_JUMP_PIC, DISP32) }, /* DISP12 */
  {    0,     0, U32_LEN_PIC, 0 },                /* DISP32 */
  {    0,     0, U32_LEN_PIC, 0 },                 /* UNDEF_WORD_DISP */

  /* C-SKY V2 relax table.  */
  /* forward  backward      length          more     */
  {  1 KB -2 , - 1 KB ,COND_DISP10_LEN ,COND_DISP16   },  /*COND_DISP10    0*/
  { 64 KB -2 , -64 KB ,COND_DISP16_LEN ,RELAX_OVERFLOW},  /*COND_DISP16    1*/

  {  1 KB -2 , - 1 KB ,UNCD_DISP10_LEN ,UNCD_DISP16   },  /*UNCD_DISP10    2*/
  { 64 KB -2 , -64 KB ,UNCD_DISP16_LEN ,RELAX_OVERFLOW},  /*UNCD_DISP16    3*/

  {  1 KB -2 , - 1 KB ,JCOND_DISP10_LEN,JCOND_DISP16  },  /*JCOND_DISP10   4*/
  { 64 KB -2 , -64 KB ,JCOND_DISP16_LEN,JCOND_DISP32  },  /*JCOND_DISP16   5*/
  {  0       ,   0    ,JCOND_DISP32_LEN,RELAX_NONE    },  /*JCOND_DISP32   6*/

  {  1 KB -2 , - 1 KB ,JUNCD_DISP10_LEN,JUNCD_DISP16  },  /*JUNCD_DISP10   7*/
  { 64 KB -2 , -64 KB ,JUNCD_DISP16_LEN,JUNCD_DISP32  },  /*JUNCD_DISP16   8*/
  {  0       ,   0    ,JUNCD_DISP32_LEN,RELAX_NONE    },  /*JUNCD_DISP32   9*/

  { 64 KB -2 , -64 KB ,JCOMPZ_DISP16_LEN,JCOMPZ_DISP32 }, /*JCOMPZ_DISP16 10*/
  {  0       ,   0    ,JCOMPZ_DISP32_LEN,RELAX_NONE    }, /*JCOMPZ_DISP32 11*/

  { 64 MB -2 , -64 MB ,BSR_DISP26_LEN  ,RELAX_OVERFLOW},  /*BSR_DISP26    13*/

  {      508 ,      0 ,LRW_DISP7_LEN   ,LRW_DISP16    },  /*LRW_DISP7     14*/
  {     1016 ,      0 ,LRW_DISP7_LEN   ,LRW_DISP16    },  /*LRW2_DISP8    15*/
  {     64 KB,      0 ,LRW_DISP16_LEN  ,RELAX_OVERFLOW},  /*LRW_DISP16    15*/

};

static void
csky_write_insn (char *ptr, valueT use, int nbytes);
void
md_number_to_chars (char * buf, valueT val, int n);
long
md_pcrel_from_section (fixS * fixP, segT seg);

/* CSKY architecture table.  */
const struct _csky_arch csky_archs[] =
{
  {"ck510", CSKY_ARCH_510, bfd_mach_ck510},
  {"ck610", CSKY_ARCH_610, bfd_mach_ck610},
  {"ck801", CSKY_ARCH_801, bfd_mach_ck801},
  {"ck802", CSKY_ARCH_802, bfd_mach_ck802},
  {"ck803", CSKY_ARCH_803, bfd_mach_ck803},
  {"ck803s", CSKY_ARCH_803S, bfd_mach_ck803s},
  {"ck807", CSKY_ARCH_807, bfd_mach_ck807},
  {"ck810", CSKY_ARCH_810, bfd_mach_ck810},
  {NULL, 0, 0}
};

/* CSKY cpus table.  */
const struct _csky_cpu csky_cpus[] =
{
  /* CK510 series.  */
#define CSKYV1_ISA_DSP   CSKY_ISA_DSP | CSKY_ISA_MAC_DSP
  {"ck510",  CSKY_ARCH_510, CSKYV1_ISA_E1, "-mcpu=ck510"},
  {"ck510e", CSKY_ARCH_510 | CSKY_ARCH_DSP, CSKYV1_ISA_E1 | CSKYV1_ISA_DSP, "-mcpu=ck510e"},
  {"ck520",  CSKY_ARCH_510 | CSKY_ARCH_MAC, CSKYV1_ISA_E1 | CSKY_ISA_MAC | CSKY_ISA_MAC_DSP, "-mcpu=ck520"},

#define CSKY_ISA_610          CSKYV1_ISA_E1 | CSKY_ISA_CP
  /* CK610 series.  */
  {"ck610",  CSKY_ARCH_610, CSKY_ISA_610, "-mcpu=ck610"},
  {"ck610e", CSKY_ARCH_610 | CSKY_ARCH_DSP, CSKY_ISA_610 | CSKYV1_ISA_DSP, "-mcpu=ck610e"},
  {"ck610f", CSKY_ARCH_610 | CSKY_ARCH_FLOAT, CSKY_ISA_610 | CSKY_ISA_FLOAT_E1, "-mcpu=ck610f"},
  {"ck610ef", CSKY_ARCH_610 | CSKY_ARCH_FLOAT | CSKY_ARCH_DSP, CSKY_ISA_610 | CSKY_ISA_FLOAT_E1 | CSKYV1_ISA_DSP, "-mcpu=ck610ef"},
  {"ck610fe", CSKY_ARCH_610 | CSKY_ARCH_FLOAT | CSKY_ARCH_DSP, CSKY_ISA_610 | CSKY_ISA_FLOAT_E1 | CSKYV1_ISA_DSP, "-mcpu=ck610ef"},
  {"ck620",  CSKY_ARCH_610 | CSKY_ARCH_MAC, CSKY_ISA_610 | CSKY_ISA_MAC | CSKY_ISA_MAC_DSP, "-mcpu=ck620"},

  /* CK801 series.  */
#define CSKY_ISA_801    CSKYV2_ISA_E1
#define CSKYV2_ISA_DSP  (CSKY_ISA_DSP | CSKY_ISA_DSP_1E2 | CSKY_ISA_DSP_FLOAT)
  {"ck801", CSKY_ARCH_801, CSKY_ISA_801, "-mcpu=ck801"},

  /* CK802 series.  */
#define CSKY_ISA_802    (CSKY_ISA_801 | CSKYV2_ISA_1E2 | CSKY_ISA_NVIC)
  {"ck802", CSKY_ARCH_802, CSKY_ISA_802, "-mcpu=ck802"},
  {"ck802e", CSKY_ARCH_802 | CSKY_ARCH_DSP, CSKY_ISA_802 | CSKYV2_ISA_DSP, "-mcpu=ck802e"},
  {"ck802j", CSKY_ARCH_802 | CSKY_ARCH_JAVA, CSKY_ISA_802 | CSKY_ISA_JAVA, "-mcpu=ck802j"},

  /* CK803 series.  */
#define CSKY_ISA_803    (CSKY_ISA_802 | CSKYV2_ISA_2E3)
  {"ck803", CSKY_ARCH_803, CSKY_ISA_803, "-mcpu=ck803"},

  /* CK803S series.  */
#define CSKY_ISA_803S   (CSKY_ISA_803 | CSKYV2_ISA_3E4 | CSKY_ISA_MP)
#define CSKY_ISA_FLOAT_803S (CSKY_ISA_DSP_FLOAT | CSKY_ISA_DSP_FLOAT_1E2 | CSKY_ISA_FLOAT_E1 | CSKY_ISA_FLOAT_1E3)
  {"ck803s", CSKY_ARCH_803S, CSKY_ISA_803S , "-mcpu=ck803s"},
  {"ck803se", CSKY_ARCH_803S | CSKY_ARCH_DSP, CSKY_ISA_803S | CSKYV2_ISA_DSP, "-mcpu=ck803se"},
  {"ck803sj", CSKY_ARCH_803S | CSKY_ARCH_JAVA, CSKY_ISA_803S | CSKY_ISA_JAVA, "-mcpu=ck803sj"},
  {"ck803sf", CSKY_ARCH_803S | CSKY_ARCH_FLOAT, CSKY_ISA_803S | CSKY_ISA_FLOAT_803S, "-mcpu=ck803sf"},
  {"ck803sef", CSKY_ARCH_803S | CSKY_ARCH_DSP | CSKY_ARCH_FLOAT, CSKY_ISA_803S | CSKYV2_ISA_DSP | CSKY_ISA_FLOAT_803S, "-mcpu=ck803sef"},

  /* CK807 series.  */
#define CSKY_ISA_807    (CSKY_ISA_803S | CSKYV2_ISA_4E7 | CSKY_ISA_DSP | CSKY_ISA_MP_1E2 | CSKY_ISA_CACHE)
#define CSKY_ISA_FLOAT_807 (CSKY_ISA_FLOAT_803S | CSKY_ISA_FLOAT_3E4 | CSKY_ISA_FLOAT_1E2)
  {"ck807", CSKY_ARCH_807, CSKY_ISA_807 , "-mcpu=ck807"},
  {"ck807e", CSKY_ARCH_807 | CSKY_ARCH_DSP, CSKY_ISA_807 | CSKYV2_ISA_DSP, "-mcpu=ck807e"},
  {"ck807f", CSKY_ARCH_807 | CSKY_ARCH_FLOAT , CSKY_ISA_807 | CSKY_ISA_FLOAT_807, "-mcpu=ck807f"},
  {"ck807ef", CSKY_ARCH_807 | CSKY_ARCH_DSP | CSKY_ARCH_FLOAT , CSKY_ISA_807 | CSKYV2_ISA_DSP | CSKY_ISA_FLOAT_807, "-mcpu=ck807ef"},

  /* CK810 series.  */
#define CSKY_ISA_810    (CSKY_ISA_807 | CSKYV2_ISA_7E10)
#define CSKY_ISA_FLOAT_810 (CSKY_ISA_DSP_FLOAT | CSKY_ISA_DSP_FLOAT_1E2 | CSKY_ISA_FLOAT_E1 | CSKY_ISA_FLOAT_1E2)
  {"ck810", CSKY_ARCH_810, CSKY_ISA_810, "-mcpu=ck810"},
  {"ck810e", CSKY_ARCH_810 | CSKY_ARCH_DSP, CSKY_ISA_810 | CSKYV2_ISA_DSP, "-mcpu=ck810e"},
  {"ck810f", CSKY_ARCH_810 | CSKY_ARCH_FLOAT, CSKY_ISA_810 | CSKY_ISA_FLOAT_810, "-mcpu=ck810f"},
  {"ck810ef", CSKY_ARCH_810 | CSKY_ARCH_DSP | CSKY_ARCH_FLOAT, CSKY_ISA_810 | CSKYV2_ISA_DSP | CSKY_ISA_FLOAT_810, "-mcpu=ck810ef"},
  {"ck810j", CSKY_ARCH_810 | CSKY_ARCH_JAVA, CSKY_ISA_810 | CSKY_ISA_JAVA, "-mcpu=ck810j"},

  {NULL, 0, 0, NULL}
};

int md_short_jump_size = 2;
int md_long_jump_size = 4;

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.   */
const char comment_chars[] = "#";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output.  */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that comments like this one will always work.  */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant
   from exp in floating point numbers.  */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.
   As in 0f12.456
   or   0d1.2345e12  */

const char FLT_CHARS[] = "rRsSfFdDxXeEpP";

const char * md_shortopts = "m::p::f::n::j::E:";

struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

static struct _csky_insn csky_insn;

static struct hash_control *csky_opcodes_hash;
static struct hash_control *csky_macros_hash;

static struct _csky_macro_info v1_macros_table[] =
{
  {"idly",   1, CSKYV1_ISA_E1, csky_idly},
  {"rolc",   2, CSKYV1_ISA_E1, csky_rolc},
  {"rotlc",  2, CSKYV1_ISA_E1, csky_rolc},
  {"sxtrb0", 2, CSKYV1_ISA_E1, csky_sxtrb},
  {"sxtrb1", 2, CSKYV1_ISA_E1, csky_sxtrb},
  {"sxtrb2", 2, CSKYV1_ISA_E1, csky_sxtrb},
  {"movtf",  3, CSKYV1_ISA_E1, csky_movtf},
  {"addc64", 3, CSKYV1_ISA_E1, csky_addc64},
  {"subc64", 3, CSKYV1_ISA_E1, csky_subc64},
  {"or64",   3, CSKYV1_ISA_E1, csky_or64},
  {"xor64",  3, CSKYV1_ISA_E1, csky_xor64},
  {NULL,0,0,0}
};

static struct _csky_macro_info v2_macros_table[] =
{
  {"neg",   1, CSKYV2_ISA_E1,  csky_neg},
  {"rsubi", 2, CSKYV2_ISA_1E2, csky_rsubi},
  {"incf",  1, CSKYV2_ISA_1E2, csky_arith},
  {"inct",  1, CSKYV2_ISA_1E2, csky_arith},
  {"decf",  1, CSKYV2_ISA_2E3, csky_arith},
  {"decgt", 1, CSKYV2_ISA_2E3, csky_arith},
  {"declt", 1, CSKYV2_ISA_2E3, csky_arith},
  {"decne", 1, CSKYV2_ISA_1E2, csky_decne},
  {"dect",  1, CSKYV2_ISA_1E2, csky_arith},
  {"lslc",  1, CSKYV2_ISA_1E2, csky_arith},
  {"lsrc",  1, CSKYV2_ISA_1E2, csky_arith},
  {"xsr",   1, CSKYV2_ISA_1E2, csky_arith},
  {NULL,0,0,0}
};

/* For option -mnolrw, replace lrw by movih & ori.  */
static struct _csky_macro_info v2_lrw_macro_opcode =
{"lrw", 2, CSKYV2_ISA_1E2, csky_lrw};

/* This function is used to show error infomations or warnings.  */

static void csky_show_info(int err, int idx, void *arg1, void *arg2)
{
  if (err == ERROR_NONE)
    {
      return;
    }

  switch (err)
    {
      case ERROR_REG_LIST:
      case ERROR_OPCODE_ILLEGAL:
      case ERROR_JMPIX_OVER_RANGE:
      case ERROR_MISSING_COMMA:
      case ERROR_MISSING_LBRACHKET:
      case ERROR_MISSING_RBRACHKET:
      case ERROR_MISSING_LSQUARE_BRACKETS:
      case ERROR_MISSING_RSQUARE_BRACKETS:
      case ERROR_MISSING_LANGLE_BRACKETS:
      case ERROR_MISSING_RANGLE_BRACKETS:
        {
          /* Add NULL to fix warnings.  */
          as_bad (err_infos[err].fmt, NULL);
          break;
        }
      case ERROR_CREG_ILLEGAL:
      case ERROR_GREG_ILLEGAL:
      case ERROR_IMM_ILLEGAL:
      case ERROR_IMM_OVERFLOW:
      case ERROR_EXP_CREG:
      case ERROR_EXP_GREG:
      case ERROR_EXP_CONSTANT:
      case ERROR_EXP_EVEN_FREG:
      case ERROR_MISSING_OPERAND:
      case ERROR_CPREG_ILLEGAL:
        {
          as_bad (err_infos[err].fmt, idx);
          break;
        }
      case ERROR_OPERANDS_NUMBER:
      case ERROR_IMM_POWER:
        {
          as_bad (err_infos[err].fmt, (long)arg1);
          break;
        }

      case ERROR_OFFSET_UNALIGNED:
        {
          as_bad (err_infos[err].fmt, idx, (long)arg1);
          break;
        }
      case ERROR_RELOC_ILLEGAL:
      case ERROR_BAD_END:
      case ERROR_OPERANDS_ILLEGAL:
        {
          as_bad (err_infos[err].fmt, (char *)arg1);
          break;
        }
      case ERROR_REG_OVER_RANGE:
      case ERROR_802J_REG_OVER_RANGE:
      case ERROR_REG_FORMAT:
        {
          as_bad (err_infos[err].fmt, idx, (char *)arg1);
          break;
        }
      case ERROR_UNDEFINE:
        {
          /* Add NULL to fix warnings.  */
          as_bad ((char *)arg1, NULL);
          break;
        }
      case WARNING_BMASKI_TO_MOVI:
        {
          /* Add NULL to fix warnings.  */
          as_warn (err_infos[err].fmt, NULL);
          break;
        }
      case WARNING_IDLY:
          as_warn (err_infos[err].fmt, (long)arg1);
      case WARNING_OPTIONS:
          as_warn (err_infos[err].fmt, (char *)arg1, (char *)arg2);
      default:
        break;
    }
}

static void
csky_branch_report_error(char* file, unsigned int line,
                         symbolS* sy, offsetT val)
{
  as_bad_where (file ? file : _("unknown"),
                line,
                _("pcrel for branch to %s too far (0x%lx)"),
                sy ? S_GET_NAME (sy) : _("<unknown>"),
                (long) val);
}

static char
csky_tolower (char c)
{
  if (c <= 'z' && c >= 'a')
    {
      return c;
    }
  else if (c <= 'Z' && c >= 'A')
    {
      return c - 'A' + 'a';
    }

  return c;
}

static char *
string_tolower(char *str)
{
  char *origin = str;
  while (*str)
    {
      *str = csky_tolower (*str);
      str++;
    }

  return origin;
}

static int
parse_cpu (const char *str)
{
  int i = 0;
  char dup_str[128];
  strcpy(dup_str, str);
  string_tolower(dup_str);
  for (; csky_cpus[i].name != NULL; i++)
    {
      if (memcmp (dup_str, csky_cpus[i].name, strlen(dup_str)) == 0)
        {
          mach_flag |= csky_cpus[i]._mach_flag;
          return 0;
        }
    }
  return 1;
}

static int
parse_arch (const char *str)
{
  int i = 0;
  char dup_str[128];
  strcpy(dup_str, str);
  string_tolower(dup_str);
  for (; csky_archs[i].name != NULL; i++)
    {
      if (memcmp (dup_str, csky_archs[i].name, strlen(dup_str)) == 0)
        {
          arch_flag |= csky_archs[i]._arch_flag;
          return 0;
        }
    }
  return 1;

}

#ifdef OBJ_ELF
const char *
elf32_csky_target_format (void)
{
  return (target_big_endian
          ? "elf32-csky-big"
          : "elf32-csky-little");
}
#endif

/* Turn an integer of n bytes (in val) into a stream of bytes appropriate
   for use in the a.out file, and stores them in the array pointed to by buf.
   This knows about the endian-ness of the target machine and does
   THE RIGHT THING, whatever it is.  Possible values for n are 1 (byte)
   2 (short) and 4 (long)  Floating numbers are put out as a series of
   LITTLENUMS (shorts, here at least).  */

void
md_number_to_chars (char * buf, valueT val, int n)
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

/* Get a log2(val).  */
static int
csky_log_2 (unsigned int val)
{
    int log = -1;
    if ((val & (val - 1)) == 0)
      {
        while (val != 0)
          {
            log ++;
            val >>= 1;
          }
      }
    else
      {
        csky_show_info (ERROR_IMM_POWER, 0, (void *)(long)val, NULL);
      }
    return log;
}
/* output one instruction.  */

static void
csky_write_insn(char * ptr, valueT use, int nbytes)
{
  if(nbytes==2)
    {
      md_number_to_chars (ptr, use, nbytes);
    }
  else  /* 32bit instruction. */
    {
      /* Significant figures are in low bits.*/
      md_number_to_chars(ptr,use>>16,2);
      md_number_to_chars(ptr+2,use&0xFFFF,2);
    }
}

static valueT
csky_read_insn (unsigned char *c,int n)
{
  valueT v=0;
  /* hi/lo byte index in binary stream.  */
  int lo,hi;
  if (target_big_endian)
    {
      hi=0;
      lo=1;
    }
  else
    {
      hi=1;
      lo=0;
    }
  v = c[lo] | (c[hi] << 8);
  if (n == 4)
    {
      v <<= 16;
      v |=  c[lo + 2] | (c[hi + 2] << 8);
    }
  return v;
}

static void
make_name (char *s, const char *p, int n)
{
  static const char hex[] = "0123456789ABCDEF";

  s[0] = p[0];
  s[1] = p[1];
  s[2] = p[2];
  s[3] = hex[(n >> 12) & 0xF];
  s[4] = hex[(n >>  8) & 0xF];
  s[5] = hex[(n >>  4) & 0xF];
  s[6] = hex[(n)       & 0xF];
  s[7] = 0;
}
/* We handle all bad expressions here, so that we can report the faulty
   instruction in the error message.  */
void
md_operand (expressionS * expressionP ATTRIBUTE_UNUSED)
{
  return;
}

/* Under ELF we need to default _GLOBAL_OFFSET_TABLE.
   Otherwise we have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (char * name ATTRIBUTE_UNUSED)
{
#ifdef OBJ_ELF
  /* TODO:  */
#endif

  return NULL;
}

const char *
md_atof (int type ATTRIBUTE_UNUSED, char * litP ATTRIBUTE_UNUSED, int * sizeP ATTRIBUTE_UNUSED)
{
  return ieee_md_atof (type, litP, sizeP, target_big_endian);
}

void
md_show_usage (FILE * fp)
{
  fprintf (fp, "\ncsky as specific options:\n");
  int i = 0;
  for (i = 0; csky_opts[i].name != NULL; i++)
    {
      /* Add NULL to fix warnings.  */
      fprintf (fp, csky_opts[i].help, NULL);
    }
  for (i = 0; csky_long_opts[i].name != NULL; i++)
    {
      /* Add NULL to fix warnings.  */
      fprintf (fp, csky_long_opts[i].help, NULL);
    }
}

/* Add literal pool functions here.  */
static char v1_nop_insn[2][2] =
{
  /* 0x1200 : mov r0, r0.  */
  /* big endian.  */
  {0x12, 0x00},
  /* little endian.  */
  {0x00, 0x12}
};

static void
make_mapping_symbol (map_state state, valueT value, fragS *frag)
{
  symbolS * symbolP;
  const char * symname;
  int type;
  switch (state)
    {
      case MAP_DATA:
        symname = "$d";
        type=BSF_NO_FLAGS;
        break;
      case MAP_TEXT:
        symname = "$t";
        type=BSF_NO_FLAGS;
        break;
      default:
        abort ();
    }

  symbolP = symbol_new (symname, now_seg, value, frag);
  symbol_get_bfdsym (symbolP)->flags |= type | BSF_LOCAL;
}

void mapping_state (map_state state )
{
  map_state current_state = seg_info (now_seg)->tc_segment_info_data.current_state;
 /*   const char * sym;*/

#define TRANSITION(from, to) (current_state == (from) && state == (to))
  if( current_state == state)
    return;
  else if(TRANSITION (MAP_UNDEFINED, MAP_DATA))
    return;
  else if (TRANSITION (MAP_UNDEFINED, MAP_TEXT))
   {
     struct frag * const frag_first = seg_info (now_seg)->frchainP->frch_root;
     const int add_symbol = (frag_now != frag_first)|| (frag_now_fix () > 0);
     if (add_symbol)
       make_mapping_symbol (MAP_DATA, (valueT) 0, frag_first);
   }

  seg_info (now_seg)->tc_segment_info_data.current_state = state;
  make_mapping_symbol (state, (valueT) frag_now_fix (), frag_now);
#undef TRANSITION
}

static void
dump_literals (int isforce)
{
#define CSKYV1_BR_INSN  0xF000
#define CSKYV2_BR_INSN  0x0400
  unsigned int i;
  struct literal * p;
  struct literal * p_tls;
  symbolS * brarsym = NULL;

  if (poolsize == 0)
      return;

  /* Must we branch around the literal table?  */
  if (isforce)
    {
      char brarname[8];
      make_name (brarname, POOL_END_LABEL, poolnumber);
      brarsym = symbol_make (brarname);
      symbol_table_insert (brarsym);
      mapping_state(MAP_TEXT);
      if (IS_CSKY_ARCH_V1 (mach_flag))
        {
          csky_insn.output = frag_var (rs_machine_dependent,
              csky_relax_table[C (UNCD_JUMP_S, DISP32)].rlx_length,
              csky_relax_table[C (UNCD_JUMP_S, DISP12)].rlx_length,
              C (UNCD_JUMP_S, 0), brarsym, 0, 0
              );
          md_number_to_chars (csky_insn.output, CSKYV1_BR_INSN, 2);
        }
      else
        {
          csky_insn.output = frag_var (
              rs_machine_dependent,
              UNCD_DISP16_LEN,
              UNCD_DISP10_LEN,
              UNCD_DISP10,
              brarsym,0,0
              );
          md_number_to_chars (csky_insn.output, CSKYV2_BR_INSN, 2);
        }
    }
  /* Make sure that the section is sufficiently aligned and that
     the literal table is aligned within it.  */

  if (do_pff)
    {
      valueT br_self;
      csky_insn.output = frag_more (2);
      /* .Lxx: br .Lxx  */
      if (IS_CSKY_V1 (mach_flag))
        br_self = CSKYV1_BR_INSN | 0x7ff;
      else
        br_self = CSKYV2_BR_INSN;
      md_number_to_chars (csky_insn.output, br_self, 2);
      if (!isforce)
        {
          csky_insn.output = frag_more (2);
          /* .Lxx: br .Lxx  */
          md_number_to_chars(csky_insn.output, br_self, 2);
        }
    }
  mapping_state (MAP_DATA);

  record_alignment (now_seg, 2);
  if (IS_CSKY_ARCH_V1 (mach_flag))
    frag_align_pattern (2, v1_nop_insn[(target_big_endian ? 0 : 1)], 2, 0);
  else
    frag_align (2, 0, 3);

  colon (S_GET_NAME (poolsym));

  for (i = 0, p = litpool; i < poolsize; i += (p->isdouble ? 2 : 1), p++)
    {
      insn_reloc = p->r_type;
      if (insn_reloc == BFD_RELOC_CKCORE_TLS_IE32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_GD32)
        {
          p_tls = litpool_tls + count_tls;
          p_tls->tls_addend.frag = p->tls_addend.frag;
          p_tls->tls_addend.offset = p->tls_addend.offset;
          literal_insn_offset = p_tls;
          count_tls++;
        }
      if(p->isdouble)
        {
          if (target_big_endian)
            {
              p->e.X_add_number = p->dbnum >> 32;
              emit_expr (& p->e, 4);
              p->e.X_add_number = p->dbnum & 0xffffffff;
              emit_expr (& p->e, 4);
            }
          else
            {
              p->e.X_add_number = p->dbnum & 0xffffffff;
              emit_expr (& p->e, 4);
              p->e.X_add_number = p->dbnum >> 32;
              emit_expr (& p->e, 4);
            }
        }
      else
        emit_expr (& p->e, 4);
    }

  if (isforce && IS_CSKY_ARCH_V2 (mach_flag))
    {
      /* add one nop insn : mov16 r0,r0 as end of literal for disassembler */
      mapping_state(MAP_TEXT);
      csky_insn.output = frag_more (2);
      md_number_to_chars (csky_insn.output, CSKYV2_INST_NOP, 2);
    }

  insn_reloc = BFD_RELOC_NONE;

  if (brarsym != NULL)
    colon (S_GET_NAME (brarsym));
  poolsize = 0;
}

static int
enter_literal (expressionS *e,
               int ispcrel,
               unsigned char isdouble,
               uint64_t dbnum)
{
  unsigned int i;
  struct literal * p;
  if (poolsize >= MAX_POOL_SIZE - 2)
    {
      /* The literal pool is as full as we can handle. We have
         to be 2 entries shy of the 1024/4=256 entries because we
         have to allow for the branch (2 bytes) and the alignment
         (2 bytes before the first insn referencing the pool and
         2 bytes before the pool itself) == 6 bytes, rounds up
         to 2 entries.  */

      /* Save the parsed symbol's reloc.  */
      enum bfd_reloc_code_real last_reloc_before_dump = insn_reloc;
      dump_literals (1);
      insn_reloc = last_reloc_before_dump;
    }

  if (poolsize == 0)
    {
      /* Create new literal pool.  */
      if (++ poolnumber > 0xFFFF)
        as_fatal (_("more than 65K literal pools"));

      make_name (poolname, POOL_START_LABEL, poolnumber);
      poolsym = symbol_make (poolname);
      symbol_table_insert (poolsym);
      poolspan = 0;
    }

  /* Search pool for value so we don't have duplicates.  */
  for (p = litpool,i = 0; i < poolsize; i += (p->isdouble?2:1), p++)
    {
      if (e->X_op == p->e.X_op
          && e->X_add_symbol == p->e.X_add_symbol
          && e->X_add_number == p->e.X_add_number
          && ispcrel == p->ispcrel
          && insn_reloc == p->r_type
          && isdouble == p->isdouble
          && ((insn_reloc != BFD_RELOC_CKCORE_TLS_GD32)
              && (insn_reloc != BFD_RELOC_CKCORE_TLS_LDM32)
              && (insn_reloc != BFD_RELOC_CKCORE_TLS_LDO32)
              && (insn_reloc != BFD_RELOC_CKCORE_TLS_IE32)
              && (insn_reloc != BFD_RELOC_CKCORE_TLS_LE32)))
        {
          p->refcnt ++;
          return i;
        }
    }
  p->refcnt = 1;
  p->ispcrel = ispcrel;
  p->e = *e;
  p->r_type = insn_reloc;
  p->isdouble = isdouble;
  if (isdouble)
    p->dbnum = dbnum;

  if (insn_reloc == BFD_RELOC_CKCORE_TLS_GD32
      || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
      || insn_reloc == BFD_RELOC_CKCORE_TLS_IE32)
    {
      p->tls_addend.frag  = frag_now;
      p->tls_addend.offset = csky_insn.output-frag_now->fr_literal;
      literal_insn_offset = p;
    }
  poolsize += (p->isdouble?2:1);
  return i;
}

static void
check_literals (int kind, int offset)
{
  poolspan += offset;

  /* SPANCLOSE and SPANEXIT are smaller numbers than SPANPANIC.
     SPANPANIC means that we must dump now.
     kind == 0 is any old instruction.
     kind  > 0 means we just had a control transfer instruction.
     kind == 1 means within a function
     kind == 2 means we just left a function

     The dump_literals (1) call inserts a branch around the table, so
     we first look to see if its a situation where we won't have to
     insert a branch (e.g., the previous instruction was an unconditional
     branch).

     SPANPANIC is the point where we must dump a single-entry pool.
     it accounts for alignments and an inserted branch.
     the 'poolsize*2' accounts for the scenario where we do:
       lrw r1,lit1; lrw r2,lit2; lrw r3,lit3
     Note that the 'lit2' reference is 2 bytes further along
     but the literal it references will be 4 bytes further along,
     so we must consider the poolsize into this equation.
     This is slightly over-cautious, but guarantees that we won't
     panic because a relocation is too distant.  */
  if (((poolspan > SPANEXIT) || (do_func_dump)) && kind > 1 && (do_br_dump || do_func_dump))
    dump_literals (0);
  else if (poolspan > SPANCLOSE && (kind > 0) && do_br_dump)
    dump_literals (0);
  else if (poolspan >= (SPANPANIC
                        - (IS_CSKY_ARCH_V1 (mach_flag) ?  poolsize * 2 : 0)))
    dump_literals (1);
  /* Have not dump literal pool before insn1,
   * and will not dump literal pool between insn1 and insnN+1,
   * set poolspan to origin length.*/
  else if (do_noliteraldump == 1)
  {
    poolspan -= offset;
  }

  if (do_noliteraldump == 1)
    do_noliteraldump = 0;
}

/* parse operand's type here.  */

/* Parse operands of the form
   <symbol>@GOTOFF+<nnn>
   and similar .plt or .got references.

   If we find one, set up the correct relocation in RELOC and copy the
   input string, minus the `@GOTOFF' into a malloc'd buffer for
   parsing by the calling routine.  Return this buffer, and if ADJUST
   is non-null set it to the length of the string we removed from the
   input line.  Otherwise return NULL.  */

static char *
lex_got (enum bfd_reloc_code_real *reloc,
         int *adjust)
{
  struct _gotrel
    {
      const char *str;
      const enum bfd_reloc_code_real rel;
    };
  static const struct _gotrel gotrel[] =
    {
        { "GOTOFF",     BFD_RELOC_CKCORE_GOTOFF      },
        { "GOTPC",      BFD_RELOC_CKCORE_GOTPC       },
        { "GOTTPOFF",   BFD_RELOC_CKCORE_TLS_IE32    },
        { "GOT",        BFD_RELOC_CKCORE_GOT32       },
        { "PLT",        BFD_RELOC_CKCORE_PLT32       },
        { "BTEXT",      BFD_RELOC_CKCORE_TOFFSET_LO16},
        { "BDATA",      BFD_RELOC_CKCORE_DOFFSET_LO16},
        { "TLSGD32",    BFD_RELOC_CKCORE_TLS_GD32    },
        { "TLSLDM32",   BFD_RELOC_CKCORE_TLS_LDM32   },
        { "TLSLDO32",   BFD_RELOC_CKCORE_TLS_LDO32   },
        { "TPOFF",      BFD_RELOC_CKCORE_TLS_LE32    }
    };

  char *cp;
  unsigned int j;

  for (cp = input_line_pointer; *cp != '@'; cp++)
    if (is_end_of_line[(unsigned char) *cp])
      return NULL;

  for (j = 0; j < sizeof (gotrel) / sizeof (gotrel[0]); j++)
    {
      int len;

      len = strlen (gotrel[j].str);
      if (strncasecmp (cp + 1, gotrel[j].str, len) == 0)
        {
          if (gotrel[j].rel!= 0)
            {
              *reloc = gotrel[j].rel;
              if (adjust)
                *adjust = len;

              /* input_line_pointer the str pointer
               * after relocation token like @GOTOFF.  */
              input_line_pointer += len + 1;
              return input_line_pointer;
            }

          csky_show_info (ERROR_RELOC_ILLEGAL, 0, (void *)gotrel[j].str, NULL);
          return NULL;
        }
    }

  /* Might be a symbol version string.  Don't as_bad here.  */
  return NULL;
}

static char *
parse_exp (char * s, expressionS * e)
{
  char * save;
  char * new;

  /* Skip whitespace.  */
  while (ISSPACE (* s))
    ++ s;

  save = input_line_pointer;
  input_line_pointer = s;

  insn_reloc = BFD_RELOC_NONE;
  expression (e);
  lex_got(&insn_reloc, NULL);

  if (e->X_op == O_absent)
      errs.err_num = ERROR_MISSING_OPERAND;

  new = input_line_pointer;
  input_line_pointer = save;

  return new;
}

static char *
parse_fexp(s, e, isdouble, dbnum)
    char * s;
    expressionS * e;
    unsigned char isdouble;
    uint64_t * dbnum;
{
  int length;                 /* Number of chars in an object.  */
  register char *err = NULL;  /* Error from scanning floating literal.  */
  char temp[8];

  /* input_line_pointer->1st char of a flonum (we hope!).  */
  input_line_pointer = s;

  if (input_line_pointer[0] == '0'
      && ISALPHA (input_line_pointer[1]))
    input_line_pointer += 2;

  if (isdouble)
    err = md_atof (100, temp, &length);
  else
    err = md_atof (102, temp, &length);
  know (length <= 8);
  know (err != NULL || length > 0);

  if ( !is_end_of_line[(unsigned char) *input_line_pointer])
    as_bad (_("the second operand must be immediate !"));
  while ( !is_end_of_line[(unsigned char) *input_line_pointer])
    input_line_pointer++;

  if (err)
    {
      as_bad (_("bad floating literal: %s"), err);
      as_bad (_("the second operand is illegal!"));
      while ( !is_end_of_line[(unsigned char) *input_line_pointer])
        input_line_pointer++;
      know (is_end_of_line[(unsigned char) input_line_pointer[-1]]);
      return input_line_pointer;
    }

  e->X_add_symbol = 0x0;
  e->X_op_symbol = 0x0;
  e->X_op = O_constant;
  e->X_unsigned = 1;
  e->X_md = 0x0;

  if ( !isdouble)
    {
      if (target_big_endian)
        e->X_add_number = ((temp[0] << 24)&0xffffffff) | ((temp[1] << 16)&0xffffff)
          | ((temp[2] << 8)&0xffff) | (temp[3]&0xff);
      else
        e->X_add_number = ((temp[3] << 24)&0xffffffff) | ((temp[2] << 16)&0xffffff)
          | ((temp[1] << 8)&0xffff) | (temp[0]&0xff);
    }
  else
    {
      if (target_big_endian)
      {
        *dbnum = (((temp[0] << 24)&0xffffffff)  | ((temp[1] << 16)&0xffffff)
          | ((temp[2] << 8)&0xffff) | (temp[3]&0xff));
        *dbnum <<= 32;
        *dbnum |= ((temp[4] << 24)&0xffffffff)  | ((temp[5] << 16)&0xffffff)
          | ((temp[6] << 8)&0xffff) | (temp[7]&0xff);
      }
      else
      {
        *dbnum = ((temp[7] << 24)&0xffffffff)  | ((temp[6] << 16)&0xffffff)
          | ((temp[5] << 8)&0xffff) | (temp[4]&0xff);
        *dbnum <<= 32;
        *dbnum |= (((temp[3] << 24)&0xffffffff)  | ((temp[2] << 16)&0xffffff)
          | ((temp[1] << 8)&0xffff) | (temp[0]&0xff));
      }
    }
  return input_line_pointer;
}

static char *
parse_rt (char *s,
          int ispcrel,
          expressionS *ep,
          long reg ATTRIBUTE_UNUSED)
{
  expressionS e;
  int n;

  if (ep)
    /* Indicate nothing there.  */
    ep->X_op = O_absent;

  if (*s == '[')
    {
      s = parse_exp (s + 1, &e);

      if (*s == ']')
        s++;
      else
        errs.err_num = ERROR_MISSING_RSQUARE_BRACKETS;

      if (ep)
       *ep = e;
    }
  else
    {
      s = parse_exp (s, &e);
      if ((BFD_RELOC_CKCORE_DOFFSET_LO16 == insn_reloc)
           || (BFD_RELOC_CKCORE_TOFFSET_LO16 == insn_reloc))
        {
          if (ep)
            *ep = e;
          return s;
        }
      if (ep)
        *ep = e;
      /* If the instruction has work, literal handling is in the work.  */
      if (!csky_insn.opcode->work)
        {
          n = enter_literal (&e, ispcrel, 0, 0);
          if (ep)
           *ep = e;

          /* Create a reference to pool entry.  */
          ep->X_op = O_symbol;
          ep->X_add_symbol = poolsym;
          ep->X_add_number = n << 2;
        }
    }
  return s;
}
static char *
parse_rtf (char *s, int ispcrel, expressionS *ep)
{
  expressionS e;
  int n = 0;

  if (ep)
    /* Indicate nothing there.  */
    ep->X_op = O_absent;

  if (*s == '[')
  {
    s = parse_exp (s + 1, & e);

    if (*s == ']')
      s++;
    else
      as_bad (_("missing ']'"));

    if (ep)
      *ep = e;
  }
  else
  {
    if ( strstr(csky_insn.opcode->mnemonic, "flrws"))
    {
      uint64_t dbnum;
      s = parse_fexp (s, &e, 0, &dbnum);
      n = enter_literal (& e, ispcrel, 0, dbnum);
    }
    else if ( strstr(csky_insn.opcode->mnemonic, "flrwd"))
    {
      uint64_t dbnum;
      s = parse_fexp (s, &e, 1, &dbnum);
      n = enter_literal (& e, ispcrel, 1, dbnum);
    }
    else
      as_bad (_("Error: no such opcode"));

    if (ep)
      *ep = e;

    /* Create a reference to pool entry.  */
    ep->X_op         = O_symbol;
    ep->X_add_symbol = poolsym;
    ep->X_add_number = n << 2;
  }
  return s;
}

void
md_begin (void)
{
  unsigned int bfd_mach_flag;
  struct _csky_opcode const *opcode;
  struct _csky_macro_info const *macro;
  struct _csky_arch const *p_arch;
  struct _csky_cpu const *p_cpu;

  if (mach_flag != 0)
    {
      if (((mach_flag & CSKY_ARCH_MASK) != arch_flag) && arch_flag != 0)
        as_warn("-mcpu conflict with -march option, actually use -mcpu");
      if (((mach_flag & ~CSKY_ARCH_MASK) != other_flag) && other_flag != 0)
        as_warn("-mcpu conflict with other model parameters, actually use -mcpu");
    }
  else if (arch_flag != 0)
    {
      mach_flag |= arch_flag | other_flag;
    }
  else
    {
#if _CSKY_ABI==1
      mach_flag |= CSKY_ARCH_610 | other_flag;
#else
      mach_flag |= CSKY_ARCH_810 | other_flag;
#endif
    }

  if (((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_610) ||
      ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_510))
    {
      if ((mach_flag & CSKY_ARCH_MP) && (mach_flag & CSKY_ARCH_MAC))
        {
          as_fatal("520/620 is conflict with -mmp option");
        }
      else if ((mach_flag & CSKY_ARCH_MP) && (mach_flag & CSKY_ARCH_DSP))
        {
          as_fatal("510e/610e is conflict with -mmp option");
        }
      else if ((mach_flag & CSKY_ARCH_DSP) && (mach_flag & CSKY_ARCH_MAC))
        {
          as_fatal("520/620 is conflict with 510e/610e or -dsp option");
        }
    }
  if (((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_510)
      && (mach_flag & CSKY_ARCH_FLOAT))
    {
      mach_flag = (mach_flag & (~CSKY_ARCH_MASK));
      mach_flag |= CSKY_ARCH_610;
    }

  /* Find bfd_mach_flag, it will set to bfd backend data.  */
  for (p_arch = csky_archs; p_arch->_arch_flag != 0; p_arch++)
    {
      if ((mach_flag & CSKY_ARCH_MASK) == p_arch->_arch_flag)
        {
          bfd_mach_flag =  p_arch->_bfd_mach_flag;
          break;
        }
    }
  /* Find isa_flag.  */
  for (p_cpu = csky_cpus; p_cpu->_mach_flag != 0; p_cpu++)
    {
      if ((mach_flag & CPU_ARCH_MASK)== p_cpu->_mach_flag)
        {
          isa_flag = p_cpu->_isa_flag;
          break;
        }
    }

  if (do_use_branchstub == -1)
    {
      if (IS_CSKY_ARCH_V1 (mach_flag))
        do_use_branchstub = 0;
      else
        do_use_branchstub = 1;
    }
  else if (do_use_branchstub == 1)
    {
      if (IS_CSKY_ARCH_V1 (mach_flag))
        {
          as_warn ("csky abiv1(ck510/ck610) do not support -mbranch-stub.");
          do_use_branchstub = 0;
        }
      else if (do_force2bsr == 0)
        {
          as_warn ("can not use -mbranch-stub and -mforce2bsr at the same time, actually use -mbranch-stub.");
          do_force2bsr = 1;
        }
    }
  /* set do_force2bsr flag.  */
  if (do_force2bsr == -1)
    {
      if (!do_use_branchstub
          && (IS_CSKY_ARCH_V1 (mach_flag)
              || ((CSKY_ARCH_MASK & mach_flag) == CSKY_ARCH_810)
              || ((CSKY_ARCH_MASK & mach_flag) == CSKY_ARCH_807)))
        {
          do_force2bsr = 0;
        }
      else
        {
          do_force2bsr = 1;
        }
    }
  else if (do_force2bsr
          && (((CSKY_ARCH_MASK & mach_flag) == CSKY_ARCH_801)
              || ((CSKY_ARCH_MASK & mach_flag) == CSKY_ARCH_802)))
    {
      /* 802/803 is not support.  */
      csky_show_info (WARNING_OPTIONS, 0, (void *)"-mfroce2bsr", (void *)"ck801/ck802");
      do_force2bsr = 0;
    }

  if(do_pff == -1)
    {
      if(IS_CSKY_ARCH_V1 (mach_flag))
        {
          do_pff = 1;
        }
      else
        {
          do_pff = 0;
        }
    }

  if (do_extend_lrw == -1)
    {
      if ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_801)
        do_extend_lrw = 1;
      else
        do_extend_lrw = 0;
    }
  if (do_intr_stack == -1)
    {
      /* control interrupt stack module, 801&802&803&803S default open,
         807&810, default close.  */
      if ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_807
          || (mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_810)
        do_intr_stack = 0;
      else
        do_intr_stack = 1;
    }
  /* TODO: add isa_flag(SIMP/CACHE/APS).  */
  isa_flag |=  (mach_flag & CSKY_ARCH_MAC) ? CSKY_ISA_MAC : 0;
  isa_flag |=  (mach_flag & CSKY_ARCH_MP) ? CSKY_ISA_MP : 0;
  isa_flag |=  (mach_flag & CSKY_ARCH_CP) ? CSKY_ISA_CP : 0;

  /* Set abi flag and get table address.  */
  if (IS_CSKY_ARCH_V1 (mach_flag))
    {
      mach_flag = mach_flag | CSKY_ABI_V1;
      opcode = csky_v1_opcodes;
      macro = v1_macros_table;
      SPANPANIC = v1_SPANPANIC;
      SPANCLOSE = v1_SPANCLOSE;
      SPANEXIT  = v1_SPANEXIT;
      md_relax_table = csky_relax_table;
    }
  else
    {
      mach_flag = mach_flag | CSKY_ABI_V2;
      opcode = csky_v2_opcodes;
      macro = v2_macros_table;
      SPANPANIC = v2_SPANPANIC;
      if (do_extend_lrw)
        {
          SPANCLOSE = v2_SPANCLOSE_ELRW;
          SPANEXIT  = v2_SPANEXIT_ELRW;
        }
      else
        {
          SPANCLOSE = v2_SPANCLOSE;
          SPANEXIT  = v2_SPANEXIT;
        }
      md_relax_table = csky_relax_table;
    }

  /* Establish hash table for opcodes and macros.  */
  csky_macros_hash = hash_new ();
  csky_opcodes_hash = hash_new ();
  for ( ; opcode->mnemonic != NULL; opcode++)
    {
      if ((isa_flag & (opcode->isa_flag16 | opcode->isa_flag32)) != 0)
        hash_insert (csky_opcodes_hash, opcode->mnemonic, (char *)opcode);
    }
  for ( ; macro->name != NULL; macro++)
    {
      if ((isa_flag & macro->isa_flag) != 0)
        hash_insert (csky_macros_hash, macro->name, (char *)macro);
    }
  if (do_nolrw && ((isa_flag & CSKYV2_ISA_1E2) != 0))
    hash_insert (csky_macros_hash,
                 v2_lrw_macro_opcode.name,
                 (char *)&v2_lrw_macro_opcode);
  /* Set e_flag to ELF Head.  */
  bfd_set_private_flags (stdoutput, mach_flag);
  /* Set bfd_mach to bfd backend data.  */
  bfd_set_arch_mach (stdoutput, bfd_arch_csky, bfd_mach_flag);
}

/* operand type is core register.  */
static bfd_boolean
parse_type_ctrlreg (char** oper)
{
  const char **regs;
  int i;
  int len;
  regs = csky_ctrl_reg;
  for (i = 0; i < (int)(sizeof (csky_ctrl_reg) / sizeof (char *)); i++)
    {
      len = strlen (regs[i]);
      if (memcmp (*oper, regs[i], len) == 0
          && !ISALNUM(*(*oper + len)))
        {
          *oper += len;
          csky_insn.opcode_end = *oper;
          break;
        }
    }

  if (i == sizeof (csky_ctrl_reg) / sizeof (char *))
    {
      regs = csky_ctrl_reg_alias;
      for (i = 0; i < (int)(sizeof (csky_ctrl_reg_alias) / sizeof (char *)); i++)
        {
          len = strlen (regs[i]);
          if (memcmp (*oper, regs[i], len) == 0)
            {
              *oper += len;
              csky_insn.opcode_end = *oper;
              break;
            }
        }
    }
  if (IS_CSKY_V2 (mach_flag))
    {
      char *s = *oper;
      int crx;
      int sel;
      if (i != sizeof (csky_ctrl_reg) / sizeof (char *))
        {
          crx = i;
          sel = 0;
        }
      else
        {
          if (s[0] == 'c' && s[1] == 'r')
            {
              s += 2;
              if (*s == '<')
                {
                  s++;
                  if (s[0] == '3' && s[1] >= '0' && s[1] <= '1')
                    {
                      crx = 30 + s[1] - '0';
                      s += 2;
                    }
                  else if (s[0] == '2' && s[1] >= '0' && s[1] <= '9')
                    {
                      crx = 20 + s[1] - '0';
                      s += 2;
                    }
                  else if (s[0] == '1' && s[1] >= '0' && s[1] <= '9')
                    {
                      crx = 10 + s[1] - '0';
                      s += 2;
                    }
                  else if (s[0] >= '0' && s[0] <= '9')
                    {
                      crx = s[0] - '0';
                      s += 1;
                    }
                  else
                    {
                      errs.err_num = ERROR_REG_OVER_RANGE;
                      errs.arg1 = (void *) "control";
                      return FALSE;
                    }
                  if (*s == ',')
                    s++;
                  else
                    {
                      errs.err_num = ERROR_CREG_ILLEGAL;
                      return FALSE;
                    }
                  char *pS = s;
                  while (*pS != '>' && !is_end_of_line[(unsigned char) *pS])
                    pS++;
                  if (*pS == '>')
                      *pS = '\0';
                  else
                    {
                      /* Error. Missing '>'.  */
                      errs.err_num = ERROR_MISSING_RANGLE_BRACKETS;
                      return FALSE;
                    }
                  expressionS e;
                  s = parse_exp (s, &e);
                  if (e.X_op == O_constant
                      && e.X_add_number >= 0
                      && e.X_add_number <= 31)
                    {
                      *oper = s;
                      sel = e.X_add_number;
                    }
                  else
                    return FALSE;
                }
              else
                {
                  /* Error. Missing '<'.  */
                  errs.err_num = ERROR_MISSING_LANGLE_BRACKETS;
                  return FALSE;
                }
            }
          else
            return FALSE;
        }
        i = (sel << 5) | crx;
    }
  csky_insn.val[csky_insn.idx++] = i;
  return TRUE;
}

static bfd_boolean
is_reg_sp_with_bracket (char **oper)
{
  const char **regs;
  int sp_idx;
  int len;
  if (IS_CSKY_V1 (mach_flag))
      sp_idx = 0;
  else
      sp_idx = 14;

  if (**oper != '(')
      return FALSE;
  *oper += 1;
  regs = csky_general_reg;
  len = strlen (regs[sp_idx]);
  if (memcmp (*oper, regs[sp_idx], len) == 0)
    {
      *oper += len;
      if (**oper != ')')
        {
          return FALSE;
        }
      *oper += 1;
      csky_insn.val[csky_insn.idx++] = sp_idx;
      return TRUE;
    }
  else
    {
      if (IS_CSKY_V1 (mach_flag))
        regs = cskyv1_general_alias_reg;
      else
        regs = cskyv2_general_alias_reg;
      len = strlen (regs[sp_idx]);
      if (memcmp (*oper, regs[sp_idx], len) == 0)
        {
          *oper += len;
          if (**oper != ')')
            {
              return FALSE;
            }
          *oper += 1;
          return TRUE;
        }
    }
  return FALSE;
}

static bfd_boolean
is_reg_sp (char **oper)
{
  const char **regs;
  int sp_idx;
  int len;
  if (IS_CSKY_V1 (mach_flag))
      sp_idx = 0;
  else
      sp_idx = 14;

  regs = csky_general_reg;
  len = strlen (regs[sp_idx]);
  if (memcmp (*oper, regs[sp_idx], len) == 0)
    {
      *oper += len;
      csky_insn.val[csky_insn.idx++] = sp_idx;
      return TRUE;
    }
  else
    {
      if (IS_CSKY_V1 (mach_flag))
        regs = cskyv1_general_alias_reg;
      else
        regs = cskyv2_general_alias_reg;
      len = strlen (regs[sp_idx]);
      if (memcmp (*oper, regs[sp_idx], len) == 0)
        {
          *oper += len;
          csky_insn.val[csky_insn.idx++] = sp_idx;
          return TRUE;
        }
    }
  return FALSE;
}
static int
csky_get_reg_val (char *str, int *len)
{
  int reg = 0;
  if (TOLOWER(str[0]) == 'r')
    {
      if (ISALNUM (str[1]) && ISALNUM (str[2]))
        {
          reg = (str[1] - '0') * 10 + str[2] - '0';
          *len = 3;
        }
      else if (ISALNUM (str[1]))
        {
          reg = str[1] - '0';
          *len = 2;
        }
      else
        return -1;
    }
  else if (TOLOWER (str[0]) == 's' && TOLOWER (str[1]) == 'p'
           && !ISALNUM (str[2]))
    {
      /* sp.  */
      if (IS_CSKY_V1 (mach_flag))
        reg = 0;
      else
        reg = 14;
      *len = 2;
    }
  else if (TOLOWER (str[0]) == 'g' && TOLOWER (str[1]) == 'b'
           && !ISALNUM (str[2]))
    {
      /* gb.  */
      if (IS_CSKY_V1 (mach_flag))
        reg = 14;
      else
        reg = 28;
      *len = 2;
    }
  else if (TOLOWER (str[0]) == 'l' && TOLOWER (str[1]) == 'r'
           && !ISALNUM (str[2]))
    {
      /* lr.  */
      reg = 15;
      *len = 2;
    }
  else if (TOLOWER (str[0]) == 't' && TOLOWER (str[1]) == 'l'
           && TOLOWER (str[2]) == 's' && !ISALNUM (str[3]))
    {
      /* tls.  */
      if (IS_CSKY_V2 (mach_flag))
        reg = 31;
      else
        return -1;
      *len = 3;
    }
  else if (TOLOWER (str[0]) == 's' && TOLOWER (str[1]) == 'v'
           && TOLOWER (str[2]) == 'b' && TOLOWER (str[3]) == 'r')
    {
      if (IS_CSKY_V2 (mach_flag))
        reg = 30;
      else
        return -1;
      *len = 4;
    }
  else if (TOLOWER (str[0]) == 'a')
    {
      if (ISALNUM (str[1]) && !ISALNUM (str[2]))
        {
          if (IS_CSKY_V1 (mach_flag) && (str[1] - '0') <= 5)
            /* a0 - a5.  */
            reg = 2 + str[1] - '0';
          else if (IS_CSKY_V2 (mach_flag) && (str[1] - '0') <= 3)
            /* a0 - a3.  */
            reg = str[1] - '0';
          else
            return -1;
          *len = 2;
        }
    }
  else if (TOLOWER (str[0]) == 't')
    {
      if (IS_CSKY_V2 (mach_flag))
        {
          reg = atoi (str + 1);
          if (reg > 9)
            return -1;

          if (reg > 1)
            /* t2 - t9.  */
            reg = reg + 16;
          else
            /* t0 - t1.  */
            reg = reg + 12;
          *len = 2;
        }
    }
  else if (TOLOWER (str[0]) == 'l')
    {
      if (str[1] < '0' || str[1] > '9')
        return -1;
      if (IS_CSKY_V2 (mach_flag))
        {
          reg = atoi (str + 1);
          if (reg > 9)
            return -1;
          if (reg > 7)
            /* l8 - l9.  */
            reg = reg + 8;
          else
            /* l0 - l7.  */
            reg = reg + 4;
        }
      else
        {
          reg = atoi (str + 1);
          if (reg > 5)
            return -1;
            /* l0 - l6 -> r8 - r13.  */
            reg = reg + 8;
        }
      *len = 2;
    }
  else
    return -1;

  return reg;
}

static int
csky_get_freg_val (char *str, int *len)
{
  int reg = 0;
  char *s = NULL;
  if ((str[0] == 'v' || (str[0] == 'f'))
       && (str[1] == 'r'))
    {
      /* It is fpu register.  */
      s = &str[2];
      while (ISALNUM(*s))
        {
          reg = reg * 10 + (*s) - '0';
          s++;
        }
      if (reg > 31)
        {
          return -1;
        }
    }
  else
    {
      return -1;
    }
  *len = s - str;
  return reg;
}

static bfd_boolean
is_reglist_legal (char **oper)
{
  int reg1 = -1;
  int reg2 = -1;
  int len = 0;
  reg1 = csky_get_reg_val  (*oper, &len);
  *oper += len;

  if (reg1 == -1
      || (IS_CSKY_V1(mach_flag)
          && (reg1 == 0 || reg1 == 15)))
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The first reg must not be r0/r15";
      return FALSE;
    }

  if (**oper != '-')
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The operand format must be rx-ry";
      return FALSE;
    }
  *oper += 1;

  reg2 = csky_get_reg_val  (*oper, &len);
  *oper += len;

  if (reg2 == -1
      || (IS_CSKY_V1(mach_flag)
          && (reg1 == 15)))
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The operand format must be r15 in C-SKY V1";
      return FALSE;
    }
  if (IS_CSKY_V2(mach_flag))
    {
      if (reg2 < reg1)
        {
          errs.err_num = ERROR_REG_FORMAT;
          errs.arg1 = (void *)"The operand format must be rx-ry(rx < ry)";
          return FALSE;
        }
      reg2 = reg2 - reg1;
      reg1 <<= 5;
      reg1 |= reg2;
    }
  csky_insn.val[csky_insn.idx++] = reg1;
  return TRUE;
}

static bfd_boolean
is_freglist_legal (char **oper)
{
  int reg1 = -1;
  int reg2 = -1;
  int len = 0;
  reg1 = csky_get_freg_val  (*oper, &len);
  *oper += len;

  if (reg1 == -1)
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The fpu register format is not recognized.";
      return FALSE;
    }

  if (**oper != '-')
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The operand format must be vrx-vry/frx-fry.";
      return FALSE;
    }
  *oper += 1;

  reg2 = csky_get_freg_val  (*oper, &len);
  *oper += len;

  if (reg2 == -1)
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The fpu register format is not recognized.";
      return FALSE;
    }
  if (reg2 < reg1)
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The operand format must be rx-ry(rx < ry)";
      return FALSE;
    }
  reg2 = reg2 - reg1;
  reg2 <<= 4;
  reg1 |= reg2;
  csky_insn.val[csky_insn.idx++] = reg1;
  return TRUE;
}

static bfd_boolean
is_reglist_dash_comma_legal (char **oper, struct operand *oprnd)
{
  int reg1 = -1;
  int reg2 = -1;
  int len = 0;
  int list = 0;
  int flag = 0;
  int temp = 0;
  while (**oper != '\n' && **oper != '\0')
    {
      reg1 = csky_get_reg_val  (*oper, &len);
      if (reg1 == -1)
        {
          errs.err_num = ERROR_REG_LIST;
          return FALSE;
        }
      flag |= (1 << reg1);
      *oper += len;
      if (**oper == '-')
        {
          *oper += 1;
          reg2 = csky_get_reg_val  (*oper, &len);
          if (reg2 == -1)
            {
              errs.err_num = ERROR_REG_LIST;
              return FALSE;
            }
          *oper += len;
          if (reg1 > reg2)
            {
              errs.err_num = ERROR_REG_LIST;
              return FALSE;
            }
          while (reg2 >= reg1)
            {
              flag |= (1 << reg2);
              reg2--;
            }
        }
      if (**oper == ',')
        *oper += 1;
    }
  /* The reglist: r4-r11, r15, r16-r17, r28.  */
#define REGLIST_BITS         0x10038ff0
  if (flag & ~(REGLIST_BITS))
    {
      errs.err_num = ERROR_REG_LIST;
      return FALSE;
    }
  /* Check r4-r11.  */
  int i = 4;
  while (i <= 11)
    {
      if (flag & (1 << i))
        temp = i - 4 + 1;
      i++;
    }
  list |= temp;

  /* Check r15.  */
  if (flag & (1 << 15))
    list |= (1 << 4);

  /* Check r16-r17.  */
  i = 16;
  temp = 0;
  while (i <= 17)
    {
      if (flag & (1 << i))
        temp = i - 16 + 1;
      i++;
    }
  list |= (temp << 5);

  /* Check r28.  */
  if (flag & (1 << 28))
    list |= (1 << 8);
  if ((oprnd->mask == OPRND_MASK_0_4)
      && (list & ~OPRND_MASK_0_4))
    {
      errs.err_num = ERROR_REG_LIST;
      return FALSE;
    }
  csky_insn.val[csky_insn.idx++] = list;
  return TRUE;
}

static bfd_boolean
is_reg_lshift_illegal (char **oper, int is_float)
{
  int value;
  int len;
  int reg;
  reg = csky_get_reg_val  (*oper, &len);
  if (reg == -1)
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The register must be r0-r31.";
      return FALSE;
    }

  *oper += len;
  if ((*oper)[0] != '<' || (*oper)[1] != '<')
    {
      errs.err_num = ERROR_UNDEFINE;
      errs.arg1 = (void *)"Operand format is error. The format is (rx, ry << n)";
    }
  *oper += 2;

  expressionS e;
  char *new_oper = parse_exp (*oper, &e);
  if (e.X_op == O_constant)
    {
      *oper = new_oper;
      /* The immediate must be in [0, 3].  */
      if ((e.X_add_number < 0
           || e.X_add_number > 3))
        {
          if (errs.err_num > ERROR_IMM_OVERFLOW)
            {
              errs.err_num = ERROR_IMM_OVERFLOW;
              return FALSE;
            }
        }
    }
  else
    {
      errs.err_num = ERROR_EXP_CONSTANT;
      return FALSE;
    }
  if (is_float)
    value = (reg << 2) | e.X_add_number;
  else
    value = (reg << 5) | (1 << e.X_add_number);
  csky_insn.val[csky_insn.idx++] = value;

  return TRUE;
}

static bfd_boolean
is_imm_over_range (char **oper, int min, int max, int ext)
{
  expressionS e;
  bfd_boolean ret = FALSE;
  char *new_oper = parse_exp (*oper, &e);
  if (e.X_op == O_constant)
    {
      ret = TRUE;
      *oper = new_oper;
      if (((int)e.X_add_number != ext)
          && (e.X_add_number < min
              || e.X_add_number > max))
        {
          ret = FALSE;
          if (errs.err_num > ERROR_IMM_OVERFLOW)
            {
              errs.err_num = ERROR_IMM_OVERFLOW;
            }
        }
      csky_insn.val[csky_insn.idx++] = e.X_add_number;
    }

  return ret;
}

static bfd_boolean
is_oimm_over_range (char **oper, int min, int max)
{
  expressionS e;
  bfd_boolean ret = FALSE;
  char *new_oper = parse_exp (*oper, &e);
  if (e.X_op == O_constant)
    {
      ret = TRUE;
      *oper = new_oper;
      if (e.X_add_number < min || e.X_add_number > max)
        {
          ret = FALSE;
        }
      csky_insn.val[csky_insn.idx++] = e.X_add_number - 1;
    }

  return ret;
}

static bfd_boolean
is_psr_bit (char **oper)
{
  const struct psrbit *bits;
  int i = 0;
  if (IS_CSKY_V1 (mach_flag))
    {
      bits = cskyv1_psr_bits;
    }
  else
    {
      bits = cskyv2_psr_bits;
    }

  while (bits[i].name != NULL)
    {
      if (memcmp (*oper, bits[i].name, 2) == 0)
        {
          *oper += 2;
          csky_insn.val[csky_insn.idx] |= bits[i].value;
          return TRUE;
        }
      i++;
    }
  return FALSE;
}

static bfd_boolean
parse_type_cpidx (char** oper)
{
  char *s = *oper;
  int idx;
  if (s[0] == 'c' && s[1] == 'p')
    {
      if ( ISALNUM (s[2])
          && ISALNUM (s[3])
          && ! ISALNUM (s[4]))
        {
          idx = (s[2] - '0') * 10 + s[3] - '0';
          *oper += 4;
        }
      else if (ISALNUM (s[2]) && !ISALNUM (s[3]))
        {
          idx = s[2] - '0';
          *oper += 3;
        }
      else
        {
          return FALSE;
        }
    }
  else
    {
      expressionS e;
      *oper = parse_exp (*oper, &e);
      if (e.X_op != O_constant)
        {
          /* Can not recognize the operand.  */
          return FALSE;
        }
      idx = e.X_add_number;
    }

  csky_insn.val[csky_insn.idx++] = idx;

  return TRUE;
}

static bfd_boolean
parse_type_cpreg (char** oper)
{
  const char **regs;
  int i;
  int len;
  regs = csky_cp_reg;
  for (i = 0; i < (int)(sizeof (csky_cp_reg) / sizeof (char *)); i++)
    {
      len = strlen (regs[i]);
      if (memcmp (*oper, regs[i], len) == 0
          && !ISALNUM(*(*oper + len)))
        {
          *oper += len;
          csky_insn.val[csky_insn.idx++] = i;
          return TRUE;
        }
    }
  errs.err_num = ERROR_CPREG_ILLEGAL;
  errs.arg1 = *oper;
  return FALSE;
}

static bfd_boolean
parse_type_cpcreg (char** oper)
{
  const char **regs;
  int i;
  int len;
  regs = csky_cp_creg;
  for (i = 0; i < (int)(sizeof (csky_cp_creg) / sizeof (char *)); i++)
    {
      len = strlen (regs[i]);
      if (memcmp (*oper, regs[i], len) == 0
          && !ISALNUM(*(*oper + len)))
        {
          *oper += len;
          csky_insn.val[csky_insn.idx++] = i;
          return TRUE;
        }
    }
  errs.err_num = ERROR_CPREG_ILLEGAL;
  errs.arg1 = *oper;
  return FALSE;
}

static bfd_boolean
parse_type_areg (char** oper)
{
  int i = 0;
  int len = 0;
  i = csky_get_reg_val (*oper, &len);
  if (i == -1)
    {
      if (errs.err_num > ERROR_REG_FORMAT)
        {
          errs.err_num = ERROR_REG_FORMAT;
          errs.arg1 = (char *) *oper;
        }
      return FALSE;
    }
  /* It check register range for ck802j.  */
  if ((mach_flag & CSKY_ARCH_MASK) ==  CSKY_ARCH_802
      && mach_flag & CSKY_ISA_JAVA)
    {
      if (i > 15
          && !(i == 30 || i == 23 || i == 24 || i == 25))
        {
          errs.err_num = ERROR_802J_REG_OVER_RANGE;
          errs.arg1 = (char *) csky_general_reg [i];
          return FALSE;
        }
    }
  *oper += len;
  csky_insn.val[csky_insn.idx++] = i;

  return TRUE;
}
static bfd_boolean
parse_type_freg (char** oper, int even)
{
  int reg;
  int len;
  reg = csky_get_freg_val (*oper, &len);
  if (reg == -1)
    {
      errs.err_num = ERROR_REG_FORMAT;
      errs.arg1 = (void *)"The fpu register format is not recognized.";
      return FALSE;
    }
  *oper += len;
  csky_insn.opcode_end = *oper;
  if (even && reg & 0x1)
    {
      errs.err_num = ERROR_EXP_EVEN_FREG;
      return FALSE;
    }
  csky_insn.val[csky_insn.idx++] = reg;
  return TRUE;
}

static bfd_boolean
parse_ldst_imm (char **oper, struct _csky_opcode_info *op, struct operand *oprnd)
{
  unsigned int mask = oprnd->mask;
  int max = 1;
  int shift = 0;

#define IS_32_OPCODE(opcode)      (opcode & 0xc0000000)
#define V1_GET_LDST_SHIFT(opcode, shift)                 \
      shift = v1_shift_tab[((opcode >> 13) & 0x3 )]
#define V2_GET_LDST_SHIFT(opcode, shift)            \
    {                                               \
      if (IS_32_OPCODE (opcode))                    \
        shift = ((opcode & 0x3000) >> 12);          \
      else                                          \
        shift = ((opcode & 0x1800) >> 11);          \
      shift = (shift > 2 ? 2 : shift);              \
    }

  if (**oper == ')' || is_end_of_line[**oper])
    {
      /* the second value is (rx).  */
      csky_insn.val[csky_insn.idx++] = 0;

      return TRUE;
    }
  if (IS_CSKY_V1 (mach_flag))
    {
      static int v1_shift_tab[] = {2, 0, 1};
      V1_GET_LDST_SHIFT(op->opcode, shift);
    }
  else
    {
      V2_GET_LDST_SHIFT (op->opcode, shift);
    }

  while (mask)
    {
      if (mask & 1)
        {
          max <<= 1;
        }
      mask >>= 1;
    }
  max = max << shift;

  expressionS e;
  *oper = parse_exp (*oper, &e);
  if (e.X_op != O_constant)
    {
      /* Not a constant.  */
      return FALSE;
    }
  else if (e.X_add_number < 0 || e.X_add_number >= max)
    {
      /* Out of range.  */
      errs.err_num = ERROR_REG_OVER_RANGE;
      return FALSE;
    }
  if ((e.X_add_number % (1 << shift)) != 0)
    {
      /* Not aligned.  */
      errs.err_num = ERROR_OFFSET_UNALIGNED;
      errs.arg1 = (void *)((unsigned long)1 << shift);
      return FALSE;
    }

  csky_insn.val[csky_insn.idx++] = e.X_add_number >> shift;

  return TRUE;

}

static bfd_boolean
parse_fldst_imm (char **oper, struct _csky_opcode_info *op, struct operand *oprnd)
{
  unsigned int mask = oprnd->mask;
  int max = 1;
  int shift = 0;

  /* vldq/vstq, shift is 4 bits.
     flds/fsts/fldd/fstdd, shift is 2 bits.  */
#define V2_GET_FLDST_SHIFT(opcode, shift)             \
    switch (opcode & 0x0f00ff00)                      \
      {                                               \
        case 0x08002400:                              \
        case 0x08002500:                              \
        case 0x08002600:                              \
        case 0x08002c00:                              \
        case 0x08002d00:                              \
        case 0x08002e00:                              \
            shift = 4;                                \
            break;                                    \
        case 0x04002000:                              \
        case 0x04002100:                              \
        case 0x04002400:                              \
        case 0x04002500:                              \
            shift = 2;                                \
            break;                                    \
        default:                                      \
            shift = 3;                                \
            break;                                    \
      }

  if (IS_CSKY_V1 (mach_flag))
    ;
  else
    {
      V2_GET_FLDST_SHIFT (op->opcode, shift);
    }

  while (mask)
    {
      if (mask & 1)
        {
          max <<= 1;
        }
      mask >>= 1;
    }
  max = max << shift;
  expressionS e;
  *oper = parse_exp (*oper, &e);
  if (e.X_op != O_constant)
    {
      /* Not a constant.  */
      return FALSE;
    }
  else if (e.X_add_number < 0 || e.X_add_number >= max)
    {
      /* Out of range.  */
      errs.err_num = ERROR_REG_OVER_RANGE;
      return FALSE;
    }
  if ((e.X_add_number % (1 << shift)) != 0)
    {
      /* Not aligned.  */
      errs.err_num = ERROR_OFFSET_UNALIGNED;
      errs.arg1 = (void *)((unsigned long)1 << shift);
      return FALSE;
    }

  csky_insn.val[csky_insn.idx++] = e.X_add_number >> shift;

  return TRUE;

}

static unsigned int
csky_count_operands (char *str)
{
  char *oper_end = str;
  unsigned int oprnd_num;
  int bracket_cnt = 0;

  if (is_end_of_line[(unsigned char) *oper_end])
    oprnd_num = 0;
  else
    oprnd_num = 1;

  /* Count how many operands.  */
  if (oprnd_num)
    {
      while (!is_end_of_line[(unsigned char) *oper_end])
        {
          if (*oper_end == '(' || *oper_end == '<')
            {
              bracket_cnt++;
              oper_end++;
              continue;
            }
          if (*oper_end == ')' || *oper_end == '>')
            {
              bracket_cnt--;
              oper_end++;
              continue;
            }
          if (!bracket_cnt && *oper_end == ',')
            oprnd_num++;
          oper_end++;
        }
    }
  return oprnd_num;
}

static bfd_boolean
parse_opcode (char *str)
{
#define IS_OPCODE32F(a) (*(a - 2) == '3' && *(a - 1) == '2')
#define IS_OPCODE16F(a) (*(a - 2) == '1' && *(a - 1) == '6')

  /* TRUE if this opcode has a suffix, like 'lrw.h'.  */
  unsigned int has_suffix = FALSE;
  unsigned int nlen = 0;
  char *opcode_end;
  char name[OPCODE_MAX_LEN + 1];
  char macro_name[OPCODE_MAX_LEN + 1];

  /* Remove space ahead of string.  */
  while (ISSPACE (*str))
    str++;
  opcode_end = str;
  /* Find the opcode end.  */
  while (nlen < OPCODE_MAX_LEN
         && !is_end_of_line [(unsigned char) *opcode_end]
         && *opcode_end != ' ')
    {
      /* Is csky force 32 or 16 instruction?  */
      if ((IS_CSKY_V2 (mach_flag))
          && (*opcode_end == '.')
          && (has_suffix == FALSE))
        {
          has_suffix = TRUE;
          if (IS_OPCODE32F (opcode_end))
            {
              csky_insn.flag_force = INSN_OPCODE32F;
              nlen -= 2;
            }
          else if (IS_OPCODE16F (opcode_end))
            {
              csky_insn.flag_force = INSN_OPCODE16F;
              nlen -= 2;
            }
        }
      name[nlen] = *opcode_end;
      nlen++;
      opcode_end++;
    }

  /* Is csky force 32 or 16 instruction?  */
  if ((IS_CSKY_V2 (mach_flag)) && (has_suffix == FALSE))
    {
      if (IS_OPCODE32F (opcode_end))
        {
          csky_insn.flag_force = INSN_OPCODE32F;
          nlen -= 2;
        }
      else if (IS_OPCODE16F (opcode_end))
        {
          csky_insn.flag_force = INSN_OPCODE16F;
          nlen -= 2;
        }
    }
  name[nlen] = '\0';

  /* Generate macro_name for finding hash in macro hash_table.  */
  if (has_suffix == TRUE)
    nlen += 2;
  strncpy (macro_name, str, nlen);
  macro_name[nlen] = '\0';

  /* Get csky_insn.opcode_end.  */
  while(ISSPACE (*opcode_end))
    opcode_end++;
  csky_insn.opcode_end = opcode_end;

  /* Count the operands.  */
  csky_insn.number = csky_count_operands(opcode_end);

  /* Find hash by name in csky_macros_hash and csky_opcodes_hash.  */
  csky_insn.macro = (struct _csky_macro_info *) hash_find (csky_macros_hash,
                                                    macro_name);
  csky_insn.opcode = (struct _csky_opcode *) hash_find (csky_opcodes_hash, name);

  if ((csky_insn.macro == NULL) && (csky_insn.opcode == NULL))
    {
      return FALSE;
    }
  return TRUE;
}

static bfd_boolean
get_operand_value (struct _csky_opcode_info *op, char **oper, struct operand *oprnd)
{
  struct soperand *soprnd = NULL;
  if (oprnd->mask == (int)HAS_SUB_OPERAND)
    {
      /* It has sub operand, it must be like:
         (oprnd1, oprnd2)
         or
         <oprnd1, oprnd2>
         We will check the format here.  */
      soprnd = (struct soperand *) oprnd;
      char lc;
      char rc;
      char *s = *oper;
      if (oprnd->type == OPRND_TYPE_BRACKET)
        {
          lc = '(';
          rc = ')';
        }
      else if (oprnd->type == OPRND_TYPE_ABRACKET)
        {
          lc = '<';
          rc = '>';
        }

      if (**oper == lc)
        *oper += 1;
      else
        {
          errs.err_num = oprnd->type == OPRND_TYPE_BRACKET ?
            ERROR_MISSING_LBRACHKET : ERROR_MISSING_LANGLE_BRACKETS;
          return FALSE;
        }

      /* If the oprnd2 is an immediate, it can not be parsed
         that end with ')'/'>'.  Modify ')'/'>' to '\0'.  */
      while (*s != rc && (*s != '\n' && *s != '\0'))
        s++;
      if (*s == rc)
        *s = '\0';
      else
        {
          errs.err_num = oprnd->type == OPRND_TYPE_BRACKET ?
            ERROR_MISSING_RBRACHKET : ERROR_MISSING_RANGLE_BRACKETS;
          return FALSE;
        }

      if (get_operand_value (op, oper, &soprnd->subs[0]) == FALSE)
        {
          *s = rc;
          return FALSE;
        }
      if (**oper == ',')
        *oper += 1;
      if (get_operand_value (op, oper, &soprnd->subs[1]) == FALSE)
        {
          *s = rc;
          return FALSE;
        }

      *s = rc;
      *oper += 1;
      return TRUE;
    }

  switch (oprnd->type)
    {
      /* TODO: add opcode type here, log errors in the function.
         If REGLIST, then j = csky_insn.number - 1.
         If there is needed to parse expressions, it will be
         handled here.  */
#define IS_REG_OVER_RANGE(reg, type)    \
      (type == OPRND_TYPE_GREG0_7 ? (reg > 7): \
       (type == OPRND_TYPE_GREG0_15 ? (reg > 15): 0) )

      case OPRND_TYPE_CTRLREG:
        /* some parse.  */
        return parse_type_ctrlreg (oper);
        break;
      case OPRND_TYPE_AREG:
        return parse_type_areg (oper);
        break;
      case OPRND_TYPE_FREG:
        return parse_type_freg (oper, 0);
        break;
      case OPRND_TYPE_FEREG:
        return parse_type_freg (oper, 1);
        break;
      case OPRND_TYPE_CPCREG:
        return parse_type_cpcreg (oper);
        break;
      case OPRND_TYPE_CPREG:
        return parse_type_cpreg (oper);
        break;
      case OPRND_TYPE_CPIDX:
        return parse_type_cpidx (oper);
        break;
      case OPRND_TYPE_GREG0_7:
      case OPRND_TYPE_GREG0_15:
        {
          int len;
          int reg;
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1
              || IS_REG_OVER_RANGE(reg, oprnd->type))
            {
              errs.err_num = ERROR_REG_OVER_RANGE;
              errs.arg1 = (void *) csky_general_reg[reg];
              return FALSE;
            }
          *oper += len;
          csky_insn.val[csky_insn.idx++] = reg;
          return TRUE;
          break;
        }
      case OPRND_TYPE_REGnsplr:
        {
          int len;
          int reg;
          reg = csky_get_reg_val (*oper, &len);

          if (reg == -1
              || (IS_CSKY_V1 (mach_flag)
                  && (reg == V1_REG_SP || reg == V1_REG_LR)))
            {
              errs.err_num = ERROR_REG_OVER_RANGE;
              errs.arg1 = (void *) csky_general_reg[reg];
              return FALSE;
            }
          csky_insn.val[csky_insn.idx++] = reg;
          *oper += len;
          return TRUE;;
        }

        break;
      case OPRND_TYPE_REGnr4_r7:
        {
          int len;
          int reg;
          if (**oper == '(')
            *oper += 1;
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1
              || (reg <= 7 && reg >= 4))
              return FALSE;

          csky_insn.val[csky_insn.idx++] = reg;
          *oper += len;

          if (**oper == ')')
            *oper += 1;
          return TRUE;;
        }
      case OPRND_TYPE_REGr4_r7:
        if (memcmp (*oper, "r4-r7", sizeof ("r4-r7") - 1) == 0)
          {
            *oper += sizeof ("r4-r7") - 1;
            csky_insn.val[csky_insn.idx++] = 0;
            return TRUE;
          }
        errs.err_num = ERROR_OPCODE_ILLEGAL;
        return FALSE;
      case OPRND_TYPE_IMM_LDST:
        return parse_ldst_imm (oper, op, oprnd);
        break;
      case OPRND_TYPE_IMM_FLDST:
        return parse_fldst_imm (oper, op, oprnd);
      case OPRND_TYPE_IMM2b:
        return is_imm_over_range (oper, 0, 3, -1);
      case OPRND_TYPE_IMM2b_JMPIX:
        {
          /* ck802j support jmpix16, but not support jmpix32.  */
          if (((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_802)
                && ((op->opcode & 0xffff0000) != 0))
            {
              errs.err_num = ERROR_OPCODE_ILLEGAL;
              return FALSE;
            }
          *oper = parse_exp(*oper, &csky_insn.e);
          if (csky_insn.e.X_op == O_constant)
            {
              csky_insn.opcode_end = *oper;
              if (csky_insn.e.X_add_number & 0x7)
                {
                  errs.err_num = ERROR_JMPIX_OVER_RANGE;
                  return FALSE;
                }
              csky_insn.val[csky_insn.idx++] = (csky_insn.e.X_add_number >> 3) - 2;
            }
          return TRUE;
        }
        break;
      case OPRND_TYPE_IMM4b:
        return is_imm_over_range (oper, 0, 15, -1);
        break;
      case OPRND_TYPE_IMM5b:
        return is_imm_over_range (oper, 0, 31, -1);
        break;
      /* This type for "bgeni" in csky v1 ISA.  */
      case OPRND_TYPE_IMM5b_7_31:
        if (is_imm_over_range (oper, 0, 31, -1))
          {
            int val = csky_insn.val[csky_insn.idx-1];
            /* immediate values of 0 -> 6 translate to movi.  */
            if (val <= 6)
              {
                const char *name = "movi";
                csky_insn.opcode = (struct _csky_opcode *)
                  hash_find (csky_opcodes_hash, name);
                csky_insn.val[csky_insn.idx-1] = 1 << val;
              }
            return TRUE;
          }
        else
          {
            return FALSE;
          }
        break;
      case OPRND_TYPE_IMM5b_1_31:
        return is_imm_over_range (oper, 1, 31, -1);
        break;
      case OPRND_TYPE_IMM5b_POWER:
        {
          if (is_imm_over_range (oper, 1, ~(1 << 31), 1 << 31))
            {
              int log;
              int val = csky_insn.val[csky_insn.idx - 1];
              log = csky_log_2 (val);
              csky_insn.val[csky_insn.idx - 1] = log;
              return ((log == -1) ? FALSE : TRUE);
            }
          else
            return FALSE;
        }
      /* This type for "mgeni" in csky v1 ISA.  */
      case OPRND_TYPE_IMM5b_7_31_POWER:
        {
          if (is_imm_over_range (oper, 1, ~(1 << 31), 1 << 31))
            {
              int log;
              int val = csky_insn.val[csky_insn.idx - 1];
              log = csky_log_2 (val);
              /* immediate values of 0 -> 6 translate to movi */
              if (log <= 6)
                {
                  const char *name = "movi";
                  csky_insn.opcode = (struct _csky_opcode *)
                    hash_find (csky_opcodes_hash, name);
                  as_warn (_("translating mgeni to movi"));
                }
              else
                csky_insn.val[csky_insn.idx - 1] = log;
              return ((log == -1) ? FALSE : TRUE);
            }
          else
            return FALSE;
        }

      case OPRND_TYPE_IMM5b_RORI:
        {
          unsigned max_shift;
          if (IS_CSKY_V1 (mach_flag))
            max_shift = 31;
          else
            max_shift = 32;
          if (is_imm_over_range (oper, 1, max_shift, -1))
            {
              int i = csky_insn.idx - 1;
              csky_insn.val[i] = 32 - csky_insn.val[i];
              return TRUE;
            }
          else
            return FALSE;
        }
      /* For csky v1 bmask inst.  */
      case OPRND_TYPE_IMM5b_BMASKI:

        if (!is_imm_over_range (oper, 8, 31, 0))
          {
            unsigned int mask_val = csky_insn.val[csky_insn.idx - 1];
            if (mask_val > 0 && mask_val < 8)
              {
                const char *op_movi = "movi";
                csky_insn.opcode = (struct _csky_opcode *)
                  hash_find (csky_opcodes_hash, op_movi);
                if (csky_insn.opcode == NULL)
                  {
                    return FALSE;
                  }
                csky_insn.val[csky_insn.idx - 1] = (1 << mask_val) - 1;
                csky_show_info (WARNING_BMASKI_TO_MOVI, 0, NULL, NULL);
                return TRUE;
              }
          }
        return TRUE;
      /* For csky v2 bmask, which will transfer to 16bits movi.  */
      case OPRND_TYPE_IMM8b_BMASKI:
        if (is_imm_over_range (oper, 1, 8, -1))
          {
            unsigned int mask_val = csky_insn.val[csky_insn.idx - 1];
            csky_insn.val[csky_insn.idx - 1] = (1 << mask_val) - 1;
            csky_show_info (WARNING_BMASKI_TO_MOVI, 0, NULL, NULL);
            return TRUE;
          }
        return FALSE;
      case OPRND_TYPE_OIMM5b:
        return is_oimm_over_range (oper, 1, 32);
        break;
      case OPRND_TYPE_OIMM5b_IDLY:
        if (is_imm_over_range (oper, 0, 32, -1))
          {
            /* imm5b for idly n: 0<=n<4, imm5b=3; 4<=n<=32, imm5b=n-1.  */
            unsigned long imm = csky_insn.val[csky_insn.idx - 1];
            if (imm < 4)
              {
                csky_show_info (WARNING_IDLY, 0, (void *)imm, NULL);
                imm = 3;
              }
            else imm--;
            csky_insn.val[csky_insn.idx - 1] = imm;
            return TRUE;
          }
        else
          return FALSE;
      /* For csky v2 bmask inst.  */
      case OPRND_TYPE_OIMM5b_BMASKI:
        if (!is_oimm_over_range (oper, 17, 32))
          {
            int mask_val = csky_insn.val[csky_insn.idx - 1];
            if ((mask_val + 1) == 0)
              {
                return TRUE;
              }
            if (mask_val > 0 && mask_val < 16)
              {
                const char *op_movi = "movi";
                csky_insn.opcode = (struct _csky_opcode *)
                  hash_find (csky_opcodes_hash, op_movi);
                if (csky_insn.opcode == NULL)
                  {
                    return FALSE;
                  }
                csky_insn.val[csky_insn.idx - 1] = (1 << (mask_val + 1)) - 1;
                csky_show_info (WARNING_BMASKI_TO_MOVI, 0, NULL, NULL);
                return TRUE;
              }
          }
        return TRUE;
      case OPRND_TYPE_IMM7b:
        return is_imm_over_range (oper, 0, 127, -1);
        break;
      case OPRND_TYPE_IMM8b:
        return is_imm_over_range (oper, 0, 255, -1);
        break;
      case OPRND_TYPE_IMM12b:
        return is_imm_over_range (oper, 0, 4095, -1);
        break;
      case OPRND_TYPE_IMM15b:
        return is_imm_over_range (oper, 0, 0xfffff, -1);
        break;
      case OPRND_TYPE_IMM16b:
        return is_imm_over_range (oper, 0, 65535, -1);
        break;
      case OPRND_TYPE_OIMM16b:
        return is_oimm_over_range (oper, 1, 65536);
        break;
      case OPRND_TYPE_IMM32b:
        {
          expressionS e;
          bfd_boolean ret = FALSE;
          char *new_oper = parse_exp (*oper, &e);
          if (e.X_op == O_constant)
            {
              ret = TRUE;
              *oper = new_oper;
              csky_insn.val[csky_insn.idx++] = e.X_add_number;
            }
          return ret;
          break;
        }
      case OPRND_TYPE_IMM16b_MOVIH:
      case OPRND_TYPE_IMM16b_ORI:
        {
          bfd_reloc_code_real_type r = BFD_RELOC_NONE;
          int len;
          char *curr = *oper;
          char * save = input_line_pointer;
          /* get the reloc type ,and set "@GOTxxx" as ' '  */
          while(((**oper) != '@') && ((**oper) != '\0'))
            *oper += 1;
          if((**oper) != '\0')
            {
              input_line_pointer = *oper;
              lex_got(&r, &len);
              while((*(*oper + len + 1)) != '\0')
              {
                  **oper = *(*oper + len + 1);
                  *(*oper + len + 1) = '\0';
                  *oper++;
              }
              **oper = '\0';
            }
          input_line_pointer = save;
          *oper = parse_exp(curr, &csky_insn.e);
          return TRUE;
        }
        break;
      case OPRND_TYPE_PSR_BITS_LIST:
        {
          int ret = TRUE;
          if (csky_insn.number == 0)
            ret = FALSE;
          else
            {
              csky_insn.val[csky_insn.idx] = 0;
              if (is_psr_bit(oper) != FALSE)
                {
                  while (**oper == ',')
                    {
                      *oper += 1;
                      if (is_psr_bit(oper) == FALSE)
                        {
                          ret = FALSE;
                          break;
                        }
                    }
                }
              else
                ret = FALSE;
              if (ret == TRUE && IS_CSKY_V1 (mach_flag)
                  && csky_insn.val[csky_insn.idx] > 8)
                {
                  ret = FALSE;
                }
            }
          if (!ret)
            {
              errs.err_num = ERROR_OPERANDS_ILLEGAL;
              errs.arg1 = (char*) csky_insn.opcode_end;
            }
          return ret;
        }
      case OPRND_TYPE_RM:
        {
          static const char *round_mode[] =
            {
              "rm_nearest",
              "rm_zero",
              "rm_posinf",
              "rm_neginf",
              NULL
            };
          /* FPU round mode.  */
          char dup_str[128];
          int i = 0;
          strcpy(dup_str, *oper);
          string_tolower(dup_str);
          while (round_mode[i])
            {
              if (strncmp (dup_str, round_mode[i], strlen(round_mode[i])) == 0)
                {
                  break;
                }
              i++;
            }
          if (i >= (int)(sizeof (round_mode) / sizeof (char *)))
            {
              return FALSE;
            }
          *oper += strlen (round_mode[i]);
          csky_insn.val[csky_insn.idx++] = i;
          return TRUE;
          break;
        }

      case OPRND_TYPE_REGLIST_COMMA:
      case OPRND_TYPE_BRACKET:
        /* TODO: using sub operand union.  */
      case OPRND_TYPE_ABRACKET:
        /* TODO: using sub operand union.  */
      case OPRND_TYPE_REGLIST_DASH:
        return is_reglist_legal (oper);
      case OPRND_TYPE_FREGLIST_DASH:
        return is_freglist_legal (oper);
      case OPRND_TYPE_AREG_WITH_BRACKET:
        {
          if (**oper != '(')
            {
              errs.err_num = ERROR_MISSING_LBRACHKET;
              return FALSE;
            }
          *oper += 1;
          int len;
          int reg;
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1)
            {
              errs.err_num = ERROR_EXP_GREG;
              return FALSE;
            }
          *oper += len;
          if (**oper != ')')
            {
              errs.err_num = ERROR_MISSING_RBRACHKET;
              return FALSE;
            }
          *oper += 1;
          csky_insn.val[csky_insn.idx++] = reg;
          return TRUE;
        }
      case OPRND_TYPE_REGsp:
        return is_reg_sp (oper);
      case OPRND_TYPE_REGbsp:
        return is_reg_sp_with_bracket (oper);
      /* For jmpi.  */
      case OPRND_TYPE_OFF8b:
      case OPRND_TYPE_OFF16b:
        *oper = parse_rt (*oper, 1, &csky_insn.e, -1);
        csky_insn.val[csky_insn.idx++] = 0;
        return TRUE;
      case OPRND_TYPE_LABEL_WITH_BRACKET:
      case OPRND_TYPE_CONSTANT:
        if (**oper == '[')
          csky_insn.val[csky_insn.idx++] = 0;
        else
          csky_insn.val[csky_insn.idx++] = NEED_OUTPUT_LITERAL;
        *oper = parse_rt (*oper, 0, &csky_insn.e, -1);
        return TRUE;
      case OPRND_TYPE_FCONSTANT:
        *oper = parse_rtf (*oper, 0, &csky_insn.e);

        return TRUE;
      /* For grs v2.  */
      case OPRND_TYPE_IMM_OFF18b:
        {
          *oper = parse_exp(*oper, &csky_insn.e);
          return TRUE;
        }

      case OPRND_TYPE_OFF10b:
      case OPRND_TYPE_OFF11b:
      case OPRND_TYPE_OFF16b_LSL1:
      case OPRND_TYPE_OFF26b:
        *oper = parse_exp(*oper, &csky_insn.e);
        if (csky_insn.e.X_op == O_symbol)
          {
            csky_insn.opcode_end = *oper;
            return TRUE;
          }
        else
          return FALSE;
        break;
      /* For xtrb0(1)(2)(3) and div in csky v1 ISA.  */
      case  OPRND_TYPE_REG_r1a:
        {
          int reg = 0;
          int len = 0;
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1 )
            {
              errs.err_num = ERROR_REG_FORMAT;
              errs.arg1 = (void *)"The first operand must be regist r1.";
              return FALSE;
            }
          if ( reg != 1)
            {
              mov_r1_after = TRUE;
            }
          *oper += len;
          csky_insn.opcode_end = *oper;
          csky_insn.val[csky_insn.idx++] = reg;
          return TRUE;
        }
        break;
      case  OPRND_TYPE_REG_r1b:
        {
          int reg = 0;
          int len = 0;
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1 )
            {
              errs.err_num = ERROR_REG_FORMAT;
              errs.arg1 = (void *)"The second operand must be regist r1.";
              return FALSE;
            }
          if ( reg != 1)
            {
              unsigned int mov_insn = CSKYV1_INST_MOV_R1_RX;
              mov_insn |= reg << 4;
              mov_r1_before = TRUE;
              csky_insn.output = frag_more (2);
              dwarf2_emit_insn (0);
              md_number_to_chars (csky_insn.output, mov_insn, 2);
            }
          *oper += len;
          csky_insn.opcode_end = *oper;
          csky_insn.val[csky_insn.idx++] = reg;
          return TRUE;
        }
        break;
      case  OPRND_TYPE_DUMMY_REG:
        {
          int reg = 0;
          int len = 0;
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1)
            {
              errs.err_num = ERROR_GREG_ILLEGAL;
              return FALSE;
            }
          if (reg != csky_insn.val[0])
            {
              errs.err_num = ERROR_REG_FORMAT;
              errs.arg1 = (void *)
                "The second regist must be the same as first regist.";
              return FALSE;
            }
          *oper += len;
          csky_insn.opcode_end = *oper;
          csky_insn.val[csky_insn.idx++] = reg;
          return TRUE;
          break;
        }
      case  OPRND_TYPE_2IN1_DUMMY:
        {
          int reg = 0;
          int len = 0;
          int max = 0;
          int min = 0;
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1)
            {
              errs.err_num = ERROR_GREG_ILLEGAL;
              return FALSE;
            }
          /* dummy reg's real type should be same with first operand.  */
          if (op->oprnd.oprnds[0].type
                   == OPRND_TYPE_GREG0_15)
            max = 15;
          else if (op->oprnd.oprnds[0].type
                   == OPRND_TYPE_GREG0_7)
            max = 7;
          else
            return FALSE;
          if (reg < min || reg > max)
            return FALSE;
          csky_insn.val[csky_insn.idx++] = reg;
          /* if it is the last operands.  */
          if (csky_insn.idx > 2)
            {
              /* For "insn rz, rx, ry", if rx or ry is equal to rz,
                 we can output the insn like "insn rz, rx".  */
              if (csky_insn.val[0] ==  csky_insn.val[1])
                csky_insn.val[1] = 0;
              else if (csky_insn.val[0] ==  csky_insn.val[2])
                csky_insn.val[2] = 0;
              else
                return FALSE;
            }
          *oper += len;
          csky_insn.opcode_end = *oper;
          return TRUE;
          break;
        }
      case  OPRND_TYPE_DUP_GREG0_7:
      case  OPRND_TYPE_DUP_GREG0_15:
      case  OPRND_TYPE_DUP_AREG:
        {
          int reg = 0;
          int len = 0;
          unsigned int max_reg;
          unsigned int shift_num;
          char err_str[40];
          if (oprnd->type == OPRND_TYPE_DUP_GREG0_7)
            {
              max_reg = 7;
              shift_num = 3;
            }
          else if (oprnd->type == OPRND_TYPE_DUP_GREG0_15)
            {
              max_reg = 15;
              shift_num = 4;
            }
          else
            {
              max_reg = 31;
              shift_num = 5;
            }
          reg = csky_get_reg_val (*oper, &len);
          if (reg == -1)
            {
              errs.err_num = ERROR_REG_FORMAT;
              sprintf (err_str, "The register must be r0-r%d", max_reg);
              errs.arg1 = (void *)err_str;
              return FALSE;
            }
          if (reg > max_reg)
            {
              errs.err_num = ERROR_REG_OVER_RANGE;
              errs.arg1 = (void *) csky_general_reg[reg];
              return FALSE;
            }
          reg |= reg << shift_num;
          *oper += len;
          csky_insn.opcode_end = *oper;
          csky_insn.val[csky_insn.idx++] = reg;
          return TRUE;
          break;
        }
      case  OPRND_TYPE_CONST1:
        *oper = parse_exp(*oper, &csky_insn.e);
        if (csky_insn.e.X_op == O_constant)
          {
            csky_insn.opcode_end = *oper;
            if (csky_insn.e.X_add_number != 1)
              {
                return FALSE;
              }
            csky_insn.val[csky_insn.idx++] = 1;
            return TRUE;
          }
      case OPRND_TYPE_UNCOND10b:
      case OPRND_TYPE_UNCOND16b:
        *oper = parse_exp(*oper, &csky_insn.e);
        if (csky_insn.e.X_op == O_constant)
          return FALSE;
        input_line_pointer = *oper;
        csky_insn.opcode_end = *oper;
        csky_insn.relax.max = UNCD_DISP16_LEN;
        csky_insn.relax.var = UNCD_DISP10_LEN;
        csky_insn.relax.subtype = UNCD_DISP10;
        csky_insn.val[csky_insn.idx++] = 0;
        return TRUE;
      case OPRND_TYPE_COND10b:
      case OPRND_TYPE_COND16b:
        *oper = parse_exp(*oper, &csky_insn.e);
        if (csky_insn.e.X_op == O_constant)
          return FALSE;
        input_line_pointer = *oper;
        csky_insn.opcode_end = *oper;
        csky_insn.relax.max = COND_DISP16_LEN;
        csky_insn.relax.var = COND_DISP10_LEN;
        csky_insn.relax.subtype = COND_DISP10;
        csky_insn.val[csky_insn.idx++] = 0;
        return TRUE;
      case OPRND_TYPE_JCOMPZ:
        *oper = parse_exp(*oper, &csky_insn.e);
        if (csky_insn.e.X_op == O_constant)
          return FALSE;
        input_line_pointer = *oper;
        csky_insn.opcode_end = *oper;
        csky_insn.relax.max = JCOMPZ_DISP32_LEN;
        csky_insn.relax.var = JCOMPZ_DISP16_LEN;
        csky_insn.relax.subtype = JCOMPZ_DISP16;
        csky_insn.max = JCOMPZ_DISP32_LEN;
        csky_insn.val[csky_insn.idx++] = 0;
        return TRUE;
      case  OPRND_TYPE_JBTF:
        *oper = parse_exp(*oper, &csky_insn.e);
        input_line_pointer = *oper;
        csky_insn.opcode_end = *oper;
        csky_insn.relax.max = csky_relax_table[C (COND_JUMP_S, DISP32)].rlx_length;
        csky_insn.relax.var = csky_relax_table[C (COND_JUMP_S, DISP12)].rlx_length;
        csky_insn.relax.subtype = C (COND_JUMP_S, 0);
        csky_insn.val[csky_insn.idx++] = 0;
        csky_insn.max = C32_LEN_S + 2;
        return TRUE;
        break;
      case  OPRND_TYPE_JBR:
        *oper = parse_exp(*oper, &csky_insn.e);
        input_line_pointer = *oper;
        csky_insn.opcode_end = *oper;
        csky_insn.relax.max = csky_relax_table[C (UNCD_JUMP_S, DISP32)].rlx_length;
        csky_insn.relax.var = csky_relax_table[C (UNCD_JUMP_S, DISP12)].rlx_length;
        csky_insn.relax.subtype = C (UNCD_JUMP_S, 0);
        csky_insn.val[csky_insn.idx++] = 0;
        csky_insn.max = U32_LEN_S + 2;
        return TRUE;
      case  OPRND_TYPE_JBSR:
        if (do_force2bsr)
          {
            *oper = parse_exp(*oper, &csky_insn.e);
          }
        else
          {
            *oper = parse_rt(*oper, 1, &csky_insn.e, -1);
          }
        input_line_pointer = *oper;
        csky_insn.opcode_end = *oper;
        csky_insn.val[csky_insn.idx++] = 0;
        return TRUE;
        break;
      case OPRND_TYPE_REGLIST_DASH_COMMA:
        return is_reglist_dash_comma_legal (oper, oprnd);

      case OPRND_TYPE_MSB2SIZE:
      case OPRND_TYPE_LSB2SIZE:
        {
          expressionS e;
          char *new_oper = parse_exp (*oper, &e);
          if (e.X_op == O_constant)
            {
              *oper = new_oper;
              if (e.X_add_number > 31)
                {
                  errs.err_num = ERROR_IMM_OVERFLOW;
                  return FALSE;
                }
              csky_insn.val[csky_insn.idx++] = e.X_add_number;
              if (oprnd->type == OPRND_TYPE_LSB2SIZE)
                {
                  if (csky_insn.val[csky_insn.idx - 1] > csky_insn.val[csky_insn.idx - 2])
                    {
                      errs.err_num = ERROR_IMM_OVERFLOW;
                      return FALSE;
                    }
                  csky_insn.val[csky_insn.idx - 2] =
                    csky_insn.val[csky_insn.idx - 2] - e.X_add_number;
                }
               return TRUE;
            }

        }
      case OPRND_TYPE_AREG_WITH_LSHIFT:
        return is_reg_lshift_illegal (oper, 0);
      case OPRND_TYPE_AREG_WITH_LSHIFT_FPU:
        return is_reg_lshift_illegal (oper, 1);
      case OPRND_TYPE_FREG_WITH_INDEX:
        {
          if (parse_type_freg (oper, 0))
            {
              if (**oper == '[')
                {
                  (*oper)++;
                  if (is_imm_over_range (oper, 0, 0xf, -1))
                    {
                      if (**oper == ']')
                        {
                          unsigned int idx = --csky_insn.idx;
                          unsigned int val = csky_insn.val[idx];
                          (*oper)++;
                          csky_insn.val[idx - 1] |= val << 4;
                          return TRUE;
                        }
                      else
                        errs.err_num = ERROR_MISSING_RSQUARE_BRACKETS;
                    }
                }
              else
                errs.err_num = ERROR_MISSING_LSQUARE_BRACKETS;
            }
          return FALSE;
        }

      default:
        break;
        /* error code.  */
    }
  return FALSE;
}

/* Subruntine of parse_operands.  */

static bfd_boolean
parse_operands_op (char *str, struct _csky_opcode_info *op)
{
  int i;
  int j;
  char *oper = str;
  int flag_pass;

  for (i = 0; i < OP_TABLE_NUM && op[i].operand_num != -2; i++)
    {
      flag_pass = TRUE;
      csky_insn.idx = 0;
      oper = str;
      /* if operand_num = -1, it is a insn with a REGLIST type operand.i */
      if (!(op[i].operand_num == csky_insn.number
            || (op[i].operand_num == -1 && csky_insn.number != 0)))
        {
          /* The smaller err_num is more serious.  */
          if ( errs.err_num > (int)ERROR_OPERANDS_NUMBER)
            {
              errs.arg1 = (void *)(op[i].operand_num);
              errs.err_num = (int)ERROR_OPERANDS_NUMBER;
            }
          flag_pass = FALSE;
          continue;
        }

      for (j = 0; j < csky_insn.number; j++)
        {
          while (ISSPACE (*oper))
            oper++;
          flag_pass = get_operand_value (&op[i], &oper, &op[i].oprnd.oprnds[j]);
          if (flag_pass == FALSE)
            break;
          while (ISSPACE (*oper))
            oper++;
          /* Skip the ','.  */
          if (j < (csky_insn.number - 1) && op[i].operand_num != -1)
            {
              if (*oper == ',')
                {
                  oper++;
                }
              else
                {
                  if (errs.err_num > ERROR_MISSING_COMMA)
                    {
                      errs.err_num = ERROR_MISSING_COMMA;
                    }
                  flag_pass = FALSE;
                  break;
                }
            }
          else if (!is_end_of_line[(unsigned char) *oper])
            {
              if (errs.err_num > ERROR_BAD_END)
                {
                  errs.err_num = ERROR_BAD_END;
                }
              flag_pass = FALSE;
              break;
            }
          else
            {
              break;
            }
        }
      /* Parse operands in one table end.  */

      if (flag_pass == TRUE)
        {
          /* Parse operands success, set opcode_idx.  */
          csky_insn.opcode_idx = i;
          return TRUE;
        }
      else
        {
          errs.idx = j + 1;
        }
    }
  /* Parse operands in ALL tables end.  */
  return FALSE;
}

/* Parse the operands according to operand type.  */

static bfd_boolean
parse_operands (char *str)
{
  char *oper = str;

  /* Parse operands according to flag_force.  */
  if ((csky_insn.flag_force == INSN_OPCODE16F)
      && ((csky_insn.opcode->isa_flag16 & isa_flag) != 0))
    {
      if (parse_operands_op(oper, csky_insn.opcode->op16) == TRUE)
        {
          csky_insn.isize = 2;
          return TRUE;
        }
      return FALSE;
    }
  else if ((csky_insn.flag_force == INSN_OPCODE32F)
           && ((csky_insn.opcode->isa_flag32 & isa_flag) != 0))
    {
      if (parse_operands_op(oper, csky_insn.opcode->op32) == TRUE)
        {
          csky_insn.isize = 4;
          return TRUE;
        }
      return FALSE;
    }
  else
    {
      if ((csky_insn.opcode->isa_flag16 & isa_flag) != 0)
        {
          if (parse_operands_op(oper, csky_insn.opcode->op16) == TRUE)
            {
              csky_insn.isize = 2;
              return TRUE;
            }
        }
      if ((csky_insn.opcode->isa_flag32 & isa_flag) != 0)
        {
          if (parse_operands_op(oper, csky_insn.opcode->op32) == TRUE)
            {
              csky_insn.isize = 4;
              return TRUE;
            }
        }
      return FALSE;
    }
}

static bfd_boolean
csky_generate_frags (void)
{
  /* frag more relax reloc.  */
  if (csky_insn.flag_force == INSN_OPCODE16F || !IS_SUPPORT_OPCODE32(csky_insn.opcode))
    {
      csky_insn.output = frag_more (csky_insn.isize);
      if (csky_insn.opcode->reloc16)
        {
          /* 16 bits opcode force, should generate fixup.  */
          reloc_howto_type *howto;
          howto = bfd_reloc_type_lookup (stdoutput,
                                         csky_insn.opcode->reloc16);
          fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                       2, &csky_insn.e, howto->pc_relative,
                       csky_insn.opcode->reloc16);
        }
    }
  else if (csky_insn.flag_force == INSN_OPCODE32F)
    {
      csky_insn.output = frag_more (csky_insn.isize);
      if (csky_insn.opcode->reloc32)
        {
          reloc_howto_type *howto;
          howto = bfd_reloc_type_lookup (stdoutput,
                                         csky_insn.opcode->reloc32);
          fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                       4, &csky_insn.e, howto->pc_relative,
                       csky_insn.opcode->reloc32);
        }
    }
  else
    {
      if (csky_insn.opcode->relax)
        {
          /* Generate the relax infomation.  */
        csky_insn.output = frag_var (rs_machine_dependent,
                           csky_insn.relax.max,
                           csky_insn.relax.var,
                           csky_insn.relax.subtype,
                           csky_insn.e.X_add_symbol,
                           csky_insn.e.X_add_number, 0);

        }
      else
        {
          csky_insn.output = frag_more (csky_insn.isize);
          if (csky_insn.opcode->reloc16 && csky_insn.isize == 2)
            {
              reloc_howto_type *howto;
              howto = bfd_reloc_type_lookup (stdoutput,
                                             csky_insn.opcode->reloc16);
              fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                           2, &csky_insn.e, howto->pc_relative,
                           csky_insn.opcode->reloc16);
            }
          else if (csky_insn.opcode->reloc32 && csky_insn.isize == 4)
            {
              reloc_howto_type *howto;
              howto = bfd_reloc_type_lookup (stdoutput,
                                             csky_insn.opcode->reloc32);
              fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                           4, &csky_insn.e, howto->pc_relative,
                           csky_insn.opcode->reloc32);
            }
        }
    }
  return TRUE;
}

static int
_generate_value(int mask, int val)
{
  int ret = 0;
  int bit = 1;
  while (mask)
    {
      if (mask & bit)
        {
          if (val & 0x1)
            {
              ret |= bit;
            }
          val = val >> 1;
          mask &= ~bit;
        }
      bit = bit << 1;
    }
  return ret;
}

static int
_generate_insn(struct operand *oprnd, int *oprnd_idx)
{
  struct soperand *soprnd = NULL;
  int mask;
  int val;
  if ((unsigned int)oprnd->mask == HAS_SUB_OPERAND)
    {
      soprnd = (struct soperand *) oprnd;
      _generate_insn (&soprnd->subs[0], oprnd_idx);
      _generate_insn (&soprnd->subs[1], oprnd_idx);
      return 0;
    }
  mask = oprnd->mask;
  val = csky_insn.val[*oprnd_idx];
  (*oprnd_idx)++;
  val = _generate_value (mask, val);
  csky_insn.inst |= val;

  return 0;
}

static bfd_boolean
csky_generate_insn (void)
{
  int i = 0;
  struct _csky_opcode_info *opinfo = NULL;

  if (csky_insn.isize == 4)
    {
      opinfo = &csky_insn.opcode->op32[csky_insn.opcode_idx];
    }
  else if (csky_insn.isize == 2)
    {
      opinfo = &csky_insn.opcode->op16[csky_insn.opcode_idx];
    }

  int sidx = 0;
  csky_insn.inst = opinfo->opcode;
  if (opinfo->operand_num == -1)
    {
      _generate_insn(&opinfo->oprnd.oprnds[i], &sidx);
      return 0;
    }
  else
    {
      for (i = 0; i < opinfo->operand_num; i++)
        {
          _generate_insn(&opinfo->oprnd.oprnds[i], &sidx);
        }
    }
  return 0;
}

void
md_assemble (char *str)
{
  csky_insn.isize = 0;
  csky_insn.idx = 0;
  csky_insn.max = 0;
  csky_insn.flag_force = INSN_OPCODE;
  csky_insn.macro = NULL;
  csky_insn.opcode = NULL;
  memset(csky_insn.val, 0, sizeof(int)*MAX_OPRND_NUM);
  /* Initialize err_num.  */
  errs.err_num = ERROR_NONE;
  mov_r1_before = FALSE;
  mov_r1_after = FALSE;

  mapping_state(MAP_TEXT);
  /* Tie dwarf2 debug info to every insn if set option --gdwarf2.  */
  dwarf2_emit_insn (0);
  while (ISSPACE (* str))
    str++;
  /* Get opcode from str.  */
  if (parse_opcode (str) == FALSE)
    {
      csky_show_info (ERROR_OPCODE_ILLEGAL,
                      0, NULL, NULL);
      return;
    }

  /* If it is a macro instruction, handle it.  */
  if (csky_insn.macro != NULL)
    {
      if (csky_insn.number == csky_insn.macro->oprnd_num)
        {
          csky_insn.macro->handle_func ();
          return;
        }
      else if (errs.err_num > ERROR_OPERANDS_NUMBER)
        {
          errs.err_num = ERROR_OPERANDS_NUMBER;
          errs.arg1 = (void *) csky_insn.macro->oprnd_num;
        }
    }

  if (csky_insn.opcode == NULL)
    {
      if (errs.err_num > ERROR_OPCODE_ILLEGAL)
        errs.err_num = (int)ERROR_OPCODE_ILLEGAL;
      csky_show_info (errs.err_num, errs.idx,
                      (void *)errs.arg1, (void *)errs.arg1);
      return;
    }

  /* Parse the operands according to operand type.  */
  if (parse_operands (csky_insn.opcode_end) == FALSE)
    {
      csky_show_info (errs.err_num, errs.idx,
                      (void *)errs.arg1, (void *)errs.arg1);
      return;
    }

  bfd_boolean is_need_check_literals = TRUE;
  /* if this insn has work in opcode table, then do it.  */
  if (csky_insn.opcode->work != NULL)
    {
      is_need_check_literals = csky_insn.opcode->work ();
    }
  else
    {
      /* Generate relax or reloc if necessary.  */
      csky_generate_frags ();
      /* Generate the insn by mask.  */
      csky_generate_insn ();
      /* Write inst to frag.  */
      csky_write_insn (csky_insn.output,
                       csky_insn.inst,
                       csky_insn.isize);
    }

  /* adjust for xtrb0/xtrb1/xtrb2/xtrb3/divs/divu in csky v1 ISA.  */
  if (mov_r1_after == TRUE)
    {
      unsigned int mov_insn = CSKYV1_INST_MOV_RX_R1;
      mov_insn |= csky_insn.val[0];
      mov_r1_before = TRUE;
      csky_insn.output = frag_more (2);
      dwarf2_emit_insn (0);
      md_number_to_chars (csky_insn.output, mov_insn, 2);
      csky_insn.isize += 2;
    }
  if (mov_r1_before == TRUE)
    csky_insn.isize += 2;

  /* Check literal.  */
  if (is_need_check_literals)
    {
      if(csky_insn.max == 0)
        check_literals (csky_insn.opcode->transfer, csky_insn.isize);
      else
        check_literals (csky_insn.opcode->transfer, csky_insn.max);
    }

  insn_reloc = BFD_RELOC_NONE;
}

int
md_parse_option (int c, const char * arg)
{
  /* handled return N-Zero, otherwise return 0.  */
  int i;
  int ret = 0;
  switch (c)
    {
      default:
        {
          for (i = 0; csky_opts[i].name != NULL; i++)
            {
              if (arg != NULL
                  && memcmp(arg - 1, csky_opts[i].name,
                         strlen(csky_opts[i].name)) == 0
                  && !ISALPHA(*(arg - 1 + strlen(csky_opts[i].name))))
                {
                  if (csky_opts[i].ver)
                    {
                      switch (csky_opts[i].operation)
                        {
                          case OPTS_OPERATION_ASSIGN:
                            *(csky_opts[i].ver) = csky_opts[i].value;
                            ret = 1;
                            break;
                          case OPTS_OPERATION_OR:
                            *(csky_opts[i].ver) |= csky_opts[i].value;
                            ret = 1;
                            break;
                          default:
                            break;
                        }
                    }
                  return ret;
                }
            }
          for (i = 0; csky_long_opts[i].name != NULL; i++)
            {
              if (memcmp(arg - 1, csky_long_opts[i].name,
                         strlen(csky_long_opts[i].name)) == 0)
                {
                  if (csky_long_opts[i].func)
                    {
                      if (csky_long_opts[i].func(arg - 1 + strlen(csky_long_opts[i].name)))
                        {
                          return 0;
                        }
                    }
                  return 1;
                }
            }
          break;
        }
      /* The following case are defined in md_longopts.  */
    }
  return 0;
}

/* Convert a machine dependent frag.  */
#define PAD_LITERAL_LENGTH                        6
#define opposite_of_stored_comp(insn)             (insn ^ 0x04000000)
#define opposite_of_stored_compz(insn)            (insn ^ 0x00200000)
#define make_insn(total_length,opcode,oprand,oprand_length)               \
  {                                                                       \
    if(total_length > 0)                                                  \
      {                                                                   \
        csky_write_insn(                                                  \
                    buf,                                                  \
                    opcode | ( oprand & ((1<<oprand_length)-1) ),         \
                    total_length );                                       \
        buf += total_length;                                              \
        fragp->fr_fix += total_length;                                    \
      }                                                                   \
  }

#define make_literal(fragp, literal_offset)                               \
  {                                                                       \
    make_insn(literal_offset, PAD_FILL_CONTENT, 0, 0);                    \
    fix_new(fragp, fragp->fr_fix, 4, fragp->fr_symbol,                    \
         fragp->fr_offset, 0, BFD_RELOC_CKCORE_ADDR32);                   \
    make_insn(4, 0, 0, 0);                                                \
    make_insn(2 - literal_offset, PAD_FILL_CONTENT, 0, 0);                \
  }

void
md_convert_frag (bfd *abfd ATTRIBUTE_UNUSED, segT asec , fragS *fragp)
{
  offsetT disp;
  char *buf = fragp->fr_fix + fragp->fr_literal;
  gas_assert(fragp->fr_symbol);

  if (IS_EXTERNAL_SYM(fragp->fr_symbol, asec))
    {
      disp = 0;
    }
  else
    {
      disp = S_GET_VALUE (fragp->fr_symbol)
              + fragp->fr_offset
              - fragp->fr_address
              - fragp->fr_fix;
    }

  switch (fragp->fr_subtype)
    {
      /* generate new insn.  */
      case C (COND_JUMP, DISP12):
      case C (UNCD_JUMP, DISP12):
      case C (COND_JUMP_PIC, DISP12):
      case C (UNCD_JUMP_PIC, DISP12):
        {
#define CSKY_V1_B_MASK   0xf8
          unsigned char t0;
          disp -= 2;
          if (disp & 1)
            {
              /* Error. odd displacement at %x, next_inst-2.  */
              ;
            }
          disp >>= 1;

          if (!target_big_endian)
            {
              t0 = buf[1] & CSKY_V1_B_MASK;
              md_number_to_chars (buf, disp, 2);
              buf[1] = (buf[1] & ~CSKY_V1_B_MASK) | t0;
            }
          else
            {
              t0 = buf[0] & CSKY_V1_B_MASK;
              md_number_to_chars (buf, disp, 2);
              buf[0] = (buf[0] & ~CSKY_V1_B_MASK) | t0;
            }
          fragp->fr_fix += 2;
          break;
        }
      case C (COND_JUMP, DISP32):
      case C (COND_JUMP, UNDEF_WORD_DISP):
        {
          /* A conditional branch wont fit into 12 bits:
             b!cond 1f
             jmpi 0f
             .align 2
             0: .long disp
             1:
             */
          int first_inst = fragp->fr_fix + fragp->fr_address;
          int is_unaligned = (first_inst & 3);

          if (!target_big_endian)
            {
              /* b!cond instruction.  */
              buf[1] ^= 0x08;
              /* jmpi instruction.  */
              buf[2] = CSKYV1_INST_JMPI & 0xff;
              buf[3] = CSKYV1_INST_JMPI >> 8;
            }
          else
            {
              /* b!cond instruction.  */
              buf[0] ^= 0x08;
              /* jmpi instruction.  */
              buf[2] = CSKYV1_INST_JMPI >> 8;
              buf[3] = CSKYV1_INST_JMPI & 0xff;
            }

          if (is_unaligned)
            {
              if (!target_big_endian)
                {
                  /* bt/bf: jump to pc + 2 + (4 << 1).  */
                  buf[0] = 4;
                  /* jmpi: jump to MEM (pc + 2 + (1 << 2)).  */
                  buf[2] = 1;
                }
              else
                {
                  /* bt/bf: jump to pc + 2 + (4 << 1).  */
                  buf[1] = 4;
                  /* jmpi: jump to MEM (pc + 2 + (1 << 2)).  */
                  buf[3] = 1;
                }
              /* Aligned 4 bytes.  */
              buf[4] = 0;
              buf[5] = 0;
              /* .long  */
              buf[6] = 0;
              buf[7] = 0;
              buf[8] = 0;
              buf[9] = 0;

              /* Make reloc for the long disp.  */
              fix_new (fragp, fragp->fr_fix + 6, 4,
                       fragp->fr_symbol, fragp->fr_offset, 0, BFD_RELOC_32);
              fragp->fr_fix += C32_LEN;
            }
          else
            {
              if (!target_big_endian)
                {
                  /* bt/bf: jump to pc + 2 + (3 << 1).  */
                  buf[0] = 3;
                  /* jmpi: jump to MEM (pc + 2 + (0 << 2)).  */
                  buf[2] = 0;
                }
              else
                {
                  /* bt/bf: jump to pc + 2 + (3 << 1).  */
                  buf[1] = 3;
                  /* jmpi: jump to MEM (pc + 2 + (0 << 2)).  */
                  buf[3] = 0;
                }
              /* .long  */
              buf[4] = 0;
              buf[5] = 0;
              buf[6] = 0;
              buf[7] = 0;

              /* Make reloc for the long disp.  */
              fix_new (fragp, fragp->fr_fix + 4, 4,
                       fragp->fr_symbol, fragp->fr_offset, 0, BFD_RELOC_32);
              fragp->fr_fix += C32_LEN;

              /* Frag is actually shorter (see the other side of this ifdef)
                 but gas isn't prepared for that.  We have to re-adjust
                 the branch displacement so that it goes beyond the
                 full length of the fragment, not just what we actually
                 filled in.  */
              if (!target_big_endian)
                {
                  buf[0] = 4;
                }
              else
                {
                  buf[1] = 4;
                }
            }
        }
        break;

      case C (COND_JUMP_PIC, DISP32):
      case C (COND_JUMP_PIC, UNDEF_WORD_DISP):
        {
#define BYTE_1(a)               (target_big_endian ? ((a) & 0xff):((a) >> 8))
#define BYTE_0(a)               (target_big_endian ? ((a) >> 8) : ((a) & 0xff))
          /* b!cond 1f
              subi sp, 8
              stw  r15, (sp, 0)
              bsr  .L0
             .L0:
              lrw r1, 0f
              add r1, r15
              addi sp, 8
              jmp r1
             .align 2
             0: .long (tar_addr - pc)
             1:
             */
          int first_inst = fragp->fr_fix + fragp->fr_address;
          int is_unaligned = (first_inst & 3);
          disp -= 8;
          if (! target_big_endian)
            {
              buf[1] ^= 0x08;
            }
          else
            {
              buf[0] ^= 0x08;  /* Toggle T/F bit */
            }
          buf[2] = BYTE_0(CSKYV1_INST_SUBI | (7 << 4));     /* subi r0, 8.  */
          buf[3] = BYTE_1(CSKYV1_INST_SUBI | (7 << 4));
          buf[4] = BYTE_0(CSKYV1_INST_STW  | (15 << 8));    /* stw r15, r0.  */
          buf[5] = BYTE_1(CSKYV1_INST_STW  | (15 << 8));
          buf[6] = BYTE_0(CSKYV1_INST_BSR);                 /* bsr pc + 2.  */
          buf[7] = BYTE_1(CSKYV1_INST_BSR);
          buf[8] = BYTE_0(CSKYV1_INST_LRW | (1 << 8));      /* lrw r1, (tar_addr - pc).  */
          buf[9] = BYTE_1(CSKYV1_INST_LRW | (1 << 8));
          buf[10] = BYTE_0(CSKYV1_INST_ADDU | (15 << 4) | 1);  /* add r1, r15.  */
          buf[11] = BYTE_1(CSKYV1_INST_ADDU | (15 << 4) | 1);
          buf[12] = BYTE_0(CSKYV1_INST_LDW | (15 << 8));     /* ldw r15, r0.  */
          buf[13] = BYTE_1(CSKYV1_INST_LDW | (15 << 8));
          buf[14] = BYTE_0(CSKYV1_INST_ADDI | (7 << 4));     /* addi r0, 8.  */
          buf[15] = BYTE_1(CSKYV1_INST_ADDI | (7 << 4));
          buf[16] = BYTE_0(CSKYV1_INST_JMP | 1);             /* jmp r1.  */
          buf[17] = BYTE_1(CSKYV1_INST_JMP | 1);

          if (!is_unaligned)
            {
              if (!target_big_endian)
                {
                  buf[0] = 11;
                  buf[8] = 3;
                  buf[20] = disp & 0xff;
                  buf[21] = (disp >> 8) & 0xff;
                  buf[22] = (disp >> 16) & 0xff;
                  buf[23] = (disp >> 24) & 0xff;
                }
              else /* if !target_big_endian.  */
                {
                  buf[1] = 11;
                  buf[9] = 3;
                  buf[20] = (disp >> 24) & 0xff;
                  buf[21] = (disp >> 16) & 0xff;
                  buf[22] = (disp >> 8) & 0xff;
                  buf[23] = disp & 0xff;
                }
              buf[18] = 0;  /* aligment.  */
              buf[19] = 0;
              fragp->fr_fix += C32_LEN_PIC;
            }
          else  /* if !is_unaligned.  */
            {
              if (!target_big_endian)
                {
                  buf[0] = 11;
                  buf[8] = 2;
                  buf[18] = disp & 0xff;
                  buf[19] = (disp >> 8) & 0xff;
                  buf[20] = (disp >> 16) & 0xff;
                  buf[21] = (disp >> 24) & 0xff;
                }
              else /* if !target_big_endian.  */
                {
                  buf[1] = 11;
                  buf[9] = 2;
                  buf[18] = (disp >> 24) & 0xff;
                  buf[19] = (disp >> 16) & 0xff;
                  buf[20] = (disp >> 8) & 0xff;
                  buf[21] = disp & 0xff;
                }
              fragp->fr_fix += C32_LEN_PIC;

            } /* end if is_unaligned.  */
        } /* end case C (COND_JUMP_PIC, DISP32)/C (COND_JUMP_PIC, UNDEF_WORD_DISP).  */
        break;
      case C (UNCD_JUMP, DISP32):
      case C (UNCD_JUMP, UNDEF_WORD_DISP):
        {
          /* jmpi 0f
             .align 2
             0: .long disp.  */
          int first_inst = fragp->fr_fix + fragp->fr_address;
          int is_unaligned = (first_inst & 3);
          /* Build jmpi.  */
          buf[0] = BYTE_0(CSKYV1_INST_JMPI);
          buf[1] = BYTE_1(CSKYV1_INST_JMPI);
          if (!is_unaligned)
            {
              if (!target_big_endian)
                buf[0] = 1;
              else
                buf[1] = 1;
              /* Alignment.  */
              buf[2] = 0;
              buf[3] = 0;
              /* .long  */
              buf[4] = 0;
              buf[5] = 0;
              buf[6] = 0;
              buf[7] = 0;
              fix_new (fragp, fragp->fr_fix + 4, 4,
                       fragp->fr_symbol, fragp->fr_offset, 0, BFD_RELOC_32);
              fragp->fr_fix += U32_LEN;
            }
          else /* if is_unaligned.  */
            {
              if (!target_big_endian)
                buf[0] = 0;
              else
                buf[1] = 0;
              /* .long  */
              buf[2] = 0;
              buf[3] = 0;
              buf[4] = 0;
              buf[5] = 0;
              fix_new (fragp, fragp->fr_fix + 2, 4,
                       fragp->fr_symbol, fragp->fr_offset, 0, BFD_RELOC_32);
              fragp->fr_fix += U32_LEN;

            }
        }
        break;
      case C (UNCD_JUMP_PIC, DISP32):
      case C (UNCD_JUMP_PIC, UNDEF_WORD_DISP):
        {
          /*    subi sp, 8
                stw  r15, (sp)
                bsr  .L0
             .L0:
                lrw  r1, 0f
                add  r1, r15
                ldw  r15, (sp)
                addi sp, 8
                jmp r1
             .align 2
             0: .long (tar_add - pc)
             1:
          */
          /* If the b!cond is 4 byte aligned, the literal which would
             goat x+4 will also be aligned.  */
          int first_inst = fragp->fr_fix + fragp->fr_address;
          int is_unaligned = (first_inst & 3);
          disp -= 6;

          buf[0] = BYTE_0(CSKYV1_INST_SUBI | (7 << 4));     /* subi r0, 8.  */
          buf[1] = BYTE_1(CSKYV1_INST_SUBI | (7 << 4));
          buf[2] = BYTE_0(CSKYV1_INST_STW  | (15 << 8));    /* stw r15, r0.  */
          buf[3] = BYTE_1(CSKYV1_INST_STW  | (15 << 8));
          buf[4] = BYTE_0(CSKYV1_INST_BSR);                 /* bsr pc + 2.  */
          buf[5] = BYTE_1(CSKYV1_INST_BSR);
          buf[6] = BYTE_0(CSKYV1_INST_LRW | (1 << 8));      /* lrw r1, (tar_addr - pc).  */
          buf[7] = BYTE_1(CSKYV1_INST_LRW | (1 << 8));
          buf[8] = BYTE_0(CSKYV1_INST_ADDU | (15 << 4) | 1);  /* add r1, r15.  */
          buf[9] = BYTE_1(CSKYV1_INST_ADDU | (15 << 4) | 1);
          buf[10] = BYTE_0(CSKYV1_INST_LDW | (15 << 8));     /* ldw r15, r0.  */
          buf[11] = BYTE_1(CSKYV1_INST_LDW | (15 << 8));
          buf[12] = BYTE_0(CSKYV1_INST_ADDI | (7 << 4));     /* addi r0, 8.  */
          buf[13] = BYTE_1(CSKYV1_INST_ADDI | (7 << 4));
          buf[14] = BYTE_0(CSKYV1_INST_JMP | 1);             /* jmp r1.  */
          buf[15] = BYTE_1(CSKYV1_INST_JMP | 1);

          if (is_unaligned)
            {
              if (!target_big_endian)
                {
                  buf[6] = 3;
                  buf[18] = disp & 0xff;
                  buf[19] = (disp >> 8) & 0xff;
                  buf[20] = (disp >> 16) & 0xff;
                  buf[21] = (disp >> 24) & 0xff;
                }
              else
                {
                  buf[7] = 3;
                  buf[18] = (disp >> 24) & 0xff;
                  buf[19] = (disp >> 16) & 0xff;
                  buf[20] = (disp >> 8) & 0xff;
                  buf[21] = disp & 0xff;
                }
              buf[16] = 0;
              buf[17] = 0;
              fragp->fr_fix += U32_LEN_PIC;
            }
          else
            {
              if (!target_big_endian)
                {
                  buf[6] = 2;
                  buf[16] = disp & 0xff;
                  buf[17] = (disp >> 8) & 0xff;
                  buf[18] = (disp >> 16) & 0xff;
                  buf[19] = (disp >> 24) & 0xff;
                }
              else
                {
                  buf[7] = 2;
                  buf[16] = (disp >> 24) & 0xff;
                  buf[17] = (disp >> 16) & 0xff;
                  buf[18] = (disp >> 8) & 0xff;
                  buf[19] = disp & 0xff;
                }
              fragp->fr_fix += U32_LEN_PIC;
            }
        }
        break;
      case COND_DISP10:
      case UNCD_DISP10:
      case JCOND_DISP10:
      case JUNCD_DISP10:
        {
          unsigned int inst = csky_read_insn (buf, 2);
          inst |= (disp >> 1) & ((1 << 10) - 1);
          csky_write_insn (buf, inst, 2);
          fragp->fr_fix += 2;
          break;
        }
      case COND_DISP16:
      case JCOND_DISP16:
        {
          unsigned int inst = csky_read_insn (buf, 2);
          inst = (inst == CSKYV2_INST_BT16 ? CSKYV2_INST_BT32 : CSKYV2_INST_BF32);
          if (IS_EXTERNAL_SYM(fragp->fr_symbol, asec))
            {
              fix_new (fragp, fragp->fr_fix, 4,
                       fragp->fr_symbol, fragp->fr_offset, 1, BFD_RELOC_CKCORE_PCREL_IMM16BY2);
            }
          inst |= (disp >> 1) & ((1 << 16) - 1);
          csky_write_insn (buf, inst, 4);
          fragp->fr_fix += 4;
          break;
        }
      case LRW_DISP7:
        {
          unsigned int inst = csky_read_insn (buf, 2);
          int imm;
          imm = (disp+2) >> 2;
          inst |= (imm >> 5) << 8;
          make_insn (2, inst, (imm & 0x1f), 5);
          break;
        }
      case LRW2_DISP8:
        {
          unsigned int inst = csky_read_insn (buf, 2);
          int imm = (disp+2) >> 2;
          if (imm >= 0x80)
            {
              inst &= 0xe0;
              inst |= (~((imm >> 5) << 8)) & 0x300;
              make_insn (2, inst, (~imm & 0x1f), 5);
            }
          else
            {
              inst |= (imm >> 5) << 8;
              make_insn (2, inst, (imm & 0x1f), 5);
            }
          break;
        }
      case LRW_DISP16:
        {
          unsigned int inst = csky_read_insn (buf, 2);
          inst = CSKYV2_INST_LRW32 | (((inst & 0xe0) >> 5) << 16);
          if (IS_EXTERNAL_SYM(fragp->fr_symbol, asec))
            {
              fix_new (fragp, fragp->fr_fix, 4,
                       fragp->fr_symbol, fragp->fr_offset, 1,
                       BFD_RELOC_CKCORE_PCREL_IMM16BY4);
            }
          make_insn (4, inst, ((disp + 2) >> 2), 16);
          break;
        }
      case JCOMPZ_DISP16:
        {
          unsigned int inst = csky_read_insn (buf, 4);
          make_insn (4, inst, disp >> 1, 16);
        }
        break;
      case JCOMPZ_DISP32:
        {
          unsigned int inst = csky_read_insn (buf, 4);
          int literal_offset;
          make_insn (4, opposite_of_stored_compz(inst), (4+4+PAD_LITERAL_LENGTH) >> 1, 16);
          literal_offset = ((fragp->fr_address + fragp->fr_fix) % 4 == 0 ?
                            0 : 2);
          make_insn(4, CSKYV2_INST_JMPI32 ,(4 + literal_offset + 2) >> 2, 10);
          make_literal (fragp, literal_offset);
        }
        break;
      case JUNCD_DISP16:
      case UNCD_DISP16:
        {
          unsigned int inst = csky_read_insn (buf, 2);
          int literal_offset;
          if (IS_EXTERNAL_SYM(fragp->fr_symbol, asec))
            {
              fix_new (fragp, fragp->fr_fix, 4,
                       fragp->fr_symbol, fragp->fr_offset, 1, BFD_RELOC_CKCORE_PCREL_IMM16BY2);
            }
          make_insn(4, CSKYV2_INST_BR32, disp >> 1, 16);
        }
        break;
      case JCOND_DISP32:
        {
          //'jbt'/'jbf'-> <bf16/bt16>; jmpi32; [pad16]+literal32
          unsigned int inst = csky_read_insn (buf, 2);
          int literal_offset;
          inst = (inst == CSKYV2_INST_BT16 ? CSKYV2_INST_BF16 : CSKYV2_INST_BT16);
          make_insn (2, inst, (2 + 4 + PAD_LITERAL_LENGTH) >> 1, 10);
          literal_offset = ((fragp->fr_address + fragp->fr_fix) % 4 == 0 ?
                            0 : 2);
          make_insn(4, CSKYV2_INST_JMPI32, (4 + literal_offset + 2) >> 2, 10);
          make_literal (fragp, literal_offset);
          break;
        }
      case JUNCD_DISP32:
        {
          int literal_offset;
          literal_offset = ((fragp->fr_address + fragp->fr_fix) % 4 == 0 ?
                            0 : 2);
          make_insn (4, CSKYV2_INST_JMPI32, (4 + literal_offset + 2) >> 2, 10);
          make_literal (fragp, literal_offset);
        }
        break;
      case RELAX_OVERFLOW:
        csky_branch_report_error (fragp->fr_file, fragp->fr_line, fragp->fr_symbol, disp);
        break;
      default:
        abort ();
        break;
    }
}

/* Round up a section size to the appropriate boundary.   */

valueT
md_section_align (segT segment ATTRIBUTE_UNUSED,
                  valueT size)
{
  return size;
}

/* MD interface: Symbol and relocation handling.  */

void md_csky_end (void)
{
  dump_literals (0);
}

/* Return the address within the segment that a PC-relative fixup is
   relative to.  */

long
md_pcrel_from_section (fixS * fixP, segT seg)
{
  /* If the symbol is undefined or defined in another section
     we leave the add number alone for the linker to fix it later.  */
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
          || (S_GET_SEGMENT (fixP->fx_addsy) != seg)))
    {
      return fixP->fx_size;
    }
  /* The case where we are going to resolve things.  */
  return  fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

/* csky_cons_fix_new is called via the expression parsing code when a
   reloc is needed.  We use this hook to get the correct .got reloc.  */
void
csky_cons_fix_new (fragS *frag,
                   unsigned int off,
                   unsigned int len,
                   expressionS *exp,
                   bfd_reloc_code_real_type reloc)

{
  if ((BFD_RELOC_CKCORE_GOTOFF == insn_reloc)
      || (BFD_RELOC_CKCORE_GOTPC == insn_reloc)
      || (BFD_RELOC_CKCORE_GOT32 == insn_reloc)
      || (BFD_RELOC_CKCORE_PLT32 == insn_reloc)
      || (BFD_RELOC_CKCORE_TLS_LE32 == insn_reloc)
      || (BFD_RELOC_CKCORE_TLS_GD32 == insn_reloc)
      || (BFD_RELOC_CKCORE_TLS_LDM32 == insn_reloc)
      || (BFD_RELOC_CKCORE_TLS_LDO32 == insn_reloc)
      || (BFD_RELOC_CKCORE_TLS_IE32 == insn_reloc))
    {
      reloc = insn_reloc;
      if (BFD_RELOC_CKCORE_TLS_IE32 == insn_reloc
          || BFD_RELOC_CKCORE_TLS_GD32 == insn_reloc
          || BFD_RELOC_CKCORE_TLS_LDM32 == insn_reloc )
        {
          exp->X_add_number = (offsetT) (&literal_insn_offset->tls_addend);
          if(count_tls > 1024)
            {
              as_bad (_("tls variable number %u more than 1024,array overflow"), count_tls);
            }
        }
    }
  else switch (len)
    {
      case 1:
        reloc = BFD_RELOC_8;
        break;
      case 2:
        reloc = BFD_RELOC_16;
        break;
      case 4:
        reloc = BFD_RELOC_32;
        break;
      case 8:
        reloc = BFD_RELOC_64;
        break;
      default:
        as_bad (_("unsupported BFD relocation size %u"), len);
        reloc = BFD_RELOC_32;
        break;
    }
  fix_new_exp (frag, off, (int) len, exp, 0, reloc);
}

/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
csky_force_relocation (fixS * fix)
{
  if (fix->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fix->fx_r_type == BFD_RELOC_VTABLE_ENTRY
      || fix->fx_r_type == BFD_RELOC_RVA
      || fix->fx_r_type == BFD_RELOC_CKCORE_ADDR_HI16
      || fix->fx_r_type == BFD_RELOC_CKCORE_ADDR_LO16
      || fix->fx_r_type == BFD_RELOC_CKCORE_TOFFSET_LO16
      || fix->fx_r_type == BFD_RELOC_CKCORE_DOFFSET_LO16)
    return 1;

  if (fix->fx_addsy == NULL)
    return 0;

  if (do_use_branchstub)
    {
      if (fix->fx_r_type == BFD_RELOC_CKCORE_PCREL_IMM26BY2
          && (symbol_get_bfdsym (fix->fx_addsy)->flags & BSF_FUNCTION))
        return 1;
    }
  return S_FORCE_RELOC (fix->fx_addsy, fix->fx_subsy == NULL);

}

/* Return true if the fix can be handled by GAS, false if it must
   be passed through to the linker.  */

bfd_boolean
csky_fix_adjustable (fixS * fixP)
{
  if (fixP->fx_addsy == NULL)
    return 1;

  /* We need the symbol name for the VTABLE entries.  */
  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY
      || fixP->fx_r_type == BFD_RELOC_CKCORE_PLT32
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOT32
      || fixP->fx_r_type == BFD_RELOC_CKCORE_PLT12
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOT12
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOT_HI16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOT_LO16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_PLT_HI16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_PLT_LO16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOTOFF
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOTOFF_HI16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOTOFF_LO16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_ADDR_HI16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_ADDR_LO16
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOT_IMM18BY4
      || fixP->fx_r_type == BFD_RELOC_CKCORE_PLT_IMM18BY4
      || fixP->fx_r_type == BFD_RELOC_CKCORE_GOTOFF_IMM18
      || fixP->fx_r_type == BFD_RELOC_CKCORE_TLS_LE32
      || fixP->fx_r_type == BFD_RELOC_CKCORE_TLS_IE32
      || fixP->fx_r_type == BFD_RELOC_CKCORE_TLS_GD32
      || fixP->fx_r_type == BFD_RELOC_CKCORE_TLS_LDM32
      || fixP->fx_r_type == BFD_RELOC_CKCORE_TLS_LDO32)
    return 0;

  if (do_use_branchstub)
    {
      if (fixP->fx_r_type == BFD_RELOC_CKCORE_PCREL_IMM26BY2
          && (symbol_get_bfdsym (fixP->fx_addsy)->flags & BSF_FUNCTION))
          return 0;
    }

  return 1;
}

void
md_apply_fix (fixS   *fixP,
              valueT *valP,
              segT   seg)
{
  reloc_howto_type *howto;
  /* Note: use offsetT because it is signed, valueT is unsigned.  */
  offsetT val = *valP;
  char *buf = fixP->fx_frag->fr_literal + fixP->fx_where;
  /* if fx_done = 0, fixup will also be processed in
   * tc_gen_reloc() after md_apply_fix(). */

  fixP->fx_done = 0;

  /* If the fix is relative to a symbol which is not defined, or not
     in the same segment as the fix, we cannot resolve it here.  */
  if (IS_CSKY_V1 (mach_flag) && fixP->fx_addsy != NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
          || (S_GET_SEGMENT (fixP->fx_addsy) != seg)))
    {
      switch (fixP->fx_r_type)
       {
         /* data fx_addnumber is geater than 16 bits,
         so fx_addnumber is assigned zero  that pubilc code ignore the following check */
         case BFD_RELOC_CKCORE_PCREL_JSR_IMM11BY2:
          *valP=0;
          break;
         case BFD_RELOC_CKCORE_TLS_IE32:
         case BFD_RELOC_CKCORE_TLS_LDM32:
         case BFD_RELOC_CKCORE_TLS_GD32:
           {
              struct tls_addend * ta;
              ta = (struct tls_addend*)(fixP->fx_offset);
              fixP->fx_offset= (fixP->fx_frag->fr_address + fixP->fx_where)
                                - (ta->frag->fr_address + ta->offset);
           }
         case BFD_RELOC_CKCORE_TLS_LE32:
         case BFD_RELOC_CKCORE_TLS_LDO32:
           S_SET_THREAD_LOCAL (fixP->fx_addsy);
           break;
         default:
           break;
       }
#ifdef OBJ_ELF
      /* For ELF we can just return and let the reloc that will be generated
         take care of everything.  For COFF we still have to insert 'val'
         into the insn since the addend field will be ignored.  */
      return;
#endif
    }

  /* We can handle these relocs.  */
  switch (fixP->fx_r_type)
    {
      case BFD_RELOC_CKCORE_PCREL32:
        break;
      case BFD_RELOC_VTABLE_INHERIT:
        fixP->fx_r_type = BFD_RELOC_CKCORE_GNU_VTINHERIT;
        if (fixP->fx_addsy && !S_IS_DEFINED (fixP->fx_addsy)
            && !S_IS_WEAK (fixP->fx_addsy))
          S_SET_WEAK (fixP->fx_addsy);
        break;
      case BFD_RELOC_VTABLE_ENTRY:
        fixP->fx_r_type = BFD_RELOC_CKCORE_GNU_VTENTRY;
        break;
      case BFD_RELOC_CKCORE_GOT12:
      case BFD_RELOC_CKCORE_PLT12:
      case BFD_RELOC_CKCORE_ADDR_HI16:
      case BFD_RELOC_CKCORE_ADDR_LO16:
      case BFD_RELOC_CKCORE_TOFFSET_LO16:
      case BFD_RELOC_CKCORE_DOFFSET_LO16:
      case BFD_RELOC_CKCORE_GOT_HI16:
      case BFD_RELOC_CKCORE_GOT_LO16:
      case BFD_RELOC_CKCORE_PLT_HI16:
      case BFD_RELOC_CKCORE_PLT_LO16:
      case BFD_RELOC_CKCORE_GOTPC_HI16:
      case BFD_RELOC_CKCORE_GOTPC_LO16:
      case BFD_RELOC_CKCORE_GOTOFF_HI16:
      case BFD_RELOC_CKCORE_GOTOFF_LO16:
      case BFD_RELOC_CKCORE_DOFFSET_IMM18:
      case BFD_RELOC_CKCORE_DOFFSET_IMM18BY2:
      case BFD_RELOC_CKCORE_DOFFSET_IMM18BY4:
      case BFD_RELOC_CKCORE_GOTOFF_IMM18:
      case BFD_RELOC_CKCORE_GOT_IMM18BY4:
      case BFD_RELOC_CKCORE_PLT_IMM18BY4:
        break;
      case BFD_RELOC_CKCORE_TLS_IE32:
      case BFD_RELOC_CKCORE_TLS_LDM32:
      case BFD_RELOC_CKCORE_TLS_GD32:
        {
          struct tls_addend * ta;
          ta = (struct tls_addend*)(fixP->fx_offset);
          fixP->fx_offset= (fixP->fx_frag->fr_address + fixP->fx_where)
                             - (ta->frag->fr_address + ta->offset);
        }
      case BFD_RELOC_CKCORE_TLS_LE32:
      case BFD_RELOC_CKCORE_TLS_LDO32:
        S_SET_THREAD_LOCAL (fixP->fx_addsy);
        break;
      case BFD_RELOC_32:
        fixP->fx_r_type = BFD_RELOC_CKCORE_ADDR32;
      case BFD_RELOC_16:
      case BFD_RELOC_8:
        if (fixP->fx_addsy == NULL)
          {
            if (fixP->fx_size == 4)
              ;
            else if (fixP->fx_size == 2 && val >= -32768 && val <= 32767)
              ;
            else if (fixP->fx_size == 1 && val >= -256 && val <= 255)
              ;
            else
                abort ();
              md_number_to_chars (buf, val, fixP->fx_size);
            fixP->fx_done = 1;
          }
        break;
      case BFD_RELOC_CKCORE_PCREL_JSR_IMM11BY2:
        {
          if (fixP->fx_addsy == 0 && val > (-2 KB) && val < 2 KB)
            {
              long nval = (val >> 1) & 0x7ff;
              nval |= CSKYV1_INST_BSR;
              csky_write_insn (buf, nval, 2);
              fixP->fx_done = 1;
            }
          else
            {
              *valP = 0;
            }
        }
        break;
      case BFD_RELOC_CKCORE_PCREL_JSR_IMM26BY2:
        {
          if (fixP->fx_addsy == 0)
            {
              if ((val >= -(1 << 26)) && (val < (1 << 26)))
                {
                  unsigned int nval = ((val + fixP->fx_size) >> 1) & 0x3ffffff;
                  nval |= CSKYV2_INST_BSR32;

                  csky_write_insn (buf, nval, 4);
                }
              /* If bsr32 cannot reach,
                 generate 'lrw r25,label;jsr r25' instead of 'jsri label'.  */
              else if ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_810)
                {
                  valueT opcode = csky_read_insn (buf, 4);
                  opcode = (opcode & howto->dst_mask) | CSKYV2_INST_JSRI_TO_LRW;
                  csky_write_insn (buf, opcode, 4);
                  opcode = CSKYV2_INST_JSR_R26;
                  csky_write_insn (buf + 4, opcode, 4);
                }
              fixP->fx_done = 1;
            }
        }
        break;

      default:
        {
          howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);

          if (fixP->fx_addsy)
            break;

          if (howto == NULL)
            {
              if ((fixP->fx_size == 4)
                  || (fixP->fx_size == 2 && val >= -32768 && val <= 32767)
                  || (fixP->fx_size == 1 && val >= -256 && val <= 255))
                {
                  md_number_to_chars (buf, val, fixP->fx_size);
                  fixP->fx_done = 1;
                  break;
                }
              else
                abort ();
            }

          if (IS_CSKY_V2(mach_flag))
            val += fixP->fx_size;

          if (howto->rightshift == 2)
            val += 2;

          val >>= howto->rightshift;

          offsetT min,max;
          unsigned int issigned=0;
          switch(fixP->fx_r_type)
            {
              /* Offset is unsigned. */
              case BFD_RELOC_CKCORE_PCREL_IMM8BY4:
              case BFD_RELOC_CKCORE_PCREL_IMM10BY4:
              case BFD_RELOC_CKCORE_PCREL_IMM16BY4:
                max = (offsetT) howto->dst_mask;
                min = 0;
                break;
              /* lrw16.  */
              case BFD_RELOC_CKCORE_PCREL_IMM7BY4:
                if (do_extend_lrw)
                  max = (offsetT)((1<<(howto->bitsize + 1)) - 2);
                else
                  max = (offsetT)((1<<howto->bitsize) - 1);
                min = 0;
                break;
              /* flrws, flrwd, the offset code divid to two part.*/
              case BFD_RELOC_CKCORE_PCREL_FLRW_IMM8BY4:
                max = (offsetT)((1<<howto->bitsize) - 1);
                min = 0;
                break;
              /* Offset is signed. */
              default:
                max = (offsetT)(howto->dst_mask >> 1);
                min = - max - 1;
                issigned = 1;
            }
          if (val < min || val > max)
            {
              csky_branch_report_error (fixP->fx_file, fixP->fx_line,
                                        fixP->fx_addsy, val);
              return;
            }
          valueT opcode = csky_read_insn(buf,fixP->fx_size);
          /* Clear redundant bits brought from the last
             operation if there is any.  */
          if (do_extend_lrw && ((opcode &0xfc00) == CSKYV2_INST_LRW16))
            val &= 0xff;
          else
            val &= (issigned?howto->dst_mask:max);


          if((fixP->fx_size == 2)&&((opcode &0xfc00) == CSKYV2_INST_LRW16))
            {
              /* 8 bit offset lrw16.  */
              if (val >= 0x80)
                csky_write_insn(buf, ((~val & 0x1f)
                                      | ((~val & 0x60)<<3) | (opcode & 0xe0)),
                                fixP->fx_size);
              /* 7 bit offset lrw16.  */
              else
                csky_write_insn(buf, (val & 0x1f) |((val & 0x60)<<3)| opcode,
                                fixP->fx_size);
            }
          else if((fixP->fx_size == 4)&&((opcode &0xfe1ffe00) == CSKYV2_INST_FLRW))
            csky_write_insn(buf, ((val & 0xf)<<4)|((val & 0xf0)<<17)|opcode,
                            fixP->fx_size);
          else
            csky_write_insn(buf, val | opcode, fixP->fx_size);
          fixP->fx_done = 1;
          break;
        }
    }
  fixP->fx_addnumber = val;
}

/* Translate internal representation of relocation info to BFD target
   format.  */

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixP)
{
  arelent *rel;

  rel = xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  rel->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
  rel->addend=fixP->fx_offset;
  if (rel->howto == NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
                    _("Cannot represent relocation type %s"),
                    bfd_get_reloc_code_name (fixP->fx_r_type));

      /* Set howto to a garbage value so that we can keep going.  */
      rel->howto = bfd_reloc_type_lookup (stdoutput, BFD_RELOC_32);
    }
  gas_assert(rel->howto!=NULL);
  rel->address = fixP->fx_frag->fr_address + fixP->fx_where;
  return rel;
}

/* Relax a fragment by scanning TC_GENERIC_RELAX_TABLE.  */
long
csky_relax_frag (segT segment, fragS *fragP, long stretch)
{
  const relax_typeS *this_type;
  const relax_typeS *start_type;
  relax_substateT next_state;
  relax_substateT this_state;
  offsetT growth;
  offsetT aim;
  addressT target;
  addressT address;
  symbolS *symbolP;
  const relax_typeS *table;

  target = fragP->fr_offset;
  address = fragP->fr_address;
  table = TC_GENERIC_RELAX_TABLE;
  this_state = fragP->fr_subtype;
  start_type = this_type = table + this_state;
  symbolP = fragP->fr_symbol;

  if (symbolP)
    {
      fragS *sym_frag;

      sym_frag = symbol_get_frag (symbolP);

#ifndef DIFF_EXPR_OK
      know (sym_frag != NULL);
#endif
      know (S_GET_SEGMENT (symbolP) != absolute_section
            || sym_frag == &zero_address_frag);
      target += S_GET_VALUE (symbolP);

      /* If SYM_FRAG has yet to be reached on this pass, assume it
         will move by STRETCH just as we did, unless there is an
         alignment frag between here and SYM_FRAG.  An alignment may
         well absorb any STRETCH, and we don't want to choose a larger
         branch insn by overestimating the needed reach of this
         branch.  It isn't critical to calculate TARGET exactly;  We
         know we'll be doing another pass if STRETCH is non-zero.  */

      if (stretch != 0
          && sym_frag->relax_marker != fragP->relax_marker
          && S_GET_SEGMENT (symbolP) == segment)
        {
          fragS *f;

          /* Adjust stretch for any alignment frag.  Note that if have
             been expanding the earlier code, the symbol may be
             defined in what appears to be an earlier frag.  FIXME:
             This doesn't handle the fr_subtype field, which specifies
             a maximum number of bytes to skip when doing an
             alignment.  */
          for (f = fragP; f != NULL && f != sym_frag; f = f->fr_next)
            {
              if (f->fr_type == rs_align || f->fr_type == rs_align_code)
                {
                  if (stretch < 0)
                    stretch = - ((- stretch) & ~ ((1 << (int) f->fr_offset) - 1));
                  else
                    stretch &= ~ ((1 << (int) f->fr_offset) - 1);
                }
              if (stretch == 0)
                break;
            }
          if (f != 0)
            target += stretch;
        }
    }

  aim = target - address - fragP->fr_fix;

/* If the fragP->fr_symbol is extern symbol, aim should be 0.  */
  if (fragP->fr_symbol && S_GET_SEGMENT (symbolP) != segment)
    {
      aim = 0;
    }

  if (aim < 0)
    {
      /* Look backwards.  */
      for (next_state = this_type->rlx_more; next_state;)
        if (aim >= this_type->rlx_backward)
          next_state = 0;
        else
          {
            /* Grow to next state.  */
            this_state = next_state;
            this_type = table + this_state;
            next_state = this_type->rlx_more;
          }
    }
  else
    {
      /* Look forwards.  */
      for (next_state = this_type->rlx_more; next_state;)
        if (aim <= this_type->rlx_forward)
          next_state = 0;
        else
          {
            /* Grow to next state.  */
            this_state = next_state;
            this_type = table + this_state;
            next_state = this_type->rlx_more;
          }
    }

  growth = this_type->rlx_length - start_type->rlx_length;
  if (growth != 0)
    fragP->fr_subtype = this_state;
  return growth;
}

int
md_estimate_size_before_relax (fragS * fragp,
                               segT  segtype)
{
  switch (fragp->fr_subtype)
    {
      case COND_DISP10:
      case COND_DISP16:
      case UNCD_DISP10:
      case UNCD_DISP16:
      case JCOND_DISP10:
      case JCOND_DISP16:
      case JCOND_DISP32:
      case JUNCD_DISP10:
      case JUNCD_DISP16:
      case JUNCD_DISP32:
      case JCOMPZ_DISP16:
      case JCOMPZ_DISP32:
      case BSR_DISP26:
      case LRW_DISP7:
      case LRW2_DISP8:
      case LRW_DISP16:
        gas_assert(fragp->fr_symbol);
        if (IS_EXTERNAL_SYM(fragp->fr_symbol, segtype))
          {
            while (csky_relax_table[fragp->fr_subtype].rlx_more > RELAX_OVERFLOW)
              {
                fragp->fr_subtype = csky_relax_table[fragp->fr_subtype].rlx_more;
              }
          }
        return csky_relax_table[fragp->fr_subtype].rlx_length;
        break;

      /* C-SKY V1 relaxes.  */
      case C (UNCD_JUMP, UNDEF_DISP):
      case C (UNCD_JUMP_PIC, UNDEF_DISP):
        if (!fragp->fr_symbol)
            fragp->fr_subtype = C (UNCD_JUMP_S, DISP12);
        else if (S_GET_SEGMENT (fragp->fr_symbol) == segtype)
          fragp->fr_subtype = C (UNCD_JUMP_S, DISP12);
        else
          fragp->fr_subtype = C (UNCD_JUMP_S, UNDEF_WORD_DISP);
        break;

      case C (COND_JUMP, UNDEF_DISP):
      case C (COND_JUMP_PIC, UNDEF_DISP):
        if (fragp->fr_symbol
            && S_GET_SEGMENT (fragp->fr_symbol) == segtype)
          {
            /* Got a symbol and it's defined in this segment, become byte
               sized. Maybe it will fix up */
            fragp->fr_subtype = C (COND_JUMP_S, DISP12);
          }
        else if (fragp->fr_symbol)
          {
            /* Its got a segment, but its not ours, so it will always be
               long.  */
            fragp->fr_subtype = C (COND_JUMP_S, UNDEF_WORD_DISP);
          }
        else
          {
            /* We know the abs value.  */
            fragp->fr_subtype = C (COND_JUMP_S, DISP12);
          }
        break;

      case C (UNCD_JUMP, DISP12):
      case C (UNCD_JUMP, DISP32):
      case C (UNCD_JUMP, UNDEF_WORD_DISP):
      case C (COND_JUMP, DISP12):
      case C (COND_JUMP, DISP32):
      case C (COND_JUMP, UNDEF_WORD_DISP):
      case C (UNCD_JUMP_PIC, DISP12):
      case C (UNCD_JUMP_PIC, DISP32):
      case C (UNCD_JUMP_PIC, UNDEF_WORD_DISP):
      case C (COND_JUMP_PIC, DISP12):
      case C (COND_JUMP_PIC, DISP32):
      case C (COND_JUMP_PIC, UNDEF_WORD_DISP):
      case RELAX_OVERFLOW:
        break;
      default:
        abort ();
    }
  return csky_relax_table[fragp->fr_subtype].rlx_length;
}

/* Parse opcode like: "op oprnd1, oprnd2, oprnd3".  */

static void
csky_macro_md_assemble (const char *op,
                        const char *oprnd1,
                        const char *oprnd2,
                        const char *oprnd3)
{
  char str[80];
  str[0] = '\0';
  strcat (str, op);
  if (oprnd1 != NULL)
    {
      strcat (str, " ");
      strcat (str, oprnd1);
      if (oprnd2 != NULL)
        {
          strcat (str, ",");
          strcat (str, oprnd2);
          if (oprnd3 != NULL)
            {
              strcat (str, ",");
              strcat (str, oprnd3);
            }
        }
    }
  md_assemble (str);
  return;
}

/* Get the string of operand.  */

static int
csky_get_macro_operand (char *src_s, char *dst_s, char end_sym)
{
  int nlen = 0;
  while (ISSPACE (*src_s))
    ++src_s;
  while(*src_s != end_sym)
    dst_s[nlen++] = *(src_s++);
  dst_s[nlen] = '\0';
  return nlen;
}

/* idly 4 -> idly4.  */

static void
csky_idly (void)
{
  char *s = csky_insn.opcode_end;
  if (!is_imm_over_range (&s, 4, 4, -1))
    {
      as_bad (_("operand must be 4."));
      return;
    }
  csky_macro_md_assemble ("idly4", NULL, NULL, NULL);
  return;
}

/* rolc rd, 1 or roltc rd, 1 -> addc rd, rd.  */

static void
csky_rolc (void)
{
  char reg[10];
  char *s = csky_insn.opcode_end;

  s += csky_get_macro_operand (s, reg, ',');
  ++s;

  if (is_imm_over_range (&s, 1, 1, -1))
    {
      csky_macro_md_assemble ("addc", reg, reg, NULL);
      return;
    }
  else
    {
      as_bad (_("operand must be 1."));
    }
}

/* sxtrb0(1)(2) r1, rx -> xtbr0(1)(2) r1,rx; sextb r1.  */

static void
csky_sxtrb (void)
{
  char reg1[10];
  char reg2[10];

  char *s = csky_insn.opcode_end;
  s += csky_get_macro_operand (s, reg1, ',');
  ++s;
  csky_get_macro_operand (s, reg2, '\0');

  csky_macro_md_assemble (csky_insn.macro->name + 1, reg1, reg2, NULL);
  csky_macro_md_assemble ("sextb", reg1, NULL, NULL);
  return;
}

static void
csky_movtf (void)
{
  char reg1[10];
  char reg2[10];
  char reg3[10];

  char *s = csky_insn.opcode_end;
  s += csky_get_macro_operand (s, reg1, ',');
  ++s;

  s += csky_get_macro_operand (s, reg2, ',');
  ++s;

  s += csky_get_macro_operand (s, reg3, '\0');
  ++s;
  csky_macro_md_assemble ("movt", reg1, reg2, NULL);
  csky_macro_md_assemble ("movf", reg1, reg3, NULL);
  return;
}

static bfd_boolean
get_macro_reg_vals (int *reg1, int *reg2, int *reg3)
{
  int nlen;
  char *s = csky_insn.opcode_end;

  *reg1 = csky_get_reg_val (s, &nlen);
  s += nlen;
  if (*s != ',')
    {
      csky_show_info (ERROR_MISSING_COMMA, 0, NULL, NULL);
      return FALSE;
    }
  s++;
  *reg2 = csky_get_reg_val (s, &nlen);
  s += nlen;
  if (*s != ',')
    {
      csky_show_info (ERROR_MISSING_COMMA, 0, NULL, NULL);
      return FALSE;
    }
  s++;
  *reg3 = csky_get_reg_val (s, &nlen);
  s += nlen;
  if (*s != '\0')
    {
      csky_show_info (ERROR_BAD_END, 0, NULL, NULL);
      return FALSE;
    }
  if (*reg1 == -1 || *reg2 == -1 || *reg3 == -1)
    {
      as_bad ("%s register is over range.", csky_insn.opcode_end);
      return FALSE;
    }
  if (*reg1 != *reg2)
    {
      as_bad("the first two registers must be same number.");
      return FALSE;
    }
  if((*reg1 >= 15) || (*reg3 >= 15))
    {
      as_bad("64 bit operator src/dst register be less than 15.");
      return FALSE;
    }
  return TRUE;
}

/* addc64 rx, rx, ry -> cmplt rx, rx, addc  rx, ry, addc  rx+1, ry+1.  */

static void
csky_addc64 (void)
{
  int reg1;
  int reg2;
  int reg3;

  if (!get_macro_reg_vals (&reg1, &reg2, &reg3))
    return;
  csky_macro_md_assemble ("cmplt",
                          csky_general_reg[reg1],
                          csky_general_reg[reg1],
                          NULL);
  csky_macro_md_assemble ("addc",
                          csky_general_reg[reg1 + (target_big_endian ? 1 : 0)],
                          csky_general_reg[reg3 + (target_big_endian ? 1 : 0)],
                          NULL);
  csky_macro_md_assemble ("addc",
                          csky_general_reg[reg1 + (target_big_endian ? 0 : 1)],
                          csky_general_reg[reg3 + (target_big_endian ? 0 : 1)],
                          NULL);
  return;
}

/* subc64 rx, rx, ry -> cmphs rx, rx, subc  rx, ry, subc  rx+1, ry+1.  */

static void
csky_subc64 (void)
{
  int reg1;
  int reg2;
  int reg3;

  if (!get_macro_reg_vals (&reg1, &reg2, &reg3))
    return;
  csky_macro_md_assemble ("cmphs",
                          csky_general_reg[reg1],
                          csky_general_reg[reg1],
                          NULL);
  csky_macro_md_assemble ("subc",
                          csky_general_reg[reg1 + (target_big_endian ? 1 : 0)],
                          csky_general_reg[reg3 + (target_big_endian ? 1 : 0)],
                          NULL);
  csky_macro_md_assemble ("subc",
                          csky_general_reg[reg1 + (target_big_endian ? 0 : 1)],
                          csky_general_reg[reg3 + (target_big_endian ? 0 : 1)],
                          NULL);
  return;
}

/* or64 rx, rx, ry -> or rx, ry, or rx+1, ry+1.  */

static void
csky_or64 (void)
{
  int reg1;
  int reg2;
  int reg3;

  if (!get_macro_reg_vals (&reg1, &reg2, &reg3))
    return;
  csky_macro_md_assemble ("or",
                          csky_general_reg[reg1 + (target_big_endian ? 1 : 0)],
                          csky_general_reg[reg3 + (target_big_endian ? 1 : 0)],
                          NULL);
  csky_macro_md_assemble ("or",
                          csky_general_reg[reg1 + (target_big_endian ? 0 : 1)],
                          csky_general_reg[reg3 + (target_big_endian ? 0 : 1)],
                          NULL);
  return;
}

/* xor64 rx, rx, ry -> xor rx, ry, xor rx+1, ry+1.  */

static void
csky_xor64 (void)
{
  int reg1;
  int reg2;
  int reg3;

  if (!get_macro_reg_vals (&reg1, &reg2, &reg3))
    return;
  csky_macro_md_assemble ("xor",
                          csky_general_reg[reg1 + (target_big_endian ? 1 : 0)],
                          csky_general_reg[reg3 + (target_big_endian ? 1 : 0)],
                          NULL);
  csky_macro_md_assemble ("xor",
                          csky_general_reg[reg1 + (target_big_endian ? 0 : 1)],
                          csky_general_reg[reg3 + (target_big_endian ? 0 : 1)],
                          NULL);
  return;
}

/* v2 macro instructions:  */
/* neg rd -> not rd, rd; addi rd, 1.  */

static void
csky_neg (void)
{
  char reg1[10];

  char *s = csky_insn.opcode_end;
  s += csky_get_macro_operand (s, reg1, '\0');
  ++s;

  csky_macro_md_assemble ("not", reg1, reg1, NULL);
  csky_macro_md_assemble ("addi", reg1, "1", NULL);
  return;
}

/* rsubi rd, imm16 -> not rd; addi rd, imm16 + 1  */

static void
csky_rsubi (void)
{
  char reg1[10];
  char str_imm16[20];
  unsigned int imm16;
  expressionS e;
  char *s = csky_insn.opcode_end;
  s += csky_get_macro_operand (s, reg1, ',');
  ++s;

  s = parse_exp (s, &e);
  if (e.X_op == O_constant)
    imm16 = e.X_add_number;
  else
    csky_show_info (ERROR_IMM_ILLEGAL, 2, NULL, NULL);

  sprintf (str_imm16, "%d", imm16 + 1);

  csky_macro_md_assemble ("not", reg1, reg1, NULL);
  csky_macro_md_assemble ("addi", reg1, str_imm16, NULL);
  return;
}

/* Such as: asrc rd -> asrc rd, rd, 1.  */

static void
csky_arith (void)
{
  char reg1[10];
  char *s = csky_insn.opcode_end;
  s += csky_get_macro_operand (s, reg1, '\0');
  ++s;
  csky_macro_md_assemble (csky_insn.macro->name , reg1, reg1, "1");
  return;
}

/* decne rd ->  if ck802: subi rd, 1; cmpnei rd, 0.
   else: decne rd, rd, 1  */

static void
csky_decne (void)
{
  char reg1[10];
  char *s = csky_insn.opcode_end;
  s += csky_get_macro_operand (s, reg1, '\0');
  ++s;
  if ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_802)
    {
      csky_macro_md_assemble ("subi", reg1, "1", NULL);
      csky_macro_md_assemble ("cmpnei", reg1, "0", NULL);
    }
  else
    csky_macro_md_assemble ("decne", reg1, reg1, "1");
  return;
}

/* If -mnolrw, lrw rd, imm -> movih rd, imm_hi16; ori rd, imm_lo16.  */

static void
csky_lrw (void)
{
  char reg1[10];
  char imm[40];
  char imm_hi16[40];
  char imm_lo16[40];

  char *s = csky_insn.opcode_end;
  s += csky_get_macro_operand (s, reg1, ',');
  ++s;
  s += csky_get_macro_operand (s, imm, '\0');
  ++s;

  imm_hi16[0] = '\0';
  strcat(imm_hi16,"(");
  strcat(imm_hi16,imm);
  strcat(imm_hi16,") >> 16");
  imm_lo16[0] = '\0';
  strcat(imm_lo16,"(");
  strcat(imm_lo16,imm);
  strcat(imm_lo16,") & 0xffff");

  csky_macro_md_assemble ("movih", reg1, imm_hi16, NULL);
  csky_macro_md_assemble ("ori", reg1, reg1, imm_lo16);

  return;
}

/* The followings are C-SKY v1 FPU instructions' works.  */

bfd_boolean
v1_work_lrw (void)
{
  int reg;
  int output_literal = csky_insn.val[1];

  reg = csky_insn.val[0];
  csky_insn.isize = 2;
  csky_insn.output = frag_more (2);
  if (csky_insn.e.X_op == O_constant
      && csky_insn.e.X_add_number <= 0x7f
      && csky_insn.e.X_add_number >= 0)
    {
      /* lrw to movi.  */
      csky_insn.inst = 0x6000 | reg | (csky_insn.e.X_add_number << 4);
    }
  else
    {
      csky_insn.inst = csky_insn.opcode->op16[0].opcode;
      csky_insn.inst |= reg << 8;
      if (output_literal)
        {
          int n = enter_literal (&csky_insn.e, 0, 0, 0);

          /* Create a reference to pool entry.  */
          csky_insn.e.X_op = O_symbol;
          csky_insn.e.X_add_symbol = poolsym;
          csky_insn.e.X_add_number = n << 2;
        }

      if (insn_reloc == BFD_RELOC_CKCORE_TLS_GD32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_IE32)
        {
          literal_insn_offset->tls_addend.frag  = frag_now;
          literal_insn_offset->tls_addend.offset =
            csky_insn.output - literal_insn_offset->tls_addend.frag->fr_literal;
        }
      fix_new_exp (frag_now, csky_insn.output - frag_now->fr_literal, 2,
                   &csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM8BY4);
    }
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);

  return TRUE;
}

bfd_boolean
v1_work_fpu_fo (void)
{
  int i = 0;
  int inst;
  int greg;
  char buff[50];
  struct _csky_opcode_info *opinfo = NULL;

  if (csky_insn.isize == 4)
    {
      opinfo = &csky_insn.opcode->op32[csky_insn.opcode_idx];
    }
  else if (csky_insn.isize == 2)
    {
      opinfo = &csky_insn.opcode->op16[csky_insn.opcode_idx];
    }

  /* Firstly, get genarel reg.  */
  for (i = 0;i < opinfo->operand_num; i++)
    {
      if (opinfo->oprnd.oprnds[i].type == OPRND_TYPE_GREG0_15)
        {
          greg = csky_insn.val[i];
        }
    }

  /* Secondly, get float inst.  */
  csky_generate_insn();
  inst = csky_insn.inst;

  /* Now get greg and inst, we can write instruction to floating uint.  */
  sprintf(buff, "lrw %s,0x%x", csky_general_reg[greg], inst);
  md_assemble(buff);
  sprintf(buff, "cpwir %s", csky_general_reg[greg]);
  md_assemble(buff);

  return FALSE;
}

bfd_boolean
v1_work_fpu_fo_fc (void)
{
  int i = 0;
  int inst;
  int greg;
  char buff[50];
  struct _csky_opcode_info *opinfo = NULL;

  if (csky_insn.isize == 4)
    {
      opinfo = &csky_insn.opcode->op32[csky_insn.opcode_idx];
    }
  else if (csky_insn.isize == 2)
    {
      opinfo = &csky_insn.opcode->op16[csky_insn.opcode_idx];
    }

  /* Firstly, get genarel reg.  */
  for (i = 0;i < opinfo->operand_num; i++)
    {
      if (opinfo->oprnd.oprnds[i].type == OPRND_TYPE_GREG0_15)
        {
          greg = csky_insn.val[i];
        }
    }

  /* Secondly, get float inst.  */
  csky_generate_insn();
  inst = csky_insn.inst;

  /* Now get greg and inst, we can write instruction to floating uint.  */
  sprintf(buff, "lrw %s,0x%x", csky_general_reg[greg], inst);
  md_assemble(buff);
  sprintf(buff, "cpwir %s", csky_general_reg[greg]);
  md_assemble(buff);
  sprintf(buff, "cprc");
  md_assemble(buff);

  return FALSE;
}

bfd_boolean
v1_work_fpu_write (void)
{
  int greg;
  int freg;
  char buff[50];

  greg = csky_insn.val[0];
  freg = csky_insn.val[1];

  /* Now get greg and freg, we can write instruction to floating uint.  */
  sprintf(buff, "cpwgr %s,%s", csky_general_reg[greg], csky_cp_reg[freg]);
  md_assemble(buff);

  return FALSE;
}

bfd_boolean
v1_work_fpu_read (void)
{
  int greg;
  int freg;
  char buff[50];

  greg = csky_insn.val[0];
  freg = csky_insn.val[1];
  /* Now get greg and freg, we can write instruction to floating uint.  */
  sprintf(buff, "cprgr %s,%s", csky_general_reg[greg], csky_cp_reg[freg]);
  md_assemble(buff);

  return FALSE;
}

bfd_boolean
v1_work_fpu_writed (void)
{
  int greg;
  int freg;
  char buff[50];

  greg = csky_insn.val[0];
  freg = csky_insn.val[1];

  if (greg & 0x1)
    {
      as_bad ("Even expected, but geven \"r%d\"", greg);
      return FALSE;
    }
  /* Now get greg and freg, we can write instruction to floating uint.  */
  if (target_big_endian)
    sprintf(buff, "cpwgr %s,%s", csky_general_reg[greg + 1], csky_cp_reg[freg]);
  else
    sprintf(buff, "cpwgr %s,%s", csky_general_reg[greg], csky_cp_reg[freg]);
  md_assemble(buff);
  if (target_big_endian)
    sprintf(buff, "cpwgr %s,%s", csky_general_reg[greg], csky_cp_reg[freg + 1]);
  else
    sprintf(buff, "cpwgr %s,%s", csky_general_reg[greg + 1], csky_cp_reg[freg + 1]);
  md_assemble(buff);

  return FALSE;
}

bfd_boolean
v1_work_fpu_readd (void)
{
  int greg;
  int freg;
  char buff[50];

  greg = csky_insn.val[0];
  freg = csky_insn.val[1];

  if (greg & 0x1)
    {
      as_bad ("Even expected, but geven \"r%d\"", greg);
      return FALSE;
    }
  /* Now get greg and freg, we can write instruction to floating uint.  */
  if (target_big_endian)
    sprintf(buff, "cprgr %s,%s", csky_general_reg[greg + 1], csky_cp_reg[freg]);
  else
    sprintf(buff, "cprgr %s,%s", csky_general_reg[greg], csky_cp_reg[freg]);
  md_assemble(buff);
  if (target_big_endian)
    sprintf(buff, "cprgr %s,%s", csky_general_reg[greg], csky_cp_reg[freg + 1]);
  else
    sprintf(buff, "cprgr %s,%s", csky_general_reg[greg + 1], csky_cp_reg[freg + 1]);
  md_assemble(buff);

  return FALSE;
}

/* The followings are for csky pseudo handling.  */

bfd_boolean
v1_work_jbsr (void)
{
  csky_insn.output = frag_more (2);
  if (do_force2bsr)
    {
      /* Generate fixup BFD_RELOC_CKCORE_PCREL_IMM11BY2.  */
      fix_new_exp (frag_now, csky_insn.output - frag_now->fr_literal,
                   2, & csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM11BY2);
    }
  else
    {
      /* Using jsri instruction.  */
      const char *name = "jsri";
      csky_insn.opcode = (struct _csky_opcode *)
        hash_find (csky_opcodes_hash, name);
      csky_insn.opcode_idx = 0;
      csky_insn.isize = 2;

      int n = enter_literal (&csky_insn.e, 1, 0, 0);

      /* Create a reference to pool entry.  */
      csky_insn.e.X_op = O_symbol;
      csky_insn.e.X_add_symbol = poolsym;
      csky_insn.e.X_add_number = n << 2;

      /* Generate fixup BFD_RELOC_CKCORE_PCREL_IMM8BY4.  */
      fix_new_exp (frag_now, csky_insn.output - frag_now->fr_literal,
                   2, & csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM8BY4);

      if (csky_insn.e.X_op != O_absent && do_jsri2bsr)
        {
          /* Generate fixup BFD_RELOC_CKCORE_PCREL_JSR_IMM11BY2. */
          fix_new_exp (frag_now, csky_insn.output - frag_now->fr_literal,
                       2, & (litpool + (csky_insn.e.X_add_number >> 2))->e,
                       1, BFD_RELOC_CKCORE_PCREL_JSR_IMM11BY2);
        }
    }
  csky_generate_insn ();

  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);

  return TRUE;
}

/* The followings are for csky v2 inst handling.  */

/* For nie/nir/ipush/ipop.  */

bfd_boolean
v2_work_istack (void)
{
  if (!do_intr_stack)
    {
      csky_show_info (ERROR_OPCODE_ILLEGAL, 0, NULL, NULL);
      return FALSE;
    }
  csky_insn.output = frag_more (csky_insn.isize);
  csky_insn.inst = csky_insn.opcode->op16[0].opcode;
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_btsti (void)
{
  if (!do_extend_lrw
      && (csky_insn.flag_force == INSN_OPCODE16F
          || (mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_801))
    {
      csky_show_info (ERROR_OPCODE_ILLEGAL, 0, NULL, NULL);
      return FALSE;
    }
  if (!do_extend_lrw && csky_insn.isize == 2)
    {
      csky_insn.isize = 4;
    }
  /* Generate relax or reloc if necessary.  */
  csky_generate_frags ();
  /* Generate the insn by mask.  */
  csky_generate_insn ();
  /* Write inst to frag.  */
  csky_write_insn (csky_insn.output,
                   csky_insn.inst,
                   csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_addi (void)
{
  csky_insn.isize = 2;
  if (csky_insn.number == 2)
    {
      if (csky_insn.val[0] == 14
          && (csky_insn.val[1] >=0 && csky_insn.val[1] <= 0x1fc)
          && ((csky_insn.val[1] & 0x3) == 0)
          && csky_insn.flag_force != INSN_OPCODE32F)
        {
          /* addi sp, sp, imm.  */
          csky_insn.inst = 0x1400 | ((csky_insn.val[1] >>2) & 0x1f);
          csky_insn.inst |= (csky_insn.val[1] << 1) & 0x300;
          csky_insn.output = frag_more (2);
        }
      else if (csky_insn.val[0] < 8
          && (csky_insn.val[1] >= 1 && csky_insn.val[1] <= 0x100)
          && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x2000 | (csky_insn.val[0] << 8);
          csky_insn.inst |=  (csky_insn.val[1] - 1);
          csky_insn.output = frag_more (2);
        }
      else if ((csky_insn.val[1] >= 1 && csky_insn.val[1] <= 0x10000)
               && csky_insn.flag_force != INSN_OPCODE16F)
        {
          csky_insn.inst = 0xe4000000 | (csky_insn.val[0] << 21);
          csky_insn.inst |= csky_insn.val[0] << 16;
          csky_insn.inst |= (csky_insn.val[1] - 1);
          csky_insn.isize = 4;
          csky_insn.output = frag_more (4);
        }
      else
        {
          csky_show_info (ERROR_OPERANDS_ILLEGAL, 0,
                          csky_insn.opcode_end, NULL);
          return FALSE;
        }
    }
  else if (csky_insn.number == 3)
    {
      if (csky_insn.val[0] == 14
          && csky_insn.val[1] == 14
          && (csky_insn.val[2] >=0 && csky_insn.val[2] <= 0x1fc)
          && ((csky_insn.val[2] & 0x3) == 0)
          && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x1400 | ((csky_insn.val[2] >>2) & 0x1f);
          csky_insn.inst |= (csky_insn.val[2] << 1) & 0x300;
          csky_insn.output = frag_more (2);
        }
      else if (csky_insn.val[0] < 8
               && csky_insn.val[1] == 14
               && (csky_insn.val[2] >=0 && csky_insn.val[2] <= 0x3fc)
               && ((csky_insn.val[2] & 0x3) == 0)
               && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x1800 | (csky_insn.val[0] << 8);
          csky_insn.inst |= csky_insn.val[2] >> 2;
          csky_insn.output = frag_more (2);
        }
      else if (csky_insn.val[0] < 8
               && (csky_insn.val[0] == csky_insn.val[1])
               && (csky_insn.val[2] >=1 && csky_insn.val[2] <= 0x100)
               && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x2000 | (csky_insn.val[0] << 8);
          csky_insn.inst |=  (csky_insn.val[2] - 1);
          csky_insn.output = frag_more (2);
        }
      else if (csky_insn.val[0] < 8
               && csky_insn.val[1] < 8
               && (csky_insn.val[2] >=1 && csky_insn.val[2] <= 0x8)
               && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x5802 | (csky_insn.val[0] << 5);
          csky_insn.inst |= csky_insn.val[1] << 8;
          csky_insn.inst |= (csky_insn.val[2] -1) << 2;
          csky_insn.output = frag_more (2);
        }
      else if (csky_insn.val[1] == 28
               && (csky_insn.val[2] >=1 && csky_insn.val[2] <= 0x40000)
               && csky_insn.flag_force != INSN_OPCODE16F)
        {
          csky_insn.inst = 0xcc1c0000 | (csky_insn.val[0] << 21);
          csky_insn.isize = 4;
          csky_insn.output = frag_more (4);
          if (insn_reloc == BFD_RELOC_CKCORE_GOTOFF)
            {
              fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                           4, &csky_insn.e, 0, BFD_RELOC_CKCORE_GOTOFF_IMM18);
            }
          else
            csky_insn.inst |= (csky_insn.val[2] - 1);
        }
      else if ((csky_insn.val[2] >=1 && csky_insn.val[2] <= 0x1000)
               && csky_insn.flag_force != INSN_OPCODE16F)
        {
          csky_insn.inst = 0xe4000000 | (csky_insn.val[0] << 21);
          csky_insn.inst |= csky_insn.val[1] << 16;
          csky_insn.inst |= (csky_insn.val[2] - 1);
          csky_insn.isize = 4;
          csky_insn.output = frag_more (4);
        }
      else
        {
          csky_show_info (ERROR_OPERANDS_ILLEGAL, 0,
                          (char *)csky_insn.opcode_end, NULL);
          return FALSE;
        }
    }
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);

  return TRUE;
}

bfd_boolean
v2_work_subi (void)
{
  csky_insn.isize = 2;
  if (csky_insn.number == 2)
    {
      if (csky_insn.val[0] == 14
          && (csky_insn.val[1] >=0 && csky_insn.val[2] <= 0x1fc)
          && ((csky_insn.val[1] & 0x3) == 0)
          && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x1420 | ((csky_insn.val[1] >>2) & 0x1f);
          csky_insn.inst |= (csky_insn.val[1] << 1) & 0x300;
        }
      else if (csky_insn.val[0] < 8
          && (csky_insn.val[1] >= 1 && csky_insn.val[1] <= 0x100)
          && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x2800 | (csky_insn.val[0] << 8);
          csky_insn.inst |=  (csky_insn.val[1] - 1);
        }
      else if ((csky_insn.val[1] >= 1 && csky_insn.val[1] <= 0x10000)
               && csky_insn.flag_force != INSN_OPCODE16F)
        {
          csky_insn.inst = 0xe4001000 | (csky_insn.val[0] << 21);
          csky_insn.inst |= csky_insn.val[0] << 16;
          csky_insn.inst |= (csky_insn.val[1] - 1);
          csky_insn.isize = 4;
        }
      else
        {
          csky_show_info (ERROR_OPERANDS_ILLEGAL, 0,
                          (char *)csky_insn.opcode_end, NULL);
          return FALSE;
        }
    }
  else if (csky_insn.number == 3)
    {
      if (csky_insn.val[0] == 14
          && csky_insn.val[1] == 14
          && (csky_insn.val[2] >=0 && csky_insn.val[2] <= 0x1fc)
          && ((csky_insn.val[2] & 0x3) == 0)
          && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x1420 | ((csky_insn.val[2] >>2) & 0x1f);
          csky_insn.inst |= (csky_insn.val[2] << 1) & 0x300;
        }

      else if (csky_insn.val[0] < 8
               && (csky_insn.val[0] == csky_insn.val[1])
               && (csky_insn.val[2] >=1 && csky_insn.val[2] <= 0x100)
               && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x2800 | (csky_insn.val[0] << 8);
          csky_insn.inst |=  (csky_insn.val[2] - 1);
        }
      else if (csky_insn.val[0] < 8
               && csky_insn.val[1] < 8
               && (csky_insn.val[2] >=1 && csky_insn.val[2] <= 0x8)
               && csky_insn.flag_force != INSN_OPCODE32F)
        {
          csky_insn.inst = 0x5803 | (csky_insn.val[0] << 5);
          csky_insn.inst |= csky_insn.val[1] << 8;
          csky_insn.inst |= (csky_insn.val[2] -1) << 2;
        }
      else if ((csky_insn.val[2] >=1 && csky_insn.val[2] <= 0x1000)
               && csky_insn.flag_force != INSN_OPCODE16F)
        {
          csky_insn.inst = 0xe4001000 | (csky_insn.val[0] << 21);
          csky_insn.inst |= csky_insn.val[1] << 16;
          csky_insn.inst |= (csky_insn.val[2] - 1);
          csky_insn.isize = 4;
        }
      else
        {
          csky_show_info (ERROR_OPERANDS_ILLEGAL, 0,
                          (char *)csky_insn.opcode_end, NULL);
          return FALSE;
        }
    }
  csky_insn.output = frag_more (csky_insn.isize);
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);

  return TRUE;
}

bfd_boolean
v2_work_add_sub (void)
{
  if (csky_insn.number == 3
           && (csky_insn.val[0] == csky_insn.val[1]
              || csky_insn.val[0] == csky_insn.val[2])
           && csky_insn.val[0] <= 15
           && csky_insn.val[1] <= 15
           && csky_insn.val[2] <= 15)
    {
      if (!strstr(csky_insn.opcode->mnemonic, "sub")
          || (csky_insn.val[0] == csky_insn.val[1]))
        {
          csky_insn.opcode_idx = 0;
          csky_insn.isize = 2;
          if (csky_insn.val[0] == csky_insn.val[1])
            csky_insn.val[1] = csky_insn.val[2];

          csky_insn.number = 2;

        }
    }
  if (csky_insn.isize == 4
      && (mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_801)
    {
      if (csky_insn.number == 3)
        {
          if (csky_insn.val[0] > 7)
            csky_show_info (ERROR_REG_OVER_RANGE, 1,
                  (void *)csky_general_reg[csky_insn.val[0]], NULL);
          if (csky_insn.val[1] > 7)
            csky_show_info (ERROR_REG_OVER_RANGE, 2,
                  (void *)csky_general_reg[csky_insn.val[1]], NULL);
          if (csky_insn.val[2] > 7)
            csky_show_info (ERROR_REG_OVER_RANGE, 3,
                  (void *)csky_general_reg[csky_insn.val[2]], NULL);
        }
      else
        {
          if (csky_insn.val[0] > 15)
            csky_show_info (ERROR_REG_OVER_RANGE, 1,
                  (void *)csky_general_reg[csky_insn.val[0]], NULL);
          if (csky_insn.val[1] > 15)
            csky_show_info (ERROR_REG_OVER_RANGE, 2,
                  (void *)csky_general_reg[csky_insn.val[1]], NULL);
        }
      return FALSE;
    }
  /* sub rz, rx.  */
  /* Generate relax or reloc if necessary.  */
  csky_generate_frags ();
  /* Generate the insn by mask.  */
  csky_generate_insn ();
  /* Write inst to frag.  */
  csky_write_insn (csky_insn.output,
                   csky_insn.inst,
                   csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_rotlc (void)
{
  const char *name = "addc";
  csky_insn.opcode = (struct _csky_opcode *)
      hash_find (csky_opcodes_hash, name);
  csky_insn.opcode_idx = 0;
  if (csky_insn.isize == 2)
    {
      /* addc rz, rx.  */
      csky_insn.number = 2;
      csky_insn.val[1] = csky_insn.val[0];
    }
  else
    {
      csky_insn.number = 3;
      /* addc rz, rx, ry.  */
      csky_insn.val[1] = csky_insn.val[0];
      csky_insn.val[2] = csky_insn.val[0];
    }
  /* Generate relax or reloc if necessary.  */
  csky_generate_frags ();
  /* Generate the insn by mask.  */
  csky_generate_insn ();
  /* Write inst to frag.  */
  csky_write_insn (csky_insn.output,
                   csky_insn.inst,
                   csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_bgeni (void)
{
  const char *name = NULL;
  int imm = csky_insn.val[1];
  int val = 1 << imm;
  if (imm < 16)
      name = "movi";
  else
    {
      name = "movih";
      val >>= 16;
    }
  csky_insn.opcode = (struct _csky_opcode *)
      hash_find (csky_opcodes_hash, name);
  csky_insn.opcode_idx = 0;
  csky_insn.val[1] = val;

  /* Generate relax or reloc if necessary.  */
  csky_generate_frags ();
  /* Generate the insn by mask.  */
  csky_generate_insn ();
  /* Write inst to frag.  */
  csky_write_insn (csky_insn.output,
                   csky_insn.inst,
                   csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_not (void)
{
  const char *name = "nor";
  csky_insn.opcode = (struct _csky_opcode *)
      hash_find (csky_opcodes_hash, name);
  csky_insn.opcode_idx = 0;
  if (csky_insn.number == 1)
    {
      csky_insn.val[1] = csky_insn.val[0];
      if (csky_insn.val[0] < 16)
        {
          /* 16 bits nor rz, rz.  */
          csky_insn.number = 2;
          csky_insn.isize = 2;
        }
      else
        {
          csky_insn.val[2] = csky_insn.val[0];
          csky_insn.number = 3;
          csky_insn.isize = 4;
        }
    }
  if (csky_insn.number == 2)
    {
      if (csky_insn.val[0] == csky_insn.val[1]
          && csky_insn.val[0] < 16)
        {
          /* 16 bits nor rz, rz.  */
          csky_insn.number = 2;
          csky_insn.isize = 2;
        }
      else
        {
          csky_insn.val[2] = csky_insn.val[1];
          csky_insn.number = 3;
          csky_insn.isize = 4;
        }
    }

  /* Generate relax or reloc if necessary.  */
  csky_generate_frags ();
  /* Generate the insn by mask.  */
  csky_generate_insn ();
  /* Write inst to frag.  */
  csky_write_insn (csky_insn.output,
                   csky_insn.inst,
                   csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_jbtf (void)
{
  if (csky_insn.e.X_add_symbol == NULL || csky_insn.e.X_op == O_constant)
    {
      csky_show_info (ERROR_UNDEFINE, 0, (void *)"operand is invalid", NULL);
      return FALSE;
    }

  if ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_801)
    {
      /* Genarate fixup.  */
      csky_insn.inst = csky_insn.opcode->op16[0].opcode;
      csky_insn.output = frag_more (2);
      csky_insn.isize = 2;
      fix_new_exp(frag_now, csky_insn.output-frag_now->fr_literal,
                  2, &csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM10BY2);
    }
  else if (do_long_jump)
    {
      /* Generate relax with jcondition.  */
      csky_insn.output = frag_var (rs_machine_dependent,
                                   JCOND_DISP32_LEN,
                                   JCOND_DISP10_LEN,
                                   JCOND_DISP10,
                                   csky_insn.e.X_add_symbol,
                                   csky_insn.e.X_add_number,
                                   0);
      csky_insn.isize = 2;
      csky_insn.max = JCOND_DISP32_LEN;
      csky_insn.inst = csky_insn.opcode->op16[0].opcode;
    }
  else
    {
      /* Generate relax with condition.  */
      csky_insn.output = frag_var (rs_machine_dependent,
                                   COND_DISP16_LEN,
                                   COND_DISP10_LEN,
                                   COND_DISP10,
                                   csky_insn.e.X_add_symbol,
                                   csky_insn.e.X_add_number,
                                   0);
      csky_insn.isize = 2;
      csky_insn.max = COND_DISP16_LEN;
      csky_insn.inst = csky_insn.opcode->op16[0].opcode;

    }
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);

  return TRUE;
}

bfd_boolean
v2_work_jbr (void)
{
  if (csky_insn.e.X_add_symbol == NULL || csky_insn.e.X_op == O_constant)
    {
      csky_show_info (ERROR_UNDEFINE, 0, (void *)"operand is invalid", NULL);
      return FALSE;
    }

  if (do_long_jump
      && (mach_flag & CSKY_ARCH_MASK) != CSKY_ARCH_801
      && (mach_flag & CSKY_ARCH_MASK) != CSKY_ARCH_802)
    {
      csky_insn.output = frag_var (rs_machine_dependent,
                                   JUNCD_DISP32_LEN,
                                   JUNCD_DISP10_LEN,
                                   JUNCD_DISP10,
                                   csky_insn.e.X_add_symbol,
                                   csky_insn.e.X_add_number,
                                   0);

      csky_insn.inst = csky_insn.opcode->op16[0].opcode;
      csky_insn.max = JUNCD_DISP32_LEN;
      csky_insn.isize = 2;
    }
  else
    {
      /* Generate relax with condition.  */
      csky_insn.output = frag_var (rs_machine_dependent,
                                   UNCD_DISP16_LEN,
                                   UNCD_DISP10_LEN,
                                   UNCD_DISP10,
                                   csky_insn.e.X_add_symbol,
                                   csky_insn.e.X_add_number,
                                   0);
      csky_insn.isize = 2;
      csky_insn.max = UNCD_DISP16_LEN;
      csky_insn.inst = csky_insn.opcode->op16[0].opcode;

    }
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);
  return TRUE;
}

#define SIZE_V2_MOVI16(x)         ((addressT)x <= 0xff)
#define SIZE_V2_MOVI32(x)         ((addressT)x <= 0xffff)
#define SIZE_V2_MOVIH(x)          ((addressT)x <= 0xffffffff && (((addressT)x & 0xffff) == 0))

bfd_boolean
v2_work_lrw (void)
{
  int reg = csky_insn.val[0];
  int output_literal = csky_insn.val[1];
  int is_done = 0;

  /* If the second operand is O_constant, We can use movi/moih
     instead of lrw.  */
  if (csky_insn.e.X_op == O_constant)
    {
      /* 801 only has movi16.  */
      if (SIZE_V2_MOVI16 (csky_insn.e.X_add_number) && reg < 8)
        {
          /* movi16 instead.  */
          csky_insn.output = frag_more (2);
          csky_insn.inst = CSKYV2_INST_MOVI16 | (reg << 8)
            | (csky_insn.e.X_add_number);
          csky_insn.isize = 2;
          is_done = 1;
        }
      else if (SIZE_V2_MOVI32 (csky_insn.e.X_add_number)
               && ((mach_flag & CSKY_ARCH_MASK) != CSKY_ARCH_801))
        {
          /* movi32 instead.  */
          csky_insn.output = frag_more (4);
          csky_insn.inst = CSKYV2_INST_MOVI32 | (reg << 16)
            | (csky_insn.e.X_add_number);
          csky_insn.isize = 4;
          is_done = 1;
        }
      else if (SIZE_V2_MOVIH (csky_insn.e.X_add_number)
               && ((mach_flag & CSKY_ARCH_MASK) != CSKY_ARCH_801))
        {
          /* movih instead.  */
          csky_insn.output = frag_more (4);
          csky_insn.inst = CSKYV2_INST_MOVIH | (reg << 16)
            | ((csky_insn.e.X_add_number >> 16) & 0xffff);
          csky_insn.isize = 4;
          is_done = 1;
        }
    }

  if (is_done)
    goto normal_out;

  if (output_literal)
    {
      int n = enter_literal (&csky_insn.e, 0, 0, 0);
      /* Create a reference to pool entry.  */
      csky_insn.e.X_op = O_symbol;
      csky_insn.e.X_add_symbol = poolsym;
      csky_insn.e.X_add_number = n << 2;
    }
  /* If 16bit force.  */
  if (csky_insn.flag_force == INSN_OPCODE16F)
    {
      /* Generate fixup.  */
      if (reg > 7)
        {
          csky_show_info (ERROR_UNDEFINE, 0, (void *)"The register is out of range.", NULL);
          return FALSE;
        }
      csky_insn.isize = 2;
      csky_insn.output = frag_more(2);

      if (insn_reloc == BFD_RELOC_CKCORE_TLS_GD32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_IE32)
        {
          literal_insn_offset->tls_addend.frag = frag_now;
          literal_insn_offset->tls_addend.offset = csky_insn.output-frag_now->fr_literal;
        }
      csky_insn.inst = csky_insn.opcode->op16[0].opcode | (reg << 5);
      csky_insn.max = 4;
      fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                   2, &csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM7BY4);
    }
  else if (csky_insn.flag_force == INSN_OPCODE32F)
    {
      csky_insn.isize = 4;
      csky_insn.output = frag_more(4);
      if( insn_reloc == BFD_RELOC_CKCORE_TLS_GD32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
          || insn_reloc == BFD_RELOC_CKCORE_TLS_IE32 )
       {
          literal_insn_offset->tls_addend.frag = frag_now;
          literal_insn_offset->tls_addend.offset =
            csky_insn.output-frag_now->fr_literal;
       }
      csky_insn.inst = csky_insn.opcode->op32[0].opcode | (reg << 16);
      fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                   4, &csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM16BY4);
    }
  else if (!is_done)
    {
      if (reg < 8)
        {
          csky_insn.isize = 2;

          if (insn_reloc == BFD_RELOC_CKCORE_TLS_GD32
              || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
              || insn_reloc == BFD_RELOC_CKCORE_TLS_IE32)
            {
              literal_insn_offset->tls_addend.frag = frag_now;
            }

          csky_insn.output = frag_var(rs_machine_dependent,
                                      LRW_DISP16_LEN,
                                      LRW_DISP7_LEN,
                                      (do_extend_lrw?LRW2_DISP8:LRW_DISP7),
                                      csky_insn.e.X_add_symbol,
                                      csky_insn.e.X_add_number, 0);
          if (insn_reloc == BFD_RELOC_CKCORE_TLS_GD32
              || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
              || insn_reloc == BFD_RELOC_CKCORE_TLS_IE32)
            {
              if (literal_insn_offset->tls_addend.frag->fr_next != frag_now)
                literal_insn_offset->tls_addend.frag = literal_insn_offset->tls_addend.frag->fr_next;
              literal_insn_offset->tls_addend.offset =
                csky_insn.output - literal_insn_offset->tls_addend.frag->fr_literal;
            }
          csky_insn.inst = csky_insn.opcode->op16[0].opcode | (reg << 5);
          csky_insn.max = LRW_DISP16_LEN;
          csky_insn.isize = 2;
        }
      else
        {
          csky_insn.isize = 4;
          csky_insn.output = frag_more(4);
          if( insn_reloc == BFD_RELOC_CKCORE_TLS_GD32
              || insn_reloc == BFD_RELOC_CKCORE_TLS_LDM32
              || insn_reloc == BFD_RELOC_CKCORE_TLS_IE32 )
           {
              literal_insn_offset->tls_addend.frag = frag_now;
              literal_insn_offset->tls_addend.offset =
                csky_insn.output-frag_now->fr_literal;
           }
          csky_insn.inst = csky_insn.opcode->op32[0].opcode | (reg << 16);
          fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                       4, &csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM16BY4);
       }
    }

normal_out:
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);

  return TRUE;
}

bfd_boolean
v2_work_lrsrsw (void)
{
  int reg = csky_insn.val[0];
  csky_insn.output = frag_more(4);
  csky_insn.inst = csky_insn.opcode->op32[0].opcode | ( reg << 21);
  csky_insn.isize = 4;

  switch (insn_reloc)
    {
      case BFD_RELOC_CKCORE_GOT32:
        fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                     4, &csky_insn.e, 0, BFD_RELOC_CKCORE_GOT_IMM18BY4);
        break;
      case BFD_RELOC_CKCORE_PLT32:
        fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                     4, &csky_insn.e, 0, BFD_RELOC_CKCORE_PLT_IMM18BY4);
        break;
      default:
        fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                     4, &csky_insn.e, 1, BFD_RELOC_CKCORE_DOFFSET_IMM18BY4);
        break;
    }
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_jbsr (void)
{
  if (do_force2bsr
      || ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_801)
      || ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_802))
    {
      csky_insn.output = frag_more (4);
      fix_new_exp (frag_now, csky_insn.output - frag_now->fr_literal,
                   4, &csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM26BY2);
      csky_insn.isize = 4;
      csky_insn.inst = CSKYV2_INST_BSR32;
    }
  else
    {
      csky_insn.output = frag_more (4);
      int n = enter_literal (&csky_insn.e, 0, 0, 0);
      csky_insn.e.X_op = O_symbol;
      csky_insn.e.X_add_symbol = poolsym;
      csky_insn.e.X_add_number = n << 2;
      fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                 4, &csky_insn.e, 1, BFD_RELOC_CKCORE_PCREL_IMM16BY4);
      if (do_jsri2bsr
          || ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_810))
        {
          fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                       4,
                       &(litpool+(csky_insn.e.X_add_number >> 2))->e,
                       1,
                       BFD_RELOC_CKCORE_PCREL_JSR_IMM26BY2);
        }
      csky_insn.inst = CSKYV2_INST_JSRI32;
      csky_insn.isize = 4;
      if ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_810)
        {
          csky_write_insn(csky_insn.output, csky_insn.inst, csky_insn.isize);
          csky_insn.output = frag_more(4);
          dwarf2_emit_insn (0);
          /* Insert "mov r0, r0".  */
          csky_insn.inst = CSKYV2_INST_MOV_R0_R0;
          csky_insn.max = 8;
        }
    }
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);
  return TRUE;
}

bfd_boolean
v2_work_jsri (void)
{
  /* dump literal.  */
  int n = enter_literal (&csky_insn.e, 1, 0, 0);
  csky_insn.e.X_op = O_symbol;
  csky_insn.e.X_add_symbol = poolsym;
  csky_insn.e.X_add_number = n << 2;

  /* Generate relax or reloc if necessary.  */
  csky_generate_frags ();
  /* Generate the insn by mask.  */
  csky_generate_insn ();
  /* Write inst to frag.  */
  csky_write_insn (csky_insn.output,
                   csky_insn.inst,
                   csky_insn.isize);
  /* control 810 not to generate jsri */
  if ((mach_flag & CSKY_ARCH_MASK) == CSKY_ARCH_810)
    {
      /* Look at adding the R_PCREL_JSRIMM26BY2.
         For 'jbsr .L1', this reolc type's symbol
         is bound to '.L1', isn't bound to literal pool */
      fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                   4, &(litpool+(csky_insn.e.X_add_number >> 2))->e, 1,
                   BFD_RELOC_CKCORE_PCREL_JSR_IMM26BY2);
      csky_insn.output = frag_more(4);
      dwarf2_emit_insn (0);
      /*The opcode of "mov32 r0,r0".  */
      csky_insn.inst = CSKYV2_INST_MOV_R0_R0;
      /*The affect of this value is to check literal*/
      csky_write_insn (csky_insn.output,
                 csky_insn.inst,
                 csky_insn.isize);
      csky_insn.max = 8;
    }
  return TRUE;
}

bfd_boolean
v2_work_movih (void)
{
  int rz = csky_insn.val[0];
  csky_insn.output = frag_more (4);
  csky_insn.inst = csky_insn.opcode->op32[0].opcode | (rz << 16);
  if (csky_insn.e.X_op == O_constant)
    {
      if ((csky_insn.e.X_unsigned == 1) && (csky_insn.e.X_add_number > 0xffff))
        {
          csky_show_info (ERROR_IMM_OVERFLOW, 2, NULL, NULL);
          return FALSE;
        }
      else if ((csky_insn.e.X_unsigned == 0) && (csky_insn.e.X_add_number < 0))
        {
          csky_show_info (ERROR_IMM_OVERFLOW, 2, NULL, NULL);
          return FALSE;
        }
      else
        csky_insn.inst |= (csky_insn.e.X_add_number & 0xffff);
    }
  else if ((csky_insn.e.X_op == O_right_shift)
           || ((csky_insn.e.X_op == O_symbol)
               && (insn_reloc != BFD_RELOC_NONE)))
    {
      if ((csky_insn.e.X_op_symbol != 0)
          && (csky_insn.e.X_op_symbol->sy_value.X_op == O_constant)
          && (16 == csky_insn.e.X_op_symbol->sy_value.X_add_number))
        {
          csky_insn.e.X_op = O_symbol;
          if (insn_reloc == BFD_RELOC_CKCORE_GOT32)
            insn_reloc = BFD_RELOC_CKCORE_GOT_HI16;
          else if (insn_reloc == BFD_RELOC_CKCORE_PLT32)
            insn_reloc = BFD_RELOC_CKCORE_PLT_HI16;
          else if(insn_reloc == BFD_RELOC_CKCORE_GOTPC)
            insn_reloc = BFD_RELOC_CKCORE_GOTPC_HI16;
          else if(insn_reloc == BFD_RELOC_CKCORE_GOTOFF)
            insn_reloc = BFD_RELOC_CKCORE_GOTOFF_HI16;
          else
            insn_reloc = BFD_RELOC_CKCORE_ADDR_HI16;
          fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                       4, &csky_insn.e, 0, insn_reloc);
        }
      else
        {
          void *arg = (void *)"the second operand must be \"SYMBOL >> 16\"";
          csky_show_info (ERROR_UNDEFINE, 0, arg, NULL);
          return FALSE;
        }
    }
  csky_insn.isize = 4;
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);

  return TRUE;
}

bfd_boolean
v2_work_ori (void)
{
  int rz = csky_insn.val[0];
  int rx = csky_insn.val[1];
  csky_insn.output = frag_more (4);
  csky_insn.inst = csky_insn.opcode->op32[0].opcode | (rz << 21) | (rx << 16);
  if (csky_insn.e.X_op == O_constant)
    {
      if (csky_insn.e.X_add_number <= 0xffff && csky_insn.e.X_add_number >= 0)
        csky_insn.inst |= csky_insn.e.X_add_number;
      else
        {
          csky_show_info (ERROR_IMM_OVERFLOW, 3, NULL, NULL);
          return FALSE;
        }
    }
  else if (csky_insn.e.X_op == O_bit_and)
    {
      if ((csky_insn.e.X_op_symbol->sy_value.X_op == O_constant)
          && (0xffff == csky_insn.e.X_op_symbol->sy_value.X_add_number))
        {
          csky_insn.e.X_op = O_symbol;
          if (insn_reloc == BFD_RELOC_CKCORE_GOT32)
            insn_reloc = BFD_RELOC_CKCORE_GOT_LO16;
          else if (insn_reloc == BFD_RELOC_CKCORE_PLT32)
            insn_reloc = BFD_RELOC_CKCORE_PLT_LO16;
          else if(insn_reloc == BFD_RELOC_CKCORE_GOTPC)
            insn_reloc = BFD_RELOC_CKCORE_GOTPC_LO16;
          else if(insn_reloc == BFD_RELOC_CKCORE_GOTOFF)
            insn_reloc = BFD_RELOC_CKCORE_GOTOFF_LO16;
          else
            insn_reloc = BFD_RELOC_CKCORE_ADDR_LO16;
          fix_new_exp (frag_now, csky_insn.output-frag_now->fr_literal,
                       4, &csky_insn.e, 0, insn_reloc);
        }
      else
        {
          void *arg = (void *)"the third operand must be \"SYMBOL & 0xffff\"";
          csky_show_info (ERROR_UNDEFINE, 0, arg, NULL);
          return FALSE;
        }
    }
  csky_insn.isize = 4;
  csky_write_insn (csky_insn.output, csky_insn.inst, csky_insn.isize);
  return TRUE;
}

/* The followings are for csky pseudo handling.  */

static void
csky_pool_count (void (*func) (int), int arg)
{
  const fragS *curr_frag = frag_now;
  offsetT added = -frag_now_fix_octets ();

  (*func) (arg);

  while (curr_frag != frag_now)
    {
      added += curr_frag->fr_fix;
      curr_frag = curr_frag->fr_next;
    }

  added += frag_now_fix_octets ();
  poolspan += added;
}

static void
csky_s_literals (int ignore ATTRIBUTE_UNUSED)
{
  dump_literals (0);
  demand_empty_rest_of_line ();
}

static void
csky_stringer (int append_zero)
{
  if (now_seg == text_section)
    csky_pool_count (stringer, append_zero);
  else
    stringer (append_zero);

  /* We call check_literals here in case a large number of strings are
     being placed into the text section with a sequence of stringer
     directives.  In theory we could be upsetting something if these
     strings are actually in an indexed table instead of referenced by
     individual labels.  Let us hope that that never happens.  */
  check_literals (2, 0);
}

static void
csky_cons (int nbytes)
{
  mapping_state (MAP_DATA);
  if(nbytes == 4)  /* @GOT.  */
    {
      do
        {
          bfd_reloc_code_real_type reloc;
          expressionS exp;

          reloc = BFD_RELOC_NONE;
          expression (& exp);
          lex_got(&reloc, NULL);

          if (exp.X_op == O_symbol
              && (reloc) != BFD_RELOC_NONE)
            {
              reloc_howto_type *howto = bfd_reloc_type_lookup (stdoutput, reloc);
              int size = bfd_get_reloc_size (howto);

              if (size > nbytes)
                as_bad ("%s relocations do not fit in %d bytes", howto->name, nbytes);
              else
              {
                register char *p = frag_more ((int) nbytes);
                int offset = nbytes - size;

                fix_new_exp (frag_now, p - frag_now->fr_literal + offset, size, &exp, 0, reloc);
              }
            }
          else
            {
              emit_expr (&exp, (unsigned int) nbytes);
            }
          if (now_seg == text_section)
            poolspan += nbytes;
        }
      while (*input_line_pointer++ == ',');

      /* Put terminator back into stream.  */
      input_line_pointer --;
      demand_empty_rest_of_line ();

      return;
    }

  if (now_seg == text_section)
    csky_pool_count (cons, nbytes);
  else
    cons (nbytes);

  /* In theory we ought to call check_literals (2,0) here in case
     we need to dump the literal table.  We cannot do this however,
     as the directives that we are intercepting may be being used
     to build a switch table, and we must not interfere with its
     contents.  Instead we cross our fingers and pray...  */
}

static void
csky_float_cons (int float_type)
{
  mapping_state(MAP_DATA);
  if (now_seg == text_section)
    csky_pool_count (float_cons, float_type);
  else
    float_cons (float_type);

  /* See the comment in csky_cons () about calling check_literals.
      It is unlikely that a switch table will be constructed using
      floating point values, but it is still likely that an indexed
      table of floating point constants is being created by these
      directives, so again we must not interfere with their placement.  */
}

static void
csky_fill (int unused)
{
  if (now_seg == text_section)
    csky_pool_count (s_fill, unused);
  else
    s_fill (unused);

  check_literals (2, 0);
}

/* Handle the section changing pseudo-ops.  These call through to the
   normal implementations, but they dump the literal pool first.  */

static void
csky_s_text (int ignore)
{
  dump_literals (0);

#ifdef OBJ_ELF
  obj_elf_text (ignore);
#else
  s_text (ignore);
#endif
}

static void
csky_s_data (int ignore)
{
  dump_literals (0);

#ifdef OBJ_ELF
  obj_elf_data (ignore);
#else
  s_data (ignore);
#endif
}

static void
csky_s_section (int ignore)
{
  /* Scan forwards to find the name of the section.  If the section
     being switched to is ".line" then this is a DWARF1 debug section
     which is arbitrarily placed inside generated code.  In this case
     do not dump the literal pool because it is a) inefficient and
     b) would require the generation of extra code to jump around the
     pool.  */
  char * ilp = input_line_pointer;

  while (*ilp != 0 && ISSPACE (*ilp))
    ++ ilp;

  if (strncmp (ilp, ".line", 5) == 0
      && (ISSPACE (ilp[5]) || *ilp == '\n' || *ilp == '\r'))
    ;
  else
    dump_literals (0);

#ifdef OBJ_ELF
  obj_elf_section (ignore);
#endif
#ifdef OBJ_COFF
  obj_coff_section (ignore);
#endif
}

static void
csky_s_bss (int needs_align)
{
  dump_literals (0);

  s_lcomm_bytes (needs_align);
}

#ifdef OBJ_ELF
static void
csky_s_comm (int needs_align)
{
  dump_literals (0);
  obj_elf_common (needs_align);
}
#endif

static void
csky_noliteraldump (int ignore ATTRIBUTE_UNUSED)
{
  do_noliteraldump = 1;
  int insn_num = get_absolute_expression ();
  /* The insn after '.no_literal_dump insn_num' is insn1,
   * Don't dump literal pool between insn1 and insn(insn_num+1)
   * The insn can not be the insn generate literal, like lrw & jsri*/
  check_literals (0, insn_num * 2);
}

/*check literals before do alignment.
 * for example, if '.align n', add (2^n-1) to poolspan and check literals*/

static void
csky_s_align_ptwo (int arg)
{
  /*get the .align's first absolute number*/
  char * temp_pointer = input_line_pointer;
  int align = get_absolute_expression ();
  check_literals (0, (1 << align)-1);
  input_line_pointer = temp_pointer;

  /*do alignment*/
  s_align_ptwo (arg);
}

static void
csky_stack_size (int arg ATTRIBUTE_UNUSED)
{
  expressionS exp;
  stack_size_entry *sse = (stack_size_entry *)
      xcalloc (1, sizeof (stack_size_entry));

  expression (&exp);
  if (exp.X_op == O_symbol)
    sse->function = exp.X_add_symbol;
  else
    {
      as_bad (_("the first operand must be a symbol"));
      ignore_rest_of_line ();
      free (sse);
      return;
    }

  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("missing stack size"));
      ignore_rest_of_line ();
      free (sse);
      return;
    }

  ++input_line_pointer;
  expression (&exp);
  if (exp.X_op == O_constant)
    {
      if (exp.X_add_number < 0 || exp.X_add_number > 0xffffffff)
        {
          as_bad (_("the second operand must be 0x0-0xffffffff"));
          ignore_rest_of_line ();
          free (sse);
          return;
        }
      else
        {
          sse->stack_size = exp.X_add_number;
        }
    }
  else
    {
      as_bad (_("the second operand must be a constant"));
      ignore_rest_of_line ();
      free (sse);
      return;
    }

  if (*last_stack_size_data != NULL)
    last_stack_size_data = &((*last_stack_size_data)->next);

  *last_stack_size_data = sse;
}

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
     pseudo-op name without dot
     function to call to execute this pseudo-op
     Integer arg to pass to the function.  */

const pseudo_typeS md_pseudo_table[] =
{
  { "export",   s_globl,          0 },
  { "import",   s_ignore,         0 },
  { "literals", csky_s_literals, 0 },
  { "page",     listing_eject,    0 },

  /* The following are to intercept the placement of data into the text
     section (eg addresses for a switch table), so that the space they
     occupy can be taken into account when deciding whether or not to
     dump the current literal pool.
     XXX - currently we do not cope with the .space and .dcb.d directives.  */
  { "ascii",    csky_stringer,       8 + 0 },
  { "asciz",    csky_stringer,       8 + 1 },
  { "byte",     csky_cons,           1 },
  { "dc",       csky_cons,           2 },
  { "dc.b",     csky_cons,           1 },
  { "dc.d",     csky_float_cons,    'd'},
  { "dc.l",     csky_cons,           4 },
  { "dc.s",     csky_float_cons,    'f'},
  { "dc.w",     csky_cons,           2 },
  { "dc.x",     csky_float_cons,    'x'},
  { "double",   csky_float_cons,    'd'},
  { "float",    csky_float_cons,    'f'},
  { "hword",    csky_cons,           2 },
  { "int",      csky_cons,           4 },
  { "long",     csky_cons,           4 },
  { "octa",     csky_cons,          16 },
  { "quad",     csky_cons,           8 },
  { "short",    csky_cons,           2 },
  { "single",   csky_float_cons,    'f'},
  { "string",   csky_stringer,       8 + 1 },
  { "word",     csky_cons,           4 },
  { "fill",     csky_fill,           0 },

  /* Allow for the effect of section changes.  */
  { "text",      csky_s_text,    0 },
  { "data",      csky_s_data,    0 },
  { "bss",       csky_s_bss,     1 },
#ifdef OBJ_ELF
  { "comm",      csky_s_comm,    0 },
#endif
  { "section",   csky_s_section, 0 },
  { "section.s", csky_s_section, 0 },
  { "sect",      csky_s_section, 0 },
  { "sect.s",    csky_s_section, 0 },
  /* When ".no_literal_dump N" is in front of insn1,
   * and instruction sequence is:
   *               insn1
   *               insn2
   *               ......
   *               insnN+1
   * it means literal will not dump between insn1 and insnN+1
   * The insn can not be the insn generate literal, like lrw & jsri*/
  { "no_literal_dump",     csky_noliteraldump,     0 },
  { "align",     csky_s_align_ptwo, 0 },
  { "stack_size",csky_stack_size, 0},
  {0,       0,            0}
};


void csky_cfi_frame_initial_instructions (void)
{
  int sp_reg = IS_CSKY_V1 (mach_flag) ? 0 : 14;
  cfi_add_CFA_def_cfa_register(sp_reg);
}

int tc_csky_regname_to_dw2regnum (char *regname)
{
  int reg_num = -1;
  int len;

  /* FIXME the reg should be parsed according to
     the abi version.  */
  reg_num = csky_get_reg_val (regname, &len);
  return reg_num;
}
