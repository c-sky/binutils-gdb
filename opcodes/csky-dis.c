/* Print i386 instructions for GDB, the GNU debugger.
   Copyright (C) 1988-2016 Free Software Foundation, Inc.

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

#include "sysdep.h"
#include "config.h"
#include <stdio.h>
#include <stdint.h>
#include "dis-asm.h"
#include "elf-bfd.h"
#include "opcode/csky.h"
#include "libiberty.h"
#include "csky-opc.h"

#define CSKY_INST_TYPE unsigned long
#define HAS_SUB_OPERAND 0xffffffff

enum sym_type
{
  CUR_TEXT,
  CUR_DATA
};
struct csky_dis_info
{
  /* Mem to disassemble.  */
  bfd_vma mem;
  /* Disassemble info.  */
  disassemble_info *info;
  /* Opcode infomations.  */
  struct _csky_opcode_info *opinfo;
  /* The value of operand to show.  */
  int value;
  /* Is need output symbol.  */
  int need_output_symbol;
} _csky_dis_info;


enum sym_type last_type;
int last_map_sym = 1;
bfd_vma last_map_addr=0;

// only for objdump tool
#define INIT_MACH_FLAG  0xffffffff
#define BINARY_MACH_FLAG 0x0

static unsigned int mach_flag = INIT_MACH_FLAG;

static void
print_insn_data (bfd_vma pc ATTRIBUTE_UNUSED,
                 struct disassemble_info *info,
                 long given)
{
  switch (info->bytes_per_chunk)
    {
    case 1:
      info->fprintf_func (info->stream, ".byte\t0x%02lx", given);
      break;
    case 2:
      info->fprintf_func (info->stream, ".short\t0x%04lx", given);
      break;
    case 4:
      info->fprintf_func (info->stream, ".long\t0x%08lx", given);
      break;
    default:
      abort ();
    }
}

static int
get_sym_code_type (struct disassemble_info *info,
                   int n,
                   enum sym_type *sym_type)
{
  const char *name;
  name = bfd_asymbol_name (info->symtab[n]);
  if (name[0] == '$' && (name[1] == 't' || name[1] == 'd' )
      && (name[2] == 0 || name[2] == '.'))
    {
      *sym_type = ((name[1] == 't') ? CUR_TEXT : CUR_DATA);
      return TRUE;
    }
  return FALSE;
}

static int
csky_get_operand_mask (struct operand *oprnd)
{
  int mask = 0;
  if (oprnd->mask == HAS_SUB_OPERAND)
    {
      struct soperand *sop = (struct soperand *)oprnd;
      mask |= csky_get_operand_mask (&sop->subs[0]);
      mask |= csky_get_operand_mask (&sop->subs[1]);
      return mask;
    }
  return oprnd->mask;
}

static int
csky_get_mask (struct _csky_opcode_info *pinfo)
{
  int i = 0;
  int mask = 0;
  /* List type.  */
  if (pinfo->operand_num == -1)
    {
      mask |= csky_get_operand_mask (&pinfo->oprnd.oprnds[i]);
    }
  else
    {
      for (i; i < pinfo->operand_num; i++)
        {
          mask |= csky_get_operand_mask (&pinfo->oprnd.oprnds[i]);
        }
    }

  mask = ~mask;
  return mask;
}

static unsigned int
csky_chars_to_number (unsigned char * buf, int n)
{
  if (n == 0)
    abort ();
  int i;
  int val = 0;
  i = 0;
  if (_csky_dis_info.info->endian == BFD_ENDIAN_BIG)
    while (n--)
      {
        val |= buf[n] << (n*8);
      }
  else
    while (i < n)
      {
        val |= buf[i] << (i*8);
        i++;
      }
  return val;
}

static struct _csky_opcode *opcodeP;
static struct _csky_opcode *
csky_find_inst_info (struct _csky_opcode_info **pinfo, CSKY_INST_TYPE inst, int length)
{
  int i;
  unsigned int mask;
  CSKY_INST_TYPE v;
  struct _csky_opcode *p;

  p = opcodeP;
  while (p->mnemonic)
    {
      /* Get the opcode mask.  */
      for (i = 0; i < OP_TABLE_NUM; i++)
        {
          if (length == 2)
            {
              mask =  csky_get_mask (&p->op16[i]);
              if (mask != 0 && ((inst & mask) == p->op16[i].opcode))
                {
                  *pinfo = &p->op16[i];
                  opcodeP = p;
                  return p;
                }
            }
          else if (length == 4)
            {
              mask =  csky_get_mask(&p->op32[i]);
              if (mask != 0 && ((unsigned long)(inst & mask) == (unsigned long)p->op32[i].opcode))
                {
                  *pinfo = &p->op32[i];
                  opcodeP = p;
                  return p;
                }
            }
        }
      p++;
    }

  return NULL;
}

bfd_boolean
is_extern_symbol (struct disassemble_info *info, int addr)
{
  long  rel_count = 0;

  if(info->section == NULL)
  {
     return 0;
  }
  if((info->section->flags & SEC_RELOC) !=0)/*fit .o file*/
  {
     struct reloc_cache_entry *pt=info->section->relocation;
     for(;rel_count<info->section->reloc_count;rel_count++,pt++)
     {
//       if((long unsigned int)addr == pt->address && ((*pt->sym_ptr_ptr)->flags & BSF_GLOBAL))
       if((long unsigned int)addr == pt->address)
       {
         return TRUE;
       }
     }
     return FALSE;
  }
  return FALSE;
}

bfd_boolean
csky_symbol_is_valid (asymbol *sym,
                      struct disassemble_info * info ATTRIBUTE_UNUSED)
{
  const char *name;

  if (sym == NULL)
    {
      return FALSE;
    }
  name = bfd_asymbol_name (sym);
  return (name && *name != '$');
}

disassembler_ftype
csky_get_disassembler (bfd *abfd)
{
  if (!abfd)
    return NULL;
  mach_flag = elf_elfheader (abfd)->e_flags;
  return print_insn_csky;
}

static int
csky_output_operand (char *str, struct operand *oprnd, CSKY_INST_TYPE inst, int reloc)
{
  int ret = 0;;
  int bit = 0;
  int result = 0;
  bfd_vma value;
  int mask = oprnd->mask;
  int max = 0;

  /* Get operand value with mask.  */
  value = inst & oprnd->mask;
  while (mask)
    {
      if (mask & 0x1)
        {
          result |= ((value & 0x1) << bit);
          max |= (1 << bit);
          bit++;
        }
      mask >>= 1;
      value >>= 1;
    }
  value = result;

  /* Here is general instructions that have no reloc.  */
  switch (oprnd->type)
    {
      case OPRND_TYPE_CTRLREG:
        if (IS_CSKY_V1 (mach_flag))
          {
            if (value < 0 || value >= sizeof (csky_ctrl_reg_alias)/sizeof(char *))
              {
                return -1;
              }
            strcat (str, csky_ctrl_reg_alias[value]);
          }
        else
          {
            int sel;
            int crx;
            char num[64];
            sel = value >> 5;
            crx = value & 0x1f;
            sprintf (num, "cr<%d, %d>", crx, sel);
            strcat (str, num);
          }
        break;
      case OPRND_TYPE_DUMMY_REG:
        mask = _csky_dis_info.opinfo->oprnd.oprnds[0].mask;
        value = inst & mask;
        while (mask)
          {
            if (mask & 0x1)
              {
                result |= ((value & 0x1) << bit);
                bit++;
              }
            mask >>= 1;
            value >>= 1;
          }
        value = result;
        strcat (str, csky_general_reg[value]);
        break;
      case OPRND_TYPE_GREG0_7:
      case OPRND_TYPE_GREG0_15:
      case OPRND_TYPE_GREG16_31:
      case OPRND_TYPE_REGnsplr:
      case OPRND_TYPE_AREG:
        {
          if (IS_CSKY_V2 (mach_flag) && value == 14)
            strcat (str, "sp");
          else
            strcat (str, csky_general_reg[value]);
          _csky_dis_info.value = value;
          break;
        }
      case OPRND_TYPE_CPREG:
        strcat (str, csky_cp_reg[value]);
        break;
      case OPRND_TYPE_FREG:
        {
          int s[64];
          sprintf (s, "fr%d", value);
          strcat (str, s);
        }
        break;
      case OPRND_TYPE_CPCREG:
        strcat (str, csky_cp_creg[value]);
        break;
      case OPRND_TYPE_CPIDX:
        strcat (str, csky_cp_idx[value]);
        break;
      case OPRND_TYPE_IMM2b_JMPIX:
        {
          char num[128];
          value = (value + 2) << 3;
          sprintf (num, "%d", value);
          strcat (str, num);
        }
          break;
      case OPRND_TYPE_IMM_LDST:
        {
#define V1_GET_LDST_SHIFT(opcode, shift)                 \
          shift = v1_shift_tab[((opcode >> 13) & 0x3 )]
#define IS_32_OPCODE(opcode)      (opcode & 0xc0000000)
#define V2_GET_LDST_SHIFT(opcode, shift)            \
          {                                               \
            if (IS_32_OPCODE (opcode))                    \
              shift = ((opcode & 0x3000) >> 12);          \
            else                                          \
              shift = ((opcode & 0x1800) >> 11);          \
            shift = (shift > 2 ? 2 : shift);              \
          }
          int shift = 0;
          if (IS_CSKY_V1 (mach_flag))
            {
              static int v1_shift_tab[] = {2, 0, 1};
              V1_GET_LDST_SHIFT (_csky_dis_info.opinfo->opcode, shift);
            }
          else
            {
              V2_GET_LDST_SHIFT (_csky_dis_info.opinfo->opcode, shift);
            }
          value <<= shift;
          char num[128];
          sprintf (num, "0x%x", value);
          strcat (str, num);
          break;
        }
      case OPRND_TYPE_IMM_FLDST:
        {
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
          int shift = 0;
          V2_GET_FLDST_SHIFT (_csky_dis_info.opinfo->opcode, shift);
          value <<= shift;
          char num[128];
          sprintf (num, "0x%x", value);
          strcat (str, num);
          break;
        }
      case OPRND_TYPE_IMM7b_LS2:
      case OPRND_TYPE_IMM8b_LS2:
        {
          char num[128];
          sprintf (num, "%d", value << 2);
          strcat (str, num);
          ret = 0;
          break;
        }
      case OPRND_TYPE_IMM5b_BMASKI:
        {
          if ((value != 0) && (value > 31 || value < 8))
            {
              ret = -1;
              break;
            }
          char num[128];
          sprintf (num, "%d", value);
          strcat (str, num);
          ret = 0;
          break;
        }
      case OPRND_TYPE_IMM5b_1_31:
        {
          if (value >31 || value < 1)
            {
              ret = -1;
              break;
            }
          char num[128];
          sprintf (num, "%d", value);
          strcat (str, num);
          ret = 0;
          break;
        }
      case OPRND_TYPE_IMM5b_7_31:
        {
          if (value > 31 || value < 7)
            {
              ret = -1;
              break;
            }
          char num[128];
          sprintf (num, "%d", value);
          strcat (str, num);
          ret = 0;
          break;
        }

      case OPRND_TYPE_MSB2SIZE:
      case OPRND_TYPE_LSB2SIZE:
        {
          static int size;
          char num[128];
          if (oprnd->type == OPRND_TYPE_MSB2SIZE)
            {
              size = value;
            }
          else
            {
              str[strlen (str) - 2] = '\0';
              sprintf (num, "%d, %d", size + value, value);
              strcat (str, num);
            }
          break;
        }
      case OPRND_TYPE_IMM2b:
      case OPRND_TYPE_IMM4b:
      case OPRND_TYPE_IMM5b:
      case OPRND_TYPE_IMM7b:
      case OPRND_TYPE_IMM8b:
      case OPRND_TYPE_IMM12b:
      case OPRND_TYPE_IMM15b:
      case OPRND_TYPE_IMM16b:
      case OPRND_TYPE_IMM16b_MOVIH:
      case OPRND_TYPE_IMM16b_ORI:
        {
          char num[128];
          sprintf (num, "%d", value);
          strcat (str, num);
          ret = 0;
          break;
        }
      case OPRND_TYPE_OFF8b:
      case OPRND_TYPE_OFF16b:
        {
          unsigned char ibytes[4];
          int shift = oprnd->shift;
          int status;
          unsigned int mem_val;
          char num[128];

          _csky_dis_info.info->stop_vma = 0;

          value = ((_csky_dis_info.mem + (value << shift)
                    + ((IS_CSKY_V1 (mach_flag)) ? 2 : 0)) & 0xfffffffc);
          status = _csky_dis_info.info->read_memory_func (value, ibytes, 4,
                                                          _csky_dis_info.info);
          if (status != 0)
            {
              _csky_dis_info.info->memory_error_func (status, _csky_dis_info.mem,
                                                      _csky_dis_info.info);
              return -1;
            }
          mem_val = csky_chars_to_number (ibytes, 4);
          /* Removed [] around literal value to match ABI syntax 12/95.  */
          sprintf (num, "0x%X", mem_val);
          strcat (str, num);
          /* For jmpi/jsri, we'll try to get a symbol for the target.  */
          if (_csky_dis_info.info->print_address_func && mem_val != 0)
            {
              _csky_dis_info.value = mem_val;
              _csky_dis_info.need_output_symbol = 1;
            }
          else
            {
              sprintf (num, "\t// from address pool at 0x%x", value);
              strcat (str, num);
            }
          break;
        }
      case OPRND_TYPE_OFF11b:
      case OPRND_TYPE_OFF16b_LSL1:
      case OPRND_TYPE_IMM_OFF18b:
      case OPRND_TYPE_OFF26b:
        {  /* TODO:  */
          int shift = oprnd->shift;
          if (value & ((max >> 1) + 1))
            {
              value |= ~(max);
            }
          if (is_extern_symbol (_csky_dis_info.info, _csky_dis_info.mem))
            value = 0;
          else if (IS_CSKY_V1 (mach_flag))
            value = _csky_dis_info.mem + 2 + (value << shift);
          else
            value = _csky_dis_info.mem + (value << shift);
          _csky_dis_info.need_output_symbol = 1;
          _csky_dis_info.value= value;
          char s[128];
          sprintf (s, "0x%x", value);
          strcat (str, s);
          break;
        }
      case OPRND_TYPE_CONSTANT:
      case OPRND_TYPE_FCONSTANT:
        {  /* TODO:  */
          int shift = oprnd->shift;
          char ibytes[4];
          int status;
          bfd_vma addr;
          _csky_dis_info.info->stop_vma = 0;
          if (value & ((max >> 1) + 1))
            {
              value |= ~(max);
            }
          if (IS_CSKY_V1 (mach_flag))
            addr = (_csky_dis_info.mem + 2 + (value << shift))
                   & 0xfffffffc;
          else
            addr = (_csky_dis_info.mem + (value << shift))
                   & 0xfffffffc;
          status = _csky_dis_info.info->read_memory_func (addr, ibytes, 4,
                                                          _csky_dis_info.info);
          if (status != 0)
            {
              _csky_dis_info.info->memory_error_func (status, addr,
                                                      _csky_dis_info.info);
              return -1;
            }

          _csky_dis_info.value = addr;
          value = csky_chars_to_number (ibytes, 4);
          char s[128];
          if (oprnd->type == OPRND_TYPE_FCONSTANT)
            {
              if (_csky_dis_info.opinfo->opcode == CSKYV2_INST_FLRW)
                {
                  /* flrws.  */
                  float *f = (float *)((void *)&value);
                  sprintf (s, "%f", *f);
                }
              else
                {
                  long long dvalue = (long long)value;
                  addr += 4;
                  status = _csky_dis_info.info->read_memory_func (addr, ibytes, 4,
                                                                  _csky_dis_info.info);
                  value = csky_chars_to_number (ibytes, 4);
                  dvalue |= ((long long)value << 32);
                  double *d = (double *)((void *)&dvalue);
                  sprintf (s, "%f", *d);
                }
            }
          else
            {
              _csky_dis_info.need_output_symbol = 1;
              sprintf (s, "0x%x", value);
            }

          strcat (str, s);
          break;
        }
      case OPRND_TYPE_LABEL_WITH_BRACKET:
        {
          char s[128];
          sprintf (s, "[0x%x]", value);
          strcat (str, s);
          strcat (str, "\t// the offset is based on .data");
          break;
        }
      case OPRND_TYPE_OIMM3b:
      case OPRND_TYPE_OIMM5b:
      case OPRND_TYPE_OIMM5b_IDLY:
      case OPRND_TYPE_OIMM8b:
      case OPRND_TYPE_OIMM12b:
      case OPRND_TYPE_OIMM16b:
      case OPRND_TYPE_OIMM18b:
        {
          value += 1;
          char num[128];
          sprintf (num, "%d", value);
          strcat (str, num);
          break;
        }
      case OPRND_TYPE_OIMM5b_BMASKI:
        {
          if (value > 32 || value < 16)
            {
              ret = -1;
              break;
            }
          char num[128];
          sprintf (num, "%d", (value + 1));
          strcat (str, num);
          ret = 0;
          break;
        }
      case OPRND_TYPE_FREGLIST_DASH:
        {
           if (IS_CSKY_V1 (mach_flag))
            {
              /* TODO:  */
            }
          else
            {
              int vrx = value & 0xf;
              int vry = vrx + (value >> 4);
              char s[64];
              sprintf (s, "fr%d-fr%d", vrx, vry);
              strcat (str, s);
            }
          break;
        }
      case OPRND_TYPE_REGLIST_DASH:
        {
          if (IS_CSKY_V1 (mach_flag))
            {
              strcat (str, csky_general_reg[value]);
              strcat (str, "-r15");
            }
          else
            {
              int rx = value >> 5;
              int ry = value & 0x1f;
              strcat (str, csky_general_reg[value >> 5]);
              strcat (str, "-");
              strcat (str, csky_general_reg[(value & 0x1f) + (value >> 5)]);
            }
          break;
        }
      case OPRND_TYPE_PSR_BITS_LIST:
        {
          struct psrbit *bits;
          int first_oprnd = TRUE;
          int i = 0;
          if (IS_CSKY_V1 (mach_flag))
            {
              if(value == 0)
                {
                  strcat (str, "af");
                  break;
                }
              bits = cskyv1_psr_bits;
            }
          else
            bits = cskyv2_psr_bits;
          while (value != 0 && bits[i].name != NULL)
            {

              if (value & bits[i].value)
                {
                  if (!first_oprnd)
                    strcat (str, ", ");
                  strcat (str, bits[i].name);
                  value &= ~bits[i].value;
                  first_oprnd = FALSE;
                }
                i++;
            }
          break;
        }
      case OPRND_TYPE_REGbsp:
        {
          if (IS_CSKY_V1 (mach_flag))
            strcat (str, "(sp)");
          else
            strcat (str, "(sp)");
          break;
        }

      case OPRND_TYPE_REGsp:
        {
          if (IS_CSKY_V1 (mach_flag))
            strcat (str, "sp");
          else
            strcat (str, "sp");
          break;
        }
      case OPRND_TYPE_REGnr4_r7:
      case OPRND_TYPE_AREG_WITH_BRACKET:
        {
          if (IS_CSKY_V1 (mach_flag)
              && (value < 4 || value > 7))
            {
              strcat (str, "(");
              strcat (str, csky_general_reg[value]);
              strcat (str, ")");
            }
          else
            {
              strcat (str, "(");
              strcat (str, csky_general_reg[value]);
              strcat (str, ")");
            }
          break;
        }
      case OPRND_TYPE_AREG_WITH_LSHIFT:
        {
          strcat (str, csky_general_reg[value >> 5]);
          strcat (str, " << ");
          if ((value & 0x1f) == 0x1)
            strcat (str, "0");
          else if ((value & 0x1f) == 0x2)
            strcat (str, "1");
          else if ((value & 0x1f) == 0x4)
            strcat (str, "2");
          else if ((value & 0x1f) == 0x8)
            strcat (str, "3");
          break;
        }
      case OPRND_TYPE_AREG_WITH_LSHIFT_FPU:
        {
          strcat (str, csky_general_reg[value >> 2]);
          strcat (str, " << ");
          if ((value & 0x3) == 0x0)
            strcat (str, "0");
          else if ((value & 0x3) == 0x1)
            strcat (str, "1");
          else if ((value & 0x3) == 0x2)
            strcat (str, "2");
          else if ((value & 0x3) == 0x3)
            strcat (str, "3");
          break;
        }
      case OPRND_TYPE_FREG_WITH_INDEX:
        {
          char s[64];
          unsigned freg_val = value & 0xf;
          unsigned index_val = (value >> 4) & 0xf;
          sprintf (s, "vr%d[%d]", freg_val, index_val);
          strcat(str, s);
          break;
        }
      case OPRND_TYPE_REGr4_r7:
        {
          if (IS_CSKY_V1 (mach_flag))
            strcat (str, "r4-r7");
          else
            /* TODO:  */
            ;
          break;
        }
      case OPRND_TYPE_CONST1:
        strcat (str, "1");
        break;
      case OPRND_TYPE_REG_r1a:
      case OPRND_TYPE_REG_r1b:
        strcat (str, "r1");
        break;
      case OPRND_TYPE_REG_r28:
        strcat (str, "r28");
        break;
      case OPRND_TYPE_REGLIST_DASH_COMMA:
        /* 16bits reglist.  */
        if (value & 0xf)
          {
            strcat (str, "r4");
            if ((value & 0xf) > 1)
              {
                strcat (str, "-");
                strcat (str, csky_general_reg[(value & 0xf) + 3]);
              }
            if (value & ~0xf)
              strcat (str, ", ");
          }
        if (value & 0x10)
          {
            /* r15.  */
            strcat (str, "r15");
            if (value & ~0x1f)
              strcat (str, ", ");
          }
        if (!(_csky_dis_info.opinfo->oprnd.oprnds[0].mask == OPRND_MASK_0_4))
          {
            /* 32bits reglist.  */
            value >>= 5;
            if (value & 0x3)
              {
                strcat (str, "r16");
                if ((value & 0x7) > 1)
                  {
                    strcat (str, "-");
                    strcat (str, csky_general_reg[(value & 0xf) + 15]);
                  }
                if (value & ~0x7)
                  strcat (str, ", ");
              }
            if (value & 0x8)
              {
                /* r15.  */
                strcat (str, "r28");
              }

          }
        break;
      case OPRND_TYPE_UNCOND10b:
      case OPRND_TYPE_UNCOND16b:
      case OPRND_TYPE_COND10b:
      case OPRND_TYPE_COND16b:
        {
          int shift = oprnd->shift;
          char num[128];

          if (value & ((max >> 1) + 1))
            {
              value |= ~(max);
            }
          if (is_extern_symbol (_csky_dis_info.info, _csky_dis_info.mem))
            value = 0;
          else
            value = _csky_dis_info.mem + (value << shift);
          sprintf (num, "0x%x", value);
          strcat (str, num);
          _csky_dis_info.need_output_symbol = 1;
          _csky_dis_info.value = value;
        }
        break;
      default:
        ret = -1;
        break;
    }
  return ret;
}

static int
csky_print_operand(char *str, struct operand *oprnd, CSKY_INST_TYPE inst, int reloc)
{
  int ret = -1;
  char *lc;
  char *rc;
  if (oprnd->mask == HAS_SUB_OPERAND)
    {
      struct soperand *sop = (struct soperand *)oprnd;
      if (oprnd->type == OPRND_TYPE_BRACKET)
        {
          lc = "(";
          rc = ")";
        }
      else if (oprnd->type == OPRND_TYPE_ABRACKET)
        {
          lc = "<";
          rc = ">";
        }
      strcat (str, lc);
      ret = csky_print_operand (str, &sop->subs[0], inst, reloc);
      if (ret)
        {
          return ret;
        }
      strcat (str, ", ");
      ret = csky_print_operand (str, &sop->subs[1], inst, reloc);
      strcat (str, rc);
      return ret;
    }
  return csky_output_operand (str, oprnd, inst, reloc);
}

static int
csky_print_operands (char *str, struct _csky_opcode_info *pinfo,
                     struct disassemble_info *info, CSKY_INST_TYPE inst,
                     int reloc)
{
  int i = 0;
  int ret = 0;
  if (pinfo->operand_num)
    {
      strcat (str, "      \t");
    }
  if (pinfo->operand_num == -1)
    {
      ret = csky_print_operand (str, &pinfo->oprnd.oprnds[i], inst, reloc);
      if (ret)
        {
          return ret;
        }
    }
  else
    {
      for (i; i < pinfo->operand_num; i++)
        {
          if (i != 0)
            {
              strcat (str, ", ");
            }
          ret = csky_print_operand (str, &pinfo->oprnd.oprnds[i], inst, reloc);
          if (ret)
            {
              return ret;
            }
        }
    }
  info->fprintf_func (info->stream, str);
  if (_csky_dis_info.need_output_symbol)
    {
      info->fprintf_func (info->stream, "\t// ");
      info->print_address_func (_csky_dis_info.value, _csky_dis_info.info);
    }
  return 0;
}

void
number_to_chars_littleendian (char *buf, CSKY_INST_TYPE val, int n)
{
  if (n <= 0)
    abort ();
  while (n--)
    {
      *buf++ = val & 0xff;
      val >>= 8;
    }
}

/*status !=0,return -1,advoid -D limitless objdump*/
#define CSKY_READ_DATA()                                        \
{                                                               \
  status = info->read_memory_func (memaddr, buf, 2, info);      \
  if (status)                                                   \
    {                                                           \
      info->memory_error_func (status, memaddr, info);          \
      return -1;                                                \
    }                                                           \
  if (info->endian == BFD_ENDIAN_BIG)                           \
    inst |= (buf[0] << 8) | buf[1];                             \
  else if (info->endian == BFD_ENDIAN_LITTLE)                   \
    inst |= (buf[1] << 8) | buf[0];                             \
  else                                                          \
    abort();                                                    \
  info->bytes_per_chunk += 2;                                   \
  memaddr += 2;                                                 \
}

int
print_insn_csky(bfd_vma memaddr, struct disassemble_info *info)
{
  unsigned char buf[4];
  CSKY_INST_TYPE inst;
  int status;
  char str[256];
  memset(str,0,sizeof(str));
  info->bytes_per_chunk = 0;
  inst = 0;
  long given;
  info->bytes_per_chunk = 0;
  int is_data = FALSE;
  void (*printer) (bfd_vma, struct disassemble_info *, long);
  bfd_boolean   found = FALSE;
  unsigned int  size = 4;
  _csky_dis_info.mem = memaddr;
  _csky_dis_info.info = info;
  _csky_dis_info.need_output_symbol = 0;
  if (mach_flag != INIT_MACH_FLAG && mach_flag != BINARY_MACH_FLAG)
    {
      info->mach = mach_flag;
    }
  else if (mach_flag == INIT_MACH_FLAG)
    {
      mach_flag = info->mach;
    }

  if (mach_flag == BINARY_MACH_FLAG
      && info->endian == BFD_ENDIAN_UNKNOWN)
    {
      info->endian = BFD_ENDIAN_LITTLE;
    }

  /* TODO: Dumping insns.  */
  /* First check the full symtab for a mapping symbol, even if there
     are no usable non-mapping symbols for this address.  */
  if (info->symtab_size != 0
        && bfd_asymbol_flavour (*info->symtab) == bfd_target_elf_flavour)
    {
      bfd_vma addr;
      int n;
      int last_sym = -1;
      enum sym_type type = CUR_TEXT;

      if (memaddr <= last_map_addr)
        last_map_sym = -1;
      /* Start scanning at the start of the function, or wherever
         we finished last time.  */
      n = 0;//info->symtab_pos+1 ;
      if (n < last_map_sym)
        n = last_map_sym;

      /* Scan up to the location being disassembled.  */
      for (; n < info->symtab_size; n++)
        {
          addr = bfd_asymbol_value (info->symtab[n]);
          if (addr > memaddr)
            break;

        if ((info->section == NULL
               || info->section == info->symtab[n]->section)
              && get_sym_code_type (info, n, &type))
            {
              last_sym = n;
              found = TRUE;
            }
        }
      last_map_sym = last_sym;
      last_type = type;
      is_data = (last_type == CUR_DATA);
      if (is_data)
        {
          size = 4 - ( memaddr& 3);
          for (n = last_sym + 1; n < info->symtab_size; n++)
            {
              addr = bfd_asymbol_value (info->symtab[n]);
              if (addr > memaddr)
                {
                  if (addr - memaddr < size)
                    size = addr - memaddr;
                  break;
                }
            }
          /* If the next symbol is after three bytes, we need to
             print only part of the data, so that we can use either
             .byte or .short.  */
          if (size == 3)
            size = (memaddr & 1) ? 1 : 2;
        }
    }
  info->bytes_per_line = 4;

  if(is_data)
    {
      int i;

      /* Size was already set above.  */
      info->bytes_per_chunk = size;
      printer = print_insn_data;

      status = info->read_memory_func (memaddr, (bfd_byte *) buf, size, info);
      given = 0;
      if (info->endian == BFD_ENDIAN_LITTLE)
        for (i = size - 1; i >= 0; i--)
          given = buf[i] | (given << 8);
      else
        for (i = 0; i < (int) size; i++)
          given = buf[i] | (given << 8);
      /* if (status)
       {
         info->memory_error_func (status, memaddr, info);
         return -1;
       }*/

      printer (memaddr, info, given);
      return info->bytes_per_chunk;
    }
  /* insts.  */
  CSKY_READ_DATA();
#define IS_32BIT(inst) ((inst & 0xc000) == 0xc000)
  if (IS_32BIT(inst) && IS_CSKY_V2 (mach_flag))
    {
      inst <<= 16;
      CSKY_READ_DATA();
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

  /* If v1 p = csky_v1_opcodes,
     else v2 p = csky_v1_opcodes.  */
  if (IS_CSKY_V1 (mach_flag))
    {
      opcodeP = csky_v1_opcodes;
    }
  else
    {
      opcodeP = csky_v2_opcodes;
    }

  struct _csky_opcode *op;
  struct _csky_opcode_info *pinfo = NULL;
  int reloc;

lable_get_inst_info:
  memset (str, 0, sizeof (str));
  op = csky_find_inst_info (&pinfo, inst, info->bytes_per_chunk);
  if (!op)
    {
      if (IS_CSKY_V1 (mach_flag))
        info->fprintf_func (info->stream, ".short: 0x%04x", inst);
      else
        info->fprintf_func (info->stream, ".long: 0x%08x", inst);
      return info->bytes_per_chunk;
    }

  if (info->bytes_per_chunk == 2)
    {
      reloc = op->reloc16;
    }
  else
    {
      reloc = op->reloc32;
    }
  _csky_dis_info.opinfo = pinfo;
  strcat (str, op->mnemonic);

  if (csky_print_operands (str, pinfo, info, inst, reloc))
    {
      opcodeP++;
      goto lable_get_inst_info;
    }

  return info->bytes_per_chunk;
}
