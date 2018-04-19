#include "sysdep.h"
#include "bfd.h"
#include "libiberty.h"
#include "safe-ctype.h"
#include "obstack.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldmain.h"
#include "ldexp.h"
#include "ldlang.h"
#include <ldgram.h>
#include "ldlex.h"
#include "ldmisc.h"
#include "ldctor.h"
#include "ldfile.h"
#include "ldemul.h"
#include "fnmatch.h"
#include "demangle.h"
#include "hashtab.h"
#include "ld_csky.h"
#include "math.h"
#include "dwarf2.h"

typedef struct dump_callgraph_target
{
  void (*dump_file_head) (const char *filename);
  void (*dump_ld_version) (void);
  void (*dump_max_stack_func) (callgraph_hash_table * func_callgraph);
  void (*dump_recursive_funcs) (void);
  void (*dump_func_pointer) (void);
  void (*dump_global_symbols) (void);
  void (*dump_local_symbols) (void);
  void (*dump_file_tail) (void);
} dump_callgraph_target;
typedef unsigned long long dwarf_vma;
typedef unsigned long long dwarf_size_type;

static dwarf_vma (*byte_get) (unsigned char *, int);
static func_link_list recursive_func_routes;
static bfd_boolean is_cycle = FALSE;
static dump_callgraph_target current_callgraph_target;

static char *trans_func_name (bfd * bfd, const char *func_name);
static bfd_boolean is_max_in_cycle (struct callgraph_hash_entry *func);
static void add_stack_size (callgraph_hash_table * func_callgraph);
static void find_stack_size_ssd (bfd * abfd,
                                 asection * section,
                                 bfd_func_link * abfd_func_link);

static bfd_boolean
calculate_max_depth (struct callgraph_hash_entry *h, void *data)
{
  func_vec *func;
  int max_depth = 0;
  struct callgraph_hash_entry *max_depth_route = NULL;
  bfd_boolean max_in_cycle = FALSE;
  bfd_func_link *func_route_stack = (bfd_func_link *) data;
  func_hash_bind *func_now = NULL;

  /*The function has traversed means that the function's
   * max_depth has already been calculated. */
  if (h->has_traversed == TRUE)
    return TRUE;

  func_now = func_stack_search (func_route_stack, h);
  if (func_now != NULL)
    {
      is_cycle = TRUE;
      return TRUE;
    }
  else
    {
      func_hash_bind func_new;

      func_new.hash_entry = h;
      func_new.next = NULL;
      func_stack_push (func_route_stack, func_new);
    }

  func = h->calls;
  while (func != NULL)
    {
      calculate_max_depth (func->hash_entry, (void *) func_route_stack);
      /*if the function is in cycle, th max_depth of the function
       * must have not been calculated*/
      if (is_cycle)
        max_in_cycle = TRUE;
      else if (func->hash_entry->max_depth > max_depth)
        {
          max_in_cycle = FALSE;
          max_depth = func->hash_entry->max_depth;
          max_depth_route = func->hash_entry;
        }
      else
        {
          /*Do nothing */
        }
      is_cycle = FALSE;
      func = func->next;
    }

  h->max_in_cycle = max_in_cycle;
  h->max_depth_route = max_depth_route;
  h->max_depth = (max_in_cycle ? 0 : max_depth) + h->stack_size;
  h->has_traversed = TRUE;
  func_stack_pop (func_route_stack);

  return TRUE;
}

static long
get_func_symbols (asymbol ** symbols, long count)
{
  asymbol **in_ptr = symbols, **out_ptr = symbols;

  while (--count >= 0)
    {
      asymbol *sym = *in_ptr++;

      if (sym->name == NULL || sym->name[0] == '\0')
        continue;
      else if (!(sym->flags & BSF_FUNCTION))
        continue;
      else if (bfd_is_und_section (sym->section)
               || bfd_is_com_section (sym->section))
        continue;

      *out_ptr++ = sym;
    }

  return out_ptr - symbols;
}

/* Sort symbols into value order.  */

int
compare_symbols (const void *ap, const void *bp)
{
  const asymbol *a = *(const asymbol **) ap;
  const asymbol *b = *(const asymbol **) bp;
  const char *an;
  const char *bn;
  size_t anl;
  size_t bnl;
  bfd_boolean af;
  bfd_boolean bf;
  flagword aflags;
  flagword bflags;

  if (bfd_asymbol_value (a) > bfd_asymbol_value (b))
    return 1;
  else if (bfd_asymbol_value (a) < bfd_asymbol_value (b))
    return -1;

  if (a->section > b->section)
    return 1;
  else if (a->section < b->section)
    return -1;

  an = bfd_asymbol_name (a);
  bn = bfd_asymbol_name (b);
  anl = strlen (an);
  bnl = strlen (bn);

  /* The symbols gnu_compiled and gcc2_compiled convey no real
     information, so put them after other symbols with the same value.  */
  af = (strstr (an, "gnu_compiled") != NULL
        || strstr (an, "gcc2_compiled") != NULL);
  bf = (strstr (bn, "gnu_compiled") != NULL
        || strstr (bn, "gcc2_compiled") != NULL);

  if (af && !bf)
    return 1;
  if (!af && bf)
    return -1;

  /* We use a heuristic for the file name, to try to sort it after
     more useful symbols.  It may not work on non Unix systems, but it
     doesn't really matter; the only difference is precisely which
     symbol names get printed.  */

#define file_symbol(s, sn, snl)			\
  (((s)->flags & BSF_FILE) != 0			\
   || ((sn)[(snl) - 2] == '.'			\
       && ((sn)[(snl) - 1] == 'o'		\
	   || (sn)[(snl) - 1] == 'a')))

  af = file_symbol (a, an, anl);
  bf = file_symbol (b, bn, bnl);

  if (af && !bf)
    return 1;
  if (!af && bf)
    return -1;

  /* Try to sort global symbols before local symbols before function
     symbols before debugging symbols.  */

  aflags = a->flags;
  bflags = b->flags;

  if ((aflags & BSF_DEBUGGING) != (bflags & BSF_DEBUGGING))
    {
      if ((aflags & BSF_DEBUGGING) != 0)
        return 1;
      else
        return -1;
    }
  if ((aflags & BSF_FUNCTION) != (bflags & BSF_FUNCTION))
    {
      if ((aflags & BSF_FUNCTION) != 0)
        return -1;
      else
        return 1;
    }
  if ((aflags & BSF_LOCAL) != (bflags & BSF_LOCAL))
    {
      if ((aflags & BSF_LOCAL) != 0)
        return 1;
      else
        return -1;
    }
  if ((aflags & BSF_GLOBAL) != (bflags & BSF_GLOBAL))
    {
      if ((aflags & BSF_GLOBAL) != 0)
        return -1;
      else
        return 1;
    }

  /* Symbols that start with '.' might be section names, so sort them
     after symbols that don't start with '.'.  */
  if (an[0] == '.' && bn[0] != '.')
    return 1;
  if (an[0] != '.' && bn[0] == '.')
    return -1;

  /* Finally, if we can't distinguish them in any other way, try to
     get consistent results by sorting the symbols by name.  */
  return strcmp (an, bn);
}


#define GET(N)	byte_get (start, N); start += N
#define LEB()	read_leb128 (start, & length_return, 0); start += length_return
#define SLEB()	read_leb128 (start, & length_return, 1); start += length_return

static unsigned long int
read_leb128 (unsigned char *data, unsigned int *length_return, int sign)
{
  unsigned long int result = 0;
  unsigned int num_read = 0;
  unsigned int shift = 0;
  unsigned char byte;

  do
    {
      byte = *data++;
      num_read++;

      result |= ((unsigned long int) (byte & 0x7f)) << shift;

      shift += 7;

    }
  while (byte & 0x80);

  if (length_return != NULL)
    *length_return = num_read;

  if (sign && (shift < 8 * sizeof (result)) && (byte & 0x40))
    result |= -1L << shift;

  return result;
}

static void
error (const char *message, ...)
{
  va_list args;

  va_start (args, message);
  fprintf (stderr, _("%s: Error: "), program_name);
  vfprintf (stderr, message, args);
  va_end (args);
}

static dwarf_vma
byte_get_little_endian (unsigned char *field, int size)
{
  switch (size)
    {
    case 1:
      return *field;

    case 2:
      return ((unsigned int) (field[0])) | (((unsigned int) (field[1])) << 8);

    case 3:
      return ((unsigned long) (field[0]))
             | (((unsigned long) (field[1])) << 8)
             | (((unsigned long) (field[2])) << 16);

    case 4:
      return ((unsigned long) (field[0]))
             | (((unsigned long) (field[1])) << 8)
             | (((unsigned long) (field[2])) << 16)
             | (((unsigned long) (field[3])) << 24);

    case 8:
      if (sizeof (dwarf_vma) == 8)
        return ((dwarf_vma) (field[0]))
               | (((dwarf_vma) (field[1])) << 8)
               | (((dwarf_vma) (field[2])) << 16)
               | (((dwarf_vma) (field[3])) << 24)
               | (((dwarf_vma) (field[4])) << 32)
               | (((dwarf_vma) (field[5])) << 40)
               | (((dwarf_vma) (field[6])) << 48)
               | (((dwarf_vma) (field[7])) << 56);
      else if (sizeof (dwarf_vma) == 4)
        /* We want to extract data from an 8 byte wide field and
           place it into a 4 byte wide field.  Since this is a little
           endian source we can just use the 4 byte extraction code.  */
        return ((unsigned long) (field[0]))
               | (((unsigned long) (field[1])) << 8)
               | (((unsigned long) (field[2])) << 16)
               | (((unsigned long) (field[3])) << 24);

    default:
      error (_("Unhandled data length: %d\n"), size);
      abort ();
    }
}

static dwarf_vma
byte_get_big_endian (unsigned char *field, int size)
{
  switch (size)
    {
    case 1:
      return *field;

    case 2:
      return ((unsigned int) (field[1])) | (((int) (field[0])) << 8);

    case 3:
      return ((unsigned long) (field[2]))
             | (((unsigned long) (field[1])) << 8)
             | (((unsigned long) (field[0])) << 16);

    case 4:
      return ((unsigned long) (field[3]))
             | (((unsigned long) (field[2])) << 8)
             | (((unsigned long) (field[1])) << 16)
             | (((unsigned long) (field[0])) << 24);

    case 8:
      if (sizeof (dwarf_vma) == 8)
        return ((dwarf_vma) (field[7]))
               | (((dwarf_vma) (field[6])) << 8)
               | (((dwarf_vma) (field[5])) << 16)
               | (((dwarf_vma) (field[4])) << 24)
               | (((dwarf_vma) (field[3])) << 32)
               | (((dwarf_vma) (field[2])) << 40)
               | (((dwarf_vma) (field[1])) << 48)
               | (((dwarf_vma) (field[0])) << 56);
      else if (sizeof (dwarf_vma) == 4)
        {
          /* Although we are extracing data from an 8 byte wide field,
             we are returning only 4 bytes of data.  */
          field += 4;
          return ((unsigned long) (field[3]))
                 | (((unsigned long) (field[2])) << 8)
                 | (((unsigned long) (field[1])) << 16)
                 | (((unsigned long) (field[0])) << 24);
        }

    default:
      error (_("Unhandled data length: %d\n"), size);
      abort ();
    }
}

static int
size_of_encoded_value (int encoding)
{
  switch (encoding & 0x7)
    {
    default:			/* ??? */
    case 0:
      return 4;			/*eh addr size */
    case 2:
      return 2;
    case 3:
      return 4;
    case 4:
      return 8;
    }
}

static dwarf_vma
byte_get_signed (unsigned char *field, int size)
{
  dwarf_vma x = byte_get (field, size);

  switch (size)
    {
    case 1:
      return (x ^ 0x80) - 0x80;
    case 2:
      return (x ^ 0x8000) - 0x8000;
    case 4:
      return (x ^ 0x80000000) - 0x80000000;
    case 8:
      return x;
    default:
      abort ();
    }
}

static dwarf_vma
get_encoded_value (unsigned char *data, int encoding)
{
  int size = size_of_encoded_value (encoding);

  if (encoding & DW_EH_PE_signed)
    return byte_get_signed (data, size);
  else
    return byte_get (data, size);
}

/* FIXME
 * use jump_to_one_fde to increase the speed of function find_stack_size_df. */
#if 0
/*Jump to the certain FDE position in frame seciton accordding to
 * the offset, and set the nearest CIE. */
static unsigned char *
jump_to_one_fde (unsigned char *start,
                 asection * section, debug_frame_entry * cie, bfd * abfd)
{
  unsigned long offset = section->output_offset;
  unsigned char *section_start = start;
  unsigned char *end = start + offset;
  int is_eh = strcmp (section->name, ".eh_frame") == 0;
  debug_frame_entry *chunks = NULL;
  unsigned int length_return;

  while (start < end)
    {
      unsigned char *saved_start;
      unsigned char *block_end;
      unsigned long length;
      unsigned long cie_id;
      unsigned char *augmentation_data = NULL;
      unsigned long augmentation_data_len = 0;
      int offset_size;
      int initial_length_size;
      debug_frame_entry *fc = NULL;
      int eh_addr_size = bfd_get_arch_size (abfd) == 64 ? 8 : 4;
      //int encoded_ptr_size = 4;/*eh addr size*/
      int encoded_ptr_size = eh_addr_size;

      saved_start = start;
      length = byte_get (start, 4);
      start += 4;

      if (length == 0)
        continue;
      if (length == 0xffffffff)
        {
          length = byte_get (start, 8);
          start += 8;
          offset_size = 8;
          initial_length_size = 12;
        }
      else
        {
          offset_size = 4;
          initial_length_size = 4;
        }

      block_end = saved_start + length + initial_length_size;
      if (block_end > end)
        block_end = end;

      cie_id = byte_get (start, offset_size);
      start += offset_size;

      if (is_eh ? (cie_id == 0) : (cie_id == DW_CIE_ID))
        {
          int version;

          fc = (debug_frame_entry *) xmalloc (sizeof (debug_frame_entry));
          memset (fc, 0, sizeof (debug_frame_entry));

          fc->next = chunks;
          chunks = fc;
          fc->chunk_start = saved_start;

          version = *start++;
          fc->augmentation = (char *) start;
          start = (unsigned char *) strchr ((char *) start, '\0') + 1;

          if (fc->augmentation[0] == 'z')
            {
              fc->code_factor = LEB ();
              SLEB ();
              if (version == 1)
                start += 1;
              else
                {
                  LEB ();
                }
              augmentation_data_len = LEB ();
              augmentation_data = start;
              start += augmentation_data_len;
            }
          else if (strcmp (fc->augmentation, "eh") == 0)
            {
              start += eh_addr_size;
              LEB ();
              SLEB ();
              if (version == 1)
                start += 1;
              else
                {
                  LEB ();
                }
              //printf ("Warning: Callgraph get unexpected '.eh_frame' section\n");
            }
          else
            {
              fc->code_factor = LEB ();
              SLEB ();
              if (version == 1)
                start += 1;
              else
                {
                  LEB ();
                }
            }
          cie = fc;

          if (augmentation_data_len)
            {
              unsigned char *p, *q;
              p = (unsigned char *) fc->augmentation + 1;
              q = augmentation_data;

              while (1)
                {
                  if (*p == 'L')
                    q++;
                  else if (*p == 'P')
                    q += 1 + size_of_encoded_value (*q);
                  else if (*p == 'R')
                    fc->fde_encoding = *q++;
                  else
                    break;
                  p++;
                }

              if (fc->fde_encoding)
                encoded_ptr_size = size_of_encoded_value (fc->fde_encoding);
            }

        }
      /*Every FDE represent a function, find function in the abfd_func_link
       * according to function start pc.*/
      else
        {
          unsigned char *look_for;
          static debug_frame_entry fde_fc;

          fc = &fde_fc;
          memset (fc, 0, sizeof (debug_frame_entry));

          look_for = is_eh ? start - 4 - cie_id : section_start + cie_id;

          for (cie = chunks; cie; cie = cie->next)
            if (cie->chunk_start == look_for)
              break;

          if (!cie)
            {
              printf
              ("Warning: Invalid CIE pointer %#08lx in FDE at %#08lx used for callgraph\n",
               cie_id, (unsigned long) (saved_start - section_start));
              cie = fc;
              fc->augmentation = "";
              fc->fde_encoding = 0;
            }
          else
            {
              fc->augmentation = cie->augmentation;
              fc->code_factor = cie->code_factor;
              fc->fde_encoding = cie->fde_encoding;
              fc->cfa_offset = cie->cfa_offset;
            }

          if (fc->fde_encoding)
            encoded_ptr_size = size_of_encoded_value (fc->fde_encoding);

          fc->pc_begin = get_encoded_value (start, fc->fde_encoding);
          if ((fc->fde_encoding & 0x70) == DW_EH_PE_pcrel)
            fc->pc_begin += (start - section_start);
          start += encoded_ptr_size;
          fc->pc_range = byte_get (start, encoded_ptr_size);
          start += encoded_ptr_size;

          if (cie->augmentation[0] == 'z')
            {
              augmentation_data_len = LEB ();
              augmentation_data = start;
              start += augmentation_data_len;
            }
#ifdef CSKY_CALLGRAPH_DEBUG
          printf ("\t\tpc=%08lx..%08lx, ",
                  fc->pc_begin, fc->pc_begin + fc->pc_range);
#endif
        }

      while (start < block_end)
        {
          unsigned op, opa;
          dwarf_vma vma;
          long l;
          unsigned long ul;

          op = *start++;
          opa = op & 0x3f;
          if (op & 0xc0)
            op &= 0xc0;

          switch (op)
            {
            case DW_CFA_advance_loc:
              break;
            case DW_CFA_offset:
              LEB ();
              break;
            case DW_CFA_restore:
              break;
            case DW_CFA_set_loc:
              start += encoded_ptr_size;
              break;
            case DW_CFA_advance_loc1:
              start += 1;
              break;
            case DW_CFA_advance_loc2:
              start += 2;
              break;
            case DW_CFA_advance_loc4:
              start += 4;
              break;
            case DW_CFA_offset_extended:
            case DW_CFA_val_offset:
            case DW_CFA_register:
            case DW_CFA_def_cfa:
              LEB ();
              LEB ();
              break;
            case DW_CFA_restore_extended:
            case DW_CFA_undefined:
            case DW_CFA_same_value:
            case DW_CFA_def_cfa_register:
              LEB ();
              break;
            case DW_CFA_remember_state:
              /*FIXME what's the usage */
              break;
            case DW_CFA_restore_state:
              break;
            case DW_CFA_def_cfa_offset:
              LEB ();
              break;
            case DW_CFA_nop:
              break;
            case DW_CFA_def_cfa_expression:
              ul = LEB ();
              start += ul;
              break;
            case DW_CFA_expression:
            case DW_CFA_val_expression:
              LEB ();
              ul = LEB ();
              start += ul;
              break;
            case DW_CFA_offset_extended_sf:
              LEB ();
              l = LEB ();
              start += l;
              break;
            case DW_CFA_val_offset_sf:
              LEB ();
              SLEB ();
              break;
            case DW_CFA_def_cfa_sf:
              LEB ();
              SLEB ();
              break;
            case DW_CFA_def_cfa_offset_sf:
              SLEB ();
              break;
            case DW_CFA_MIPS_advance_loc8:
              start += 8;
              break;
            case DW_CFA_GNU_window_save:
              break;
            case DW_CFA_GNU_args_size:
              LEB ();
              break;
            case DW_CFA_GNU_negative_offset_extended:
              LEB ();
              LEB ();
              break;
            default:
              start = block_end;

            }
        }

      start = block_end;
    }

  return start;
}
#endif

/*Find stack_size for function in debug_frame section*/
static void
find_stack_size_df (bfd * abfd,
                    asection * section, bfd_func_link * abfd_func_link)
{
  unsigned char *start, *section_malloc;
  dwarf_size_type size, osec_size;
  bfd_boolean ret = FALSE;
  unsigned char *end;
  unsigned char *section_start;
  unsigned int length_return;
  debug_frame_entry *chunks = NULL;
  debug_frame_entry *cie = NULL;
  func_hash_bind *current_func = NULL;
  func_hash_bind *last_func = NULL;
  asection *output_section = section->output_section;
  int is_eh = strcmp (section->name, ".eh_frame") == 0;


#if 0
  if (abfd->flags & (EXEC_P | DYNAMIC))
    ret = bfd_get_section_contents (abfd, section, start, 0, size);
  else
    ret = bfd_simple_get_relocated_section_contents (abfd,
          section, start,
          (asymbol **) NULL) !=
          NULL;
#endif
  if (output_section != NULL)
    {
      osec_size = output_section->rawsize > output_section->size ?
                  output_section->rawsize : output_section->size;
      size = section->rawsize > section->size ?
             section->rawsize : section->size;
      section_malloc = (unsigned char *) xmalloc (osec_size);
      /* From the output frame section's begin to the end where,
       * input frame section end locate in the output frame section.
       * Do it these way because the ld will merge the CIE of the
       * input frame sections. */
      start = section_malloc;
      end = start + section->output_offset + size;
      section_start = start;
      ret = bfd_get_section_contents (output_section->owner,
                                      output_section, start, 0, osec_size);
    }
  else
    return;

  if (ret == FALSE)
    {
      /*Can't get contents for section debug_frame */
      printf
      ("Warning: Can't get contents for section debug_frame for callgraph.\n");
      return;
    }

  while (start < end)
    {
      unsigned char *saved_start;
      unsigned char *block_end;
      unsigned long length;
      unsigned long cie_id;
      unsigned char *augmentation_data = NULL;
      unsigned long augmentation_data_len = 0;
      int offset_size;
      int initial_length_size;
      debug_frame_entry *fc = NULL;
      int eh_addr_size = bfd_get_arch_size (abfd) == 64 ? 8 : 4;
      //int encoded_ptr_size = 4;/*eh addr size*/
      int encoded_ptr_size = eh_addr_size;

      saved_start = start;
      length = byte_get (start, 4);
      start += 4;

      if (length == 0)
        continue;
      if (length == 0xffffffff)
        {
          length = byte_get (start, 8);
          start += 8;
          offset_size = 8;
          initial_length_size = 12;
        }
      else
        {
          offset_size = 4;
          initial_length_size = 4;
        }

      block_end = saved_start + length + initial_length_size;
      if (block_end > end)
        block_end = end;

      cie_id = byte_get (start, offset_size);
      start += offset_size;

      if (is_eh ? (cie_id == 0) : (cie_id == DW_CIE_ID))
        {
          int version;

          fc = (debug_frame_entry *) xmalloc (sizeof (debug_frame_entry));
          memset (fc, 0, sizeof (debug_frame_entry));

          fc->next = chunks;
          chunks = fc;
          fc->chunk_start = saved_start;

          version = *start++;
          fc->augmentation = (char *) start;
          start = (unsigned char *) strchr ((char *) start, '\0') + 1;

          if (fc->augmentation[0] == 'z')
            {
              fc->code_factor = LEB ();
              SLEB ();
              if (version == 1)
                start += 1;
              else
                {
                  LEB ();
                }
              augmentation_data_len = LEB ();
              augmentation_data = start;
              start += augmentation_data_len;
            }
          else if (strcmp (fc->augmentation, "eh") == 0)
            {
              start += eh_addr_size;
              LEB ();
              SLEB ();
              if (version == 1)
                start += 1;
              else
                {
                  LEB ();
                }
              //printf ("Warning: Callgraph get unexpected '.eh_frame' section\n");
            }
          else
            {
              fc->code_factor = LEB ();
              SLEB ();
              if (version == 1)
                start += 1;
              else
                {
                  LEB ();
                }
            }
          cie = fc;

          if (augmentation_data_len)
            {
              unsigned char *p, *q;
              p = (unsigned char *) fc->augmentation + 1;
              q = augmentation_data;

              while (1)
                {
                  if (*p == 'L')
                    q++;
                  else if (*p == 'P')
                    q += 1 + size_of_encoded_value (*q);
                  else if (*p == 'R')
                    fc->fde_encoding = *q++;
                  else
                    break;
                  p++;
                }

              if (fc->fde_encoding)
                encoded_ptr_size = size_of_encoded_value (fc->fde_encoding);
            }

          current_func = NULL;
        }
      /*Every FDE represent a function, find function in the abfd_func_link
       * according to function start pc.*/
      else
        {
          unsigned char *look_for;
          static debug_frame_entry fde_fc;

          fc = &fde_fc;
          memset (fc, 0, sizeof (debug_frame_entry));

          look_for = is_eh ? start - 4 - cie_id : section_start + cie_id;

          for (cie = chunks; cie; cie = cie->next)
            if (cie->chunk_start == look_for)
              break;

          if (!cie)
            {
              printf
              ("Warning: Invalid CIE pointer %#08lx in FDE at %#08lx used for callgraph\n",
               cie_id, (unsigned long) (saved_start - section_start));
              cie = fc;
              fc->augmentation = "";
              fc->fde_encoding = 0;
            }
          else
            {
              fc->augmentation = cie->augmentation;
              fc->code_factor = cie->code_factor;
              fc->fde_encoding = cie->fde_encoding;
              fc->cfa_offset = cie->cfa_offset;
            }

          if (fc->fde_encoding)
            encoded_ptr_size = size_of_encoded_value (fc->fde_encoding);

          fc->pc_begin = get_encoded_value (start, fc->fde_encoding);
          if ((fc->fde_encoding & 0x70) == DW_EH_PE_pcrel)
            fc->pc_begin += (start - section_start);
          start += encoded_ptr_size;
          fc->pc_range = byte_get (start, encoded_ptr_size);
          start += encoded_ptr_size;

          if (cie->augmentation[0] == 'z')
            {
              augmentation_data_len = LEB ();
              augmentation_data = start;
              start += augmentation_data_len;
            }
#ifdef CSKY_CALLGRAPH_DEBUG
          printf ("\t\tpc=%08lx..%08lx, ",
                  fc->pc_begin, fc->pc_begin + fc->pc_range);
#endif
          current_func = find_func_with_addr (abfd_func_link,
                                              current_func, fc->pc_begin, 1);
        }

      while (start < block_end)
        {
          unsigned op, opa;
          dwarf_vma vma;
          long ofs, l;
          unsigned long ul;

          op = *start++;
          opa = op & 0x3f;
          if (op & 0xc0)
            op &= 0xc0;

          switch (op)
            {
            case DW_CFA_advance_loc:
              fc->pc_begin += opa * fc->code_factor;
              break;
            case DW_CFA_offset:
              LEB ();
              break;
            case DW_CFA_restore:
              break;
            case DW_CFA_set_loc:
              vma = get_encoded_value (start, fc->fde_encoding);
              if ((fc->fde_encoding & 0x70) == DW_EH_PE_pcrel)
                vma += (start - section_start);
              start += encoded_ptr_size;
              fc->pc_begin = vma;
              break;
            case DW_CFA_advance_loc1:
              ofs = byte_get (start, 1);
              start += 1;
              fc->pc_begin += ofs * fc->code_factor;
              break;
            case DW_CFA_advance_loc2:
              ofs = byte_get (start, 2);
              start += 2;
              fc->pc_begin += ofs * fc->code_factor;
              break;
            case DW_CFA_advance_loc4:
              ofs = byte_get (start, 4);
              start += 4;
              fc->pc_begin += ofs * fc->code_factor;
              break;
            case DW_CFA_offset_extended:
            case DW_CFA_val_offset:
            case DW_CFA_register:
            case DW_CFA_def_cfa:
              LEB ();
              LEB ();
              break;
            case DW_CFA_restore_extended:
            case DW_CFA_undefined:
            case DW_CFA_same_value:
            case DW_CFA_def_cfa_register:
              LEB ();
              break;
            case DW_CFA_remember_state:
              /*FIXME what's the usage */
              break;
            case DW_CFA_restore_state:
              break;
            case DW_CFA_def_cfa_offset:
              /*The DW_CFA_def_cfa_offset represent the stack size of
               * the function the current FDE represented.*/
              fc->cfa_offset = LEB ();
              if (current_func != NULL)
                {
                  if (current_func != last_func)
                    {
                      current_func->hash_entry->stack_size = fc->cfa_offset;
                      last_func = current_func;
                    }
                  /* If the offset belong to the same function,
                   * pick the larger one. */
                  else
                    {
                      if (current_func->hash_entry->stack_size <
                          fc->cfa_offset)
                        current_func->hash_entry->stack_size = fc->cfa_offset;
                      else
                        {
                          /* do nothing */
                        }
                    }
                }
#ifdef CSKY_CALLGRAPH_DEBUG
              if (current_func != NULL)
                printf ("offset:%d, function:%s\n",
                        fc->cfa_offset,
                        current_func->hash_entry->root.string);
              else
                printf ("offset:%d\n", current_func->hash_entry->stack_size);
#endif
              break;
            case DW_CFA_nop:
              break;
            case DW_CFA_def_cfa_expression:
              ul = LEB ();
              start += ul;
              break;
            case DW_CFA_expression:
            case DW_CFA_val_expression:
              LEB ();
              ul = LEB ();
              start += ul;
              break;
            case DW_CFA_offset_extended_sf:
              LEB ();
              l = LEB ();
              start += l;
              break;
            case DW_CFA_val_offset_sf:
              LEB ();
              SLEB ();
              break;
            case DW_CFA_def_cfa_sf:
              LEB ();
              SLEB ();
              break;
            case DW_CFA_def_cfa_offset_sf:
              SLEB ();
              break;
            case DW_CFA_MIPS_advance_loc8:
              ofs = byte_get (start, 8);
              start += 8;
              fc->pc_begin += ofs * fc->code_factor;
              break;
            case DW_CFA_GNU_window_save:
              break;
            case DW_CFA_GNU_args_size:
              LEB ();
              break;
            case DW_CFA_GNU_negative_offset_extended:
              LEB ();
              LEB ();
              break;
            default:
              start = block_end;

            }
        }

      start = block_end;
    }

  free (section_malloc);
}

void
build_func_hash (void)
{
  bfd *abfd;
  asymbol **syms;
  long symcount = 0;
  long count;
  asymbol **sorted_syms;
  long sorted_symcount = 0;

  callgraph_hash_table_init (&func_hash_table,
                             callgraph_hash_newfunc,
                             sizeof (callgraph_hash_entry));

  if (bfd_big_endian (link_info.output_bfd))
    byte_get = byte_get_big_endian;
  else if (bfd_little_endian (link_info.output_bfd))
    byte_get = byte_get_little_endian;

  /*Traversal each input file to build function symbol hash table */
  for (abfd = link_info.input_bfds; abfd != (bfd *) NULL;
       abfd = abfd->link.next)
    {
      bfd_func_link abfd_func_link;

      /*Get all the symbol in the file first, then pink up the
       * symbols represent function name, sort the symbols and
       * store in in sorted_syms.The sorted table will make it
       * easy to find which function BFD_RELOC_CKCORE_CALLGRAPH
       * belong to in csky_elf_relocate_section() later.*/
      syms = slurp_symtab (abfd, &symcount, 1);
      sorted_symcount = get_func_symbols (syms, symcount);
      qsort (syms, sorted_symcount, sizeof (asymbol *), compare_symbols);
      sorted_syms =
        (asymbol **) xmalloc (sorted_symcount * sizeof (asymbol *));
      memcpy (sorted_syms, syms, sorted_symcount * sizeof (asymbol *));

      abfd_func_link.sorted_symcount = sorted_symcount;
      abfd_func_link.abfd = abfd;
      sym_hash_bind_list_init (&abfd_func_link);
      /*Traversal hash table to build bfd_func_link for each bfd,
       * each func_hash_bind is corresponding to a callgraph_hash_entry,
       * if the symbol is local, the name in the hash table will
       * add bfd_id and 'LOC_SYM_SEPARATOR' in the front to diff
       * the local symbol in other file. */
      for (count = 0; count < sorted_symcount; count++)
        {
          func_hash_bind afunc_hash_bind;
          const char *name = sorted_syms[count]->name;
          callgraph_hash_entry *ret = NULL;

          if (sorted_syms[count]->section->output_section == NULL)
            continue;

          if (sorted_syms[count]->flags & BSF_LOCAL)
            {
              char *loc_name = trans_func_name (abfd, name);
              ret = callgraph_hash_lookup (&func_hash_table, loc_name, 1, 1);
              free (loc_name);
            }
          else
            ret = callgraph_hash_lookup (&func_hash_table, name, 1, 1);

          if (ret != NULL)
            {
              ret->section = sorted_syms[count]->section;
              ASSERT (ret->section != NULL);
              ASSERT (ret->section->output_section != NULL);
              ret->start_addr = sorted_syms[count]->value
                                + ret->section->output_offset
                                + ret->section->output_section->vma;
              ret->function_size =
                ((elf_symbol_type *) sorted_syms[count])->internal_elf_sym.
                st_size;

              afunc_hash_bind.hash_entry = ret;
              afunc_hash_bind.next = NULL;
              sym_hash_bind_list_insert (&abfd_func_link, afunc_hash_bind);
            }
        }
      bfd_list_insert (&func_hash_table, abfd_func_link);

      free (syms);
      free (sorted_syms);
    }
  //add_stack_size (&func_hash_table);
}

void
set_func_stack_size (void)
{
  bfd *abfd;
#ifdef CSKY_CALLGRAPH_DEBUG
  int is_found = 0;
  int size = 0;
  printf ("The result of .csky_stack_size or .debug_frame of each file:\n");
#endif
  /*Traversal each input file to build set function stack size. */
  for (abfd = link_info.input_bfds; abfd != (bfd *) NULL;
       abfd = abfd->link.next)
    {
      asection *asec;
      struct bfd_section *needed_debug_frame = NULL;
      bfd_func_link *abfd_func_link = bfd_list_search (func_hash_table, abfd);
      if (abfd_func_link == NULL)
        continue;
      /*Traversal each section in the file to find debug_frame section,
       * find out stack_size for each funtion and store it in
       * callgraph_hash_entry*/
#ifdef CSKY_CALLGRAPH_DEBUG
      is_found = 0;
      printf ("\t%s", abfd->filename);
#endif
      for (asec = abfd->sections; asec!= NULL;
           asec = asec->next)
        {
          const char *name = bfd_get_section_name (abfd, asec);
          if (strcmp (".csky_stack_size", name) == 0)
            {
#ifdef CSKY_CALLGRAPH_DEBUG
              is_found = 1;
              size = bfd_get_section_size (asec);
              printf ("\t.csky_stack_size Found, size:0x%x\n ", size);
#endif
              needed_debug_frame = NULL;
              find_stack_size_ssd (abfd, asec, abfd_func_link);
              break;

            }
          if (strcmp (".debug_frame", name) == 0
              || strcmp (".eh_frame", name) == 0)
            {
#ifdef CSKY_CALLGRAPH_DEBUG
              size = bfd_get_section_size (asec);
#endif
              needed_debug_frame = asec;
            }
        }
      if (needed_debug_frame != NULL)
        {
#ifdef CSKY_CALLGRAPH_DEBUG
          is_found = 1;
          printf ("\t.debug_frame Found, size:0x%x\n ", size);
#endif
          find_stack_size_df (abfd, needed_debug_frame, abfd_func_link);
        }
#ifdef CSKY_CALLGRAPH_DEBUG
      if (is_found == 0)
        printf ("\n");
#endif
    }
}

static char *
trans_func_name (bfd * abfd, const char *func_name)
{
  char *final_name;
  char prefix[20];
  int size;

  ASSERT (abfd != NULL);
  size = sprintf (prefix, "%d%s", abfd->id, LOC_SYM_SEPARATOR);
  size += strlen (func_name);
  size++;
  final_name = (char *) malloc (size * sizeof (char));
  sprintf (final_name, "%s%s", prefix, func_name);

  return final_name;
}

static char *
untrans_func_name (const char *func_name)
{
  char *final_name = NULL;
  char *loc_sym_separator = strstr (func_name, LOC_SYM_SEPARATOR);

  if (loc_sym_separator != NULL)
    {
      final_name = loc_sym_separator + strlen (LOC_SYM_SEPARATOR);
      return final_name;
    }
  else
    return (char *) func_name;

}

static bfd_boolean
find_recursive_route (struct callgraph_hash_entry *h, void *data)
{
  bfd_func_link *func_route_stack = (bfd_func_link *) data;
  func_hash_bind *func_now = NULL;
  func_vec *func = NULL;


  func_now = func_stack_search (func_route_stack, h);
  if (func_now != NULL)
    {
      bfd_func_link new_recuresive_route;

      /*The function has traversed means that the recuresive
       * route has stored, the only different is the head
       * function of route. */
      if (h->has_traversed == TRUE)
        return TRUE;
      else
        {
          sym_hash_bind_list_init (&new_recuresive_route);
          sym_hash_bind_list_copy (func_now, &new_recuresive_route);
          func_link_list_insert (&recursive_func_routes,
                                 new_recuresive_route);
          return TRUE;
        }
    }
  else
    {
      func_hash_bind func_new;

      func_new.hash_entry = h;
      func_new.next = NULL;
      func_stack_push (func_route_stack, func_new);
    }

  func = h->calls;
  while (func != NULL)
    {
      find_recursive_route (func->hash_entry, (void *) func_route_stack);
      func = func->next;
    }
  func_stack_pop (func_route_stack);

  h->has_traversed = TRUE;

  return TRUE;
}

static void
find_recursive_funcs (callgraph_hash_table * func_callgraph)
{
  bfd_func_link func_route_stack;

  reset_has_traversed (func_callgraph);
  func_link_list_init (&recursive_func_routes);
  sym_hash_bind_list_init (&func_route_stack);

  callgraph_hash_traverse (func_callgraph,
                           find_recursive_route, (void *) &func_route_stack);

  return;
}

static bfd_boolean
is_max_in_cycle (struct callgraph_hash_entry *func)
{
  callgraph_hash_entry *route = func->max_depth_route;

  while (route != NULL)
    {
      if (route->max_in_cycle == TRUE)
        return TRUE;
      route = route->max_depth_route;
    }

  return FALSE;
}

static void
calculate_all_max_depth (callgraph_hash_table * func_callgraph)
{
  bfd_func_link func_route_stack;

  reset_has_traversed (func_callgraph);
  sym_hash_bind_list_init (&func_route_stack);

  callgraph_hash_traverse (func_callgraph,
                           calculate_max_depth, (void *) &func_route_stack);
}

/*Find stack_size for function in .csky_stack_size section*/
static void
find_stack_size_ssd (bfd * abfd ATTRIBUTE_UNUSED,
                     asection * section, bfd_func_link * abfd_func_link)
{
  unsigned char *start, *section_malloc;
  dwarf_size_type size;
  bfd_boolean ret = FALSE;
  unsigned char *end;
  func_hash_bind *current_func = NULL;

  size = section->rawsize > section->size ? section->rawsize : section->size;
  section_malloc = (unsigned char *) xmalloc (size);
  start = section_malloc;
  end = start + size;

  if (section->output_section != NULL)
    ret = bfd_get_section_contents (section->output_section->owner,
                                    section->output_section,
                                    start, section->output_offset, size);
  else
    return;

  if (ret == FALSE)
    {
      /*Can't get contents for section csky_stack_size */
      printf
      ("Warning: Can't get contents for section csky_stack_size for callgraph.\n");
      return;
    }

  /* ignore the toolchain version information */
  start += 4;
  while (start < end)
    {
      unsigned long func_addr;
      unsigned int stack_size = 0;

      func_addr = byte_get (start, 4);
      start += 4;
      current_func = find_func_with_addr (abfd_func_link,
                                          current_func, func_addr, 1);
      if (current_func != NULL)
        {
#ifdef CSKY_CALLGRAPH_DEBUG
          printf ("\t\tpc=%08lx..%08lx, ", func_addr,
                  func_addr + current_func->hash_entry->function_size);
#endif
          stack_size = byte_get (start, 4);
          current_func->hash_entry->stack_size = stack_size;
#ifdef CSKY_CALLGRAPH_DEBUG
          printf ("stack size:%d, function:%s\n",
                  stack_size, current_func->hash_entry->root.string);
#endif
        }
      start += 4;
    }

  free (section_malloc);
}

static void
add_stack_size (callgraph_hash_table * func_callgraph)
{
  bfd *abfd;
  asection *asec;
  bfd_func_link *abfd_func_link;

#ifdef CSKY_CALLGRAPH_DEBUG
  int is_found = 0;
  printf ("The result of .csky_stack_size of each file:\n");
#endif
  /*Traversal each input file to find .csky_stack_size section */
  for (abfd_func_link = func_callgraph->bfd_head;
       abfd_func_link != (bfd_func_link *) NULL;
       abfd_func_link = abfd_func_link->next)
    {
      abfd = abfd_func_link->abfd;

#ifdef CSKY_CALLGRAPH_DEBUG
      is_found = 0;
      printf ("\t%s", abfd->filename);
#endif
      for (asec = abfd->sections; asec!= NULL;
           asec = asec->next)
        {
          const char *name = bfd_get_section_name (abfd, asec);
          if (strcmp (".csky_stack_size", name) == 0)
            {
#ifdef CSKY_CALLGRAPH_DEBUG
              is_found = 1;
              int size = 0;
              size = bfd_get_section_size (asec);
              printf ("\tFound, size:0x%x\n ", size);
#endif
              find_stack_size_ssd (abfd, asec, abfd_func_link);
              break;
            }
        }
#ifdef CSKY_CALLGRAPH_DEBUG
      if (is_found == 0)
        printf ("\n");
#endif
    }
}

/* html format dump interface */
static void
dump_html_head (const char *filename)
{
  fprintf (csky_config.callgraph_file,
           "<!DOCTYPE html PUBLIC \"-//w3c//dtd html 4.0 transitional//en\">\n");
  fprintf (csky_config.callgraph_file, "<html>\n");
  fprintf (csky_config.callgraph_file, "<head>\n");
  fprintf (csky_config.callgraph_file,
           "<title>Static Call Graph - [%s]</title>\n", filename);
  fprintf (csky_config.callgraph_file, "</head>\n");
  fprintf (csky_config.callgraph_file, "<body><hr>\n");
  fprintf (csky_config.callgraph_file,
           "<h1>Static Call Graph for image %s</h1><hr>\n", filename);
  fprintf (csky_config.callgraph_file, "<br>\n");
}

static void
dump_html_tail (void)
{
  fprintf (csky_config.callgraph_file, "</body>\n");
  fprintf (csky_config.callgraph_file, "</html>\n");

  return;
}

static void
dump_func_li_html (callgraph_hash_entry * func_entry)
{
  char *func_name = untrans_func_name (func_entry->root.string);

  fprintf (csky_config.callgraph_file,
           "<li><a href=\"#[%x]\">&gt;&gt;</a>&nbsp;&nbsp;&nbsp;%s\n",
           func_entry->id, func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\t%s\n", func_name);
#endif
}

static void
dump_add_ref_html (add_ref * add_ref_elem)
{
  asection *section = add_ref_elem->section;

  ASSERT (section != NULL);
  ASSERT (section->owner != NULL);

  fprintf (csky_config.callgraph_file,
           "<li>%s(%s)\n", section->owner->filename, section->name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\t%s(%s)\n", section->owner->filename, section->name);
#endif
}

static void
dump_max_depth_route_html (callgraph_hash_entry * func_entry,
                           int dump_current_func)
{
  callgraph_hash_entry *route = func_entry->max_depth_route;
  char *func_name = NULL;

  if (dump_current_func == TRUE)
    {
      func_name = untrans_func_name (func_entry->root.string);
      fprintf (csky_config.callgraph_file, "%s ", func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("%s ", func_name);
#endif
      if (route != NULL)
        {
          fprintf (csky_config.callgraph_file, "&rArr; ");
#ifdef CSKY_CALLGRAPH_DEBUG
          printf ("=> ");
#endif
        }
    }

  if (route != NULL)
    {
      func_name = untrans_func_name (route->root.string);
      fprintf (csky_config.callgraph_file, "%s ", func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("%s ", func_name);
#endif

      route = route->max_depth_route;
      while (route != NULL)
        {
          func_name = untrans_func_name (route->root.string);
          fprintf (csky_config.callgraph_file, "&rArr; %s  ", func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
          printf (" => %s", func_name);
#endif
          route = route->max_depth_route;
        }
      if (is_max_in_cycle (func_entry) == TRUE)
        {
          fprintf (csky_config.callgraph_file, " (Cycle)");
#ifdef CSKY_CALLGRAPH_DEBUG
          printf (" (Cycle)");
#endif
        }
      else
        {
          /*Do nothing */
        }
    }
  else
    {
      /*Do nothing */
    }

}

static void
dump_func_detail_html (callgraph_hash_entry * func_entry)
{
  char *func_name = untrans_func_name (func_entry->root.string);
  func_vec *func;
  add_ref_list add_refs;
  add_ref *add_ref_elem;

  ASSERT (func_entry->section != NULL);
  ASSERT (func_entry->section->owner != NULL);

  fprintf (csky_config.callgraph_file,
           "<p><strong><a name=\"[%x]\"></a>%s</strong>(%d bytes, Stack size %d btyes, %s(%s))<br>\n",
           func_entry->id,
           func_name,
           func_entry->function_size,
           func_entry->stack_size,
           func_entry->section->owner->filename, func_entry->section->name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("%s (%d bytes, Stack size %d btyes, %s(%s))\n",
          func_name,
          func_entry->function_size,
          func_entry->stack_size,
          func_entry->section->owner->filename, func_entry->section->name);
#endif

  if (func_entry->max_depth_route != NULL)
    {
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[stack]\n\tMax Depth = %d %s\n\tCall Chain = ",
              func_entry->max_depth,
              (is_max_in_cycle (func_entry) == FALSE) ? "" : "+ In Cycle");
#endif
      fprintf (csky_config.callgraph_file,
               "<br>[Stack]<ul><li>Max Depth = %d %s\n<li>Call Chain = ",
               func_entry->max_depth,
               (is_max_in_cycle (func_entry) == FALSE) ? "" : "+ In Cycle");
      dump_max_depth_route_html (func_entry, TRUE);
      fprintf (csky_config.callgraph_file, "</ul>\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("\n");
#endif
    }
  else
    {
      /*Do nothing */
    }

  func = func_entry->calls;
  if (func != NULL)
    {
      fprintf (csky_config.callgraph_file, "<br>[Calls]<ul>\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[Calls]\n");
#endif
      while (func != NULL)
        {
          dump_func_li_html (func->hash_entry);
          func = func->next;
        }
      fprintf (csky_config.callgraph_file, "</ul>\n");
    }
  else
    {
      /*Do nothing */
    }

  func = func_entry->call_bys;
  if (func != NULL)
    {
      fprintf (csky_config.callgraph_file, "<br>[Called By]<ul>\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[Called By]\n");
#endif
      while (func != NULL)
        {
          dump_func_li_html (func->hash_entry);
          func = func->next;
        }
      fprintf (csky_config.callgraph_file, "</ul>\n");
    }
  else
    {
      /*Do nothing */
    }

  add_refs = func_entry->add_refs;
  if (add_refs.ref_count != 0)
    {
      fprintf (csky_config.callgraph_file,
               "<br>[Address Reference Count : %d]<ul>\n",
               add_refs.ref_count);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[Address Reference Count : %d]\n", add_refs.ref_count);
#endif
      add_ref_elem = add_refs.head;
      while (add_ref_elem != NULL)
        {
          dump_add_ref_html (add_ref_elem);
          add_ref_elem = (add_ref *)add_ref_elem->next;
        }
      fprintf (csky_config.callgraph_file, "</ul>\n");
    }
  else
    {
      /*Do nothing */
    }
}

static void
dump_global_symbols_html (void)
{
  bfd_func_link *p_bfd;
  func_hash_bind *p_sym;

  fprintf (csky_config.callgraph_file, "<h3>Global Symbols</h3>\n");
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\n\n\nGlobal Symbols\n");
#endif

  for (p_bfd = func_hash_table.bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          if (strstr (p_sym->hash_entry->root.string, LOC_SYM_SEPARATOR) ==
              NULL)
            dump_func_detail_html (p_sym->hash_entry);
          else
            {
              /*Do nothing */
            }
        }
    }

  return;
}

static void
dump_local_symbols_html (void)
{
  bfd_func_link *p_bfd;
  func_hash_bind *p_sym;

  fprintf (csky_config.callgraph_file, "<h3>Local Symbols</h3>\n");

  for (p_bfd = func_hash_table.bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          if (strstr (p_sym->hash_entry->root.string, LOC_SYM_SEPARATOR) !=
              NULL)
            dump_func_detail_html (p_sym->hash_entry);
          else
            {
              /*Do nothing */
            }
        }
    }

  return;
}

static void
dump_func_referenced_html (callgraph_hash_entry * func_entry,
                           asection * referenced_section)
{
  char *func_name = untrans_func_name (func_entry->root.string);

  ASSERT (func_entry->section != NULL);
  ASSERT (func_entry->section->owner != NULL);
  ASSERT (referenced_section != NULL);
  ASSERT (referenced_section->owner != NULL);

  fprintf (csky_config.callgraph_file,
           "<li><a href=\"#[%x]\">%s</a> from %s(%s) referenced from %s(%s)\n",
           func_entry->id,
           func_name,
           func_entry->section->owner->filename,
           func_entry->section->name,
           referenced_section->owner->filename, referenced_section->name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("%s from %s(%s) referenced from %s(%s)\n",
          func_name,
          func_entry->section->owner->filename,
          func_entry->section->name,
          referenced_section->owner->filename, referenced_section->name);
#endif

  return;
}

static void
dump_func_pointer_html (void)
{
  bfd_func_link *p_bfd;
  func_hash_bind *p_sym;
  func_vec *referenced = NULL;
  add_ref_list add_refs;

  fprintf (csky_config.callgraph_file, "<h3>Function Pointers</h3><ul>\n");
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\n\n\nFunction Pointers\n");
#endif

  for (p_bfd = func_hash_table.bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          referenced = p_sym->hash_entry->call_bys;
          add_refs = p_sym->hash_entry->add_refs;
          if (referenced != NULL)
            dump_func_referenced_html (p_sym->hash_entry,
                                       referenced->hash_entry->section);
          else if (add_refs.ref_count != 0)
            {
              ASSERT (add_refs.head != NULL);
              dump_func_referenced_html (p_sym->hash_entry,
                                         add_refs.head->section);
            }
          else
            {
              /*Do nothing */
            }
        }
    }

  fprintf (csky_config.callgraph_file, "</ul>\n");
}


static void
dump_recursive_route_html (bfd_func_link * recursive_route)
{
  func_hash_bind *route = recursive_route->func_head;
  char *func_name = NULL;

  if (route != NULL)
    {
      func_name = untrans_func_name (route->hash_entry->root.string);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("%s", func_name);
#endif
      fprintf (csky_config.callgraph_file,
               "<li><a href=\"#[%x]\">%s</a>",
               route->hash_entry->id, func_name);

      route = route->next;
      while (route != NULL)
        {
          func_name = untrans_func_name (route->hash_entry->root.string);
#ifdef CSKY_CALLGRAPH_DEBUG
          printf (" => %s", func_name);
#endif
          fprintf (csky_config.callgraph_file,
                   "&nbsp;&nbsp;&nbsp;&rArr;&nbsp;&nbsp;&nbsp;<a href=\"#[%x]\">%s</a>",
                   route->hash_entry->id, func_name);
          route = route->next;
        }
      route = recursive_route->func_head;
      func_name = untrans_func_name (route->hash_entry->root.string);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf (" => %s", func_name);
      printf ("\n");
#endif
      fprintf (csky_config.callgraph_file,
               "&nbsp;&nbsp;&nbsp;&rArr;&nbsp;&nbsp;&nbsp;<a href=\"#[%x]\">%s</a><br>",
               route->hash_entry->id, func_name);
    }
  else
    {
      /*Do nothing */
    }

  return;
}

static void
dump_recursive_funcs_html (void)
{
  bfd_func_link *recursive_route = NULL;

#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\n\n\nMutually Recursive functions\n");
#endif
  fprintf (csky_config.callgraph_file,
           "<h3>Mutually Recursive functions</h3><ul>\n");

  recursive_route = recursive_func_routes.head;
  while (recursive_route != NULL)
    {
      dump_recursive_route_html (recursive_route);
      recursive_route = recursive_route->next;
    }

  fprintf (csky_config.callgraph_file, "</ul>");

}

static void
dump_max_stack_func_html (callgraph_hash_table * func_callgraph)
{
  bfd_func_link *p_bfd = NULL;
  func_hash_bind *p_sym = NULL;
  callgraph_hash_entry *max_stack_func = NULL;
  int max_stack_size = 0;

  for (p_bfd = func_callgraph->bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          if (p_sym->hash_entry->max_depth > max_stack_size
              || max_stack_func == NULL)
            {
              max_stack_size = p_sym->hash_entry->max_depth;
              max_stack_func = p_sym->hash_entry;
            }
          else
            {
              /* do nothing */
            }
        }
    }

  if (max_stack_func != NULL)
    {
      fprintf (csky_config.callgraph_file,
               "<h3>Maximum Stack Usage = %d bytes + Unknown(Cycles, Untraceable Function Pointers)</h3>\n",
               max_stack_size);
      fprintf (csky_config.callgraph_file,
               "<h3>Call Chain for Maximum Stack Depth:</h3>\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf
      ("\n\n\nMaximum Stack Usage = %d bytes + Unknown(Cycles, Untraceable Function Pointers)\n",
       max_stack_size);
      printf ("Call Chain for Maximum Stack Depth:\n");
#endif
      dump_max_depth_route_html (max_stack_func, TRUE);
    }
  else
    {
      /*Do nothing */
    }

  fprintf (csky_config.callgraph_file, "</ul>\n");

  return;
}

static void
dump_ld_version_html (void)
{
  fprintf (csky_config.callgraph_file,
           "<p>#&#060CALLGRAPH&#062# Csky GNU Linker<br>\n");
}

/* end of html format dump interface */

/* text format dump interface */
static void
dump_text_head (const char *filename)
{
  fprintf (csky_config.callgraph_file, "\nStatic Call Graph for image %s\n",
           filename);
}

static void
dump_text_tail (void)
{
  return;
}

static void
dump_ld_version_text (void)
{
  fprintf (csky_config.callgraph_file, "#<CALLGRAPH># Csky GNU Linker\n");
}

static void
dump_max_depth_route_text (callgraph_hash_entry * func_entry,
                           int dump_current_func)
{
  callgraph_hash_entry *route = func_entry->max_depth_route;
  char *func_name = NULL;

  if (dump_current_func == TRUE)
    {
      func_name = untrans_func_name (func_entry->root.string);
      fprintf (csky_config.callgraph_file, "%s ", func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("%s ", func_name);
#endif
      if (route != NULL)
        {
          fprintf (csky_config.callgraph_file, "=> ");
#ifdef CSKY_CALLGRAPH_DEBUG
          printf ("=> ");
#endif
        }
    }

  if (route != NULL)
    {
      func_name = untrans_func_name (route->root.string);
      fprintf (csky_config.callgraph_file, "%s ", func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("%s ", func_name);
#endif

      route = route->max_depth_route;
      while (route != NULL)
        {
          func_name = untrans_func_name (route->root.string);
          fprintf (csky_config.callgraph_file, "=> %s  ", func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
          printf (" => %s", func_name);
#endif
          route = route->max_depth_route;
        }
      if (is_max_in_cycle (func_entry) == TRUE)
        {
          fprintf (csky_config.callgraph_file, " (Cycle)");
#ifdef CSKY_CALLGRAPH_DEBUG
          printf (" (Cycle)");
#endif
        }
      else
        {
          /*Do nothing */
        }
    }
  else
    {
      /*Do nothing */
    }

}

static void
dump_max_stack_func_text (callgraph_hash_table * func_callgraph)
{
  bfd_func_link *p_bfd = NULL;
  func_hash_bind *p_sym = NULL;
  callgraph_hash_entry *max_stack_func = NULL;
  int max_stack_size = 0;

  for (p_bfd = func_callgraph->bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          if (p_sym->hash_entry->max_depth > max_stack_size
              || max_stack_func == NULL)
            {
              max_stack_size = p_sym->hash_entry->max_depth;
              max_stack_func = p_sym->hash_entry;
            }
          else
            {
              /* do nothing */
            }
        }
    }

  if (max_stack_func != NULL)
    {
      fprintf (csky_config.callgraph_file,
               "\nMaximum Stack Usage = %d bytes + Unknown(Cycles, Untraceable Function Pointers)\n",
               max_stack_size);
      fprintf (csky_config.callgraph_file,
               "Call Chain for Maximum Stack Depth:\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf
      ("\n\n\nMaximum Stack Usage = %d bytes + Unknown(Cycles, Untraceable Function Pointers)\n",
       max_stack_size);
      printf ("Call Chain for Maximum Stack Depth:\n");
#endif
      dump_max_depth_route_text (max_stack_func, TRUE);
    }
  else
    {
      /*Do nothing */
    }

  fprintf (csky_config.callgraph_file, "\n\n\n");

  return;
}

static void
dump_recursive_route_text (bfd_func_link * recursive_route)
{
  func_hash_bind *route = recursive_route->func_head;
  char *func_name = NULL;

  if (route != NULL)
    {
      func_name = untrans_func_name (route->hash_entry->root.string);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("%s", func_name);
#endif
      fprintf (csky_config.callgraph_file, "%s", func_name);

      route = route->next;
      while (route != NULL)
        {
          func_name = untrans_func_name (route->hash_entry->root.string);
#ifdef CSKY_CALLGRAPH_DEBUG
          printf (" => %s", func_name);
#endif
          fprintf (csky_config.callgraph_file, " => %s", func_name);
          route = route->next;
        }
      route = recursive_route->func_head;
      func_name = untrans_func_name (route->hash_entry->root.string);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf (" => %s", func_name);
      printf ("\n");
#endif
      fprintf (csky_config.callgraph_file, " => %s\n", func_name);
    }
  else
    {
      /*Do nothing */
    }

  return;
}

static void
dump_recursive_funcs_text (void)
{
  bfd_func_link *recursive_route = NULL;

#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\n\n\nMutually Recursive functions\n");
#endif
  fprintf (csky_config.callgraph_file, "Mutually Recursive functions\n");

  recursive_route = recursive_func_routes.head;
  while (recursive_route != NULL)
    {
      dump_recursive_route_text (recursive_route);
      recursive_route = recursive_route->next;
    }

  fprintf (csky_config.callgraph_file, "\n");

}

static void
dump_func_referenced_text (callgraph_hash_entry * func_entry,
                           asection * referenced_section)
{
  char *func_name = untrans_func_name (func_entry->root.string);

  ASSERT (func_entry->section != NULL);
  ASSERT (func_entry->section->owner != NULL);
  ASSERT (referenced_section != NULL);
  ASSERT (referenced_section->owner != NULL);

  fprintf (csky_config.callgraph_file,
           "%s from %s(%s) referenced from %s(%s)\n",
           func_name,
           func_entry->section->owner->filename,
           func_entry->section->name,
           referenced_section->owner->filename, referenced_section->name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("%s from %s(%s) referenced from %s(%s)\n",
          func_name,
          func_entry->section->owner->filename,
          func_entry->section->name,
          referenced_section->owner->filename, referenced_section->name);
#endif

  return;
}

static void
dump_func_pointer_text (void)
{
  bfd_func_link *p_bfd;
  func_hash_bind *p_sym;
  func_vec *referenced = NULL;
  add_ref_list add_refs;

  fprintf (csky_config.callgraph_file, "\n\n\nFunction Pointers\n");
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\n\n\nFunction Pointers\n");
#endif

  for (p_bfd = func_hash_table.bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          referenced = p_sym->hash_entry->call_bys;
          add_refs = p_sym->hash_entry->add_refs;
          if (referenced != NULL)
            dump_func_referenced_text (p_sym->hash_entry,
                                       referenced->hash_entry->section);
          else if (add_refs.ref_count != 0)
            {
              ASSERT (add_refs.head != NULL);
              dump_func_referenced_text (p_sym->hash_entry,
                                         add_refs.head->section);
            }
          else
            {
              /*Do nothing */
            }
        }
    }

  fprintf (csky_config.callgraph_file, "\n");
}

static void
dump_func_li_text (callgraph_hash_entry * func_entry)
{
  char *func_name = untrans_func_name (func_entry->root.string);

  fprintf (csky_config.callgraph_file, "\t%s\n", func_name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\t%s\n", func_name);
#endif
}

static void
dump_add_ref_text (add_ref * add_ref_elem)
{
  asection *section = add_ref_elem->section;

  ASSERT (section != NULL);
  ASSERT (section->owner != NULL);

  fprintf (csky_config.callgraph_file, "\t%s(%s)\n",
           section->owner->filename, section->name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\t%s(%s)\n", section->owner->filename, section->name);
#endif
}

static void
dump_func_detail_text (callgraph_hash_entry * func_entry)
{
  char *func_name = untrans_func_name (func_entry->root.string);
  func_vec *func;
  add_ref_list add_refs;
  add_ref *add_ref_elem;

  ASSERT (func_entry->section != NULL);
  ASSERT (func_entry->section->owner != NULL);

  fprintf (csky_config.callgraph_file,
           "%s (%d bytes, Stack size %d btyes, %s(%s))\n",
           func_name,
           func_entry->function_size,
           func_entry->stack_size,
           func_entry->section->owner->filename, func_entry->section->name);
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("%s (%d bytes, Stack size %d btyes, %s(%s))\n",
          func_name,
          func_entry->function_size,
          func_entry->stack_size,
          func_entry->section->owner->filename, func_entry->section->name);
#endif

  if (func_entry->max_depth_route != NULL)
    {
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[stack]\n\tMax Depth = %d %s\n\tCall Chain = ",
              func_entry->max_depth,
              (is_max_in_cycle (func_entry) == FALSE) ? "" : "+ In Cycle");
#endif
      fprintf (csky_config.callgraph_file,
               "[stack]\n\tMax Depth = %d %s\n\tCall Chain = ",
               func_entry->max_depth,
               (is_max_in_cycle (func_entry) == FALSE) ? "" : "+ In Cycle");
      dump_max_depth_route_text (func_entry, TRUE);
      fprintf (csky_config.callgraph_file, "\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("\n");
#endif
    }
  else
    {
      /*Do nothing */
    }

  func = func_entry->calls;
  if (func != NULL)
    {
      fprintf (csky_config.callgraph_file, "[Calls]\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[Calls]\n");
#endif
      while (func != NULL)
        {
          dump_func_li_text (func->hash_entry);
          func = func->next;
        }
      fprintf (csky_config.callgraph_file, "\n");
    }
  else
    {
      /*Do nothing */
    }

  func = func_entry->call_bys;
  if (func != NULL)
    {
      fprintf (csky_config.callgraph_file, "[Called By]\n");
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[Called By]\n");
#endif
      while (func != NULL)
        {
          dump_func_li_text (func->hash_entry);
          func = func->next;
        }
      fprintf (csky_config.callgraph_file, "\n");
    }
  else
    {
      /*Do nothing */
    }

  add_refs = func_entry->add_refs;
  if (add_refs.ref_count != 0)
    {
      fprintf (csky_config.callgraph_file,
               "[Address Reference Count : %d]\n", add_refs.ref_count);
#ifdef CSKY_CALLGRAPH_DEBUG
      printf ("[Address Reference Count : %d]\n", add_refs.ref_count);
#endif
      add_ref_elem = add_refs.head;
      while (add_ref_elem != NULL)
        {
          dump_add_ref_text (add_ref_elem);
          add_ref_elem = (add_ref *)add_ref_elem->next;
        }
      fprintf (csky_config.callgraph_file, "\n");
    }
  else
    {
      /*Do nothing */
    }
}

static void
dump_global_symbols_text (void)
{
  bfd_func_link *p_bfd;
  func_hash_bind *p_sym;

  fprintf (csky_config.callgraph_file, "\n\n\nGlobal Symbols\n");
#ifdef CSKY_CALLGRAPH_DEBUG
  printf ("\n\n\nGlobal Symbols\n");
#endif

  for (p_bfd = func_hash_table.bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          if (strstr (p_sym->hash_entry->root.string, LOC_SYM_SEPARATOR) ==
              NULL)
            dump_func_detail_text (p_sym->hash_entry);
          else
            {
              /*Do nothing */
            }
        }
    }

  return;
}

static void
dump_local_symbols_text (void)
{
  bfd_func_link *p_bfd;
  func_hash_bind *p_sym;

  fprintf (csky_config.callgraph_file, "\n\n\nLocal Symbols\n");

  for (p_bfd = func_hash_table.bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          if (strstr (p_sym->hash_entry->root.string, LOC_SYM_SEPARATOR) !=
              NULL)
            dump_func_detail_text (p_sym->hash_entry);
          else
            {
              /*Do nothing */
            }
        }
    }

  return;
}

/* end of text format dump interface */

void
call_graph_free (void)
{
  func_link_list_clean (&recursive_func_routes);
  callgraph_hash_table_clean (&func_hash_table);
}

void
dump_callgraph (void)
{
  if (csky_config.callgraph_fmt == html)
    {
      current_callgraph_target.dump_file_head = dump_html_head;
      current_callgraph_target.dump_ld_version = dump_ld_version_html;
      current_callgraph_target.dump_max_stack_func = dump_max_stack_func_html;
      current_callgraph_target.dump_recursive_funcs =
        dump_recursive_funcs_html;
      current_callgraph_target.dump_func_pointer = dump_func_pointer_html;
      current_callgraph_target.dump_global_symbols = dump_global_symbols_html;
      current_callgraph_target.dump_local_symbols = dump_local_symbols_html;
      current_callgraph_target.dump_file_tail = dump_html_tail;
    }
  else
    {
      current_callgraph_target.dump_file_head = dump_text_head;
      current_callgraph_target.dump_ld_version = dump_ld_version_text;
      current_callgraph_target.dump_max_stack_func = dump_max_stack_func_text;
      current_callgraph_target.dump_recursive_funcs =
        dump_recursive_funcs_text;
      current_callgraph_target.dump_func_pointer = dump_func_pointer_text;
      current_callgraph_target.dump_global_symbols = dump_global_symbols_text;
      current_callgraph_target.dump_local_symbols = dump_local_symbols_text;
      current_callgraph_target.dump_file_tail = dump_text_tail;
    }

  find_recursive_funcs (&func_hash_table);
  calculate_all_max_depth (&func_hash_table);

  current_callgraph_target.dump_file_head (link_info.output_bfd->filename);
  current_callgraph_target.dump_ld_version ();
  current_callgraph_target.dump_max_stack_func (&func_hash_table);
  current_callgraph_target.dump_recursive_funcs ();
  current_callgraph_target.dump_func_pointer ();
  current_callgraph_target.dump_global_symbols ();
  current_callgraph_target.dump_local_symbols ();
  current_callgraph_target.dump_file_tail ();
}
