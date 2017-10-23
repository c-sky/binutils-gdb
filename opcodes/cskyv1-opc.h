/* Assembler instructions for C-SKY's ckcore processor
   Copyright 1999, 2000 Free Software Foundation, Inc.

   
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef __CSKY_OPC_H__
#define __CSKY_OPC_H__

#include "ansidecl.h"
#include "bfd.h"
#include "elf/csky.h"

/*
 * CKCORE Instruction Class
 */
typedef enum
{
  O0,    OT,   O1,   OC,   O2,    X1,    OI,    OB,
  OMa,   SI,   I7,   LS,   BR,    BSR,   BL,    LR,    LJ,  LJS,
  RM,    RQ,   JSR,  JMP,  OBRa,  OBRb,  OBRc,  OBR2,
  O1R1,  OMb,  OMc,  SIa,
  MULSH, OPSR,
  O1_CP, O2_CPGR, O2_CPCR,
  O_KWGJ1, O_KWGJ2, /* For CAA */
  JC,    JU,   JL,   RSI,  DO21,  OB2,
  O1_E2, O1_E, O2_E, OI_E, OB_E, O1R1_E, SIa_E
}ckcore_opclass;

/*
 * CKCORE instruction information & opcode.
 */
typedef struct inst
{
  char *         name;
  /* the instruction class */
  ckcore_opclass  opclass;
  unsigned char  transfer;
  /* The instruction opcode */
  unsigned short inst;
  /* 
   * One bit cpu_flags for the opcode.  These are used to indicate which
   * specific processors support the instructions.  The values are the
   * same as those for the struct powerpc_opcode flags field.  
   */
  unsigned long cpu_flag;
}ckcore_opcode_info;


#ifdef DEFINE_TABLE
static ckcore_opcode_info ckcore_table[] =
{
  { "bkpt", O0, 0, 0x0000, M_CK510 | M_CK610 },
  { "sync", O0, 0, 0x0001, M_CK510 | M_CK610 },
  { "rte",  O0, 2, 0x0002, M_CK510 | M_CK610 },
  { "rfe",  O0, 2, 0x0002, M_CK510 | M_CK610 },
  { "rfi",  O0, 2, 0x0003, M_CK510 | M_CK610 },
  { "stop", O0, 0, 0x0004, M_CK510 | M_CK610 },
  { "wait", O0, 0, 0x0005, M_CK510 | M_CK610 },
  { "doze", O0, 0, 0x0006, M_CK510 | M_CK610 },
  { "idly4",O0, 0, 0x0007, M_CK510 | M_CK610 },
  { "trap", OT,	0, 0x0008, M_CK510 | M_CK610 },
  /* DSP/Coprocessor */
  { "mvtc", O0, 0, 0x000C, M_DSP },
  { "cprc", O0,	0, 0x000D, M_CK610 | M_CP },
  /* SPACE:        0x000E - 0x000F */
  { "cpseti", O1_CP, 0, 0x0010, M_CK610 | M_CP },
  { "mvc",  O1, 0, 0x0020, M_CK510 | M_CK610 },
  { "mvcv", O1, 0, 0x0030, M_CK510 | M_CK610 },
  { "ldq",  RQ, 0, 0x0040, M_CK510 | M_CK610 },
  { "stq",  RQ, 0, 0x0050, M_CK510 | M_CK610 },
  { "ldm",  RM, 0, 0x0060, M_CK510 | M_CK610 },
  { "stm",  RM, 0, 0x0070, M_CK510 | M_CK610 },
  { "dect", O1_E2, 0, 0x0080, M_CK510 | M_CK610 },
  { "decf", O1_E2, 0, 0x0090, M_CK510 | M_CK610 },
  { "inct", O1_E2, 0, 0x00A0, M_CK510 | M_CK610 },
  { "incf", O1_E2, 0, 0x00B0, M_CK510 | M_CK610 },
  { "jmp", JMP, 2, 0x00C0, M_CK510 | M_CK610 },
#define	ckcore_INST_JMP	0x00C0
  { "jsr", JSR, 0, 0x00D0, M_CK510 | M_CK610 },
#define	ckcore_INST_JSR	0x00E0
  { "ff1",   O1_E, 0, 0x00E0, M_CK510 | M_CK610 },
  { "brev",  O1_E, 0, 0x00F0, M_CK510 | M_CK610 },
  { "xtrb3", X1, 0, 0x0100, M_CK510 | M_CK610 },
  { "xtrb2", X1, 0, 0x0110, M_CK510 | M_CK610 },
  { "xtrb1", X1, 0, 0x0120, M_CK510 | M_CK610 },
  { "xtrb0", X1, 0, 0x0130, M_CK510 | M_CK610 },
  { "zextb", O1_E, 0, 0x0140, M_CK510 | M_CK610 },
  { "sextb", O1_E, 0, 0x0150, M_CK510 | M_CK610 },
  { "zexth", O1_E, 0, 0x0160, M_CK510 | M_CK610 },
  { "sexth", O1_E, 0, 0x0170, M_CK510 | M_CK610 },
  { "declt", O1_E2, 0, 0x0180, M_CK510 | M_CK610 },
  { "tstnbz",O1, 0, 0x0190, M_CK510 | M_CK610 },
  { "decgt", O1_E2, 0, 0x01A0, M_CK510 | M_CK610 },
  { "decne", O1_E2, 0, 0x01B0, M_CK510 | M_CK610 },
  { "clrt",  O1, 0, 0x01C0, M_CK510 | M_CK610 },
  { "clrf",  O1, 0, 0x01D0, M_CK510 | M_CK610 },
  { "abs",   O1_E, 0, 0x01E0, M_CK510 | M_CK610 },
  { "not",   O1_E, 0, 0x01F0, M_CK510 | M_CK610 },
  { "movt",  O2, 0, 0x0200, M_CK510 | M_CK610 },
  { "mult",  O2_E, 0, 0x0300, M_CK510 | M_CK610 },
  { "mac",   O2, 0, 0x0400, M_MAC},
  { "subu",  O2_E, 0, 0x0500, M_CK510 | M_CK610 },
  { "sub",   O2_E, 0, 0x0500, M_CK510 | M_CK610 }, /* Official alias.  */
  { "addc",  O2_E, 0, 0x0600, M_CK510 | M_CK610 },
  { "subc",  O2_E, 0, 0x0700, M_CK510 | M_CK610 },
  /*
   * SPACE: 0x0800-0x09ff for a diadic operation,
   *   Used now by "xnor/xadd" and "cprgr" overlapped,
   *   but they are not existed in same time.
   */
  /* CK610 Coprocessor Instruction*/
  { "cprgr", O2_CPGR, 0, 0x0800, M_CK610 | M_CP },

  { "movf",  O2, 0, 0x0A00, M_CK510 | M_CK610 },
  { "lsr",   O2_E, 0, 0x0B00, M_CK510 | M_CK610 },
  { "cmphs", O2, 0, 0x0C00, M_CK510 | M_CK610 },
  { "cmplt", O2, 0, 0x0D00, M_CK510 | M_CK610 },
  { "tst",   O2, 0, 0x0E00, M_CK510 | M_CK610 },
  { "cmpne", O2, 0, 0x0F00, M_CK510 | M_CK610 },
  /*
   *  We must search psrclr or psrset before mfcr,
   *  becase psrclr or psrset is a subcollection.
   *  Or we will get mfcr instruction when disassemble
   *  psrclr or psrset.
   *  Modified by Li Chunqiang (chunqiang_li@c-sky.com)
   */
  { "psrclr", OPSR, 0, 0x11F0, M_CK510 | M_CK610 },
  { "psrset", OPSR, 0, 0x11F8, M_CK510 | M_CK610 },
  { "mfcr",   OC,   0, 0x1000, M_CK510 | M_CK610 },

  { "mov",    O2,   0, 0x1200, M_CK510 | M_CK610 },
  { "bgenr",  O2,   0, 0x1300, M_CK510 | M_CK610 },
  { "rsub",   O2_E,   0, 0x1400, M_CK510 | M_CK610 },
  { "ixw",    O2_E,   0, 0x1500, M_CK510 | M_CK610 },
  { "and",    O2_E,   0, 0x1600, M_CK510 | M_CK610 },
  { "xor",    O2_E,   0, 0x1700, M_CK510 | M_CK610 },
  { "mtcr",   OC,   0, 0x1800, M_CK510 | M_CK610 },
  { "asr",    O2_E,   0, 0x1A00, M_CK510 | M_CK610 },
  { "lsl",    O2_E,   0, 0x1B00, M_CK510 | M_CK610 },
  { "addu",   O2_E,   0, 0x1C00, M_CK510 | M_CK610 },
  { "add",    O2,   0, 0x1C00, M_CK510 | M_CK610 }, /* Official alias.  */
#define	ckcore_INST_ADDU	0x1C00
  { "ixh",    O2_E,   0, 0x1D00, M_CK510 | M_CK610 },
  { "or",     O2_E,   0, 0x1E00, M_CK510 | M_CK610 },
  { "andn",   O2_E,   0, 0x1F00, M_CK510 | M_CK610 },
  { "addi",   OI_E,   0, 0x2000, M_CK510 | M_CK610 },
#define	ckcore_INST_ADDI	0x2000
  { "cmplti", OI,   0, 0x2200, M_CK510 | M_CK610 },
  { "subi",   OI_E,   0, 0x2400, M_CK510 | M_CK610 }, /* 0x2400 ~ 0x25ff */
#define	ckcore_INST_SUBI	0x2400
  /*
   * SPACE: 0x2600-0x27ff,
   *   open for a register+immediate  operation,
   *   Used now by "perm/rxor" and "cpwgr" are overlapped,
   *   but they are not existed in the same time.
   */
  /* CK610 Coprocessor instructions */
  { "cpwgr",  O2_CPGR, 0, 0x2600, M_CK610 | M_CP },

  { "rsubi",  OB_E,   0, 0x2800, M_CK510 | M_CK610 },
  { "cmpnei", OB,   0, 0x2A00, M_CK510 | M_CK610 },
  { "bmaski", OMa,  0, 0x2C00, M_CK510 | M_CK610 },
  { "divu",   O1R1_E, 0, 0x2C10, M_CK510 | M_CK610 },
 
  /*
   * SPACE: 0x2C20-0x2C3f
   *   Used  by DSP, but they are not existed in
   *   the same time.
   */
  /* DSP instructions */
  { "mflos",  O1,   0, 0x2C20, M_DSP | M_MAC },
  { "mfhis",  O1,   0, 0x2C30, M_DSP | M_MAC },

  { "mtlo",   O1,   0, 0x2C40, M_DSP | M_MAC },
  { "mthi",   O1,   0, 0x2C50, M_DSP | M_MAC },
  { "mflo",   O1,   0, 0x2C60, M_DSP | M_MAC },
  { "mfhi",   O1,   0, 0x2C70, M_DSP | M_MAC },

  { "bmaski", OMb,  0, 0x2C80, M_CK510 | M_CK610 },
  { "bmaski", OMc,  0, 0x2D00, M_CK510 | M_CK610 },
  { "andi",   OB_E,   0, 0x2E00, M_CK510 | M_CK610 },
  { "bclri",  OB_E,   0, 0x3000, M_CK510 | M_CK610 },

  /*
   * SPACE: 0x3200-0x320f
   *   Used  by Coprocessor, but they are not existed in
   *   the same time.
   */
  /* CK610 Coprocessor instructions */
  { "cpwir",  O1,   0, 0x3200, M_CK610 | M_CP },

  { "divs",   O1R1_E, 0, 0x3210, M_CK510 | M_CK610 },
  /*
   * SPACE: 0x3200-0x320f
   *   Used  by Coprocessor, but they are not existed in
   *   the same time.
   */
  /* SPACE:           0x3260 - 0x326f */  
  /* CK610 Coprocessor instructions */
  { "cprsr",  O1,   0, 0x3220, M_CK610 | M_CP },
  { "cpwsr",  O1,   0, 0x3230, M_CK610 | M_CP },
  /* SPACE:            0x3240 - 0x326f */  

  { "bgeni",  OBRa, 0, 0x3270, M_CK510 | M_CK610 },
  { "bgeni",  OBRb, 0, 0x3280, M_CK510 | M_CK610 },
  { "bgeni",  OBRc, 0, 0x3300, M_CK510 | M_CK610 },
  { "bseti",  OB_E,   0, 0x3400, M_CK510 | M_CK610 },
  { "btsti",  OB,   0, 0x3600, M_CK510 | M_CK610 },
  { "xsr",    O1_E2,   0, 0x3800, M_CK510 | M_CK610 },
  { "rotli",  SIa_E,  0, 0x3800, M_CK510 | M_CK610 },
  { "asrc",   O1_E2,   0, 0x3A00, M_CK510 | M_CK610 },
  { "asri",   SIa_E,  0, 0x3A00, M_CK510 | M_CK610 },
  { "lslc",   O1_E2,   0, 0x3C00, M_CK510 | M_CK610 },
  { "lsli",   SIa_E,  0, 0x3C00, M_CK510 | M_CK610 },
  { "lsrc",   O1_E2,   0, 0x3E00, M_CK510 | M_CK610 },
  { "lsri",   SIa_E,  0, 0x3E00, M_CK510 | M_CK610 },

  { "omflip0",O2,   0, 0x4000, M_MAC },
  { "omflip1",O2,   0, 0x4100, M_MAC },
  { "omflip2",O2,   0, 0x4200, M_MAC },
  { "omflip3",O2,   0, 0x4300, M_MAC },
  { "muls",    O2, 0, 0x5000, M_DSP },
  { "mulsa",   O2, 0, 0x5100, M_DSP },
  { "mulss",   O2, 0, 0x5200, M_DSP },
  /* SPACE:           0x5300 - 0x53FF */
  { "mulu",    O2, 0, 0x5400, M_DSP },
  { "mulua",   O2, 0, 0x5500, M_DSP },
  { "mulus",   O2, 0, 0x5600, M_DSP },
  /* SPACE:           0x5700 - 0x57FF */
  { "vmulsh",  O2, 0, 0x5800, M_DSP },
  { "vmulsha", O2, 0, 0x5900, M_DSP },
  { "vmulshs", O2, 0, 0x5A00, M_DSP },
   /* SPACE:          0x5B00 - 0x5BFF */
  { "vmulsw",  O2, 0, 0x5C00, M_DSP },
  { "vmulswa", O2, 0, 0x5D00, M_DSP },
  { "vmulsws", O2, 0, 0x5E00, M_DSP },
  /* SPACE:           0x5F00 - 0x5FFF */
  /* SPACE:           0x4000 - 0x5fff */
  { "movi",    I7, 0, 0x6000, M_CK510 | M_CK610 },
#define ckcore_INST_BMASKI_ALT	0x6000
#define ckcore_INST_BGENI_ALT	0x6000
  { "mulsh",   O2_E, 0, 0x6800, M_CK510 | M_CK610 },
  { "muls.h",  O2_E, 0, 0x6800, M_CK510 | M_CK610 },

  /*
   * SPACE: 0x6900-0x6fff
   *   Used  by DSP/Coprocessor, but they are not
   *   existed in the same time.
   */
  /* DSP/Coprocessor Instructions */
  { "mulsha",  O2,     0, 0x6900, M_DSP },
  { "mulshs",  O2,     0, 0x6A00, M_DSP },
  { "cprcr",   O2_CPCR,0, 0x6B00, M_CK610 | M_CP },
  { "mulsw",   O2,     0, 0x6C00, M_DSP },
  { "mulswa",  O2,     0, 0x6D00, M_DSP },
  { "mulsws",  O2,     0, 0x6E00, M_DSP },
  { "cpwcr",   O2_CPCR,0, 0x6F00, M_CK610 | M_CP },

  /*
   *  We must search jsri/jumpi before lrw,
   *  becase jsri/jumpi is a subcollection.
   *  Or we will get lrw instruction when disassemble
   *  jsri/jumpi.
   */
  { "jmpi", LJ, 1, 0x7000, M_CK510 | M_CK610 },
  { "jsri", LJS, 0, 0x7F00, M_CK510 | M_CK610 },
  { "lrw",  LR, 0, 0x7000, M_CK510 | M_CK610 },
#define	ckcore_INST_JMPI	0x7000
#define	ckcore_INST_JSRI	0x7F00
#define	ckcore_INST_LRW		0x7000
#define	ckcore_INST_LDW		0x8000
#define	ckcore_INST_STW		0x9000
  { "ld",   LS, 0, 0x8000, M_CK510 | M_CK610 },
  { "ldw",  LS, 0, 0x8000, M_CK510 | M_CK610 },
  { "ld.w", LS, 0, 0x8000, M_CK510 | M_CK610 },
  { "st",   LS, 0, 0x9000, M_CK510 | M_CK610 },
  { "stw",  LS, 0, 0x9000, M_CK510 | M_CK610 },
  { "st.w", LS, 0, 0x9000, M_CK510 | M_CK610 },
  { "ldex",   LS, 0, 0x4000, M_MP },
  { "ldwex",  LS, 0, 0x4000, M_MP },
  { "ldex.w", LS, 0, 0x4000, M_MP },
  { "stex",   LS, 0, 0x5000, M_MP },
  { "stwex",  LS, 0, 0x5000, M_MP },
  { "stex.w", LS, 0, 0x5000, M_MP },
  { "ldb",  LS, 0, 0xA000, M_CK510 | M_CK610 },
  { "ld.b", LS, 0, 0xA000, M_CK510 | M_CK610 },
  { "stb",  LS, 0, 0xB000, M_CK510 | M_CK610 },
  { "st.b", LS, 0, 0xB000, M_CK510 | M_CK610 },
  { "ldh",  LS, 0, 0xC000, M_CK510 | M_CK610 },
  { "ld.h", LS, 0, 0xC000, M_CK510 | M_CK610 },
  { "sth",  LS, 0, 0xD000, M_CK510 | M_CK610 },
  { "st.h", LS, 0, 0xD000, M_CK510 | M_CK610 },
  { "bt",   BR, 0, 0xE000, M_CK510 | M_CK610 },
  { "bf",   BR, 0, 0xE800, M_CK510 | M_CK610 },
  { "br",   BR, 1, 0xF000, M_CK510 | M_CK610 },
#define	ckcore_INST_BR	0xF000
  { "bsr",  BSR, 0, 0xF800, M_CK510 | M_CK610 },
#define	ckcore_INST_BSR	0xF800

  /* The following are relaxable branches */
  { "jbt",  JC, 0, 0xE000, M_CK510 | M_CK610 },
  { "jbf",  JC, 0, 0xE800, M_CK510 | M_CK610 },
  { "jbr",  JU, 1, 0xF000, M_CK510 | M_CK610 },
  { "jbsr", JL, 0, 0xF800, M_CK510 | M_CK610 },

  /* The following are aliases for other instructions */
  { "rts",   O0,   2, 0x00CF, M_CK510 | M_CK610 }, /* jmp r15 */
  { "rolc",  DO21, 0, 0x0600, M_CK510 | M_CK610 }, /* addc rd,rd */
  { "rotlc", DO21, 0, 0x0600, M_CK510 | M_CK610 }, /* addc rd,rd */
  { "setc",  O0,   0, 0x0C00, M_CK510 | M_CK610 }, /* cmphs r0,r0 */
  { "clrc",  O0,   0, 0x0F00, M_CK510 | M_CK610 }, /* cmpne r0,r0 */
  { "tstle", O1,   0, 0x2200, M_CK510 | M_CK610 }, /* cmplti rd,1 */
  /* cmplei rd,X -> cmplti rd,X+1 */
  { "cmplei",OB,   0, 0x2200, M_CK510 | M_CK610 }, 
  { "neg",   O1,   0, 0x2800, M_CK510 | M_CK610 }, /* rsubi rd,0 */
  { "tstne", O1,   0, 0x2A00, M_CK510 | M_CK610 }, /* cmpnei rd,0 */
  { "tstlt", O1,   0, 0x37F0, M_CK510 | M_CK610 }, /* btsti rx,31 */
  { "mclri", OB2,  0, 0x3000, M_CK510 | M_CK610 }, /* bclri rx,log2(imm) */
  { "mgeni", OBR2, 0, 0x3200, M_CK510 | M_CK610 }, /* bgeni rx,log2(imm) */
  { "mseti", OB2,  0, 0x3400, M_CK510 | M_CK610 }, /* bseti rx,log2(imm) */
  { "mtsti", OB2,  0, 0x3600, M_CK510 | M_CK610 }, /* btsti rx,log2(imm) */
  { "rori",  RSI,  0, 0x3800, M_CK510 | M_CK610 },
  { "rotri", RSI,  0, 0x3800, M_CK510 | M_CK610 },
  { "nop",   O0,   0, 0x1200, M_CK510 | M_CK610 }, /* mov r0, r0 */
  { 0,       0,    0, 0,      0 }
};
#endif

#endif // __CSKY_OPC_H__

