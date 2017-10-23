/* Disassemble Motorola M*Core instructions.
   Copyright 1993, 1999, 2000, 2001, 2002, 2005, 2007
   Free Software Foundation, Inc.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include <stdio.h>
#define STATIC_TABLE
#define DEFINE_TABLE

#include "dis-asm.h"
#include "elf/csky.h"


typedef enum {
	MENDP, /*end of parsing operands*/
	MREGP, /*parsing operand as register, two values after 'MREGP': operand-maskp, rightshift*/
	MIMMP, /*parsing operand as oimm, three values after 'MIMMP': operand-maskp, rightshift, offset */
	MSYMP, /* convert offset to label name */
	MSYMPI, /* convert pool offset to lable name*/
	MIMMP0, /* parsing operand as imm, two values after 'MIMMP0': operand-maskp, rightshift*/
	MADDP, /* calculate as hex */
	MADDDP, /*calculate as decimal*/
	MSUBDP, /* calculate as decimal */
	MSPEP,   /*output a single character*/
	MERRP,  /*error*/
	MSPEP1, /* suppress nearby comma */
	MPSRP,  /*specially for instruction psrclr,psrset*/
	MLOG2P /* return clog2(x) */
} PARSE_METHOD_P;

typedef struct {
	CSKY_INST_TYPE maskp;
	CSKY_INST_TYPE opcodep; 
	char* name;      /*instruction name*/
	int *data;    /*point to array for parsing operands*/ 
} INST_PARSE_INFO_P;

void number_to_chars_bigendian (char *buf, CSKY_INST_TYPE val, int n);
void number_to_chars_littleendian (char *buf, CSKY_INST_TYPE val, int n);

int CBKPTP[] = {MENDP };
int CBRP[] = {
	MSYMP, 0x3FFFFFF, -1, 0x4000000, 0xFC000000,
	MENDP };
int CSCEP[] = {
	MIMMP, 0x01E00000, 21, 0,
	MENDP };
int CTRAPP[] = {
	MIMMP0, 0xC00, 10,
	MENDP };
int CPSRCLRP[] = {
	MPSRP, 0x03E00000, 21,
	MENDP };
int CCLRFP[] = {
	MREGP, 0x03E00000, 21,
	MENDP };
int CMFHIP[] = {
	MREGP, 0x1F, 0,
	MENDP };
int CMTHIP[] = {
	MREGP, 0x1F0000, 16,
	MENDP };
int CJMPIH[] = {
	MSYMPI, 0xFFFF, -2,
	MENDP };
int CBTP[] = {
	MSYMP, 0xFFFF, -1, 0x10000, 0xFFFF0000, 
	MENDP };
int CCPOPP[] = {MERRP };
int CCPRCP[] = {MERRP };
int CCPRCPR[] = {MERRP };
int CLDCPRP[] = {MERRP };
int CBEPZP[] = {
	MREGP, 0x1F0000, 16, 
	MSYMP, 0xFFFF, -1, 0x10000, 0xFFFF0000,
	MENDP };
int CCMPNEPIP[] = {
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0xFFFF, 0,
	MENDP };
int CCMPHSIP[] = {
	MREGP, 0x1F0000, 16, 
	MIMMP, 0xFFFF, 0, 1,
	MENDP };
int CPLDRP[] = {
	MSPEP1, '(', 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0xFFF, -2,
	MSPEP, ')',
	MENDP };
int CBGENRP[] = {
	MREGP, 0x1F, 0, 
	MREGP, 0x1F0000, 16,
	MENDP };
int CCMPNEP[] = {
	MREGP, 0x1F0000, 16, 
	MREGP, 0x03E00000, 21,
	MENDP };
int CCMPLSP[] = {MERRP };
int CBTPSTIP[] = {
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0x03E00000, 21,
	MENDP };
int CBMASKIP[] = {
	MREGP, 0x1F, 0, 
	MIMMP, 0x03E00000, 21, 1,
	MENDP };
int CADDIP[] = {
	MREGP, 0x03E00000, 21, 
	MREGP, 0x1F0000, 16, 
	MIMMP, 0xFFFF, 0, 1,
	MENDP };
int CANDIP[] = {
	MREGP, 0x03E00000, 21, 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0xFFFF, 0,
	MENDP };
int CLDBP[] = {
	MREGP, 0x03E00000, 21, 
	MSPEP1, '(', 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0xFFF, 0,
	MSPEP, ')',
	MENDP };
int CLDHP[] = {
	MREGP, 0x03E00000, 21, 
	MSPEP1, '(', 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0xFFF, -1,
	MSPEP, ')',
	MENDP };
int CLDWP[] = {
	MREGP, 0x03E00000, 21, 
	MSPEP1, '(', 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0xFFF, -2,
	MSPEP, ')',
	MENDP };
int CLDDP[] = {
	MREGP, 0x03E00000, 21, 
	MSPEP1, '(', 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0xFFF, -3,
	MSPEP, ')',
	MENDP };
int CADDUP[] = {
	MREGP, 0x01F, 0,
	MREGP, 0x1F0000, 16, 
	MREGP, 0x03E00000, 21, 
	MENDP };
int CNOTP[] = {MERRP };
int CRSUBP[] = {MERRP };
int CBEP[] = {
	MREGP, 0x1F0000, 16, 
	MREGP, 0x03E00000, 21, 
	MSYMP, 0xFFFF, -1, 0x10000, 0xFFFF0000,
	MENDP };
int CINCFP[] = {
	MREGP, 0x03E00000, 21, 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0x1F, 0,
	MENDP };
int CDECGTP[] = {
	MREGP, 0x1F, 0, 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0x03E00000, 21,
	MENDP };
int CSRCP[] = {
	MREGP, 0x1F, 0, 
	MREGP, 0x1F0000, 16, 
	MIMMP, 0x03E00000, 21, 1,
	MENDP };
int CINSP[] = {
	MREGP, 0x03E00000, 21, 
	MREGP, 0x1F0000, 16, 
	MADDP, 0x3E0, 5, 0x1F, 0, 
	MIMMP0, 0x1F, 0,
	MENDP };
int CSEXTP[] = {
	MREGP, 0x1F, 0, 
	MREGP, 0x1F0000, 16, 
	MIMMP0, 0x3E0, 5, 
	MIMMP0, 0x03E00000, 21,
	MENDP };
int CLDMP[] = {
	MREGP, 0x03E00000, 21, 
	MSPEP, '-', 
	MSPEP, 'r', 
	MADDDP, 0x03E00000, 21, 0X1F, 0, 
	MSPEP1, '(', 
	MREGP, 0x1F0000, 16, 
	MSPEP, ')',
	MENDP };
int CLDRHP[] = {
	MREGP, 0x1F, 0,
	MSPEP1, '(',
	MREGP, 0x001F0000, 16,
	MREGP, 0x03E00000, 21,
	MSPEP, '<',
	MSPEP, '<',
	MLOG2P, 0x1E0, 5,
	MSPEP, ')',
	MERRP };
int CMFCRP[] = {
	MREGP, 0x1F, 0, 
	MSPEP1, 'c', 
	MSPEP, 'r', 
	MSPEP, '<', 
	MADDDP, 0x1F0000, 16, 0, 0,
	MIMMP0, 0x03E00000, 21,
	MSPEP, '>',
	MENDP };
int CMTCRP[] = {
	MREGP, 0x1F0000, 16, 
	MSPEP1, 'c', 
	MSPEP, 'r', 
	MSPEP, '<', 
	MADDDP, 0x1F, 0, 0, 0,
	MIMMP0, 0x03E00000, 21,
	MSPEP, '>',
	MENDP };
int CLRWP[] = {
	MREGP, 0x1F0000, 16,
	MSYMPI, 0xFFFF, -2,
	MENDP };
int HLRWP[] = {
	MREGP, 0x780, 7,
	MSYMPI, 0x7F, -2,
	MENDP };
int HBRP[] = {
	MSYMP, 0x3FF, -1, 0x400, 0xFFFFFC00, 
	MENDP };
int HJSRIP[] = {
	MSYMPI, 0x3FF, -2,
	MENDP};
int HADDIP[] = {
	MREGP, 0x03C0, 6, 
	MIMMP, 0x3E, 1, 1,
	MENDP };
int HADDIP9SP[] = {
	MSPEP1, 'r', 
	MSPEP, '0', 
	MSPEP1, ',', 
	MSPEP1, ' ', 
    MIMMP0, 0x01ff, -2, 
    MENDP };
int HLSLIP[] = {
	MREGP, 0x03C0, 6, 
	MIMMP0, 0x3E, 1,
	MENDP };
int HLDBP[] = {
	MREGP, 0x03C0, 6, 
	MSPEP1, '(', 
	MREGP, 0x3C, 2, 
	MIMMP0, 0x3, 0, 
	MSPEP, ')',
	MENDP };
int HLDHP[] = {
	MREGP, 0x03C0, 6, 
	MSPEP1, '(', 
	MREGP, 0x3C, 2, 
	MIMMP0, 0x3, -1, 
	MSPEP, ')',
	MENDP };
int HLDWP[] = {
	MREGP, 0x03C0, 6, 
	MSPEP1, '(', 
	MREGP, 0x3C, 2, 
	MIMMP0, 0x3, -2, 
	MSPEP, ')',
	MENDP };
int HLDMP[] = {
	MREGP, 0x03C0, 6, 
	MSPEP, '-', 
	MSPEP, 'r', 
	MADDDP, 0x03C0, 6, 0x3, 0, 
	MSPEP1, '(', 
	MREGP, 0x3C, 2,
	MSPEP, ')',
	MENDP };
int HADDUP[] = {
	MREGP, 0x3C0, 6, 
	MREGP, 0x3C, 2,
	MENDP };
int HCMPHSP[] = {
	MREGP, 0x3c, 2,
	MREGP, 0x3c0, 6,
	MENDP };
int HMOVP[] = {
	MSPEP, 'r', 
	MADDDP, 0x3C0, 6, 0X2, -3, 
	MSPEP1, 'r', 
	MADDDP, 0x3C, 2, 0X1, -4,
	MENDP };
int HTSTNBZP[] = {
	MREGP, 0x3c, 2, 
	MENDP };
int HMVCH[] = {
	MREGP, 0x03C0, 6,
	MENDP };
int HRTSP[] = {
	MENDP };
int PSEUDOP[] = {MERRP };

INST_PARSE_INFO_P* csky_find_inst_info_p(CSKY_INST_TYPE inst,int length);
void strcat_int(char *, int, int);
inline int clog2(int s);

INST_PARSE_INFO_P csky_inst_info_32_p[] = {
	{ 0xffffffff, 0x80000000, "bkpt", CBKPTP },
	{ 0xffffffff, 0xc0005020, "doze", CBKPTP },
	{ 0xffffffff, 0xc0004420, "rfi", CBKPTP },
	{ 0xffffffff, 0xc0004020, "rte", CBKPTP },
	{ 0xffffffff, 0xc0005820, "se", CBKPTP },
	{ 0xffffffff, 0xc0004820, "stop", CBKPTP },
	{ 0xffffffff, 0xc0000420, "sync", CBKPTP },
	{ 0xffffffff, 0xc0004c20, "wait", CBKPTP },
	{ 0xffffffff, 0xc0005420, "we", CBKPTP },
	{ 0xffffffff, 0xc4009a00, "mvtc", CBKPTP },
	{ 0xffffffff, 0xc0001c20, "idly",CBKPTP},
	{ 0xfc000000, 0x84000000, "br", CBRP },
	{ 0xfc000000, 0x80000000, "bsr", CBRP },
	{ 0xfe1fffff, 0xc0001820, "sce",CSCEP},
	{ 0xfffff3ff, 0xc0002020, "trap", CTRAPP },
	{ 0xfc1fffff, 0xc0007020, "psrclr", CPSRCLRP },
	{ 0xfc1fffff, 0xc0007420, "psrset", CPSRCLRP },
	{ 0xfc1fffff, 0xc4002c20, "clrf", CCLRFP },
	{ 0xfc1fffff, 0xc4002c40, "clrt", CCLRFP },
	{ 0xffffffe0, 0xc4009c20, "mfhi", CMFHIP },
	{ 0xffffffe0, 0xc4009820, "mfhis", CMFHIP },
	{ 0xffffffe0, 0xc4009880, "mflos", CMFHIP },
	{ 0xffffffe0, 0xc4009c80, "mflo", CMFHIP },
	{ 0xffffffe0, 0xc4000500, "mvc", CMFHIP },
	{ 0xffffffe0, 0xc4000600, "mvcv", CMFHIP },
	{ 0xffe0ffff, 0xc4009c40, "mthi", CMTHIP },
	{ 0xffe0ffff, 0xc4009d00, "mtlo", CMTHIP },
	{ 0xffe0ffff, 0xc4002100, "tstnbz", CMTHIP },
	{ 0xffe0ffff, 0x93800000, "jmp", CMTHIP },
	{ 0xffe0ffff, 0x93a00000, "jsr", CMTHIP },
	{ 0xffff0000, 0x93000000, "jmpi", CJMPIH },
	{ 0xffff0000, 0x93200000, "jsri", CJMPIH },
	{ 0xffff0000, 0x92200000, "bt", CBTP },
	{ 0xffff0000, 0x92000000, "bf", CBTP },
	{ 0xfc000000, 0xe0000000, "cpop", CCPOPP },
	{ 0xffff0000, 0xe6000000, "cprc", CCPRCP },
	{ 0xffe00000, 0xe4400000, "cprcr", CCPRCPR },
	{ 0xffe00000, 0xe4000000, "cprgr", CCPRCPR },
	{ 0xffe00000, 0xe4600000, "cpwcr", CCPRCPR },
	{ 0xffe00000, 0xe4200000, "cpwgr", CCPRCPR },
	{ 0xffe00000, 0xe8000000, "ldcpr", CLDCPRP },
	{ 0xffe00000, 0xe8008000, "stcpr", CLDCPRP },
	{ 0xffe00000, 0x90000000, "bez", CBEPZP },
	{ 0xffe00000, 0x90200000, "bnez", CBEPZP },
	{ 0xffe00000, 0x90400000, "bhz", CBEPZP },
	{ 0xffe00000, 0x90600000, "blsz", CBEPZP },
	{ 0xffe00000, 0x90800000, "blz", CBEPZP },
	{ 0xffe00000, 0x90a00000, "bhsz", CBEPZP },
	{ 0xffe00000, 0x9e400000, "cmpnei", CCMPNEPIP },
	{ 0xffe00000, 0x9c000000, "movi", CCMPNEPIP },
	{ 0xffe00000, 0x9c200000, "movih", CCMPNEPIP },
	{ 0xffe00000, 0x9e000000, "cmphsi", CCMPHSIP },
	{ 0xffe00000, 0x9e200000, "cmplti", CCMPHSIP },
	{ 0xffe0f000, 0xd8006000, "pldr", CPLDRP },
	{ 0xffe0f000, 0xdc006000, "pldw", CPLDRP },
	{ 0xffe0ffe0, 0xc4005040, "bgenr", CBGENRP },
	{ 0xffe0ffe0, 0xc4007020, "xtrb0", CBGENRP },
	{ 0xffe0ffe0, 0xc4007040, "xtrb1", CBGENRP },
	{ 0xffe0ffe0, 0xc4007080, "xtrb2", CBGENRP },
	{ 0xffe0ffe0, 0xc4007100, "xtrb3", CBGENRP },
	{ 0xffe0ffe0, 0xc4006200, "brev", CBGENRP },
	{ 0xffe0ffe0, 0xc4006080, "revb", CBGENRP },
	{ 0xffe0ffe0, 0xc4006100, "revh", CBGENRP },
	{ 0xffe0ffe0, 0xc4000200, "abs", CBGENRP },
	{ 0xffe0ffe0, 0xc4007c40, "ff1", CBGENRP },
	{ 0xffe0ffe0, 0xc4007c20, "ff0", CBGENRP },
	{ 0xffe0ffe0, 0xc40058e0, "sextb", CBGENRP },
	{ 0xffe0ffe0, 0xc40059e0, "sexth", CBGENRP },
	{ 0xffe0ffe0, 0xc40054e0, "zextb", CBGENRP },
	{ 0xffe0ffe0, 0xc40055e0, "zexth", CBGENRP },
	{ 0xfc00ffff, 0xc4000480, "cmpne", CCMPNEP },
	{ 0xfc00ffff, 0xc4000420, "cmphs", CCMPNEP },
	{ 0xfc00ffff, 0xc4000440, "cmplt", CCMPNEP },
	{ 0xfc00ffff, 0xc4002080, "tst", CCMPNEP },
	{ 0xfc00ffff, 0xc4008820, "mulu", CCMPNEP },
	{ 0xfc00ffff, 0xc4008840, "mulua", CCMPNEP },
	{ 0xfc00ffff, 0xc4008880, "mulus", CCMPNEP },
	{ 0xfc00ffff, 0xc4008c20, "muls", CCMPNEP },
	{ 0xfc00ffff, 0xc4009040, "mulsha", CCMPNEP },
	{ 0xfc00ffff, 0xc4009080, "mulshs", CCMPNEP },
	{ 0xfc00ffff, 0xc4009440, "mulswa", CCMPNEP },
	{ 0xfc00ffff, 0xc4009480, "mulsws", CCMPNEP },
	{ 0xfc00ffff, 0xc400b020, "vmulsh", CCMPNEP },
	{ 0xfc00ffff, 0xc400b040, "vmulsha", CCMPNEP },
	{ 0xfc00ffff, 0xc400b080, "vmulshs", CCMPNEP },
	{ 0xfc00ffff, 0xc400b420, "vmulsw", CCMPNEP },
	{ 0xfc00ffff, 0xc400b440, "vmulswa", CCMPNEP },
	{ 0xfc00ffff, 0xc400b480, "vmulsws", CCMPNEP },
	{ 0xfc00ffff, 0xc4008c40, "mulsa", CCMPNEP },
	{ 0xfc00ffff, 0xc4008c80, "mulss", CCMPNEP },
	{ 0xffe0ffe0, 0xc4000420, "cmpls", CCMPLSP },
	{ 0xffe0ffe0, 0xc4000440, "cmpgt", CCMPLSP },
	{ 0xfc00ffff, 0xc4002880, "btsti", CBTPSTIP },
	{ 0xfc1fffe0, 0xc4005020, "bmaski", CBMASKIP },
	{ 0xfc000000, 0xa0000000, "addi", CADDIP },
	{ 0xfc000000, 0xa4000000, "subi", CADDIP },
	{ 0xfc000000, 0xa8000000, "andi", CANDIP },
	{ 0xfc000000, 0xac000000, "andni", CANDIP },
	{ 0xfc000000, 0xb4000000, "xori", CANDIP },
	{ 0xfc000000, 0xb0000000, "ori", CANDIP },
	{ 0xfc00f000, 0xd8000000, "ld.b", CLDBP },
	{ 0xfc00f000, 0xdc000000, "st.b", CLDBP },
	{ 0xfc00f000, 0xd8004000, "ld.bs", CLDBP },
	{ 0xfc00f000, 0xd8001000, "ld.h", CLDHP },
	{ 0xfc00f000, 0xd8005000, "ld.hs", CLDHP },
	{ 0xfc00f000, 0xdc001000, "st.h", CLDHP },
	{ 0xfc00f000, 0xd8002000, "ld.w", CLDWP },
	{ 0xfc00f000, 0xd8003000, "ld.d", CLDDP },
	{ 0xfc00f000, 0xd8007000, "ldex.w", CLDWP },
	{ 0xfc00f000, 0xdc002000, "st.w", CLDWP },
	{ 0xfc00f000, 0xdc003000, "st.d", CLDDP },
	{ 0xfc00f000, 0xdc007000, "stex.w", CLDWP },
	{ 0xfc00ffe0, 0xc4000020, "addu", CADDUP },
	{ 0xfc00ffe0, 0xc4000040, "addc", CADDUP },
	{ 0xfc00ffe0, 0xc4000080, "subu", CADDUP },
	{ 0xfc00ffe0, 0xc4000100, "subc", CADDUP },
	{ 0xfc00ffe0, 0xc4000820, "ixh", CADDUP },
	{ 0xfc00ffe0, 0xc4000840, "ixw", CADDUP },
	{ 0xfc00ffe0, 0xc4000880, "ixd", CADDUP },
	{ 0xfc00ffe0, 0xc4002020, "and", CADDUP },
	{ 0xfc00ffe0, 0xc4002040, "andn", CADDUP },
	{ 0xfc00ffe0, 0xc4002420, "or", CADDUP },
	{ 0xfc00ffe0, 0xc4002440, "xor", CADDUP },
	{ 0xfc00ffe0, 0xc4002480, "nor", CADDUP },
	{ 0xfc00ffe0, 0xc4004020, "lsl", CADDUP },
	{ 0xfc00ffe0, 0xc4004040, "lsr", CADDUP },
	{ 0xfc00ffe0, 0xc4004080, "asr", CADDUP },
	{ 0xfc00ffe0, 0xc4004100, "rotl", CADDUP },
	{ 0xfc00ffe0, 0xc4008020, "divu", CADDUP },
	{ 0xfc00ffe0, 0xc4008040, "divs", CADDUP },
	{ 0xfc00ffe0, 0xc4008420, "mult", CADDUP },
	{ 0xfc00ffe0, 0xc4009420, "mulsw", CADDUP },
	{ 0xfc00ffe0, 0xc4009020, "mulsh", CADDUP },
	{ 0xfc00ffe0, 0xc4002480, "not", CNOTP },
	{ 0xfc00ffe0, 0xc4000080, "rsub", CRSUBP },
	{ 0xfc000000, 0x88000000, "be", CBEP },
	{ 0xfc000000, 0x8c000000, "bne", CBEP },
	{ 0xfc00ffe0, 0xc4000c20, "incf", CINCFP },
	{ 0xfc00ffe0, 0xc4000c40, "inct", CINCFP },
	{ 0xfc00ffe0, 0xc4000c80, "decf", CINCFP },
	{ 0xfc00ffe0, 0xc4000d00, "dect", CINCFP },
	{ 0xfc00ffe0, 0xc4001020, "decgt", CDECGTP },
	{ 0xfc00ffe0, 0xc4001040, "declt", CDECGTP },
	{ 0xfc00ffe0, 0xc4001080, "decne", CDECGTP },
	{ 0xfc00ffe0, 0xc4004820, "lsli",  CDECGTP },
	{ 0xfc00ffe0, 0xc4004840, "lsri",  CDECGTP },
	{ 0xfc00ffe0, 0xc4004880, "asri",  CDECGTP },
	{ 0xfc00ffe0, 0xc4004900, "rotli", CDECGTP },
	{ 0xfc00ffe0, 0xc4002820, "bclri", CDECGTP },
	{ 0xfc00ffe0, 0xc4002840, "bseti", CDECGTP },
	{ 0xfc00ffe0, 0xc4004c80, "asrc", CSRCP },
	{ 0xfc00ffe0, 0xc4004d00, "xsr", CSRCP },
	{ 0xfc00ffe0, 0xc4004c20, "lslc", CSRCP },
	{ 0xfc00ffe0, 0xc4004c40, "lsrc", CSRCP },
	{ 0xfc00fc00, 0xc4005c00, "ins", CINSP },
	{ 0xfc00fc00, 0xc4005800, "sext", CSEXTP },
	{ 0xfc00fc00, 0xc4005400, "zext", CSEXTP },
	{ 0xfc00ffe0, 0xd0001c20, "ldm", CLDMP },
	{ 0xfc00ffe0, 0xd4001c20, "stm", CLDMP },
	{ 0xfc00fe00, 0xd0000400, "ldr.h", CLDRHP },
	{ 0xfc00fe00, 0xd0000800, "ldr.w", CLDRHP },
	{ 0xfc00fe00, 0xd0001000, "ldr.bs", CLDRHP },
	{ 0xfc00fe00, 0xd0001400, "ldr.hs", CLDRHP },
	{ 0xfc00fe00, 0xd4000000, "str.b", CLDRHP },
	{ 0xfc00fe00, 0xd4000400, "str.h", CLDRHP },
	{ 0xfc00fe00, 0xd4000800, "str.w", CLDRHP },
	{ 0xfc00fe00, 0xd0000000, "ldr.b", CLDRHP },
	{ 0xfc00ffe0, 0xc0006020, "mfcr", CMFCRP },
	{ 0xfc00ffe0, 0xc0006420, "mtcr", CMTCRP },
	{ 0xffe00000, 0x9d000000, "lrw", CLRWP},
	{ 0xffe00000, 0xe4000000, "cprgr", PSEUDOP },
	{ 0xffe00000, 0xe4400000, "cprgr", PSEUDOP },
	{ 0xffe00000, 0xe4200000, "cpwcr", PSEUDOP },
	{ 0xffe00000, 0xe4600000, "cpwgr", PSEUDOP },
	{ 0, 0, 0, 0}
};

INST_PARSE_INFO_P csky_inst_info_16_p[] = {
	{ 0xffff, 0x783c, "rts", HRTSP},
	{ 0xffff, 0x0000, "bkpt", HRTSP},
	{ 0xfc00, 0x0400, "br", HBRP },
	{ 0xfc00, 0x0000, "bsr", HBRP },
	{ 0xfc00, 0x0800, "bt", HBRP },
	{ 0xfc00, 0x0c00, "bf", HBRP },
	//{ 0xfc00, 0x1000, "jsri", HJSRIP },
	{ 0xfe00, 0x1800, "addi", HADDIP9SP },
	{ 0xfc01, 0x2000, "addi", HADDIP },
	{ 0xfe00, 0x1a00, "subi", HADDIP9SP },
	{ 0xfc01, 0x2001, "subi", HADDIP },
	{ 0xfc01, 0x2400, "cmphsi", HADDIP },
	{ 0xfc01, 0x2401, "cmplti", HADDIP },
	{ 0xfc01, 0x2c00, "lsli", HLSLIP },
	{ 0xfc01, 0x2c01, "lsri", HLSLIP },
	{ 0xfc01, 0x3000, "asri", HLSLIP },
	{ 0xfc01, 0x3001, "rotli", HLSLIP },
	{ 0xfc01, 0x3400, "bclri", HLSLIP },
	{ 0xfc01, 0x3401, "bseti", HLSLIP },
	{ 0xfc01, 0x3800, "movi", HLSLIP },
	{ 0xfc01, 0x2800, "cmpnei", HLSLIP },
	{ 0xfc01, 0x2401, "cmplei", HLSLIP },
	{ 0xfc00, 0x4000, "ld.b", HLDBP },
	{ 0xfc00, 0x5000, "st.b", HLDBP },
	{ 0xfc00, 0x4400, "ld.h", HLDHP },
	{ 0xfc00, 0x5400, "st.h", HLDHP },
	{ 0xfc00, 0x4800, "ld.w", HLDWP },
	{ 0xfc00, 0x5800, "st.w", HLDWP },
	{ 0xfc00, 0x4c00, "ldm", HLDMP },
	{ 0xfc00, 0x5c00, "stm", HLDMP },
	{ 0xfc00, 0x4c00, "ldq", HLDMP },
	{ 0xfc00, 0x5c00, "stq", HLDMP },
	{ 0xfc03, 0x6000, "addu", HADDUP },
	{ 0xfc03, 0x6001, "addc", HADDUP },
	{ 0xfc03, 0x6002, "subu", HADDUP },
	{ 0xfc03, 0x6003, "subc", HADDUP },
	{ 0xfc03, 0x6800, "and", HADDUP },
	{ 0xfc03, 0x6801, "andn", HADDUP },
	{ 0xfc03, 0x6c00, "or", HADDUP },
	{ 0xfc03, 0x6c01, "xor", HADDUP },
	{ 0xfc03, 0x6c02, "nor", HADDUP },
	{ 0xfc03, 0x7000, "lsl", HADDUP },
	{ 0xfc03, 0x7001, "lsr", HADDUP },
	{ 0xfc03, 0x7002, "asr", HADDUP },
	{ 0xfc03, 0x7003, "rotl", HADDUP },
	{ 0xfc03, 0x7c00, "mult", HADDUP },
	{ 0xfc03, 0x7c01, "mulsh", HADDUP },
	{ 0xfc03, 0x7400, "zextb", HADDUP },
	{ 0xfc03, 0x7401, "zexth", HADDUP },
	{ 0xfc03, 0x7402, "sextb", HADDUP },
	{ 0xfc03, 0x7403, "sexth", HADDUP },
	{ 0xfc03, 0x7802, "revb", HADDUP },
	{ 0xfc03, 0x7803, "revh", HADDUP },
	{ 0xfc03, 0x6400, "cmphs", HCMPHSP },
	{ 0xfc03, 0x6401, "cmplt", HCMPHSP },
	{ 0xfc03, 0x6402, "cmpne", HCMPHSP },
	{ 0xfc03, 0x6802, "tst", HADDUP },
	{ 0xfc00, 0x1c00, "mov", HMOVP },
	{ 0xfc03, 0x6803, "tstnbz", HTSTNBZP },
	{ 0xfc3f, 0x6403, "mvc", HMVCH },
	{ 0xf800, 0x1000, "lrw", HLRWP},
	{ 0, 0, 0, 0}
};

/*concatenate integer to string, according to the given radix*/
/*void strcat_int(char *s, int value, int radix)
{
	char tmp[33];
	char *tp=tmp;
	int i;
	unsigned v;
	int sign;
	if(value == 0)
	{
		strcat(s,"0");
		return;
	}

	sign = (radix == 10 && value < 0);
	if (sign) v = -value;
	else v = (unsigned)value;
	while (v || tp == tmp)
	{
		i = v % radix;
		v = v / radix;
		if (i < 10) *tp++ = i+'0';
		else *tp++ = i + 'a' - 10;
	}

	if (sign)strcat(s,"-");
	s+=strlen(s);
	while (tp > tmp)*s++ = *--tp;
}*/

/*inline int clog2(int s)
{
	int r=0;
	if(s == 0) return -1;
	while(s!=1)
	{
		s>>=1;
		r++;
	}
	return r;
}*/

/*void
number_to_chars_littleendian (char *buf, CSKY_INST_TYPE val, int n)
{
	if (n <= 0)
		abort ();
	while (n--)
	{
		*buf++ = val & 0xff;
		val >>= 8;
	}
}*/

/*find instruction info according to instruction inst*/
INST_PARSE_INFO_P* csky_find_inst_info_p(CSKY_INST_TYPE inst,int length)
{
	int i;
	CSKY_INST_TYPE v;
	INST_PARSE_INFO_P *d;
	if(length==2)d=csky_inst_info_16_p;   /*16 bit instruction*/
	else d=csky_inst_info_32_p;        /*32 bit instruction*/
	for(i=0;d[i].name;i++) 
	{
		v=d[i].maskp & inst;      /*get instruction opcodep*/
		if(v == d[i].opcodep)
		{
			return &d[i];
		}
	}
	return NULL;
}



static int 
v2p_print_insn_csky(bfd_vma memaddr, struct disassemble_info *info)
{
	unsigned char buf[2];
	CSKY_INST_TYPE inst;
	CSKY_INST_TYPE inst_lbytes;
	int status;
	char str[40];
	memset(str,0,sizeof(str));
	info->bytes_per_chunk = 0;
	inst = 0;

	csky_read_data;
#define insn_is_32bit_p ((inst & 0x8000) != 0)
	if(insn_is_32bit_p)
	{
		inst <<= 16;
		csky_read_data;
		/* revert 32-bit-insn endian conflict */
		if(info->buffer && (info->endian == BFD_ENDIAN_LITTLE))
		{
			char* src =info->buffer+ 
				   (memaddr - 4 - info->buffer_vma) * info->octets_per_byte;
			if (info->endian == BFD_ENDIAN_LITTLE)
			{
				number_to_chars_littleendian (src, inst, 4);
			}
		}
	}

	INST_PARSE_INFO_P *ii;
	ii = csky_find_inst_info_p(inst, info->bytes_per_chunk);
	if(ii == NULL)
	{
		info->fprintf_func(
				info->stream,
				" .long: 0x%08x",
				inst );
		return info->bytes_per_chunk;
	}

	/* disassemble start: */

	int method;
	int c;

	int v=-999; /* primary value dump */
	int w=-999; /* secondary value dump */
	int t=-999;

	int need_comma;

	strcat(str,ii->name);
	strcat(str,"\t");

	c=0;
	need_comma=0;
#define GetData (ii->data[c++])
#define NOTENDP (method!=MENDP && method!=MERRP)
#define cout(s) strcat(str,s)
#define cprint(s) info->fprintf_func(info->stream, s)
	for(method=GetData;NOTENDP;)
	{
		if(need_comma>0)
		{
			if(method!=MSPEP)
			{
				strcat(str,", ");
			}
			need_comma=0;
		}

		/* calculate */
		switch(method)
		{   case MSPEP:
			case MSPEP1:
				break;
			default:
				v = (inst & (CSKY_INST_TYPE)GetData);
				t = GetData;
				if(t>=0) v >>= t;
				else v<<= -t;
		}
		switch(method)
		{
			case MADDP:
			case MADDDP:
			case MSUBDP:
				w = (inst & (CSKY_INST_TYPE)GetData);
				t = GetData;
				if(t>=0) w >>= t;
				else w<<= -t;
				break;
		}
		switch(method)
		{
			case MLOG2P:
				v = clog2(v);
				break;
			case MIMMP:
				v += GetData;
				break;
			case MADDP:
			case MADDDP:
				v += w; 
				break;
			case MSUBDP:
				v -= w; 
				break;
		}

		/* print */
		switch(method)
		{
			case MREGP:
				strcat(str,"r");
				strcat_int(str,v,10);
				break;
			case MSYMP:
				/*PC + sign_extend(offset << 1)*/
				if(v & (CSKY_INST_TYPE)GetData)
					v |= (CSKY_INST_TYPE)GetData;   //sign extension
				v += memaddr - info->bytes_per_chunk;
				strcat(str,"0x");
				strcat_int(str,v,16);
				info->fprintf_func(info->stream,str);
				// if(strncmp(str,"bsr",3) == 0)
				// {
				if (info->print_address_func)
				{	
					info->fprintf_func(info->stream,"\t//");
					info->print_address_func (v, info);
				}
				// }
				return info->bytes_per_chunk;
			case MSYMPI:
				/*MEM[(PC + unsign_extend(offset << 2)) & 0xFFFFFFFC]*/
				v = (memaddr - info->bytes_per_chunk + v) & 0xFFFFFFFC;
				inst=0;
				memaddr = v;
				if (info->endian == BFD_ENDIAN_BIG)                          
				{
					csky_read_data;
					inst<<=16;
					csky_read_data;
				}
				else if (info->endian == BFD_ENDIAN_LITTLE)
				{
					csky_read_data;
					inst_lbytes = inst;
					inst = 0;
					csky_read_data;
					inst <<=16;
					inst = inst | inst_lbytes; 
				}
				else abort();                 
				info->bytes_per_chunk -= 4;
				strcat(str,"0x");
				strcat_int(str,inst,16);
				info->fprintf_func(info->stream,str);
				if(info->print_address_func && inst != 0)
				{
					info->fprintf_func(info->stream,"\t//");
					info->print_address_func (inst, info);
				}
				else
				{
					info->fprintf_func(info->stream,"\t// from address pool at 0x%lx",v);
				}
				return info->bytes_per_chunk;
			case MIMMP0:
			case MIMMP:
			case MADDP:
				strcat(str,"0x");
				strcat_int(str,v,16);
				break; 
			case MPSRP:
				{
					static char *fields[] = 
					{"", "af", "fe", "fe,af", "ie", "ie,af", "ie,fe",
						"ie,fe,af", "ee", "ee,af", "ee,fe", "ee,fe,af",
						"ee,ie", "ee,ie,af", "ee,ie,fe", "ee,ie,fe,af"
					};
					strcat(str,fields[v & 0xf]);
				}
				break;
			case MADDDP:
			case MSUBDP:
			case MLOG2P:
				strcat_int(str,v,10);
				break;
			case MSPEP:
			case MSPEP1:
				str[strlen(str)]=(char)GetData;
				break;
		}
		switch(method)
		{
			case MSPEP:
			case MSPEP1:
				method=GetData;
				continue;
		}
		need_comma++;
		method=GetData;
	}
	info->fprintf_func(info->stream, str);
	return info->bytes_per_chunk;
}



