/* mips.h.  Mips opcode list for GDB, the GNU debugger.
   Copyright 1993, 94, 95, 96, 1997 Free Software Foundation, Inc.
   Contributed by Ralph Campbell and OSF
   Commented and modified by Ian Lance Taylor, Cygnus Support

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
1, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _MIPS_H_
#define _MIPS_H_

/* These are bit masks and shift counts to use to access the various
   fields of an instruction.  To retrieve the X field of an
   instruction, use the expression
	(i >> OP_SH_X) & OP_MASK_X
   To set the same field (to j), use
	i = (i &~ (OP_MASK_X << OP_SH_X)) | (j << OP_SH_X)

   Make sure you use fields that are appropriate for the instruction,
   of course.  

   The 'i' format uses OP, RS, RT and IMMEDIATE.  

   The 'j' format uses OP and TARGET.

   The 'r' format uses OP, RS, RT, RD, SHAMT and FUNCT.

   The 'b' format uses OP, RS, RT and DELTA.

   The floating point 'i' format uses OP, RS, RT and IMMEDIATE.

   The floating point 'r' format uses OP, FMT, FT, FS, FD and FUNCT.

   A breakpoint instruction uses OP, CODE and SPEC (10 bits of the
   breakpoint instruction are not defined; Kane says the breakpoint
   code field in BREAK is 20 bits; yet MIPS assemblers and debuggers
   only use ten bits).

   The syscall instruction uses SYSCALL.

   The general coprocessor instructions use COPZ.  */

#define OP_MASK_OP		0x3f
#define OP_SH_OP		26
#define OP_MASK_RS		0x1f
#define OP_SH_RS		21
#define OP_MASK_FR		0x1f
#define OP_SH_FR		21
#define OP_MASK_FMT		0x1f
#define OP_SH_FMT		21
#define OP_MASK_BCC		0x7
#define OP_SH_BCC		18
#define OP_MASK_CODE		0x3ff
#define OP_SH_CODE		16
#define OP_MASK_RT		0x1f
#define OP_SH_RT		16
#define OP_MASK_FT		0x1f
#define OP_SH_FT		16
#define OP_MASK_CACHE		0x1f
#define OP_SH_CACHE		16
#define OP_MASK_RD		0x1f
#define OP_SH_RD		11
#define OP_MASK_FS		0x1f
#define OP_SH_FS		11
#define OP_MASK_PREFX		0x1f
#define OP_SH_PREFX		11
#define OP_MASK_CCC		0x7
#define OP_SH_CCC		8
#define OP_MASK_SYSCALL		0xfffff
#define OP_SH_SYSCALL		6
#define OP_MASK_SHAMT		0x1f
#define OP_SH_SHAMT		6
#define OP_MASK_FD		0x1f
#define OP_SH_FD		6
#define OP_MASK_TARGET		0x3ffffff
#define OP_SH_TARGET		0
#define OP_MASK_COPZ		0x1ffffff
#define OP_SH_COPZ		0
#define OP_MASK_IMMEDIATE	0xffff
#define OP_SH_IMMEDIATE		0
#define OP_MASK_DELTA		0xffff
#define OP_SH_DELTA		0
#define OP_MASK_FUNCT		0x3f
#define OP_SH_FUNCT		0
#define OP_MASK_SPEC		0x3f
#define OP_SH_SPEC		0
#define OP_SH_LOCC              8       /* FP condition code */
#define OP_SH_HICC              18      /* FP condition code */
#define OP_MASK_CC              0x7
#define OP_SH_COP1NORM          25      /* Normal COP1 encoding */
#define OP_MASK_COP1NORM        0x1     /* a single bit */
#define OP_SH_COP1SPEC          21      /* COP1 encodings */
#define OP_MASK_COP1SPEC        0xf
#define OP_MASK_COP1SCLR        0x4
#define OP_MASK_COP1CMP         0x3
#define OP_SH_COP1CMP           4
#define OP_SH_FORMAT            21      /* FP short format field */
#define OP_MASK_FORMAT          0x7
#define OP_SH_TRUE              16
#define OP_MASK_TRUE            0x1
#define OP_SH_GE                17
#define OP_MASK_GE              0x01
#define OP_SH_UNSIGNED          16
#define OP_MASK_UNSIGNED        0x1
#define OP_SH_HINT              16
#define OP_MASK_HINT            0x1f
#define OP_SH_MMI               0       /* Multimedia (parallel) op */
#define OP_MASK_MMI             0x3f 
#define OP_SH_MMISUB            6
#define OP_MASK_MMISUB          0x1f
/* start-sanitize-vr5400 */
#define OP_MASK_PERFREG		0x1f	/* Performance monitoring */
#define OP_SH_PERFREG		1
#define OP_MASK_VECBYTE		0x7	/* Selector field is really 4 bits,
					   but 0x8-0xf don't select bytes.  */
#define OP_SH_VECBYTE		22
#define OP_MASK_VECALIGN	0x7	/* Vector byte-align (alni.ob) op.  */
#define OP_SH_VECALIGN		21
/* end-sanitize-vr5400 */

/* This structure holds information for a particular instruction.  */

struct mips_opcode
{
  /* The name of the instruction.  */
  const char *name;
  /* A string describing the arguments for this instruction.  */
  const char *args;
  /* The basic opcode for the instruction.  When assembling, this
     opcode is modified by the arguments to produce the actual opcode
     that is used.  If pinfo is INSN_MACRO, then this is instead the
     ISA level of the macro (0 or 1 is always supported, 2 is ISA 2,
     etc.).  */
  unsigned long match;
  /* If pinfo is not INSN_MACRO, then this is a bit mask for the
     relevant portions of the opcode when disassembling.  If the
     actual opcode anded with the match field equals the opcode field,
     then we have found the correct instruction.  If pinfo is
     INSN_MACRO, then this field is the macro identifier.  */
  unsigned long mask;
  /* For a macro, this is INSN_MACRO.  Otherwise, it is a collection
     of bits describing the instruction, notably any relevant hazard
     information.  */
  unsigned long pinfo;
  /* A collection of bits describing the instruction sets of which this
     instruction is a member. */
  unsigned long membership;
};

/* These are the characters which may appears in the args field of an
   instruction.  They appear in the order in which the fields appear
   when the instruction is used.  Commas and parentheses in the args
   string are ignored when assembling, and written into the output
   when disassembling.

   Each of these characters corresponds to a mask field defined above.

   "<" 5 bit shift amount (OP_*_SHAMT)
   ">" shift amount between 32 and 63, stored after subtracting 32 (OP_*_SHAMT)
   "a" 26 bit target address (OP_*_TARGET)
   "b" 5 bit base register (OP_*_RS)
   "c" 10 bit breakpoint code (OP_*_CODE)
   "d" 5 bit destination register specifier (OP_*_RD)
   "h" 5 bit prefx hint (OP_*_PREFX)
   "i" 16 bit unsigned immediate (OP_*_IMMEDIATE)
   "j" 16 bit signed immediate (OP_*_DELTA)
   "k" 5 bit cache opcode in target register position (OP_*_CACHE)
start-sanitize-vr5400
	     also vr5400 vector ops immediate operand
end-sanitize-vr5400
   "o" 16 bit signed offset (OP_*_DELTA)
   "p" 16 bit PC relative branch target address (OP_*_DELTA)
   "r" 5 bit same register used as both source and target (OP_*_RS)
   "s" 5 bit source register specifier (OP_*_RS)
   "t" 5 bit target register (OP_*_RT)
   "u" 16 bit upper 16 bits of address (OP_*_IMMEDIATE)
   "v" 5 bit same register used as both source and destination (OP_*_RS)
   "w" 5 bit same register used as both target and destination (OP_*_RT)
   "C" 25 bit coprocessor function code (OP_*_COPZ)
   "B" 20 bit syscall function code (OP_*_SYSCALL)
   "x" accept and ignore register name
   "z" must be zero register

   Floating point instructions:
   "D" 5 bit destination register (OP_*_FD)
   "M" 3 bit compare condition code (OP_*_CCC) (only used for mips4 and up)
   "N" 3 bit branch condition code (OP_*_BCC) (only used for mips4 and up)
   "S" 5 bit fs source 1 register (OP_*_FS)
   "T" 5 bit ft source 2 register (OP_*_FT)
   "R" 5 bit fr source 3 register (OP_*_FR)
   "V" 5 bit same register used as floating source and destination (OP_*_FS)
   "W" 5 bit same register used as floating target and destination (OP_*_FT)

   Coprocessor instructions:
   "E" 5 bit target register (OP_*_RT)
   "G" 5 bit destination register (OP_*_RD)
start-sanitize-vr5400
   "P" 5 bit performance-monitor register (OP_*_PERFREG)
   "e" 5 bit vector register byte specifier (OP_*_VECBYTE)
   "%" 3 bit immediate vr5400 vector alignment operand (OP_*_VECALIGN)
   see also "k" above
end-sanitize-vr5400

   Macro instructions:
   "A" General 32 bit expression
   "I" 32 bit immediate
   "F" 64 bit floating point constant in .rdata
   "L" 64 bit floating point constant in .lit8
   "f" 32 bit floating point constant
   "l" 32 bit floating point constant in .lit4

   Other:
   "()" parens surrounding optional value
   ","  separates operands
start-sanitize-vr5400
   "[]" brackets around index for vector-op scalar operand specifier (vr5400)
end-sanitize-vr5400

   Characters used so far, for quick reference when adding more:
start-sanitize-vr5400
   "Pe%[]" plus...
end-sanitize-vr5400
   "<>(),"
   "ABCDEFGILMNSTRVW"
   "abcdfhijkloprstuvwxz"
*/

/* These are the bits which may be set in the pinfo field of an
   instructions, if it is not equal to INSN_MACRO.  */

/* Modifies the general purpose register in OP_*_RD.  */
#define INSN_WRITE_GPR_D            0x00000001
/* Modifies the general purpose register in OP_*_RT.  */
#define INSN_WRITE_GPR_T            0x00000002
/* Modifies general purpose register 31.  */
#define INSN_WRITE_GPR_31           0x00000004
/* Modifies the floating point register in OP_*_FD.  */
#define INSN_WRITE_FPR_D            0x00000008
/* Modifies the floating point register in OP_*_FS.  */
#define INSN_WRITE_FPR_S            0x00000010
/* Modifies the floating point register in OP_*_FT.  */
#define INSN_WRITE_FPR_T            0x00000020
/* Reads the general purpose register in OP_*_RS.  */
#define INSN_READ_GPR_S             0x00000040
/* Reads the general purpose register in OP_*_RT.  */
#define INSN_READ_GPR_T             0x00000080
/* Reads the floating point register in OP_*_FS.  */
#define INSN_READ_FPR_S             0x00000100
/* Reads the floating point register in OP_*_FT.  */
#define INSN_READ_FPR_T             0x00000200
/* Reads the floating point register in OP_*_FR.  */
#define INSN_READ_FPR_R		    0x00000400
/* Modifies coprocessor condition code.  */
#define INSN_WRITE_COND_CODE        0x00000800
/* Reads coprocessor condition code.  */
#define INSN_READ_COND_CODE         0x00001000
/* TLB operation.  */
#define INSN_TLB                    0x00002000
/* Reads coprocessor register other than floating point register.  */
#define INSN_COP                    0x00004000
/* Instruction loads value from memory, requiring delay.  */
#define INSN_LOAD_MEMORY_DELAY      0x00008000
/* Instruction loads value from coprocessor, requiring delay.  */
#define INSN_LOAD_COPROC_DELAY	    0x00010000
/* Instruction has unconditional branch delay slot.  */
#define INSN_UNCOND_BRANCH_DELAY    0x00020000
/* Instruction has conditional branch delay slot.  */
#define INSN_COND_BRANCH_DELAY      0x00040000
/* Conditional branch likely: if branch not taken, insn nullified.  */
#define INSN_COND_BRANCH_LIKELY	    0x00080000
/* Moves to coprocessor register, requiring delay.  */
#define INSN_COPROC_MOVE_DELAY      0x00100000
/* Loads coprocessor register from memory, requiring delay.  */
#define INSN_COPROC_MEMORY_DELAY    0x00200000
/* Reads the HI register.  */
#define INSN_READ_HI		    0x00400000
/* Reads the LO register.  */
#define INSN_READ_LO		    0x00800000
/* Modifies the HI register.  */
#define INSN_WRITE_HI		    0x01000000
/* Modifies the LO register.  */
#define INSN_WRITE_LO		    0x02000000
/* Takes a trap (easier to keep out of delay slot).  */
#define INSN_TRAP                   0x04000000
/* Instruction stores value into memory.  */
#define INSN_STORE_MEMORY	    0x08000000
/* Instruction uses single precision floating point.  */
#define FP_S			    0x10000000
/* Instruction uses double precision floating point.  */
#define FP_D			    0x20000000

/* As yet unused bits:              0x40000000 */

/* Instruction is actually a macro.  It should be ignored by the
   disassembler, and requires special treatment by the assembler.  */
#define INSN_MACRO                  0xffffffff





/* MIPS ISA field--CPU level at which insn is supported.  */
#define INSN_ISA		    0x0000000F
/* An instruction which is not part of any basic MIPS ISA.
   (ie it is a chip specific instruction)  */
#define INSN_NO_ISA		    0x00000000
/* MIPS ISA 1 instruction.  */
#define INSN_ISA1		    0x00000001
/* MIPS ISA 2 instruction (R6000 or R4000).  */
#define INSN_ISA2		    0x00000002
/* MIPS ISA 3 instruction (R4000).  */
#define INSN_ISA3		    0x00000003
/* MIPS ISA 4 instruction (R8000).  */
#define INSN_ISA4		    0x00000004

/* Chip specific instructions.  These are bitmasks.  */
/* MIPS R4650 instruction.  */
#define INSN_4650		    0x00000010
/* LSI R4010 instruction.  */
#define INSN_4010		    0x00000020
/* NEC VR4100 instruction. */
#define INSN_4100                   0x00000040
/* Toshiba R3900 instruction.  */
#define INSN_3900                   0x00000080
/* start-sanitize-vr5400 */
/* NEC VR5400 instruction.  */
#define INSN_5400		    0x00001000
/* end-sanitize-vr5400 */
/* start-sanitize-r5900 */
/* Toshiba R5900 instruction */
#define INSN_5900                   0x00000100
/* end-sanitize-r5900 */

/* This is a list of macro expanded instructions.
 *
 * _I appended means immediate
 * _A appended means address
 * _AB appended means address with base register
 * _D appended means 64 bit floating point constant
 * _S appended means 32 bit floating point constant
 */
enum {
    M_ABS,
    M_ADD_I,
    M_ADDU_I,
    M_AND_I,
    M_BEQ,
    M_BEQ_I,
    M_BEQL_I,
    M_BGE,
    M_BGEL,
    M_BGE_I,
    M_BGEL_I,
    M_BGEU,
    M_BGEUL,
    M_BGEU_I,
    M_BGEUL_I,
    M_BGT,
    M_BGTL,
    M_BGT_I,
    M_BGTL_I,
    M_BGTU,
    M_BGTUL,
    M_BGTU_I,
    M_BGTUL_I,
    M_BLE,
    M_BLEL,
    M_BLE_I,
    M_BLEL_I,
    M_BLEU,
    M_BLEUL,
    M_BLEU_I,
    M_BLEUL_I,
    M_BLT,
    M_BLTL,
    M_BLT_I,
    M_BLTL_I,
    M_BLTU,
    M_BLTUL,
    M_BLTU_I,
    M_BLTUL_I,
    M_BNE,
    M_BNE_I,
    M_BNEL_I,
    M_DABS,
    M_DADD_I,
    M_DADDU_I,
    M_DDIV_3,
    M_DDIV_3I,
    M_DDIVU_3,
    M_DDIVU_3I,
    M_DIV_3,
    M_DIV_3I,
    M_DIVU_3,
    M_DIVU_3I,
    M_DLA_AB,
    M_DLI,
    M_DMUL,
    M_DMUL_I, 
    M_DMULO,
    M_DMULO_I, 
    M_DMULOU,
    M_DMULOU_I, 
    M_DREM_3,
    M_DREM_3I,
    M_DREMU_3,
    M_DREMU_3I,
    M_DSUB_I,
    M_DSUBU_I,
    M_DSUBU_I_2,
    M_J_A,
    M_JAL_1,
    M_JAL_2,
    M_JAL_A,
    M_L_DOB,
    M_L_DAB,
    M_LA_AB,
    M_LB_A,
    M_LB_AB,
    M_LBU_A,
    M_LBU_AB,
    M_LD_A,
    M_LD_OB,
    M_LD_AB,
    M_LDC1_AB,
    M_LDC2_AB,
    M_LDC3_AB,
    M_LDL_AB,
    M_LDR_AB,
    M_LH_A,
    M_LH_AB,
    M_LHU_A,
    M_LHU_AB,
    M_LI,
    M_LI_D,
    M_LI_DD,
    M_LI_S,
    M_LI_SS,
    M_LL_AB,
    M_LLD_AB,
    M_LS_A,
    M_LW_A,
    M_LW_AB,
    M_LWC0_A,
    M_LWC0_AB,
    M_LWC1_A,
    M_LWC1_AB,
    M_LWC2_A,
    M_LWC2_AB,
    M_LWC3_A,
    M_LWC3_AB,
    M_LWL_A,
    M_LWL_AB,
    M_LWR_A,
    M_LWR_AB,
    M_LWU_AB,
    M_MUL,
    M_MUL_I, 
    M_MULO,
    M_MULO_I, 
    M_MULOU,
    M_MULOU_I, 
    M_NOR_I,
    M_OR_I,
    M_REM_3,
    M_REM_3I,
    M_REMU_3,
    M_REMU_3I,
    M_ROL,
    M_ROL_I,
    M_ROR,
    M_ROR_I,
    M_S_DA,
    M_S_DOB,
    M_S_DAB,
    M_S_S,
    M_SC_AB,
    M_SCD_AB,
    M_SD_A,
    M_SD_OB,
    M_SD_AB,
    M_SDC1_AB,
    M_SDC2_AB,
    M_SDC3_AB,
    M_SDL_AB,
    M_SDR_AB,
    M_SEQ,
    M_SEQ_I,
    M_SGE,
    M_SGE_I,
    M_SGEU,
    M_SGEU_I,
    M_SGT,
    M_SGT_I,
    M_SGTU,
    M_SGTU_I,
    M_SLE,
    M_SLE_I,
    M_SLEU,
    M_SLEU_I,
    M_SLT_I,
    M_SLTU_I,
    M_SNE,
    M_SNE_I,
    M_SB_A,
    M_SB_AB,
    M_SH_A,
    M_SH_AB,
    M_SW_A,
    M_SW_AB,
    M_SWC0_A,
    M_SWC0_AB,
    M_SWC1_A,
    M_SWC1_AB,
    M_SWC2_A,
    M_SWC2_AB,
    M_SWC3_A,
    M_SWC3_AB,
    M_SWL_A,
    M_SWL_AB,
    M_SWR_A,
    M_SWR_AB,
    M_SUB_I,
    M_SUBU_I,
    M_SUBU_I_2,
    M_TEQ_I,
    M_TGE_I,
    M_TGEU_I,
    M_TLT_I,
    M_TLTU_I,
    M_TNE_I,
    M_TRUNCWD,
    M_TRUNCWS,
    M_ULD,
    M_ULD_A,
    M_ULH,
    M_ULH_A,
    M_ULHU,
    M_ULHU_A,
    M_ULW,
    M_ULW_A,
    M_USH,
    M_USH_A,
    M_USW,
    M_USW_A,
    M_USD,
    M_USD_A,
    M_XOR_I,
    M_COP0,
    M_COP1,
    M_COP2,
    M_COP3,
    M_NUM_MACROS
};


/* The order of overloaded instructions matters.  Label arguments and
   register arguments look the same. Instructions that can have either
   for arguments must apear in the correct order in this table for the
   assembler to pick the right one. In other words, entries with
   immediate operands must apear after the same instruction with
   registers.

   Many instructions are short hand for other instructions (i.e., The
   jal <register> instruction is short for jalr <register>).  */

extern const struct mips_opcode mips_builtin_opcodes[];
extern const int bfd_mips_num_builtin_opcodes;
extern struct mips_opcode *mips_opcodes;
extern int bfd_mips_num_opcodes;
#define NUMOPCODES bfd_mips_num_opcodes


/* The rest of this file adds definitions for the mips16 TinyRISC
   processor.  */

/* These are the bitmasks and shift counts used for the different
   fields in the instruction formats.  Other than OP, no masks are
   provided for the fixed portions of an instruction, since they are
   not needed.

   The I format uses IMM11.

   The RI format uses RX and IMM8.

   The RR format uses RX, and RY.

   The RRI format uses RX, RY, and IMM5.

   The RRR format uses RX, RY, and RZ.

   The RRI_A format uses RX, RY, and IMM4.

   The SHIFT format uses RX, RY, and SHAMT.

   The I8 format uses IMM8.

   The I8_MOVR32 format uses RY and REGR32.

   The IR_MOV32R format uses REG32R and MOV32Z.

   The I64 format uses IMM8.

   The RI64 format uses RY and IMM5.
   */

#define MIPS16OP_MASK_OP	0x1f
#define MIPS16OP_SH_OP		11
#define MIPS16OP_MASK_IMM11	0x7ff
#define MIPS16OP_SH_IMM11	0
#define MIPS16OP_MASK_RX	0x7
#define MIPS16OP_SH_RX		8
#define MIPS16OP_MASK_IMM8	0xff
#define MIPS16OP_SH_IMM8	0
#define MIPS16OP_MASK_RY	0x7
#define MIPS16OP_SH_RY		5
#define MIPS16OP_MASK_IMM5	0x1f
#define MIPS16OP_SH_IMM5	0
#define MIPS16OP_MASK_RZ	0x7
#define MIPS16OP_SH_RZ		2
#define MIPS16OP_MASK_IMM4	0xf
#define MIPS16OP_SH_IMM4	0
#define MIPS16OP_MASK_REGR32	0x1f
#define MIPS16OP_SH_REGR32	0
#define MIPS16OP_MASK_REG32R	0x1f
#define MIPS16OP_SH_REG32R	3
#define MIPS16OP_EXTRACT_REG32R(i) ((((i) >> 5) & 7) | ((i) & 0x18))
#define MIPS16OP_MASK_MOVE32Z	0x7
#define MIPS16OP_SH_MOVE32Z	0
#define MIPS16OP_MASK_IMM6	0x3f
#define MIPS16OP_SH_IMM6	5

/* These are the characters which may appears in the args field of an
   instruction.  They appear in the order in which the fields appear
   when the instruction is used.  Commas and parentheses in the args
   string are ignored when assembling, and written into the output
   when disassembling.

   "y" 3 bit register (MIPS16OP_*_RY)
   "x" 3 bit register (MIPS16OP_*_RX)
   "z" 3 bit register (MIPS16OP_*_RZ)
   "Z" 3 bit register (MIPS16OP_*_MOVE32Z)
   "v" 3 bit same register as source and destination (MIPS16OP_*_RX)
   "w" 3 bit same register as source and destination (MIPS16OP_*_RY)
   "0" zero register ($0)
   "S" stack pointer ($sp or $29)
   "P" program counter
   "R" return address register ($ra or $31)
   "X" 5 bit MIPS register (MIPS16OP_*_REGR32)
   "Y" 5 bit MIPS register (MIPS16OP_*_REG32R)
   "6" 6 bit unsigned break code (MIPS16OP_*_IMM6)
   "a" 26 bit jump address
   "e" 11 bit extension value
   "l" register list for entry instruction
   "L" register list for exit instruction

   The remaining codes may be extended.  Except as otherwise noted,
   the full extended operand is a 16 bit signed value.
   "<" 3 bit unsigned shift count * 0 (MIPS16OP_*_RZ) (full 5 bit unsigned)
   ">" 3 bit unsigned shift count * 0 (MIPS16OP_*_RX) (full 5 bit unsigned)
   "[" 3 bit unsigned shift count * 0 (MIPS16OP_*_RZ) (full 6 bit unsigned)
   "]" 3 bit unsigned shift count * 0 (MIPS16OP_*_RX) (full 6 bit unsigned)
   "4" 4 bit signed immediate * 0 (MIPS16OP_*_IMM4) (full 15 bit signed)
   "5" 5 bit unsigned immediate * 0 (MIPS16OP_*_IMM5)
   "H" 5 bit unsigned immediate * 2 (MIPS16OP_*_IMM5)
   "W" 5 bit unsigned immediate * 4 (MIPS16OP_*_IMM5)
   "D" 5 bit unsigned immediate * 8 (MIPS16OP_*_IMM5)
   "j" 5 bit signed immediate * 0 (MIPS16OP_*_IMM5)
   "8" 8 bit unsigned immediate * 0 (MIPS16OP_*_IMM8)
   "V" 8 bit unsigned immediate * 4 (MIPS16OP_*_IMM8)
   "C" 8 bit unsigned immediate * 8 (MIPS16OP_*_IMM8)
   "U" 8 bit unsigned immediate * 0 (MIPS16OP_*_IMM8) (full 16 bit unsigned)
   "k" 8 bit signed immediate * 0 (MIPS16OP_*_IMM8)
   "K" 8 bit signed immediate * 8 (MIPS16OP_*_IMM8)
   "p" 8 bit conditional branch address (MIPS16OP_*_IMM8)
   "q" 11 bit branch address (MIPS16OP_*_IMM11)
   "A" 8 bit PC relative address * 4 (MIPS16OP_*_IMM8)
   "B" 5 bit PC relative address * 8 (MIPS16OP_*_IMM5)
   "E" 5 bit PC relative address * 4 (MIPS16OP_*_IMM5)
   */

/* For the mips16, we use the same opcode table format and a few of
   the same flags.  However, most of the flags are different.  */

/* Modifies the register in MIPS16OP_*_RX.  */
#define MIPS16_INSN_WRITE_X		    0x00000001
/* Modifies the register in MIPS16OP_*_RY.  */
#define MIPS16_INSN_WRITE_Y		    0x00000002
/* Modifies the register in MIPS16OP_*_RZ.  */
#define MIPS16_INSN_WRITE_Z		    0x00000004
/* Modifies the T ($24) register.  */
#define MIPS16_INSN_WRITE_T		    0x00000008
/* Modifies the SP ($29) register.  */
#define MIPS16_INSN_WRITE_SP		    0x00000010
/* Modifies the RA ($31) register.  */
#define MIPS16_INSN_WRITE_31		    0x00000020
/* Modifies the general purpose register in MIPS16OP_*_REG32R.  */
#define MIPS16_INSN_WRITE_GPR_Y		    0x00000040
/* Reads the register in MIPS16OP_*_RX.  */
#define MIPS16_INSN_READ_X		    0x00000080
/* Reads the register in MIPS16OP_*_RY.  */
#define MIPS16_INSN_READ_Y		    0x00000100
/* Reads the register in MIPS16OP_*_MOVE32Z.  */
#define MIPS16_INSN_READ_Z		    0x00000200
/* Reads the T ($24) register.  */
#define MIPS16_INSN_READ_T		    0x00000400
/* Reads the SP ($29) register.  */
#define MIPS16_INSN_READ_SP		    0x00000800
/* Reads the RA ($31) register.  */
#define MIPS16_INSN_READ_31		    0x00001000
/* Reads the program counter.  */
#define MIPS16_INSN_READ_PC		    0x00002000
/* Reads the general purpose register in MIPS16OP_*_REGR32.  */
#define MIPS16_INSN_READ_GPR_X		    0x00004000

/* The following flags have the same value for the mips16 opcode
   table:
   INSN_UNCOND_BRANCH_DELAY
   INSN_COND_BRANCH_DELAY
   INSN_COND_BRANCH_LIKELY (never used)
   INSN_READ_HI
   INSN_READ_LO
   INSN_WRITE_HI
   INSN_WRITE_LO
   INSN_TRAP
   INSN_ISA3
   */

extern const struct mips_opcode mips16_opcodes[];
extern const int bfd_mips16_num_opcodes;

#endif /* _MIPS_H_ */
