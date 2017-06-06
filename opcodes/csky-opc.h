/* Declarations for CSKY opcode table
   Copyright (C) 2007-2016 Free Software Foundation, Inc.

   This file is part of the GNU opcodes library.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   It is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "opcode/csky.h"

#define OP_TABLE_NUM    2
#define MAX_OPRND_NUM   4

enum operand_type
{
  OPRND_TYPE_NONE = 0,
  /* control register.  */
  OPRND_TYPE_CTRLREG,
  /* r0 - r7.  */
  OPRND_TYPE_GREG0_7,
  /* r0 - r15.  */
  OPRND_TYPE_GREG0_15,
  /* r16 - r31.  */
  OPRND_TYPE_GREG16_31,
  /* r0 - r31.  */
  OPRND_TYPE_AREG,
  /* (rx).  */
  OPRND_TYPE_AREG_WITH_BRACKET,
  OPRND_TYPE_AREG_WITH_LSHIFT,
  OPRND_TYPE_AREG_WITH_LSHIFT_FPU,

  OPRND_TYPE_FREG_WITH_INDEX,
  /* r1 only, for xtrb0(1)(2)(3) in csky v1 ISA.  */
  OPRND_TYPE_REG_r1a,
  /* r1 only, for divs/divu in csky v1 ISA.  */
  OPRND_TYPE_REG_r1b,
  /* r28.  */
  OPRND_TYPE_REG_r28,
  OPRND_TYPE_REGr4_r7,
  /* sp register with bracket.  */
  OPRND_TYPE_REGbsp,
  /* sp register.  */
  OPRND_TYPE_REGsp,
  /* register with bracket.  */
  OPRND_TYPE_REGnr4_r7,
  /* not sp register.  */
  OPRND_TYPE_REGnsp,
  /* not lr register.  */
  OPRND_TYPE_REGnlr,
  /* not sp/lr register.  */
  OPRND_TYPE_REGnsplr,
  /* hi/lo register.  */
  OPRND_TYPE_REGhilo,

  /* cp index.  */
  OPRND_TYPE_CPIDX,
  /* cp regs.  */
  OPRND_TYPE_CPREG,
  /* cp cregs.  */
  OPRND_TYPE_CPCREG,
  /* fpu regs.  */
  OPRND_TYPE_FREG,
  /* fpu even regs.  */
  OPRND_TYPE_FEREG,
  /* float round mode.  */
  OPRND_TYPE_RM,
  /* PSR bits.  */
  OPRND_TYPE_PSR_BITS_LIST,

  /* Constant.  */
  OPRND_TYPE_CONSTANT,
  /* Floating Constant.  */
  OPRND_TYPE_FCONSTANT,
  /* [label].  */
  OPRND_TYPE_LABEL_WITH_BRACKET,
  /* The operand is the same as first reg. it is a dummy reg that won't output
     to binary code of the instruction. it will also used by disassemble.
     For example: bclri rz, rz, imm5 -> bclri rz, imm5.  */
  OPRND_TYPE_DUMMY_REG,
  /* The type of the operand is same as the 1st operand. if the val of the
     operand is same as the 1st operand, we can use 16bits inst to parse the
     opcode.
     For example: addc r1, r1, r2 -> addc16 r1, r2.  */
  OPRND_TYPE_2IN1_DUMMY,
  /* Output a reg same as the 1st reg.
     For example: addc r17, r1 -> addc32 r17, r17, r1.
     The old "addc" can not parse in 16bits instruction because 16bits "addc"
     only support regs from r0 to r15. So we use "addc32" which has
     3 operands, and duple the 1st operands to 2nd operand.  */
  OPRND_TYPE_DUP_GREG0_7,
  OPRND_TYPE_DUP_GREG0_15,
  OPRND_TYPE_DUP_AREG,
  /* Immediate.  */
  OPRND_TYPE_IMM2b,
  OPRND_TYPE_IMM3b,
  OPRND_TYPE_IMM4b,
  OPRND_TYPE_IMM5b,
  OPRND_TYPE_IMM7b,
  OPRND_TYPE_IMM8b,
  OPRND_TYPE_IMM12b,
  OPRND_TYPE_IMM15b,
  OPRND_TYPE_IMM16b,
  OPRND_TYPE_IMM18b,
  OPRND_TYPE_IMM32b,
  /* Immediate left shift 2bits.  */
  OPRND_TYPE_IMM7b_LS2,
  OPRND_TYPE_IMM8b_LS2,
  /* OPRND_TYPE_IMM5b_a_b means: Immediate in (a, b).  */
  OPRND_TYPE_IMM5b_1_31,
  OPRND_TYPE_IMM5b_7_31,
  /* operand type for rori and rotri.  */
  OPRND_TYPE_IMM5b_RORI,
  OPRND_TYPE_IMM5b_POWER,
  OPRND_TYPE_IMM5b_7_31_POWER,
  OPRND_TYPE_IMM5b_BMASKI,
  OPRND_TYPE_IMM8b_BMASKI,

  /* for v2 movih.  */
  OPRND_TYPE_IMM16b_MOVIH,
  /* for v2 ori.  */
  OPRND_TYPE_IMM16b_ORI,
  /* for v2 ld/st.  */
  OPRND_TYPE_IMM_LDST,
  OPRND_TYPE_IMM_FLDST,
  OPRND_TYPE_IMM2b_JMPIX,

  /* Offset for jump.  */
  OPRND_TYPE_OFF8b,
  OPRND_TYPE_OFF10b,
  OPRND_TYPE_OFF11b,
  OPRND_TYPE_OFF16b,
  OPRND_TYPE_OFF16b_LSL1,
  OPRND_TYPE_OFF26b,
  /* An immmediate or label.  */
  OPRND_TYPE_IMM_OFF18b,

  /* Offset immediate.  */
  OPRND_TYPE_OIMM3b,
  OPRND_TYPE_OIMM5b,
  OPRND_TYPE_OIMM8b,
  OPRND_TYPE_OIMM12b,
  OPRND_TYPE_OIMM16b,
  OPRND_TYPE_OIMM18b,
  /* For csky v2 idly.  */
  OPRND_TYPE_OIMM5b_IDLY,
  /* For v2 bmaski.  */
  OPRND_TYPE_OIMM5b_BMASKI,
  /* Constants.  */
  OPRND_TYPE_CONST1,

  /* PC relative offset.  */
  OPRND_TYPE_PCR_OFFSET_16K,
  OPRND_TYPE_PCR_OFFSET_64K,
  OPRND_TYPE_PCR_OFFSET_64M,

  OPRND_TYPE_CPFUNC,

  OPRND_TYPE_GOT_PLT,
  OPRND_TYPE_REGLIST_LDM,
  OPRND_TYPE_REGLIST_DASH,
  OPRND_TYPE_FREGLIST_DASH,
  OPRND_TYPE_REGLIST_COMMA,
  OPRND_TYPE_REGLIST_DASH_COMMA,

  OPRND_TYPE_BRACKET,
  OPRND_TYPE_ABRACKET,
  OPRND_TYPE_JBTF,
  OPRND_TYPE_JBR,
  OPRND_TYPE_JBSR,
  OPRND_TYPE_UNCOND10b,
  OPRND_TYPE_UNCOND16b,
  OPRND_TYPE_COND10b,
  OPRND_TYPE_COND16b,
  OPRND_TYPE_JCOMPZ,
  OPRND_TYPE_LSB2SIZE,
  OPRND_TYPE_MSB2SIZE,
  OPRND_TYPE_LSB,
  OPRND_TYPE_MSB,
};

/* Operands number.  */

enum operand_number
{
  OPRND_NUMBER_0 = 0,
  OPRND_NUMBER_1,
  OPRND_NUMBER_2,
  OPRND_NUMBER_3
};

/* Opcode infomation.  */
struct operand
{
  /* Mask for suboperand.  */
  int mask;
  /* Subperand type.  */
  int type;
  /* Operand shift.  */
  int shift;
};

struct soperand
{
  /* Mask for operand.  */
  int mask;
  /* Operand type.  */
  int type;
  /* Operand shift.  */
  int shift;
  /* Suboperand.  */
  struct operand subs[2];
};
union csky_operand
{
  struct operand oprnds[4];
  struct suboperand1
  {
    struct operand oprnd;
    struct soperand soprnd;
  } soprnd1;
  struct suboperand2
  {
    struct soperand soprnd;
    struct operand oprnd;
  } soprnd2;
};
struct _csky_opcode_info
{
  /* How many operands.  */
  long operand_num;
  /* The instruction opcode.  */
  unsigned int opcode;
  /* Operand infomation.  */
  union csky_operand oprnd;
};

/* csky instruction description template.  */
struct _csky_opcode
{
  /* The instruction name.  */
  const char *mnemonic;
  /* Whether can the insn be in front of literal, 0 : can't, >0 : can.  */
  int transfer;
  /* 16bits opcodes.  */
  struct _csky_opcode_info op16[OP_TABLE_NUM];
  /* 32bits opcodes.  */
  struct _csky_opcode_info op32[OP_TABLE_NUM];
  /* Instruction set flag.  */
  unsigned int isa_flag16;
  unsigned int isa_flag32;
  /* Whether this insn need relocation, 0: no, !=0: yes.  */
  signed int reloc16;
  signed int reloc32;
  /* Whether this insn need relax, 0: no, != 0: yes.   */
  signed int relax;
  /* Function to call when this instruction has special circumstances.  */
  bfd_boolean (*work)(void);
};

/* The followings are the opcode used in relax/fix process.  */
#define CSKYV1_INST_JMPI            0x7000
#define CSKYV1_INST_ADDI            0x2000
#define CSKYV1_INST_SUBI            0x2400
#define CSKYV1_INST_LDW             0x8000
#define CSKYV1_INST_STW             0x9000
#define CSKYV1_INST_BSR             0xf800
#define CSKYV1_INST_LRW             0x7000
#define CSKYV1_INST_ADDU            0x1c00
#define CSKYV1_INST_JMP             0x00c0
#define CSKYV1_INST_MOV_R1_RX       0x1201
#define CSKYV1_INST_MOV_RX_R1       0x1210

#define CSKYV2_INST_BT16            0x0800
#define CSKYV2_INST_BF16            0x0c00
#define CSKYV2_INST_BT32            0xe8600000
#define CSKYV2_INST_BF32            0xe8400000
#define CSKYV2_INST_BR32            0xe8000000
#define CSKYV2_INST_NOP             0x6c03
#define CSKYV2_INST_MOVI16          0x3000
#define CSKYV2_INST_MOVI32          0xea000000
#define CSKYV2_INST_MOVIH           0xea200000
#define CSKYV2_INST_LRW16           0x1000
#define CSKYV2_INST_LRW32           0xea800000
#define CSKYV2_INST_BSR32           0xe0000000
#define CSKYV2_INST_BR32            0xe8000000
#define CSKYV2_INST_FLRW            0xf4003800
#define CSKYV2_INST_JMPI32          0xeac00000
#define CSKYV2_INST_JSRI32          0xeae00000
#define CSKYV2_INST_JSRI_TO_LRW     0xea9a0000
#define CSKYV2_INST_JSR_R26         0xe8fa0000
#define CSKYV2_INST_MOV_R0_R0       0xc4004820


#define OPRND_SHIFT_0_BIT           0
#define OPRND_SHIFT_1_BIT           1
#define OPRND_SHIFT_2_BIT           2

#define OPRND_MASK_NONE             0x0
#define OPRND_MASK_0_1              0x3
#define OPRND_MASK_0_2              0x7
#define OPRND_MASK_0_3              0xf
#define OPRND_MASK_0_4              0x1f
#define OPRND_MASK_0_7              0xff
#define OPRND_MASK_0_8              0x1ff
#define OPRND_MASK_0_9              0x3ff
#define OPRND_MASK_0_10             0x7ff
#define OPRND_MASK_0_11             0xfff
#define OPRND_MASK_0_14             0x7fff
#define OPRND_MASK_0_15             0xffff
#define OPRND_MASK_0_17             0x3ffff
#define OPRND_MASK_0_25             0x3ffffff
#define OPRND_MASK_2_4              0x1c
#define OPRND_MASK_2_5              0x3c
#define OPRND_MASK_3_7              0xf8
#define OPRND_MASK_4_7              0xf0
#define OPRND_MASK_4_8              0x1f0
#define OPRND_MASK_4_10             0x7f0
#define OPRND_MASK_5_6              0x60
#define OPRND_MASK_5_7              0xe0
#define OPRND_MASK_5_9              0x3e0
#define OPRND_MASK_6_9              0x3c0
#define OPRND_MASK_8_9              0x300
#define OPRND_MASK_8_10             0x700
#define OPRND_MASK_8_11             0xf00
#define OPRND_MASK_10_11            0xc00
#define OPRND_MASK_10_14            0x7c00
#define OPRND_MASK_13_17            0x3e000
#define OPRND_MASK_16_19            0xf0000
#define OPRND_MASK_16_20            0x1f0000
#define OPRND_MASK_16_25            0x3ff0000
#define OPRND_MASK_21_24            0x1e00000
#define OPRND_MASK_21_25            0x3e00000
#define OPRND_MASK_RSV              0xffffffff
#define OPRND_MASK_0_3or21_24       OPRND_MASK_0_3 | OPRND_MASK_21_24
#define OPRND_MASK_0_4or21_25       OPRND_MASK_0_4 | OPRND_MASK_21_25
#define OPRND_MASK_0_4or16_20       OPRND_MASK_0_4 | OPRND_MASK_16_20
#define OPRND_MASK_0_4or8_10        OPRND_MASK_0_4 | OPRND_MASK_8_10
#define OPRND_MASK_0_4or8_9         OPRND_MASK_0_4 | OPRND_MASK_8_9
#define OPRND_MASK_0_14or16_20      OPRND_MASK_0_14 | OPRND_MASK_16_20
#define OPRND_MASK_2_5or6_9         OPRND_MASK_2_5 | OPRND_MASK_6_9
#define OPRND_MASK_4_7or21_24       OPRND_MASK_4_7 | OPRND_MASK_21_24
#define OPRND_MASK_5_6or21_25       OPRND_MASK_5_6 | OPRND_MASK_21_25
#define OPRND_MASK_5_7or8_10        OPRND_MASK_5_7 | OPRND_MASK_8_10
#define OPRND_MASK_5_9or21_25       OPRND_MASK_5_9 | OPRND_MASK_21_25
#define OPRND_MASK_16_19or21_24     OPRND_MASK_16_19 | OPRND_MASK_21_24
#define OPRND_MASK_16_20or21_25     OPRND_MASK_16_20 | OPRND_MASK_21_25

#define OPERAND_INFO(mask, type, shift)    {OPRND_MASK_##mask, OPRND_TYPE_##type, shift}

#define OPCODE_INFO_NONE() \
          {-2, 0, {{OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT), OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT), OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT), OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT)}}}
#define OPCODE_INFO(num, op, oprnd1, oprnd2, oprnd3, oprnd4) \
          {num, op, {OPERAND_INFO oprnd1, OPERAND_INFO oprnd2, OPERAND_INFO oprnd3, OPERAND_INFO oprnd4}}

#define OPCODE_INFO0(op) \
          {0, op, {{OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT), OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT), OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT) , OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT)}}}
#define OPCODE_INFO1(op, oprnd) \
          {1, op, {{OPERAND_INFO oprnd, OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT), OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT) , OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT)}}}
#define OPCODE_INFO2(op, oprnd1, oprnd2) \
          {2, op, {{OPERAND_INFO oprnd1, OPERAND_INFO oprnd2, OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT) , OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT)}}}
#define OPCODE_INFO3(op, oprnd1, oprnd2, oprnd3) \
          {3, op, {{OPERAND_INFO oprnd1, OPERAND_INFO oprnd2, OPERAND_INFO oprnd3 , OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT)}}}
#define OPCODE_INFO4(op, oprnd1, oprnd2, oprnd3, oprnd4) \
          {4, op, {{OPERAND_INFO oprnd1, OPERAND_INFO oprnd2, OPERAND_INFO oprnd3 , OPERAND_INFO oprnd4}}}
#define OPCODE_INFO_LIST(op, oprnd) \
          {-1, op, {{OPERAND_INFO oprnd, OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT), OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT) , OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT)}}}

#define BRACKET_OPRND(oprnd1, oprnd2)  \
          OPERAND_INFO(RSV, BRACKET, OPRND_SHIFT_0_BIT), OPERAND_INFO oprnd1, OPERAND_INFO oprnd2
#define ABRACKET_OPRND(oprnd1, oprnd2)  \
          OPERAND_INFO(RSV, ABRACKET, OPRND_SHIFT_0_BIT), OPERAND_INFO oprnd1, OPERAND_INFO oprnd2
#define SOPCODE_INFO1(op, soprnd) \
          {1, op, {{soprnd, OPERAND_INFO(NONE, NONE, OPRND_SHIFT_0_BIT)}}}
#define SOPCODE_INFO2(op, oprnd, soprnd) \
          {2, op, {{OPERAND_INFO oprnd, soprnd}}}

#define OP16(mnem, opcode16, isa)  \
          {mnem, _TRANSFER, {opcode16, OPCODE_INFO_NONE()}, \
                {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, isa, 0, _RELOC16, 0, _RELAX, NULL}
#ifdef BUILD_AS
#define OP16_WITH_WORK(mnem, opcode16, isa, work)  \
          {mnem, _TRANSFER, {opcode16, OPCODE_INFO_NONE()}, \
                {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, isa, 0, _RELOC16, 0, _RELAX, work}
#define OP32_WITH_WORK(mnem, opcode32, isa, work)  \
          {mnem, _TRANSFER, {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, \
                {opcode32, OPCODE_INFO_NONE()}, 0, isa, 0, _RELOC32, _RELAX, work}
#define OP16_OP32_WITH_WORK(mnem, opcode16, isa16, opcode32, isa32, work)  \
          {mnem, _TRANSFER, {opcode16, OPCODE_INFO_NONE()}, \
                {opcode32, OPCODE_INFO_NONE()}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, work}
#define DOP16_DOP32_WITH_WORK(mnem, opcode16a, opcode16b, isa16, opcode32a, opcode32b, isa32, work)  \
          {mnem, _TRANSFER, {opcode16a, opcode16b}, \
                {opcode32a, opcode32b}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, work}
#define DOP32_WITH_WORK(mnem, opcode32a, opcode32b, isa, work)  \
          {mnem, _TRANSFER, {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, \
                {opcode32a,opcode32b}, 0, isa, 0, _RELOC32, _RELAX, work}
#else
#define OP16_WITH_WORK(mnem, opcode16, isa, work)  \
          {mnem, _TRANSFER, {opcode16, OPCODE_INFO_NONE()}, \
                {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, isa, 0, _RELOC16, 0, _RELAX, NULL}
#define OP32_WITH_WORK(mnem, opcode32, isa, work)  \
          {mnem, _TRANSFER, {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, \
                {opcode32, OPCODE_INFO_NONE()}, 0, isa, 0, _RELOC32, _RELAX, NULL}
#define OP16_OP32_WITH_WORK(mnem, opcode16, isa16, opcode32, isa32, work)  \
          {mnem, _TRANSFER, {opcode16, OPCODE_INFO_NONE()}, \
                {opcode32, OPCODE_INFO_NONE()}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, NULL}
#define DOP16_DOP32_WITH_WORK(mnem, opcode16a, opcode16b, isa16, opcode32a, opcode32b, isa32, work)  \
          {mnem, _TRANSFER, {opcode16a, opcode16b}, \
                {opcode32a, opcode32b}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, NULL}
#define DOP32_WITH_WORK(mnem, opcode32a, opcode32b, isa, work)  \
          {mnem, _TRANSFER, {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, \
                {opcode32a,opcode32b}, 0, isa, 0, _RELOC32, _RELAX, NULL}
#endif

#define DOP16(mnem, opcode16_1, opcode16_2, isa)  \
          {mnem, _TRANSFER, {opcode16_1, opcode16_2}, \
                {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, isa, 0, _RELOC16, 0, _RELAX, NULL}
#define OP32(mnem, opcode32, isa)  \
          {mnem, _TRANSFER, {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, \
                {opcode32, OPCODE_INFO_NONE()}, 0, isa, 0, _RELOC32, _RELAX, NULL}
#define DOP32(mnem, opcode32a, opcode32b, isa)  \
          {mnem, _TRANSFER, {OPCODE_INFO_NONE(), OPCODE_INFO_NONE()}, \
                {opcode32a,opcode32b}, 0, isa, 0, _RELOC32, _RELAX, NULL}
#define OP16_OP32(mnem, opcode16, isa16, opcode32, isa32) \
          {mnem, _TRANSFER, {opcode16, OPCODE_INFO_NONE()}, \
                {opcode32, OPCODE_INFO_NONE()}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, NULL}
#define DOP16_OP32(mnem, opcode16a, opcode16b, isa16, opcode32, isa32) \
          {mnem, _TRANSFER, {opcode16a, opcode16b}, \
                {opcode32, OPCODE_INFO_NONE()}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, NULL}
#define OP16_DOP32(mnem, opcode16, isa16, opcode32a, opcode32b, isa32) \
          {mnem, _TRANSFER, {opcode16, OPCODE_INFO_NONE()}, \
                {opcode32a, opcode32b}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, NULL}
#define DOP16_DOP32(mnem, opcode16a, opcode16b, isa16, opcode32a, opcode32b, isa32) \
          {mnem, _TRANSFER, {opcode16a, opcode16b}, \
                {opcode32a, opcode32b}, isa16, isa32, _RELOC16, _RELOC32, _RELAX, NULL}

#define V1_REG_SP              0
#define V1_REG_LR             15

const char *csky_general_reg[] =
{
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
    NULL,
};

const char *cskyv2_general_alias_reg[] =
{
    "a0", "a1", "a2", "a3", "l0", "l1", "l2", "l3",
    "l4", "l5", "l6", "l7", "t0", "t1", "sp", "lr",
    "l8", "l9", "t2", "t3", "t4", "t5", "t6", "t7",
    "t8", "t9", "r26", "r27", "rdb", "gb", "r30", "r31",
    NULL,
};
const char *cskyv1_general_alias_reg[] =
{
    "sp", "r1", "a0", "a1", "a2", "a3", "a4", "a5",
    "fp", "l0", "l1", "l2", "l3", "l4", "gb", "lr",
    NULL,
};

const char *csky_fpu_reg[] =
{
    "fr0",  "fr1",  "fr2",  "fr3",  "fr4",  "fr5",  "fr6",  "fr7",
    "fr8",  "fr9",  "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
    "fr16", "fr17", "fr18", "fr19", "fr20", "fr21", "fr22", "fr23",
    "fr24", "fr25", "fr26", "fr27", "fr28", "fr29", "fr30", "fr31",
    NULL,
};

const char *csky_ctrl_reg[] =
{
    "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7",
    "cr8", "cr9", "cr10", "cr11", "cr12", "cr13", "cr14", "cr15",
    "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23"
    "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "cr31"
};
const char *csky_ctrl_reg_alias[] =
{
    "psr", "vbr", "epsr", "fpsr", "epc", "fpc", "ss0", "ss1",
    "ss2", "ss3", "ss4", "gcr", "gsr", "cpidr", "dcsr", "cwr",
    "cfr", "ccr", "capr", "pacr", "prsr", "cr21", "cr22", "cr23"
    "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "cr31"
};

const char *csky_cp_idx[] =
{
    "cp0", "cp1", "cp2", "cp3", "cp4", "cp5", "cp6", "cp7",
    "cp8", "cp9", "cp10", "cp11", "cp12", "cp13", "cp14", "cp15",
    "cp16", "cp17", "cp18", "cp19", "cp20",
    NULL,
};

const char *csky_cp_reg[] =
{
    "cpr0",  "cpr1",  "cpr2",  "cpr3",  "cpr4",  "cpr5",  "cpr6",  "cpr7",
    "cpr8",  "cpr9",  "cpr10", "cpr11", "cpr12", "cpr13", "cpr14", "cpr15",
    "cpr16", "cpr17", "cpr18", "cpr19", "cpr20", "cpr21", "cpr22", "cpr23",
    "cpr24", "cpr25", "cpr26", "cpr27", "cpr28", "cpr29", "cpr30", "cpr31",
    "cpr32", "cpr33", "cpr34", "cpr35", "cpr36", "cpr37", "cpr38", "cpr39",
    "cpr40", "cpr41", "cpr42", "cpr43", "cpr44", "cpr45", "cpr46", "cpr47",
    "cpr48", "cpr49", "cpr50", "cpr51", "cpr52", "cpr53", "cpr54", "cpr55",
    "cpr56", "cpr57", "cpr58", "cpr59", "cpr60", "cpr61", "cpr62", "cpr63",
    NULL,
};

const char *csky_cp_creg[] =
{
    "cpcr0",  "cpcr1",  "cpcr2",  "cpcr3",  "cpcr4",  "cpcr5",  "cpcr6",  "cpcr7",
    "cpcr8",  "cpcr9",  "cpcr10", "cpcr11", "cpcr12", "cpcr13", "cpcr14", "cpcr15",
    "cpcr16", "cpcr17", "cpcr18", "cpcr19", "cpcr20", "cpcr21", "cpcr22", "cpcr23",
    "cpcr24", "cpcr25", "cpcr26", "cpcr27", "cpcr28", "cpcr29", "cpcr30", "cpcr31",
    "cpcr32", "cpcr33", "cpcr34", "cpcr35", "cpcr36", "cpcr37", "cpcr38", "cpcr39",
    "cpcr40", "cpcr41", "cpcr42", "cpcr43", "cpcr44", "cpcr45", "cpcr46", "cpcr47",
    "cpcr48", "cpcr49", "cpcr50", "cpcr51", "cpcr52", "cpcr53", "cpcr54", "cpcr55",
    "cpcr56", "cpcr57", "cpcr58", "cpcr59", "cpcr60", "cpcr61", "cpcr62", "cpcr63",
    NULL,
};

struct psrbit
{
    int value;
    const char *name;
};
const struct psrbit cskyv1_psr_bits[] =
{
    {1 ,"ie"},
    {2 ,"fe"},
    {4 ,"ee"},
    {8 ,"af"},
    {0 ,NULL},
};
const struct psrbit cskyv2_psr_bits[] =
{
    {8 ,"ee"},
    {4 ,"ie"},
    {2 ,"fe"},
    {1 ,"af"},
    {0 ,NULL},
};


/* csky v1 opcodes.  */
const struct _csky_opcode csky_v1_opcodes[] =
{
#define _TRANSFER   0
#define _RELOC16    0
#define _RELOC32    0
#define _RELAX      0
  OP16("bkpt",    OPCODE_INFO0(0x0000), CSKYV1_ISA_E1),
  OP16("sync",    OPCODE_INFO0(0x0001), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER   2
  OP16("rfi",     OPCODE_INFO0(0x0003), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER   0
  OP16("stop",    OPCODE_INFO0(0x0004), CSKYV1_ISA_E1),
  OP16("wait",    OPCODE_INFO0(0x0005), CSKYV1_ISA_E1),
  OP16("doze",    OPCODE_INFO0(0x0006), CSKYV1_ISA_E1),
  OP16("idly4",   OPCODE_INFO0(0x0007), CSKYV1_ISA_E1),
  OP16("trap",    OPCODE_INFO1(0x0008, (0_1, IMM2b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mvtc",    OPCODE_INFO0(0x000c), CSKY_ISA_DSP),
  OP16("cprc",    OPCODE_INFO0(0x000d), CSKY_ISA_CP),
  OP16("cpseti",  OPCODE_INFO1(0x0010, (0_3, CPIDX, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
  OP16("mvc",     OPCODE_INFO1(0x0020, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mvcv",    OPCODE_INFO1(0x0030, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("ldq",     OPCODE_INFO2(0x0040, (NONE, REGr4_r7, OPRND_SHIFT_0_BIT), (0_3, REGnr4_r7, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("stq",     OPCODE_INFO2(0x0050, (NONE, REGr4_r7, OPRND_SHIFT_0_BIT), (0_3, REGnr4_r7, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("ldm",     OPCODE_INFO2(0x0060, (0_3, REGLIST_DASH, OPRND_SHIFT_0_BIT), (NONE, REGbsp, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("stm",     OPCODE_INFO2(0x0070, (0_3, REGLIST_DASH, OPRND_SHIFT_0_BIT), (NONE, REGbsp, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("dect",   OPCODE_INFO3(0x0080, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x0080, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("decf",   OPCODE_INFO3(0x0090, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x0090, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("inct",   OPCODE_INFO3(0x00a0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x00a0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("incf",   OPCODE_INFO3(0x00b0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x00b0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 2
  OP16("jmp",     OPCODE_INFO1(0x00c0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 0
  OP16("jsr",     OPCODE_INFO1(0x00d0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("ff1",    OPCODE_INFO2(0x00e0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x00e0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("brev",   OPCODE_INFO2(0x00f0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x00f0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("xtrb3",  OPCODE_INFO2(0x0100, (NONE, REG_r1a, OPRND_SHIFT_0_BIT), (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0100, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("xtrb2",  OPCODE_INFO2(0x0110, (NONE, REG_r1a, OPRND_SHIFT_0_BIT), (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0110, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("xtrb1",  OPCODE_INFO2(0x0120, (NONE, REG_r1a, OPRND_SHIFT_0_BIT), (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0120, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("xtrb0",  OPCODE_INFO2(0x0130, (NONE, REG_r1a, OPRND_SHIFT_0_BIT), (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0130, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("zextb",  OPCODE_INFO2(0x0140, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0140, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("sextb",  OPCODE_INFO2(0x0150, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0150, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("zexth",  OPCODE_INFO2(0x0160, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0160, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("sexth",  OPCODE_INFO2(0x0170, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x0170, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("declt",  OPCODE_INFO3(0x0180, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x0180, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("tstnbz",  OPCODE_INFO1(0x0190, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("decgt",  OPCODE_INFO3(0x01a0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x01a0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("decne",  OPCODE_INFO3(0x01b0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x01b0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("clrt",    OPCODE_INFO1(0x01c0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("clrf",    OPCODE_INFO1(0x01d0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("abs",    OPCODE_INFO2(0x01e0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x01e0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("not",    OPCODE_INFO2(0x01f0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT)), OPCODE_INFO1(0x01f0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("movt",    OPCODE_INFO2(0x0200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("mult",   OPCODE_INFO3(0x0300, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x0300, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mac",     OPCODE_INFO2(0x0400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC),
  DOP16("subu",   OPCODE_INFO3(0x0500, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x0500, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("sub",    OPCODE_INFO3(0x0500, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x0500, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("addc",   OPCODE_INFO3(0x0600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x0600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("subc",   OPCODE_INFO3(0x0700, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x0700, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cprgr",   OPCODE_INFO2(0x0800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, CPREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
  OP16("movf",    OPCODE_INFO2(0x0a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("lsr",    OPCODE_INFO3(0x0b00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x0b00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cmphs",   OPCODE_INFO2(0x0c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cmplt",   OPCODE_INFO2(0x0d00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("tst",     OPCODE_INFO2(0x0e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cmpne",   OPCODE_INFO2(0x0f00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mfcr",    OPCODE_INFO2(0x1000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, CTRLREG, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("psrclr",  OPCODE_INFO_LIST(0x11f0, (0_2, PSR_BITS_LIST, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("psrset",  OPCODE_INFO_LIST(0x11f8, (0_2, PSR_BITS_LIST, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mov",     OPCODE_INFO2(0x1200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("bgenr",   OPCODE_INFO2(0x1300, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("rsub",   OPCODE_INFO3(0x1400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("ixw",    OPCODE_INFO3(0x1500, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1500, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("and",    OPCODE_INFO3(0x1600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("xor",    OPCODE_INFO3(0x1700, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1700, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mtcr",    OPCODE_INFO2(0x1800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, CTRLREG, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("asr",    OPCODE_INFO3(0x1a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("lsl",    OPCODE_INFO3(0x1b00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1b00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("addu",   OPCODE_INFO3(0x1c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("add",     OPCODE_INFO2(0x1c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("ixh",    OPCODE_INFO3(0x1d00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1d00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("or",     OPCODE_INFO3(0x1e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("andn",   OPCODE_INFO3(0x1f00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x1f00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("addi",   OPCODE_INFO3(0x2000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, OIMM5b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x2000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cmplti",  OPCODE_INFO2(0x2200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("subi",   OPCODE_INFO3(0x2400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, OIMM5b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x2400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cpwgr",   OPCODE_INFO2(0x2600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, CPREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
  DOP16("rsubi",  OPCODE_INFO3(0x2800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x2800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cmpnei",  OPCODE_INFO2(0x2a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("bmaski",  OPCODE_INFO2(0x2c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_BMASKI, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("divu",   OPCODE_INFO3(0x2c10, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, REG_r1b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x2c10, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, REG_r1b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mflos",   OPCODE_INFO1(0x2c20, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC_DSP),
  OP16("mfhis",   OPCODE_INFO1(0x2c30, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC_DSP),
  OP16("mtlo",    OPCODE_INFO1(0x2c40, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC_DSP),
  OP16("mthi",    OPCODE_INFO1(0x2c50, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC_DSP),
  OP16("mflo",    OPCODE_INFO1(0x2c60, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC_DSP),
  OP16("mfhi",    OPCODE_INFO1(0x2c70, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC_DSP),
  DOP16("andi",   OPCODE_INFO3(0x2e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x2e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("bclri",  OPCODE_INFO3(0x3000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x3000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("bgeni",   OPCODE_INFO2(0x3200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_7_31, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cpwir",   OPCODE_INFO1(0x3200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
  DOP16("divs",   OPCODE_INFO3(0x3210, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, REG_r1b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x3210, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, REG_r1b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("cprsr",   OPCODE_INFO1(0x3220, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
  OP16("cpwsr",   OPCODE_INFO1(0x3230, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
  DOP16("bseti",  OPCODE_INFO3(0x3400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x3400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("btsti",   OPCODE_INFO2(0x3600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("rotli",  OPCODE_INFO3(0x3800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x3800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("xsr",    OPCODE_INFO3(0x3800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x3800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("asrc",   OPCODE_INFO3(0x3a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x3a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("asri",   OPCODE_INFO3(0x3a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x3a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("lslc",   OPCODE_INFO3(0x3c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x3c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("lsli",   OPCODE_INFO3(0x3c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x3c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("lsrc",   OPCODE_INFO3(0x3e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)),  OPCODE_INFO1(0x3e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("lsri",   OPCODE_INFO3(0x3e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x3e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_1_31, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("ldex",    SOPCODE_INFO2(0x4000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP),
  OP16("ldex.w",  SOPCODE_INFO2(0x4000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP),
  OP16("ldwex",   SOPCODE_INFO2(0x4000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP),
  OP16("stex",    SOPCODE_INFO2(0x5000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP),
  OP16("stex.w",  SOPCODE_INFO2(0x5000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP),
  OP16("stwex",   SOPCODE_INFO2(0x5000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP),
  OP16("omflip0", OPCODE_INFO2(0x4000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC),
  OP16("omflip1", OPCODE_INFO2(0x4100, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC),
  OP16("omflip2", OPCODE_INFO2(0x4200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC),
  OP16("omflip3", OPCODE_INFO2(0x4300, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_MAC),
  OP16("muls",    OPCODE_INFO2(0x5000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulsa",   OPCODE_INFO2(0x5100, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulss",   OPCODE_INFO2(0x5200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulu",    OPCODE_INFO2(0x5400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulua",   OPCODE_INFO2(0x5500, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulus",   OPCODE_INFO2(0x5600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("vmulsh",  OPCODE_INFO2(0x5800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("vmulsha", OPCODE_INFO2(0x5900, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("vmulshs", OPCODE_INFO2(0x5a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("vmulsw",  OPCODE_INFO2(0x5c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("vmulswa", OPCODE_INFO2(0x5d00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("vmulsws", OPCODE_INFO2(0x5e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("movi",    OPCODE_INFO2(0x6000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_10, IMM7b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("mulsh",  OPCODE_INFO3(0x6800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x6800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("mulsh.h", OPCODE_INFO3(0x6800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), OPCODE_INFO2(0x6800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("mulsha",  OPCODE_INFO2(0x6900, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulshs",  OPCODE_INFO2(0x6a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("cprcr",   OPCODE_INFO2(0x6b00, (0_2, GREG0_7, OPRND_SHIFT_0_BIT), (3_7, CPCREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
  OP16("mulsw",   OPCODE_INFO2(0x6c00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulswa",  OPCODE_INFO2(0x6d00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("mulsws",  OPCODE_INFO2(0x6e00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16("cpwcr",   OPCODE_INFO2(0x6f00, (0_2, GREG0_7, OPRND_SHIFT_0_BIT), (3_7, CPCREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_CP),
#undef _RELOC16
#define _RELOC16 BFD_RELOC_CKCORE_PCREL_IMM8BY4
#undef _TRANSFER
#define _TRANSFER 1
  OP16("jmpi",    OPCODE_INFO1(0x7000, (0_7, OFF8b, OPRND_SHIFT_2_BIT)), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 0
  OP16("jsri",    OPCODE_INFO1(0x7f00, (0_7, OFF8b, OPRND_SHIFT_2_BIT)), CSKYV1_ISA_E1),
  OP16_WITH_WORK("lrw",     OPCODE_INFO2(0x7000, (8_11, REGnsplr, OPRND_SHIFT_0_BIT), (0_7, CONSTANT, OPRND_SHIFT_2_BIT)), CSKYV1_ISA_E1, v1_work_lrw),
#undef _RELOC16
#define _RELOC16 0

  DOP16("ld.w",   SOPCODE_INFO2(0x8000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0x8000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)),CSKYV1_ISA_E1),
  DOP16("ldw",    SOPCODE_INFO2(0x8000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0x8000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)),CSKYV1_ISA_E1),
  DOP16("ld",     SOPCODE_INFO2(0x8000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0x8000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)),CSKYV1_ISA_E1),
  DOP16("st.w",   SOPCODE_INFO2(0x9000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0x9000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)),CSKYV1_ISA_E1),
  DOP16("stw",    SOPCODE_INFO2(0x9000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0x9000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)),CSKYV1_ISA_E1),
  DOP16("st",     SOPCODE_INFO2(0x9000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0x9000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)),CSKYV1_ISA_E1),
  DOP16("ld.b",   SOPCODE_INFO2(0xa000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xa000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("ldb",    SOPCODE_INFO2(0xa000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xa000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("st.b",   SOPCODE_INFO2(0xb000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xb000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("stb",    SOPCODE_INFO2(0xb000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xb000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("ld.h",   SOPCODE_INFO2(0xc000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xc000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("ldh",    SOPCODE_INFO2(0xc000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xc000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("st.h",   SOPCODE_INFO2(0xd000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xd000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  DOP16("sth",    SOPCODE_INFO2(0xd000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), BRACKET_OPRND((0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_7, IMM_LDST, OPRND_SHIFT_0_BIT))),
                  OPCODE_INFO2(0xd000, (8_11, GREG0_15, OPRND_SHIFT_0_BIT), (0_3, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),

#undef _RELOC16
#define _RELOC16    BFD_RELOC_CKCORE_PCREL_IMM11BY2
  OP16("bt",      OPCODE_INFO1(0xe000, (0_10, OFF11b, OPRND_SHIFT_1_BIT)), CSKYV1_ISA_E1),
  OP16("bf",      OPCODE_INFO1(0xe800, (0_10, OFF11b, OPRND_SHIFT_1_BIT)), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 1
  OP16("br",      OPCODE_INFO1(0xf000, (0_10, OFF11b, OPRND_SHIFT_1_BIT)), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 0
  OP16("bsr",     OPCODE_INFO1(0xf800, (0_10, OFF11b, OPRND_SHIFT_1_BIT)), CSKYV1_ISA_E1),
#undef _RELOC16
#define _RELOC16    0

#undef _RELAX
#define _RELAX 1
  OP16("jbt",      OPCODE_INFO1(0xe000, (0_10, JBTF, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("jbf",      OPCODE_INFO1(0xe800, (0_10, JBTF, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 1
  OP16("jbr",      OPCODE_INFO1(0xf000, (0_10, JBR, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 0
#undef _RELAX
#define _RELAX 0

  OP16_WITH_WORK("jbsr",     OPCODE_INFO1(0xf800, (0_10, JBSR, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1, v1_work_jbsr),

  /* The following are aliases for other instructions.  */
  /* rts -> jmp r15.  */
#undef _TRANSFER
#define _TRANSFER 2
  OP16("rts",     OPCODE_INFO0(0x00CF), CSKYV1_ISA_E1),
  OP16("rte",     OPCODE_INFO0(0x0002), CSKYV1_ISA_E1),
  OP16("rfe",     OPCODE_INFO0(0x0002), CSKYV1_ISA_E1),
#undef _TRANSFER
#define _TRANSFER 0
  /* cmphs r0,r0 */
  OP16("setc",  OPCODE_INFO0(0x0c00), CSKYV1_ISA_E1),
  /* cmpne r0,r0 */
  OP16("clrc",  OPCODE_INFO0(0x0f00), CSKYV1_ISA_E1),
  /* cmplti rd,1 */
  OP16("tstle",  OPCODE_INFO1(0x2200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* cmplei rd,X -> cmplti rd,X+1 */
  OP16("cmplei",  OPCODE_INFO2(0x2200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* rsubi rd,0 */
  OP16("neg",   OPCODE_INFO1(0x2800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* cmpnei rd,0.  */
  OP16("tstne", OPCODE_INFO1(0x2a00, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* btsti rx,31.  */
  OP16("tstlt", OPCODE_INFO1(0x37f0, (0_3, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* bclri rx,log2(imm).  */
  OP16("mclri", OPCODE_INFO2(0x3000, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_POWER, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* bgeni rx,log2(imm).  */
  OP16("mgeni", OPCODE_INFO2(0x3200, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_7_31_POWER, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* bseti rx,log2(imm).  */
  OP16("mseti", OPCODE_INFO2(0x3400, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_POWER, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* btsti rx,log2(imm).  */
  OP16("mtsti", OPCODE_INFO2(0x3600, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_POWER, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("rori",  OPCODE_INFO2(0x3800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_RORI, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  OP16("rotri", OPCODE_INFO2(0x3800, (0_3, GREG0_15, OPRND_SHIFT_0_BIT), (4_8, IMM5b_RORI, OPRND_SHIFT_0_BIT)), CSKYV1_ISA_E1),
  /* mov r0, r0.  */
  OP16("nop",     OPCODE_INFO0(0x1200), CSKYV1_ISA_E1),

  /* Float instruction with work.  */
  OP16_WITH_WORK("fabss",   OPCODE_INFO3(0xffe04400, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnegs",   OPCODE_INFO3(0xffe04c00, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fsqrts",  OPCODE_INFO3(0xffe05400, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("frecips", OPCODE_INFO3(0xffe05c00, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fadds",   OPCODE_INFO4(0xffe38000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fsubs",   OPCODE_INFO4(0xffe48000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmacs",   OPCODE_INFO4(0xffe58000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmscs",   OPCODE_INFO4(0xffe68000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmuls",   OPCODE_INFO4(0xffe78000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fdivs",   OPCODE_INFO4(0xffe88000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmacs",  OPCODE_INFO4(0xffe98000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmscs",  OPCODE_INFO4(0xffea8000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmuls",  OPCODE_INFO4(0xffeb8000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (10_14, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fabsd",   OPCODE_INFO3(0xffe04000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnegd",   OPCODE_INFO3(0xffe04800, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fsqrtd",  OPCODE_INFO3(0xffe05000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("frecipd", OPCODE_INFO3(0xffe05800, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("faddd",   OPCODE_INFO4(0xffe30000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fsubd",   OPCODE_INFO4(0xffe40000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmacd",   OPCODE_INFO4(0xffe50000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmscd",   OPCODE_INFO4(0xffe60000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmuld",   OPCODE_INFO4(0xffe70000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fdivd",   OPCODE_INFO4(0xffe80000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmacd",  OPCODE_INFO4(0xffe90000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmscd",  OPCODE_INFO4(0xffea0000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmuld",  OPCODE_INFO4(0xffeb0000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fabsm",   OPCODE_INFO3(0xffe06000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnegm",   OPCODE_INFO3(0xffe06400, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("faddm",   OPCODE_INFO4(0xffec0000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fsubm",   OPCODE_INFO4(0xffec8000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmacm",   OPCODE_INFO4(0xffed8000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmscm",   OPCODE_INFO4(0xffee0000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmulm",   OPCODE_INFO4(0xffed0000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmacm",  OPCODE_INFO4(0xffee8000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmscm",  OPCODE_INFO4(0xffef0000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fnmulm",  OPCODE_INFO4(0xffef8000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (10_14, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fcmphsd", OPCODE_INFO3(0xffe00800, (0_4, FEREG, OPRND_SHIFT_0_BIT), (5_9, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpltd", OPCODE_INFO3(0xffe00c00, (0_4, FEREG, OPRND_SHIFT_0_BIT), (5_9, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpned", OPCODE_INFO3(0xffe01000, (0_4, FEREG, OPRND_SHIFT_0_BIT), (5_9, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpuod", OPCODE_INFO3(0xffe01400, (0_4, FEREG, OPRND_SHIFT_0_BIT), (5_9, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmphss", OPCODE_INFO3(0xffe01800, (0_4, FREG, OPRND_SHIFT_0_BIT), (5_9, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmplts", OPCODE_INFO3(0xffe01c00, (0_4, FREG, OPRND_SHIFT_0_BIT), (5_9, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpnes", OPCODE_INFO3(0xffe02000, (0_4, FREG, OPRND_SHIFT_0_BIT), (5_9, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpuos", OPCODE_INFO3(0xffe02400, (0_4, FREG, OPRND_SHIFT_0_BIT), (5_9, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpzhsd", OPCODE_INFO2(0xffe00400, (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpzltd", OPCODE_INFO2(0xffe00480, (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpzned", OPCODE_INFO2(0xffe00500, (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpzuod", OPCODE_INFO2(0xffe00580, (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpzhss", OPCODE_INFO2(0xffe00600, (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpzlts", OPCODE_INFO2(0xffe00680, (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpznes", OPCODE_INFO2(0xffe00700, (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fcmpzuos", OPCODE_INFO2(0xffe00780, (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo_fc),
  OP16_WITH_WORK("fstod",    OPCODE_INFO3(0xffe02800, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fdtos",    OPCODE_INFO3(0xffe02c00, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fsitos",   OPCODE_INFO3(0xffe03400, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fsitod",   OPCODE_INFO3(0xffe03000, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fuitos",   OPCODE_INFO3(0xffe03c00, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fuitod",   OPCODE_INFO3(0xffe03800, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fstosi",   OPCODE_INFO4(0xffe10000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (13_17, RM, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fdtosi",   OPCODE_INFO4(0xffe08000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (13_17, RM, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fstoui",   OPCODE_INFO4(0xffe20000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (13_17, RM, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fdtoui",   OPCODE_INFO4(0xffe18000, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FEREG, OPRND_SHIFT_0_BIT), (13_17, RM, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmovd",    OPCODE_INFO3(0xffe06800, (5_9, FEREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmovs",    OPCODE_INFO3(0xffe06c00, (5_9, FREG, OPRND_SHIFT_0_BIT), (0_4, FREG, OPRND_SHIFT_0_BIT), (NONE, GREG0_15, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_fo),
  OP16_WITH_WORK("fmts",     OPCODE_INFO2(0x00000000, (NONE, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_write),
  OP16_WITH_WORK("fmfs",     OPCODE_INFO2(0x00000000, (NONE, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_read),
  OP16_WITH_WORK("fmtd",     OPCODE_INFO2(0x00000000, (NONE, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, FEREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_writed),
  OP16_WITH_WORK("fmfd",     OPCODE_INFO2(0x00000000, (NONE, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, FEREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1, v1_work_fpu_readd),
  {NULL}
};

#undef _TRANSFER
#undef _RELOC16
#undef _RELOC32
#undef _RELAX

/* csky v2 opcodes.  */
const struct _csky_opcode csky_v2_opcodes[] =
{
#define _TRANSFER   0
#define _RELOC16    0
#define _RELOC32    0
#define _RELAX      0

  OP16("bkpt",  OPCODE_INFO0(0x0000), CSKYV2_ISA_E1),
  OP16_WITH_WORK("nie",   OPCODE_INFO0(0x1460), CSKYV2_ISA_1E2, v2_work_istack),
  OP16_WITH_WORK("nir",   OPCODE_INFO0(0x1461), CSKYV2_ISA_1E2, v2_work_istack),
  OP16_WITH_WORK("ipush", OPCODE_INFO0(0x1462), CSKYV2_ISA_1E2, v2_work_istack),
  OP16_WITH_WORK("ipop",  OPCODE_INFO0(0x1463), CSKYV2_ISA_1E2, v2_work_istack),
  OP16("bpop.h", OPCODE_INFO1(0x14a0, (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKY_ISA_JAVA),
  OP16("bpop.w", OPCODE_INFO1(0x14a2, (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKY_ISA_JAVA),
  OP16("bpush.h", OPCODE_INFO1(0x14e0, (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKY_ISA_JAVA),
  OP16("bpush.w", OPCODE_INFO1(0x14e2, (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKY_ISA_JAVA),
  OP32("bmset", OPCODE_INFO0(0xc0001020),CSKY_ISA_JAVA),
  OP32("bmclr", OPCODE_INFO0(0xc0001420),CSKY_ISA_JAVA),
  OP32("sce",   OPCODE_INFO1(0xc0001820, (21_24, IMM4b, OPRND_SHIFT_0_BIT)), CSKY_ISA_MP),
  OP32("trap",  OPCODE_INFO1(0xc0002020, (10_11, IMM2b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  /* Secure/nsecure world switch.  */
  OP32("wsc",  OPCODE_INFO0(0xc0003c20), CSKYV2_ISA_1E2),
  OP32("mtcr", OPCODE_INFO2(0xc0006420, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_4or21_25, CTRLREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  OP32("mfcr", OPCODE_INFO2(0xc0006020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20or21_25, CTRLREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
#undef _TRANSFER
#define _TRANSFER   2
  OP32("rte",   OPCODE_INFO0(0xc0004020), CSKYV2_ISA_E1),
  OP32("rfi",   OPCODE_INFO0(0xc0004420), CSKYV2_ISA_2E3),
#undef _TRANSFER
#define _TRANSFER   0
  OP32("stop",  OPCODE_INFO0(0xc0004820), CSKYV2_ISA_E1),
  OP32("wait",  OPCODE_INFO0(0xc0004c20), CSKYV2_ISA_E1),
  OP32("doze",  OPCODE_INFO0(0xc0005020), CSKYV2_ISA_E1),
  OP32("we",    OPCODE_INFO0(0xc0005420), CSKY_ISA_MP_1E2),
  OP32("se",    OPCODE_INFO0(0xc0005820), CSKY_ISA_MP_1E2),
  OP32("psrclr",OPCODE_INFO_LIST(0xc0007020, (21_25, PSR_BITS_LIST, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  OP32("psrset",OPCODE_INFO_LIST(0xc0007420, (21_25, PSR_BITS_LIST, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  DOP32("abs",  OPCODE_INFO2(0xc4000200, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
        OPCODE_INFO1(0xc4000200, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("mvc",   OPCODE_INFO1(0xc4000500, (0_4, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("incf",  OPCODE_INFO3(0xc4000c20, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("movf",  OPCODE_INFO2(0xc4000c20, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("inct",  OPCODE_INFO3(0xc4000c40, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("movt",  OPCODE_INFO2(0xc4000c40, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("decf",  OPCODE_INFO3(0xc4000c80, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("dect",  OPCODE_INFO3(0xc4000d00, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("decgt", OPCODE_INFO3(0xc4001020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("declt", OPCODE_INFO3(0xc4001040, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("decne", OPCODE_INFO3(0xc4001080, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("clrf",  OPCODE_INFO1(0xc4002c20, (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("clrt",  OPCODE_INFO1(0xc4002c40, (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP32("rotli",OPCODE_INFO3(0xc4004900, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)),
       OPCODE_INFO2(0xc4004900, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("lslc",  OPCODE_INFO3(0xc4004c20, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("lsrc",  OPCODE_INFO3(0xc4004c40, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP32("asrc",  OPCODE_INFO3(0xc4004c80, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, OIMM5b, OPRND_SHIFT_0_BIT)),
       OPCODE_INFO1(0xc4004c80, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("xsr",   OPCODE_INFO3(0xc4004d00, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("bgenr", OPCODE_INFO2(0xc4005040, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP32("brev",  OPCODE_INFO2(0xc4006200, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
       OPCODE_INFO1(0xc4006200, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("xtrb0", OPCODE_INFO2(0xc4007020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("xtrb1", OPCODE_INFO2(0xc4007040, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("xtrb2", OPCODE_INFO2(0xc4007080, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("xtrb3", OPCODE_INFO2(0xc4007100, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("ff0",   OPCODE_INFO2(0xc4007c20, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP32("ff1",   OPCODE_INFO2(0xc4007c40, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
       OPCODE_INFO1(0xc4007c40, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("mulu",  OPCODE_INFO2(0xc4008820, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulua", OPCODE_INFO2(0xc4008840, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulus", OPCODE_INFO2(0xc4008880, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("muls",  OPCODE_INFO2(0xc4008c20, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulsa", OPCODE_INFO2(0xc4008c40, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulss", OPCODE_INFO2(0xc4008c80, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulsha",OPCODE_INFO2(0xc4009040, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulshs",OPCODE_INFO2(0xc4009080, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulswa",OPCODE_INFO2(0xc4009440, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mulsws",OPCODE_INFO2(0xc4009480, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mfhis", OPCODE_INFO1(0xc4009820, (0_4, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mflos", OPCODE_INFO1(0xc4009880, (0_4, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mvtc",  OPCODE_INFO0(0xc4009a00), CSKY_ISA_DSP),
  OP32("mfhi",  OPCODE_INFO1(0xc4009c20, (0_4, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mthi",  OPCODE_INFO1(0xc4009c40, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mflo",  OPCODE_INFO1(0xc4009c80, (0_4, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("mtlo",  OPCODE_INFO1(0xc4009d00, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP32("vmulsh",  OPCODE_INFO2(0xc400b020, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_1E2),
  OP32("vmulsha", OPCODE_INFO2(0xc400b040, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_1E2),
  OP32("vmulshs", OPCODE_INFO2(0xc400b080, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_1E2),
  OP32("vmulsw",  OPCODE_INFO2(0xc400b420, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_1E2),
  OP32("vmulswa", OPCODE_INFO2(0xc400b440, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_1E2),
  OP32("vmulsws", OPCODE_INFO2(0xc400b480, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_1E2),
  OP32("ldr.b", SOPCODE_INFO2(0xd0000000, (0_4, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_9or21_25, AREG_WITH_LSHIFT, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_2E3),
  OP32("ldr.h", SOPCODE_INFO2(0xd0000400, (0_4, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_9or21_25, AREG_WITH_LSHIFT, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_2E3),
  OP32("ldr.w", SOPCODE_INFO2(0xd0000800, (0_4, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_9or21_25, AREG_WITH_LSHIFT, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_2E3),
  OP32("ldm",   OPCODE_INFO2(0xd0001c20, (0_4or21_25, REGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("ldq",   OPCODE_INFO2(0xd0801c23, (NONE, REGr4_r7, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("str.b", SOPCODE_INFO2(0xd4000000, (0_4, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_9or21_25, AREG_WITH_LSHIFT, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_2E3),
  OP32("str.h", SOPCODE_INFO2(0xd4000400, (0_4, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_9or21_25, AREG_WITH_LSHIFT, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_2E3),
  OP32("str.w", SOPCODE_INFO2(0xd4000800, (0_4, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_9or21_25, AREG_WITH_LSHIFT, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_2E3),
  OP32("stm",   OPCODE_INFO2(0xd4001c20, (0_4or21_25, REGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("stq",   OPCODE_INFO2(0xd4801c23, (NONE, REGr4_r7, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("ld.bs", SOPCODE_INFO2(0xd8004000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_1E2),
  OP32("ldbs",  SOPCODE_INFO2(0xd8004000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_1E2),
  OP32("ld.hs", SOPCODE_INFO2(0xd8005000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_1E2),
  OP32("ldhs",  SOPCODE_INFO2(0xd8005000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_1E2),
  OP32("ld.d",  SOPCODE_INFO2(0xd8003000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_4E7),
  OP32("ldex.w",SOPCODE_INFO2(0xd8007000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP_1E2),
  OP32("ldexw",SOPCODE_INFO2(0xd8007000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP_1E2),
  OP32("ldex",  SOPCODE_INFO2(0xd8007000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP_1E2),
  OP32("st.d",  SOPCODE_INFO2(0xdc003000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_4E7),
  OP32("stex.w",SOPCODE_INFO2(0xdc007000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP_1E2),
  OP32("stexw",SOPCODE_INFO2(0xdc007000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP_1E2),
  OP32("stex",  SOPCODE_INFO2(0xdc007000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_MP_1E2),
  DOP32("andi",  OPCODE_INFO3(0xe4002000, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM12b, OPRND_SHIFT_0_BIT)),
        OPCODE_INFO2(0xe4002000, (16_20or21_25, DUP_AREG, OPRND_SHIFT_0_BIT), (0_11, IMM12b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("andni", OPCODE_INFO3(0xe4003000, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM12b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("xori",  OPCODE_INFO3(0xe4004000, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM12b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP32("ins",   OPCODE_INFO4(0xc4005c00, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (5_9, MSB2SIZE, OPRND_SHIFT_0_BIT), (0_4, LSB2SIZE, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _TRANSFER
#undef _RELOC32
#define _TRANSFER   1
#define _RELOC32  BFD_RELOC_CKCORE_PCREL_IMM16BY4
  OP32("jmpi",  OPCODE_INFO1(0xeac00000, (0_15, OFF16b, OPRND_SHIFT_2_BIT)), CSKYV2_ISA_2E3),
#undef _TRANSFER
#undef _RELOC32
#define _TRANSFER   0
#define _RELOC32  0


  OP32("fadds",     OPCODE_INFO3(0xf4000000, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fsubs",     OPCODE_INFO3(0xf4000020, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fmovs",     OPCODE_INFO2(0xf4000080, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fabss",     OPCODE_INFO2(0xf40000c0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fnegs",     OPCODE_INFO2(0xf40000e0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmpzhss",  OPCODE_INFO1(0xf4000100, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmpzlss",  OPCODE_INFO1(0xf4000120, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmpznes",  OPCODE_INFO1(0xf4000140, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmpzuos",  OPCODE_INFO1(0xf4000160, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmphss",   OPCODE_INFO2(0xf4000180, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmplts",   OPCODE_INFO2(0xf40001a0, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmpnes",   OPCODE_INFO2(0xf40001c0, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fcmpuos",   OPCODE_INFO2(0xf40001e0, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fmuls",     OPCODE_INFO3(0xf4000200, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fmacs",     OPCODE_INFO3(0xf4000280, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fmscs",     OPCODE_INFO3(0xf40002a0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fnmacs",    OPCODE_INFO3(0xf40002c0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fnmscs",    OPCODE_INFO3(0xf40002e0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fnmuls",    OPCODE_INFO3(0xf4000220, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fdivs",     OPCODE_INFO3(0xf4000300, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("frecips",   OPCODE_INFO2(0xf4000320, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fsqrts",    OPCODE_INFO2(0xf4000340, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("faddd",     OPCODE_INFO3(0xf4000800, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fsubd",     OPCODE_INFO3(0xf4000820, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmovd",     OPCODE_INFO2(0xf4000880, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fabsd",     OPCODE_INFO2(0xf40008c0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnegd",     OPCODE_INFO2(0xf40008e0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmpzhsd",  OPCODE_INFO1(0xf4000900, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmpzlsd",  OPCODE_INFO1(0xf4000920, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmpzned",  OPCODE_INFO1(0xf4000940, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmpzuod",  OPCODE_INFO1(0xf4000960, (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmphsd",   OPCODE_INFO2(0xf4000980, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmpltd",   OPCODE_INFO2(0xf40009a0, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmpned",   OPCODE_INFO2(0xf40009c0, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fcmpuod",   OPCODE_INFO2(0xf40009e0, (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmuld",     OPCODE_INFO3(0xf4000a00, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnmuld",    OPCODE_INFO3(0xf4000a20, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmacd",     OPCODE_INFO3(0xf4000a80, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmscd",     OPCODE_INFO3(0xf4000aa0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnmacd",    OPCODE_INFO3(0xf4000ac0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnmscd",    OPCODE_INFO3(0xf4000ae0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdivd",     OPCODE_INFO3(0xf4000b00, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("frecipd",   OPCODE_INFO2(0xf4000b20, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fsqrtd",    OPCODE_INFO2(0xf4000b40, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("faddm",     OPCODE_INFO3(0xf4001000, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fsubm",     OPCODE_INFO3(0xf4001020, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmovm",     OPCODE_INFO2(0xf4001080, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fabsm",     OPCODE_INFO2(0xf40010c0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnegm",     OPCODE_INFO2(0xf40010e0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmulm",     OPCODE_INFO3(0xf4001200, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnmulm",    OPCODE_INFO3(0xf4001220, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmacm",     OPCODE_INFO3(0xf4001280, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmscm",     OPCODE_INFO3(0xf40012a0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnmacm",    OPCODE_INFO3(0xf40012c0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fnmscm",    OPCODE_INFO3(0xf40012e0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT), (21_24, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fstosi.rn", OPCODE_INFO2(0xf4001800, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fstosi.rz", OPCODE_INFO2(0xf4001820, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fstosi.rpi",OPCODE_INFO2(0xf4001840, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fstosi.rni",OPCODE_INFO2(0xf4001860, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fstoui.rn", OPCODE_INFO2(0xf4001880, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fstoui.rz", OPCODE_INFO2(0xf40018a0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fstoui.rpi",OPCODE_INFO2(0xf40018c0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fstoui.rni",OPCODE_INFO2(0xf40018e0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fdtosi.rn", OPCODE_INFO2(0xf4001900, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtosi.rz", OPCODE_INFO2(0xf4001920, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtosi.rpi",OPCODE_INFO2(0xf4001940, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtosi.rni",OPCODE_INFO2(0xf4001960, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtoui.rn", OPCODE_INFO2(0xf4001980, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtoui.rz", OPCODE_INFO2(0xf40019a0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtoui.rpi",OPCODE_INFO2(0xf40019c0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtoui.rni",OPCODE_INFO2(0xf40019e0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fsitos",    OPCODE_INFO2(0xf4001a00, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fuitos",    OPCODE_INFO2(0xf4001a20, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fsitod",    OPCODE_INFO2(0xf4001a80, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fuitod",    OPCODE_INFO2(0xf4001aa0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fdtos",     OPCODE_INFO2(0xf4001ac0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fstod",     OPCODE_INFO2(0xf4001ae0, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmfvrh",    OPCODE_INFO2(0xf4001b00, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmfvrl",    OPCODE_INFO2(0xf4001b20, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_19, FREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("fmtvrh",    OPCODE_INFO2(0xf4001b40, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_1E2),
  OP32("fmtvrl",    OPCODE_INFO2(0xf4001b60, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_FLOAT_E1),
  OP32("flds",      SOPCODE_INFO2(0xf4002000, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT_1E2),
  OP32("fldd",      SOPCODE_INFO2(0xf4002100, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fldm",      SOPCODE_INFO2(0xf4002200, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fsts",      SOPCODE_INFO2(0xf4002400, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT_1E2),
  OP32("fstd",      SOPCODE_INFO2(0xf4002500, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fstm",      SOPCODE_INFO2(0xf4002600, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fldrs",     SOPCODE_INFO2(0xf4002800, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT_1E2),
  OP32("fstrs",     SOPCODE_INFO2(0xf4002c00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT_1E2),
  OP32("fldrd",     SOPCODE_INFO2(0xf4002900, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fldrm",     SOPCODE_INFO2(0xf4002a00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fstrd",     SOPCODE_INFO2(0xf4002d00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fstrm",     SOPCODE_INFO2(0xf4002e00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("fldms",     OPCODE_INFO2(0xf4003000, (0_3or21_24, FREGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("fldmd",     OPCODE_INFO2(0xf4003100, (0_3or21_24, FREGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("fldmm",     OPCODE_INFO2(0xf4003200, (0_3or21_24, FREGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("fstms",     OPCODE_INFO2(0xf4003400, (0_3or21_24, FREGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("fstmd",     OPCODE_INFO2(0xf4003500, (0_3or21_24, FREGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("fstmm",     OPCODE_INFO2(0xf4003600, (0_3or21_24, FREGLIST_DASH, OPRND_SHIFT_0_BIT), (16_20, AREG_WITH_BRACKET,OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),

  OP32("vdup.8",    OPCODE_INFO2(0xf8000e80, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vdup.16",   OPCODE_INFO2(0xf8100e80, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vdup.32",   OPCODE_INFO2(0xfa000e80, (0_3, FREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmfvr.u8",  OPCODE_INFO2(0xf8001200, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmfvr.u16", OPCODE_INFO2(0xf8001220, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmfvr.u32", OPCODE_INFO2(0xf8001240, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmfvr.s8",  OPCODE_INFO2(0xf8001280, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmfvr.s16", OPCODE_INFO2(0xf80012a0, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_19or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmtvr.u8",  OPCODE_INFO2(0xf8001300, (0_3or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmtvr.u16", OPCODE_INFO2(0xf8001320, (0_3or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vmtvr.u32", OPCODE_INFO2(0xf8001340, (0_3or21_24, FREG_WITH_INDEX, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP_FLOAT),
  OP32("vldd.8",    SOPCODE_INFO2(0xf8002000, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldd.16",   SOPCODE_INFO2(0xf8002100, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldd.32",   SOPCODE_INFO2(0xf8002200, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldq.8",    SOPCODE_INFO2(0xf8002400, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldq.16",   SOPCODE_INFO2(0xf8002500, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldq.32",   SOPCODE_INFO2(0xf8002600, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstd.8",    SOPCODE_INFO2(0xf8002800, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstd.16",   SOPCODE_INFO2(0xf8002900, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstd.32",   SOPCODE_INFO2(0xf8002a00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstq.8",    SOPCODE_INFO2(0xf8002c00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstq.16",   SOPCODE_INFO2(0xf8002d00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstq.32",   SOPCODE_INFO2(0xf8002e00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (4_7or21_24, IMM_FLDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldrd.8",   SOPCODE_INFO2(0xf8003000, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldrd.16",  SOPCODE_INFO2(0xf8003100, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldrd.32",  SOPCODE_INFO2(0xf8003200, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldrq.8",   SOPCODE_INFO2(0xf8003400, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldrq.16",  SOPCODE_INFO2(0xf8003500, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vldrq.32",  SOPCODE_INFO2(0xf8003600, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstrd.8",   SOPCODE_INFO2(0xf8003800, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstrd.16",  SOPCODE_INFO2(0xf8003900, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstrd.32",  SOPCODE_INFO2(0xf8003a00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstrq.8",   SOPCODE_INFO2(0xf8003c00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstrq.16",  SOPCODE_INFO2(0xf8003d00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),
  OP32("vstrq.32",  SOPCODE_INFO2(0xf8003e00, (0_3, FREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (5_6or21_25, AREG_WITH_LSHIFT_FPU, OPRND_SHIFT_0_BIT))), CSKY_ISA_DSP_FLOAT),

  DOP32("sync",       OPCODE_INFO1(0xc0000420, (21_25, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO0(0xc0000420), CSKYV2_ISA_E1),
  DOP32("idly",       OPCODE_INFO1(0xc0001c20, (21_25, OIMM5b_IDLY, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO0(0xc0601c20), CSKYV2_ISA_E1),

#undef _RELOC32
#define _RELOC32 BFD_RELOC_CKCORE_PCREL_IMM18BY2
  OP32("grs", OPCODE_INFO2(0xcc0c0000, (21_25, AREG, OPRND_SHIFT_0_BIT), (0_17, IMM_OFF18b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_2E3),
#undef _RELOC32
#define _RELOC32 0
  DOP32("ixh",   OPCODE_INFO3(0xc4000820, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
       OPCODE_INFO2(0xc4000820, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP32("ixw",   OPCODE_INFO3(0xc4000840, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
       OPCODE_INFO2(0xc4000840, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("ixd",   OPCODE_INFO3(0xc4000880, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP32("divu", OPCODE_INFO3(0xc4008020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
        OPCODE_INFO2(0xc4008020, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP32("divs", OPCODE_INFO3(0xc4008040, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
        OPCODE_INFO2(0xc4008040, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("pldr",  SOPCODE_INFO1(0xd8006000, BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_CACHE),
  OP32("pldw",  SOPCODE_INFO1(0xdc006000, BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKY_ISA_CACHE),
  OP32("cprgr", SOPCODE_INFO2(0xfc000000, (16_20, AREG, OPRND_SHIFT_0_BIT),  ABRACKET_OPRND((21_25, IMM5b, OPRND_SHIFT_0_BIT), (0_11 , IMM12b, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP32("cpwgr", SOPCODE_INFO2(0xfc001000, (16_20, AREG, OPRND_SHIFT_0_BIT),  ABRACKET_OPRND((21_25, IMM5b, OPRND_SHIFT_0_BIT), (0_11 , IMM12b, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP32("cprcr", SOPCODE_INFO2(0xfc002000, (16_20, AREG, OPRND_SHIFT_0_BIT),  ABRACKET_OPRND((21_25, IMM5b, OPRND_SHIFT_0_BIT), (0_11 , IMM12b, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP32("cpwcr", SOPCODE_INFO2(0xfc003000, (16_20, AREG, OPRND_SHIFT_0_BIT),  ABRACKET_OPRND((21_25, IMM5b, OPRND_SHIFT_0_BIT), (0_11 , IMM12b, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP32("cprc",  SOPCODE_INFO1(0xfc004000, ABRACKET_OPRND((21_25, IMM5b, OPRND_SHIFT_0_BIT), (0_11, IMM12b, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP32("cpop",  SOPCODE_INFO1(0xfc008000, ABRACKET_OPRND((21_25, IMM5b, OPRND_SHIFT_0_BIT), (0_14or16_20 , IMM15b, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),

  OP16_OP32("push", OPCODE_INFO_LIST(0x14c0, (0_4, REGLIST_DASH_COMMA, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO_LIST(0xebe00000, (0_8, REGLIST_DASH_COMMA, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _TRANSFER
#define _TRANSFER   2
  OP16_OP32("pop", OPCODE_INFO_LIST(0x1480, (0_4, REGLIST_DASH_COMMA, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO_LIST(0xebc00000, (0_8, REGLIST_DASH_COMMA, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _TRANSFER
#define _TRANSFER   0
  OP16_OP32("movi",   OPCODE_INFO2(0x3000, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_7, IMM8b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xea000000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, IMM16b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  /* bmaski will transfer to movi when imm < 17.  */
  OP16_OP32("bmaski", OPCODE_INFO2(0x3000, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_7, IMM8b_BMASKI, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2,
       OPCODE_INFO2(0xc4005020, (0_4, AREG, OPRND_SHIFT_0_BIT), (21_25, OIMM5b_BMASKI, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("cmphsi", OPCODE_INFO2(0x3800, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xeb000000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OIMM16b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("cmplti", OPCODE_INFO2(0x3820, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, OIMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xeb200000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OIMM16b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("cmpnei", OPCODE_INFO2(0x3840, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xeb400000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, IMM16b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
#undef _TRANSFER
#define _TRANSFER   1
  OP16_OP32("jmpix", OPCODE_INFO2(0x38e0, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_1, IMM2b_JMPIX, OPRND_SHIFT_0_BIT)), CSKY_ISA_JAVA,
          OPCODE_INFO2(0xe9e00000, (16_20, GREG0_7, OPRND_SHIFT_0_BIT), (0_1, IMM2b_JMPIX, OPRND_SHIFT_0_BIT)), CSKY_ISA_JAVA),
#undef _TRANSFER
#define _TRANSFER   0
  DOP16_DOP32("bclri", OPCODE_INFO3(0x3880, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0x3880, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4002820, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4002820, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("bseti", OPCODE_INFO3(0x38a0, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0x38a0, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4002840, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4002840, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32_WITH_WORK("btsti", OPCODE_INFO2(0x38c0, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4002880, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2, v2_work_btsti),
  DOP16_DOP32("lsli", OPCODE_INFO3(0x4000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0x4000, (5_7or8_10, DUP_GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4004820, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4004820, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("lsri", OPCODE_INFO3(0x4800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0x4800, (5_7or8_10, DUP_GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4004840, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4004840, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("asri", OPCODE_INFO3(0x5000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4004880, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("addc", OPCODE_INFO2(0x6001, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x6001, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4000040, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4000040, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("subc", OPCODE_INFO2(0x6003, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x6003, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4000100, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4000100, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("cmphs", OPCODE_INFO2(0x6400, (2_5, GREG0_15, OPRND_SHIFT_0_BIT), (6_9, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4000420, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP16_OP32("cmplt", OPCODE_INFO2(0x6401, (2_5, GREG0_15, OPRND_SHIFT_0_BIT), (6_9, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4000440, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP16_OP32("cmpne", OPCODE_INFO2(0x6402, (2_5, GREG0_15, OPRND_SHIFT_0_BIT), (6_9, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4000480, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP16_OP32("mvcv", OPCODE_INFO1(0x6403, (6_9, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO1(0xc4000600, (0_4, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP16_DOP32("and", OPCODE_INFO2(0x6800, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x6800, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4002020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
         OPCODE_INFO2(0xc4002020, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("andn", OPCODE_INFO2(0x6801, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x6801, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4002040, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4002040, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("tst", OPCODE_INFO2(0x6802, (2_5, GREG0_15, OPRND_SHIFT_0_BIT), (6_9, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4002080, (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP16_OP32("tstnbz", OPCODE_INFO1(0x6803, (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO1(0xc4002100, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP16_DOP32("or", OPCODE_INFO2(0x6c00, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x6c00, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4002420, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4002420, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("xor", OPCODE_INFO2(0x6c01, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x6c01, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4002440, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4002440, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("nor", OPCODE_INFO2(0x6c02, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x6c02, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4002480, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4002480, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("mov", OPCODE_INFO2(0x6c03, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4004820, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("nop", OPCODE_INFO0(0x6c03), CSKYV2_ISA_E1,
          OPCODE_INFO0(0xc4004820), CSKYV2_ISA_E1),
  DOP16_DOP32("lsl", OPCODE_INFO2(0x7000, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x7000, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4004020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4004020, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("lsr", OPCODE_INFO2(0x7001, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x7001, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4004040, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4004040, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("asr", OPCODE_INFO2(0x7002, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x7002, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4004080, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4004080, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("rotl", OPCODE_INFO2(0x7003, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x7003, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, DUMMY_REG, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4004100, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4004100, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  DOP16_DOP32("zextb", OPCODE_INFO2(0x7400, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0x7400, (2_5or6_9, DUP_GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc40054e0, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0xc40054e0, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP16_DOP32("zexth", OPCODE_INFO2(0x7401, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0x7401, (2_5or6_9, DUP_GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc40055e0, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0xc40055e0, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP16_DOP32("sextb", OPCODE_INFO2(0x7402, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0x7402, (2_5or6_9, DUP_GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc40058e0, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0xc40058e0, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP16_DOP32("sexth", OPCODE_INFO2(0x7403, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0x7403, (2_5or6_9, DUP_GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc40059e0, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO1(0xc40059e0, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("zext",  OPCODE_INFO4(0xc4005400, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (5_9, IMM5b, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("sext",  OPCODE_INFO4(0xc4005800, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (5_9, IMM5b, OPRND_SHIFT_0_BIT), (21_25, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _TRANSFER
#define _TRANSFER   2
  OP16_OP32("rts", OPCODE_INFO0(0x783c), CSKYV2_ISA_E1,
          OPCODE_INFO0(0xe8cf0000), CSKYV2_ISA_E1),
#undef _TRANSFER
#define _TRANSFER   1
  OP16_OP32("jmp", OPCODE_INFO1(0x7800, (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO1(0xe8c00000, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _TRANSFER
#define _TRANSFER   0
  OP16_OP32("revb", OPCODE_INFO2(0x7802, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2,
          OPCODE_INFO2(0xc4006080, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP16_OP32("revh", OPCODE_INFO2(0x7803, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2,
          OPCODE_INFO2(0xc4006100, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP16_OP32("jsr", OPCODE_INFO1(0x7bc1, (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO1(0xe8e00000, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP16_DOP32("mult", OPCODE_INFO2(0x7c00, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x7c00, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4008420, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4008420, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  OP16("mul", OPCODE_INFO2(0x7c00, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),CSKYV2_ISA_E1),
  DOP16_DOP32("mulsh", OPCODE_INFO2(0x7c01, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x7c01, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT), (2_5, 2IN1_DUMMY, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3,
          OPCODE_INFO3(0xc4009020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4009020, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP16("muls.h", OPCODE_INFO2(0x7c01, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP32("mulsw", OPCODE_INFO3(0xc4009420, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4009420, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKY_ISA_DSP),
  OP16_OP32("ld.b", SOPCODE_INFO2(0x8000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xd8000000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP16_OP32("ldb", SOPCODE_INFO2(0x8000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xd8000000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP16_OP32("st.b", SOPCODE_INFO2(0xa000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xdc000000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP16_OP32("stb", SOPCODE_INFO2(0xa000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xdc000000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),

  OP16_OP32("ld.h", SOPCODE_INFO2(0x8800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xd8001000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP16_OP32("ldh", SOPCODE_INFO2(0x8800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xd8001000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP16_OP32("st.h", SOPCODE_INFO2(0xa800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xdc001000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  OP16_OP32("sth", SOPCODE_INFO2(0xa800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xdc001000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),

  DOP16_OP32("ld.w", SOPCODE_INFO2(0x9000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))),
            SOPCODE_INFO2(0x9800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_10, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xd8002000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  DOP16_OP32("ldw", SOPCODE_INFO2(0x9000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))),
            SOPCODE_INFO2(0x9800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_10, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xd8002000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  DOP16_OP32("ld", SOPCODE_INFO2(0x9000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))),
            SOPCODE_INFO2(0x9800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_10, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xd8002000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  DOP16_OP32("st.w", SOPCODE_INFO2(0xb000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))),
            SOPCODE_INFO2(0xb800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_10, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xdc002000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  DOP16_OP32("stw", SOPCODE_INFO2(0xb000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))),
            SOPCODE_INFO2(0xb800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_10, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xdc002000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
  DOP16_OP32("st", SOPCODE_INFO2(0xb000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM_LDST, OPRND_SHIFT_0_BIT))),
            SOPCODE_INFO2(0xb800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), BRACKET_OPRND((NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_10, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1,
            SOPCODE_INFO2(0xdc002000, (21_25, AREG, OPRND_SHIFT_0_BIT), BRACKET_OPRND((16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, IMM_LDST, OPRND_SHIFT_0_BIT))), CSKYV2_ISA_E1),
#ifdef BUILD_AS
  DOP16_DOP32_WITH_WORK("addi", OPCODE_INFO2(0x2000, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, IMM32b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0x2000, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, IMM32b, OPRND_SHIFT_0_BIT)),CSKYV2_ISA_E1,
            OPCODE_INFO2(0xe4000000, (NONE, AREG, OPRND_SHIFT_0_BIT),(NONE, IMM32b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0xe4000000, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, AREG, OPRND_SHIFT_0_BIT),(NONE, IMM32b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1, v2_work_addi),
#else
  DOP16("addi", OPCODE_INFO2(0x2000, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_7, OIMM8b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0x5802, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (2_4, OIMM3b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  DOP16("addi", OPCODE_INFO3(0x1800, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (NONE, REGsp, OPRND_SHIFT_0_BIT),(0_7, IMM8b_LS2, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0x1400, (NONE, REGsp, OPRND_SHIFT_0_BIT),(NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_9, IMM7b_LS2, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  DOP32("addi", OPCODE_INFO3(0xe4000000, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, OIMM12b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0xcc1c0000, (21_25, AREG, OPRND_SHIFT_0_BIT), (NONE, REG_r28, OPRND_SHIFT_0_BIT), (0_17, OIMM18b, OPRND_SHIFT_0_BIT)),CSKYV2_ISA_E1),
#endif
#ifdef BUILD_AS
  DOP16_DOP32_WITH_WORK("subi", OPCODE_INFO2(0x2800, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, IMM32b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0x2800, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, IMM32b, OPRND_SHIFT_0_BIT)),CSKYV2_ISA_E1,
            OPCODE_INFO2(0xe4001000, (NONE, AREG, OPRND_SHIFT_0_BIT),(NONE, IMM32b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0xe4001000, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, AREG, OPRND_SHIFT_0_BIT),(NONE, IMM32b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1, v2_work_subi),
#else
  DOP16("subi", OPCODE_INFO2(0x2800, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_7, OIMM8b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO3(0x5803, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (2_4, OIMM3b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  OP32("subi", OPCODE_INFO3(0xe4001000, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_11, OIMM12b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
  OP16("subi", OPCODE_INFO3(0x1420, (NONE, REGsp, OPRND_SHIFT_0_BIT),(NONE, REGsp, OPRND_SHIFT_0_BIT), (0_4or8_9, IMM7b_LS2, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1),
#endif
  DOP16_DOP32_WITH_WORK("addu", OPCODE_INFO2(0x6000, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x5800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4000020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4000020, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1, v2_work_add_sub),
  DOP16_DOP32_WITH_WORK("add", OPCODE_INFO2(0x6000, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x5800, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4000020, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4000020, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1, v2_work_add_sub),
  DOP16_DOP32_WITH_WORK("subu", OPCODE_INFO2(0x6002, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x5801, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4000080, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4000080, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1, v2_work_add_sub),
  DOP16_DOP32_WITH_WORK("sub", OPCODE_INFO2(0x6002, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO3(0x5801, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (2_4, GREG0_7, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO3(0xc4000080, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)),
          OPCODE_INFO2(0xc4000080, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1, v2_work_add_sub),
#undef _RELOC32
#define _RELOC32 BFD_RELOC_CKCORE_PCREL_IMM26BY2
  OP32("bsr", OPCODE_INFO1(0xe0000000, (0_25, OFF26b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1),
#undef _RELOC32
#define _RELOC32 BFD_RELOC_CKCORE_DOFFSET_IMM18
  OP32("lrs.b", OPCODE_INFO2(0xcc100000, (21_25, AREG, OPRND_SHIFT_0_BIT), (0_17, LABEL_WITH_BRACKET, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("srs.b", OPCODE_INFO2(0xcc100000, (21_25, AREG, OPRND_SHIFT_0_BIT), (0_17, LABEL_WITH_BRACKET, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _RELOC32
#define _RELOC32 BFD_RELOC_CKCORE_DOFFSET_IMM18BY2
  OP32("lrs.h", OPCODE_INFO2(0xcc140000, (21_25, AREG, OPRND_SHIFT_0_BIT), (0_17, LABEL_WITH_BRACKET, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("srs.h", OPCODE_INFO2(0xcc140000, (21_25, AREG, OPRND_SHIFT_0_BIT), (0_17, LABEL_WITH_BRACKET, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _RELOC32
#define _RELOC32 BFD_RELOC_CKCORE_PCREL_FLRW_IMM8BY4
  OP32("flrws", OPCODE_INFO2(0xf4003800, (0_3, FREG, OPRND_SHIFT_0_BIT), (4_7or21_24, FCONSTANT, OPRND_SHIFT_2_BIT)), CSKY_ISA_FLOAT_1E3),
  OP32("flrwd", OPCODE_INFO2(0xf4003900, (0_3, FREG, OPRND_SHIFT_0_BIT), (4_7or21_24, FCONSTANT, OPRND_SHIFT_2_BIT)), CSKY_ISA_FLOAT_3E4),

#undef _RELOC32
#define _RELOC32 BFD_RELOC_CKCORE_DOFFSET_IMM18BY4
  OP32_WITH_WORK("lrs.w", OPCODE_INFO2(0xcc080000, (21_25, AREG, OPRND_SHIFT_0_BIT), (0_17, LABEL_WITH_BRACKET, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3, v2_work_lrsrsw),
  OP32_WITH_WORK("srs.w", OPCODE_INFO2(0xcc180000, (21_25, AREG, OPRND_SHIFT_0_BIT), (0_17, LABEL_WITH_BRACKET, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3, v2_work_lrsrsw),

#undef _RELOC32
#define _RELOC32    BFD_RELOC_CKCORE_PCREL_IMM16BY4
  OP32_WITH_WORK("jsri", OPCODE_INFO1(0xeae00000, (0_15, OFF16b, OPRND_SHIFT_2_BIT)), CSKYV2_ISA_2E3, v2_work_jsri),
#undef _RELOC32
#define _RELOC32    BFD_RELOC_CKCORE_PCREL_IMM16BY2
  OP32("bez",  OPCODE_INFO2(0xe9000000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OFF16b_LSL1, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_2E3),
  OP32("bnez", OPCODE_INFO2(0xe9200000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OFF16b_LSL1, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_2E3),
  OP32("bhz",  OPCODE_INFO2(0xe9400000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OFF16b_LSL1, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_2E3),
  OP32("blsz", OPCODE_INFO2(0xe9600000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OFF16b_LSL1, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_2E3),
  OP32("blz",  OPCODE_INFO2(0xe9800000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OFF16b_LSL1, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_2E3),
  OP32("bhsz", OPCODE_INFO2(0xe9a00000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, OFF16b_LSL1, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_2E3),
#undef _RELAX
#undef _RELOC16
#undef _TRANSFER
#define _TRANSFER   1
#define _RELAX      1
#define _RELOC16    BFD_RELOC_CKCORE_PCREL_IMM10BY2
  OP16_OP32("br", OPCODE_INFO1(0x0400, (0_9, UNCOND10b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1,
            OPCODE_INFO1(0xe8000000, (0_15, UNCOND16b, OPRND_SHIFT_1_BIT)),CSKYV2_ISA_E1),
#undef _TRANSFER
#define _TRANSFER   0
  OP16_OP32("bt",  OPCODE_INFO1(0x0800, (0_9, COND10b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1,
            OPCODE_INFO1(0xe8600000, (0_15, COND16b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_1E2),
  OP16_OP32("bf",  OPCODE_INFO1(0x0c00, (0_9, COND10b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1,
            OPCODE_INFO1(0xe8400000, (0_15, COND16b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_1E2),
#undef _RELOC16
#undef _RELOC32
#undef _RELAX
#define _RELOC16    0
#define _RELOC32    0
#define _RELAX      0
#undef _TRANSFER
#define _TRANSFER   1
  OP16_WITH_WORK("jbr",  OPCODE_INFO1(0x0400, (0_10, UNCOND10b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1, v2_work_jbr),
#undef _TRANSFER
#define _TRANSFER   0
  OP16_WITH_WORK("jbt",  OPCODE_INFO1(0x0800, (0_10, COND10b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1, v2_work_jbtf),
  OP16_WITH_WORK("jbf",  OPCODE_INFO1(0x0c00, (0_10, COND10b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1, v2_work_jbtf),
  OP32_WITH_WORK("jbsr", OPCODE_INFO1(0xe0000000, (0_25, OFF26b, OPRND_SHIFT_1_BIT)), CSKYV2_ISA_E1, v2_work_jbsr),
  OP32_WITH_WORK("movih", OPCODE_INFO2(0xea200000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, IMM16b_MOVIH, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2, v2_work_movih),
  OP32_WITH_WORK("ori",  OPCODE_INFO3(0xec000000, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, IMM16b_ORI, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2, v2_work_ori),
  DOP32_WITH_WORK("bgeni", OPCODE_INFO2(0xea000000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_4, IMM4b, OPRND_SHIFT_0_BIT)),
            OPCODE_INFO2(0xea200000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)),CSKYV2_ISA_E1, v2_work_bgeni),
#undef _RELOC16
#undef _RELOC32
#define _RELOC16    BFD_RELOC_CKCORE_PCREL_IMM7BY4
#define _RELOC32    BFD_RELOC_CKCORE_PCREL_IMM16BY4
  OP16_OP32_WITH_WORK("lrw",  OPCODE_INFO2(0x1000, (5_7, GREG0_7, OPRND_SHIFT_0_BIT), (0_4or8_9, CONSTANT, OPRND_SHIFT_2_BIT)), CSKYV2_ISA_E1,
                     OPCODE_INFO2(0xea800000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, CONSTANT, OPRND_SHIFT_2_BIT)), CSKYV2_ISA_E1, v2_work_lrw),
#undef _RELOC16
#undef _RELOC32
#define _RELOC16    0
#define _RELOC32    0

#undef _RELAX
#define _RELAX      1
  OP32("jbez",  OPCODE_INFO2(0xe9000000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, JCOMPZ, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("jbnez", OPCODE_INFO2(0xe9200000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, JCOMPZ, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("jbhz",  OPCODE_INFO2(0xe9400000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, JCOMPZ, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("jblsz", OPCODE_INFO2(0xe9600000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, JCOMPZ, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("jblz",  OPCODE_INFO2(0xe9800000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, JCOMPZ, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  OP32("jbhsz", OPCODE_INFO2(0xe9a00000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, JCOMPZ, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
#undef _RELAX
#define _RELAX      0
  /* The following are aliases for other instructions.  */
  /* setc -> cmphs r0, r0.  */
  OP16("setc",  OPCODE_INFO0(0x6400), CSKYV2_ISA_E1),
  /* clrc -> cmpne r0, r0.  */
#undef _RELAX
#define _RELAX      0
  /* The following are aliases for other instructions.  */
  /* setc -> cmphs r0, r0.  */
  OP16("setc",  OPCODE_INFO0(0x6400), CSKYV2_ISA_E1),
  /* clrc -> cmpne r0, r0.  */
  OP16("clrc",  OPCODE_INFO0(0x6402), CSKYV2_ISA_E1),
  /* tstlt rd -> btsti rd,31 */
  OP32("tstlt", OPCODE_INFO1(0xc7e02880, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  /* rsub rz, ry, rx -> subu rz, rx, ry.  */
  DOP32("rsub",  OPCODE_INFO3(0xc4000080, (0_4, AREG, OPRND_SHIFT_0_BIT), (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)),
       OPCODE_INFO2(0xc4000080, (0_4or21_25, DUP_AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  /* cmplei rd,X -> cmplti rd,X+1 */
  OP16_OP32("cmplei", OPCODE_INFO2(0x3820, (8_10, GREG0_7, OPRND_SHIFT_0_BIT), (0_4, IMM5b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xeb200000, (16_20, AREG, OPRND_SHIFT_0_BIT), (0_15, IMM16b, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  /* cmpls -> cmphs.  */
  OP16_OP32("cmpls", OPCODE_INFO2(0x6400, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4000420, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  /* cmpgt -> cmplt.  */
  OP16_OP32("cmpgt", OPCODE_INFO2(0x6401, (6_9, GREG0_15, OPRND_SHIFT_0_BIT), (2_5, GREG0_15, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4000440, (21_25, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  /* tstle rd -> cmplti rd,1 */
  OP16_OP32("tstle", OPCODE_INFO1(0x3820,  (8_10, GREG0_7, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
            OPCODE_INFO1(0xeb200000, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  /* tstne rd -> cmpnei rd,0 */
  OP16_OP32("tstne", OPCODE_INFO1(0x3840, (8_10, GREG0_7, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO1(0xeb400000, (16_20, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_1E2),
  /* rotri rz, rx, imm5 -> rotli rz, rx, 32-imm5.  */
  DOP32("rotri",  OPCODE_INFO3(0xc4004900, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b_RORI, OPRND_SHIFT_0_BIT)),
        OPCODE_INFO2(0xc4004900, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b_RORI, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),
  DOP32("rori",  OPCODE_INFO3(0xc4004900, (0_4, AREG, OPRND_SHIFT_0_BIT), (16_20, AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b_RORI, OPRND_SHIFT_0_BIT)),
        OPCODE_INFO2(0xc4004900, (0_4or16_20, DUP_AREG, OPRND_SHIFT_0_BIT), (21_25, IMM5b_RORI, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3),

  /* rotlc rd -> addc rd, rd/ addc rd, rd, rd.  */
  OP16_OP32_WITH_WORK("rotlc", OPCODE_INFO2(0x6001, (NONE, GREG0_15, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4000040, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, CONST1, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_2E3, v2_work_rotlc),
  /* not rd -> nor rd, rd, not rz, rx -> nor rz, rx, rx.  */
  OP16_OP32_WITH_WORK("not", OPCODE_INFO1(0x6c02, (NONE, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1,
          OPCODE_INFO2(0xc4002480, (NONE, AREG, OPRND_SHIFT_0_BIT), (NONE, AREG, OPRND_SHIFT_0_BIT)), CSKYV2_ISA_E1, v2_work_not),
  {NULL}
};


