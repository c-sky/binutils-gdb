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
#include "math.h"
#include "ld_csky.h"

#define OUT_DIVID_LENGTH 70
#define IN_DIVID_LENGTH 60

#define LOC_SYM_SEPARATOR "#cskycg#"

/*Use for check if achive file has been printed*/
ld_csky_config_type csky_config;


static lang_memory_region_type *lang_memory_region_list;
static lang_statement_list_type statement_list;
static bfd_extra_list bfdextra_list;
static archive_printed_list archprinted_list;
static bfd *current_statement_bfd = NULL;

static void
print_memmap_statement (lang_statement_union_type * s,
                        lang_output_section_statement_type * os,
                        lang_memory_region_type * current);
static void
print_memmap_statement_list (lang_statement_union_type * s,
                             lang_output_section_statement_type * os,
                             lang_memory_region_type * current);
static void
print_memmap_padding_statement (lang_padding_statement_type * s,
                                lang_memory_region_type * current);
static void object_component_size_reset (object_component_size * current);
static void
build_bfdextra_table_statement_list (lang_statement_union_type * s,
                                     lang_output_section_statement_type * os);
static void
build_bfdextra_table_statement (lang_statement_union_type * s,
                                lang_output_section_statement_type * os);
static void
build_bfdextra_table_padding_statement (lang_padding_statement_type * s);
static void
build_bfdextra_table_data_statement (lang_data_statement_type * data);
static void bfd_extra_list_init (bfd_extra_list * list);
static void bfd_extra_list_insert (bfd_extra_list * list, bfd_extra elem);
static void bfd_extra_list_del (bfd_extra_list * list, bfd_extra * elem);
static void bfd_extra_list_clean (bfd_extra_list * list);
static void archive_printed_list_init (archive_printed_list * list);
static void
archive_printed_list_insert (archive_printed_list * list,
                             archive_printed elem);
static void archive_printed_list_clean (archive_printed_list * list);
static int
check_archive_has_printed (archive_printed_list * list, bfd * archive);
static object_component_size
find_bfd_extra (bfd_extra_list * list, unsigned int bfd_id);
static void
object_component_size_add (object_component_size * current,
                           object_component_size adden);
static void build_bfdextra_table_statements (void);
static object_component_size print_object_component_sizes (bfd * input_bfd);
static void print_dividing_line (FILE * fp, char *elem, unsigned int length);
asymbol **slurp_symtab (bfd * abfd, long *symcount, int check_flags);

lang_memory_region_type *
csky_get_lang_memory_region (void);

static void
bfd_fatal (const char *string)
{
  const char *errmsg = bfd_errmsg (bfd_get_error ());

  if (string)
    fprintf (stderr, "%s: %s \n", string, errmsg);
  else
    fprintf (stderr, "%s \n", errmsg);
}

/*check_flasg: 0 ---- if the bfd's flags HAS_SYMS is not set,
 *                    get the symbol table however
 *             1 ---- if the bfd's flags HAS_SYMS is not set,
 *                    return NULL */
asymbol **
slurp_symtab (bfd * abfd, long *symcount, int check_flags)
{
  long storage;
  asymbol **sy = NULL;

  if (!(bfd_get_file_flags (abfd) & HAS_SYMS) && (check_flags != 0))
    {
      *symcount = 0;
      return NULL;
    }

  storage = bfd_get_symtab_upper_bound (abfd);
  if (storage < 0)
    bfd_fatal (bfd_get_filename (abfd));
  if (storage)
    sy = (asymbol **) xmalloc (storage);

  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  *symcount = bed->s->slurp_symbol_table (abfd, sy, FALSE);
  if (*symcount < 0)
    bfd_fatal (bfd_get_filename (abfd));

  return sy;
}

/*Output length*elem int one line at the fp to perform as dividing line*/
static void
print_dividing_line (FILE * fp, char *elem, unsigned int length)
{
  unsigned int i;
  for (i = 0; i < length; i++)
    {
      fprintf (fp, "%s", _(elem));
    }
  fprintf (fp, _("\n"));
}

static void
dump_func_referenced (callgraph_hash_entry * func_entry,
                      callgraph_hash_entry * referenced)
{
  const char *func_name = func_entry->root.string;

  ASSERT (func_entry->section != NULL);
  ASSERT (func_entry->section->owner != NULL);
  ASSERT (referenced->section != NULL);
  ASSERT (referenced->section->owner != NULL);

  fprintf (csky_config.cskymap_file,
           "\t%s(%s) refers to %s(%s) for %s\n",
           referenced->section->owner->filename,
           referenced->section->name,
           func_entry->section->owner->filename,
           func_entry->section->name, func_name);

  return;
}

static void
print_section_cross_references (void)
{
  bfd_func_link *p_bfd;
  func_hash_bind *p_sym;
  func_vec *referenced = NULL;

  fprintf (csky_config.cskymap_file, _("\nSection Cross References\n\n"));

  for (p_bfd = func_hash_table.bfd_head; p_bfd != (bfd_func_link *) NULL;
       p_bfd = p_bfd->next)
    {
      for (p_sym = p_bfd->func_head; p_sym != (func_hash_bind *) NULL;
           p_sym = p_sym->next)
        {
          if (strstr (p_sym->hash_entry->root.string, LOC_SYM_SEPARATOR) ==
              NULL)
            {
              referenced = p_sym->hash_entry->call_bys;
              while (referenced != NULL)
                {
                  dump_func_referenced (p_sym->hash_entry,
                                        referenced->hash_entry);
                  referenced = referenced->next;
                }
            }
          else
            {
              /*Do nothing */
            }
        }
    }

  fprintf (csky_config.cskymap_file, "\n\n");
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);
}

static void
print_discard_sections (void)
{
  int remove_section_count = 0;
  int remove_bytes = 0;

  fprintf (csky_config.cskymap_file,
           _("\nRemoving Unused input sections from the image. \n\n"));

  /*Traversal each section, judge the section is whether removed,
   * if it is, output the information;if not, do nothing. */
  LANG_FOR_EACH_INPUT_STATEMENT (file)
  {
    asection *s;

    if ((file->the_bfd->flags & (BFD_LINKER_CREATED | DYNAMIC)) != 0
        || file->flags.just_syms)
      continue;

    for (s = file->the_bfd->sections; s != NULL; s = s->next)
      if ((s->output_section == NULL
           || s->output_section->owner != link_info.output_bfd)
          && (s->flags & (SEC_LINKER_CREATED | SEC_KEEP)) == 0)
        {
          fprintf (csky_config.cskymap_file,
                   "\tRemoving %s(%s), (%d bytes).\n", _(s->name),
                   _(s->owner->filename), (int) s->size);
          remove_section_count++;
          remove_bytes += s->size;
        }
  }

  fprintf (csky_config.cskymap_file,
           "\n%d unused seciton(s) (total %d bytes) removed from the image.\n\n",
           remove_section_count, remove_bytes);
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);
}

static void
print_sym_info (asymbol * sym)
{
  const char *name = sym->name;
  flagword type = sym->flags;
  bfd_vma val;

  fprintf (csky_config.cskymap_file, "\t%-35s", name);
  if (sym->section != NULL)
    fprintf (csky_config.cskymap_file, "0x%08x",
             (unsigned int) (sym->value + sym->section->vma));
  else
    fprintf (csky_config.cskymap_file, "0x%08x", (unsigned int) sym->value);

  fprintf (csky_config.cskymap_file, "%5c%c%c",
           (type & BSF_WEAK) ? 'w' : ' ',
           (type & BSF_DEBUGGING) ? 'd' : ' ',
           ((type & BSF_FUNCTION) ? 'F' :
            ((type & BSF_FILE) ? 'f' : ((type & BSF_OBJECT) ? 'O' : ' '))));

  if (sym->section && bfd_is_com_section (sym->section))
    val = ((elf_symbol_type *) sym)->internal_elf_sym.st_value;
  else
    val = ((elf_symbol_type *) sym)->internal_elf_sym.st_size;

  fprintf (csky_config.cskymap_file, "%9d", (int) val);
  if (sym->section)
    fprintf (csky_config.cskymap_file, "  %s", sym->section->name);
  else
    {
      /*Do nothing */
    }
  fprintf (csky_config.cskymap_file, "\n");
}

static void
print_type_info (void)
{
  fprintf (csky_config.cskymap_file,
           "\n\t(w:Weak    d:Deubg    F:Function    f:File name    O:Zero)\n");
}

static void
print_image_symbol_table (void)
{
  asymbol **syms, **sorted_syms;
  long symcount = 0;
  int i = 0;

  syms = slurp_symtab (link_info.output_bfd, &symcount, 0);
  qsort (syms, symcount, sizeof (asymbol *), compare_symbols);
  sorted_syms = (asymbol **) xmalloc (symcount * sizeof (asymbol *));
  memcpy (sorted_syms, syms, symcount * sizeof (asymbol *));

  fprintf (csky_config.cskymap_file, _("\nImage Symbol Table \n\n"));

  fprintf (csky_config.cskymap_file, "\tLocal Symbols\n\n");
  fprintf (csky_config.cskymap_file, "\t%-35s%-13s%-5s%6s%9s\n",
           "Symbol Name", "Value", "Type", "Size", "Section");

  for (i = 0; i < symcount; i++)
    {
      const char *name = sorted_syms[i]->name;
      flagword type = sorted_syms[i]->flags;

      if (name != NULL && *name != '\0' && !strstr (name, "(null)"))
        {
          if (type & BSF_LOCAL)
            {
              print_sym_info (sorted_syms[i]);
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

  fprintf (csky_config.cskymap_file, "\n\tGlobal Symbols\n\n");
  fprintf (csky_config.cskymap_file, "\t%-35s%-13s%-5s%6s%9s\n",
           "Symbol Name", "Value", "Type", "Size", "Section");

  for (i = 0; i < symcount; i++)
    {
      const char *name = sorted_syms[i]->name;
      flagword type = sorted_syms[i]->flags;

      if (name != NULL && *name != '\0')
        {
          if ((type & BSF_GLOBAL) || (type & BSF_WEAK))
            {
              print_sym_info (sorted_syms[i]);
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

  print_type_info ();

  fprintf (csky_config.cskymap_file, "\n\n");
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);

  free (syms);
  free (sorted_syms);
}

static void
print_memmap_input_section (asection * i, lang_memory_region_type * current)
{
  bfd_vma addr;

  if (i->output_section == NULL
      || i->output_section->owner != link_info.output_bfd
      || strstr (i->output_section->name, ".comment")
      || ((i->flags & SEC_ALLOC) == 0) || ((i->flags & SEC_EXCLUDE) != 0))
    return;
  else
    addr = i->output_section->vma + i->output_offset;

  if (addr > current->current
      || (addr == current->current && i->size != 0) || addr < current->origin)
    return;

  char *type = NULL;
  char *attr = NULL;

  if ((i->flags & SEC_CODE) != 0)
    type = "Code";
  else if ((i->flags & SEC_DATA) != 0)
    type = "Data";
  /* .bss's attribute:SHT_NOBITS  SHF_ALLOC+SHF_WRITE,
     it is same as SEC_ALLOC, see in elf.c in function
     '_bfd_elf_make_section_from_shdr' */
  else if (i->flags == SEC_ALLOC)
    type = "Zero";
  else
    return;

  if ((i->flags & SEC_READONLY) != 0)
    attr = "RO";
  else
    attr = "RW";

  fprintf (csky_config.cskymap_file,
           _("    0x%08x   0x%08x   %-7s%s%11d  %-15s\t%s\n"),
           (unsigned int) addr, (unsigned int) i->size, type, attr,
           (int) i->id, i->name, i->owner->filename);

}

static void
print_memmap_data_statement (lang_data_statement_type * data,
                             lang_memory_region_type * current)
{
  bfd_vma addr;
  char *attr;
  bfd_size_type size;

  addr = data->output_offset;
  if (data->output_section == NULL
      || ((data->output_section->flags & SEC_ALLOC) == 0))
    return;
  else
    addr += data->output_section->vma;

  switch (data->type)
    {
    default:
      abort ();
    case BYTE:
      size = BYTE_SIZE;
      break;
    case SHORT:
      size = SHORT_SIZE;
      break;
    case LONG:
      size = LONG_SIZE;
      break;
    case QUAD:
      size = QUAD_SIZE;
      break;
    case SQUAD:
      size = QUAD_SIZE;
      break;
    }

  if (addr > current->current
      || (addr == current->current && size != 0) || addr < current->origin)
    return;


  if ((data->output_section->flags & SEC_READONLY) != 0)
    attr = "RO";
  else
    attr = "RW";

  fprintf (csky_config.cskymap_file, _("    0x%08x   0x%08x   LD_GEN %s\n"),
           (unsigned int) addr, (unsigned int) size, attr);
}

static void
print_memmap_padding_statement (lang_padding_statement_type * s,
                                lang_memory_region_type * current)
{
  bfd_vma addr;

  addr = s->output_offset;
  if (s->output_section == NULL
      || ((s->output_section->flags & SEC_ALLOC) == 0))
    return;
  else
    addr += s->output_section->vma;

  if (addr > current->current
      || (addr == current->current && s->size != 0) || addr < current->origin)
    return;

  fprintf (csky_config.cskymap_file, _("    0x%08x   0x%08x   PAD\n"),
           (unsigned int) addr, (unsigned int) s->size);

}

static void
print_memmap_statement (lang_statement_union_type * s,
                        lang_output_section_statement_type * os,
                        lang_memory_region_type * current)
{
  switch (s->header.type)
    {
    default:
      fprintf (csky_config.cskymap_file, _("Fail with %d\n"), s->header.type);
      FAIL ();
      break;
    case lang_address_statement_enum:
    case lang_object_symbols_statement_enum:
    case lang_fill_statement_enum:
    case lang_reloc_statement_enum:
    case lang_assignment_statement_enum:
    case lang_target_statement_enum:
    case lang_output_statement_enum:
    case lang_input_statement_enum:
    case lang_insert_statement_enum:
      break;
#if ENABLE_ANY
    case lang_any_statement_enum:
    {
      lang_wild_statement_type *any_wildcard =
        &s->any_statement.children.head->wild_statement;

      if (any_wildcard != NULL)
        print_memmap_statement_list (any_wildcard->children.head, os,
                                     current);
      break;
    }
#endif
    case lang_constructors_statement_enum:
      if (constructor_list.head != NULL)
        print_memmap_statement_list (constructor_list.head, os, current);
      break;
    case lang_wild_statement_enum:
      print_memmap_statement_list (s->wild_statement.children.head, os,
                                   current);
      break;
    case lang_data_statement_enum:
      print_memmap_data_statement (&s->data_statement, current);
      break;
    case lang_input_section_enum:
      print_memmap_input_section (s->input_section.section, current);
      break;
    case lang_padding_statement_enum:
      print_memmap_padding_statement (&s->padding_statement, current);
      break;
    case lang_output_section_statement_enum:
    {
      if (s->output_section_statement.region == current)
        print_memmap_statement_list (s->output_section_statement.children.
                                     head, &s->output_section_statement,
                                     current);
      break;
    }
    case lang_group_statement_enum:
      print_memmap_statement_list (s->group_statement.children.head, os,
                                   current);
      break;
    }
}

static void
print_memmap_statement_list (lang_statement_union_type * s,
                             lang_output_section_statement_type * os,
                             lang_memory_region_type * current)
{
  while (s != NULL)
    {
      print_memmap_statement (s, os, current);
      s = s->header.next;
    }
}

static void
print_memmap_statements (lang_memory_region_type * current)
{
  print_memmap_statement_list (statement_list.head, abs_output_section,
                               current);
}


static void
print_memory_map (void)
{
  lang_memory_region_type *current = NULL;

  fprintf (csky_config.cskymap_file, _("\nMemory Map of the image\n\n"));
  fprintf (csky_config.cskymap_file, _("    Image Entry point : 0x%08x\n"),
           (unsigned int) link_info.output_bfd->start_address);

  /*Traversal the statement list to find output section statement,
   * print the segment the section belong to,
   * and print each input section of the output seciton. */
  for (current = lang_memory_region_list;
       current != NULL; current = current->next)
    {
      fprintf (csky_config.cskymap_file,
               _
               ("\n    Region %s (Base: 0x%08x, Size: 0x%08x, Max: 0x%08x)\n"),
               current->name_list.name, (unsigned int) current->origin,
               (unsigned int) (current->current - current->origin),
               (unsigned int) current->length);
      if (current->current - current->origin)
        {
          fprintf (csky_config.cskymap_file,
                   _("\n%13s%8s%13s%7s%9s%15s%13s\n"), "Base Addr", "Size",
                   "Type", "Attr", "Idx", "Section Name", "Object");
          print_memmap_statements (current);
        }
      else
        {
          /*Do nothing */
        }
    }

  fprintf (csky_config.cskymap_file, _("\n\n"));
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);
}

static void
archive_printed_list_init (archive_printed_list * list)
{
  list->head = NULL;
  list->tail = NULL;
  return;
}

static void
archive_printed_list_insert (archive_printed_list * list,
                             archive_printed elem)
{
  archive_printed *p_elem = NULL;
  p_elem = (archive_printed *) malloc (sizeof (archive_printed));
  memset (p_elem, 0, sizeof (archive_printed));

  p_elem->bfd_id = elem.bfd_id;
  p_elem->next = NULL;

  if (list->head == NULL)
    {
      list->head = p_elem;
      list->tail = p_elem;
    }
  else
    {
      list->tail->next = p_elem;
      list->tail = p_elem;
    }

  return;
}

static void
archive_printed_list_clean (archive_printed_list * list)
{
  archive_printed *p_current = list->head;
  archive_printed *p_next = NULL;

  while (p_current != NULL)
    {
      p_next = p_current->next;
      free (p_current);
      p_current = p_next;
    }
  list->head = NULL;
  list->tail = NULL;

  return;
}

static int
check_archive_has_printed (archive_printed_list * list, bfd * archive)
{
  archive_printed *p_current = list->head;

  while (p_current != NULL)
    {
      if (p_current->bfd_id == archive->id)
        return 1;
      else
        p_current = p_current->next;
    }
  return 0;
}

static void
bfd_extra_list_init (bfd_extra_list * list)
{
  list->head = NULL;
  list->tail = NULL;
  return;
}

static void
bfd_extra_list_insert (bfd_extra_list * list, bfd_extra elem)
{
  bfd_extra *p_elem = NULL;
  p_elem = (bfd_extra *) malloc (sizeof (bfd_extra));
  memset (p_elem, 0, sizeof (bfd_extra));

  p_elem->bfd_id = elem.bfd_id;
  p_elem->size = elem.size;
  p_elem->type = elem.type;
  p_elem->attr = elem.attr;
  p_elem->next = NULL;
  p_elem->prev = NULL;

  if (list->head == NULL)
    {
      list->head = p_elem;
      list->tail = p_elem;
    }
  else
    {
      list->tail->next = p_elem;
      p_elem->prev = list->tail;
      list->tail = p_elem;
    }

  return;
}

static void
bfd_extra_list_del (bfd_extra_list * list, bfd_extra * elem)
{
  if (list->head == elem && list->tail == elem)
    {
      list->head = NULL;
      list->tail = NULL;
      free (elem);
    }
  else if (list->head == elem)
    {
      list->head = list->head->next;
      list->head->prev = NULL;
      free (elem);
    }
  else if (list->tail == elem)
    {
      list->tail = list->tail->prev;
      list->tail->next = NULL;
      free (elem);
    }
  else
    {
      elem->prev->next = elem->next;
      elem->next->prev = elem->prev;
      free (elem);
    }

  return;
}

static void
bfd_extra_list_clean (bfd_extra_list * list)
{
  bfd_extra *p_current = list->head;
  bfd_extra *p_next = NULL;

  while (p_current != NULL)
    {
      p_next = p_current->next;
      free (p_current);
      p_current = p_next;
    }
  list->head = NULL;
  list->tail = NULL;

  return;
}

static object_component_size
find_bfd_extra (bfd_extra_list * list, unsigned int bfd_id)
{
  bfd_extra *p_current = list->head;
  bfd_extra *p_next = NULL;
  object_component_size ibfd_component_size;
  object_component_size_reset (&ibfd_component_size);

  while (p_current != NULL)
    {
      if (p_current->bfd_id == bfd_id)
        {
          if (p_current->type == pad_enum)
            {
              if (p_current->attr == code_enum)
                ibfd_component_size.code_pad += p_current->size;
              else if (p_current->attr == rodata_enum)
                ibfd_component_size.rodata_pad += p_current->size;
              else if (p_current->attr == rwdata_enum)
                ibfd_component_size.rwdata_pad += p_current->size;
              else if (p_current->attr == zidata_enum)
                ibfd_component_size.zidata_pad += p_current->size;
              else if (p_current->attr == debug_enum)
                ibfd_component_size.debug_pad += p_current->size;
            }
          else if (p_current->type == data_enum)
            {
              if (p_current->attr == code_enum)
                ibfd_component_size.code_gen += p_current->size;
              else if (p_current->attr == rodata_enum)
                ibfd_component_size.rodata_gen += p_current->size;
              else if (p_current->attr == rwdata_enum)
                ibfd_component_size.rwdata_gen += p_current->size;
              else if (p_current->attr == zidata_enum)
                ibfd_component_size.zidata_gen += p_current->size;
              else if (p_current->attr == debug_enum)
                ibfd_component_size.debug_gen += p_current->size;
            }

          p_next = p_current->next;
          bfd_extra_list_del (list, p_current);
          p_current = p_next;
        }
      else
        p_current = p_current->next;
    }

  return ibfd_component_size;
}

static void
build_bfdextra_table_padding_statement (lang_padding_statement_type * s)
{
  bfd_extra bfdextra;

  if (current_statement_bfd != NULL)
    {
      bfdextra.bfd_id = current_statement_bfd->id;
      bfdextra.type = pad_enum;
      bfdextra.size = s->size;

      if ((s->output_section->flags & SEC_CODE) != 0)
        bfdextra.attr = code_enum;
      else if ((s->output_section->flags & SEC_DEBUGGING) != 0)
        bfdextra.attr = debug_enum;
      else if ((s->output_section->flags & SEC_DATA) != 0)
        {
          if ((s->output_section->flags & SEC_READONLY) != 0)
            bfdextra.attr = rodata_enum;
          else
            bfdextra.attr = rwdata_enum;
        }
      else if (strstr (s->output_section->name, ".bss"))
        bfdextra.attr = zidata_enum;
      else
        return;

      bfd_extra_list_insert (&bfdextra_list, bfdextra);
    }
  else
    return;

}

static void
build_bfdextra_table_data_statement (lang_data_statement_type * data)
{
  bfd_extra bfdextra;
  bfd_size_type size;

  switch (data->type)
    {
    default:
      abort ();
    case BYTE:
      size = BYTE_SIZE;
      break;
    case SHORT:
      size = SHORT_SIZE;
      break;
    case LONG:
      size = LONG_SIZE;
      break;
    case QUAD:
      size = QUAD_SIZE;
      break;
    case SQUAD:
      size = QUAD_SIZE;
      break;
    }

  if (current_statement_bfd != NULL)
    {
      bfdextra.bfd_id = current_statement_bfd->id;
      bfdextra.type = data_enum;
      bfdextra.size = size;

      if ((data->output_section->flags & SEC_CODE) != 0)
        bfdextra.attr = code_enum;
      else if ((data->output_section->flags & SEC_DEBUGGING) != 0)
        bfdextra.attr = debug_enum;
      else if ((data->output_section->flags & SEC_DATA) != 0)
        {
          if ((data->output_section->flags & SEC_READONLY) != 0)
            bfdextra.attr = rodata_enum;
          else
            bfdextra.attr = rwdata_enum;
        }
      else if (strstr (data->output_section->name, ".bss"))
        bfdextra.attr = zidata_enum;
      else
        return;

      bfd_extra_list_insert (&bfdextra_list, bfdextra);
    }
  else
    return;

}

static void
build_bfdextra_table_statement (lang_statement_union_type * s,
                                lang_output_section_statement_type * os)
{
  switch (s->header.type)
    {
    default:
      fprintf (csky_config.cskymap_file, _("Fail with %d\n"), s->header.type);
      FAIL ();
      break;
    case lang_address_statement_enum:
    case lang_object_symbols_statement_enum:
    case lang_fill_statement_enum:
    case lang_reloc_statement_enum:
    case lang_assignment_statement_enum:
    case lang_target_statement_enum:
    case lang_output_statement_enum:
    case lang_insert_statement_enum:
      break;
#if ENABLE_ANY
    case lang_any_statement_enum:
    {
      lang_wild_statement_type *any_wildcard =
        &s->any_statement.children.head->wild_statement;

      if (any_wildcard != NULL)
        build_bfdextra_table_statement_list (any_wildcard->children.head,
                                             os);
      break;
    }
#endif
    case lang_constructors_statement_enum:
      if (constructor_list.head != NULL)
        build_bfdextra_table_statement_list (constructor_list.head, os);
      break;
    case lang_wild_statement_enum:
      build_bfdextra_table_statement_list (s->wild_statement.children.head,
                                           os);
      break;
    case lang_input_statement_enum:
      current_statement_bfd = s->input_statement.the_bfd;
      break;
    case lang_input_section_enum:
      current_statement_bfd = s->input_section.section->owner;
      break;
    case lang_padding_statement_enum:
      build_bfdextra_table_padding_statement (&s->padding_statement);
      break;
    case lang_data_statement_enum:
      build_bfdextra_table_data_statement (&s->data_statement);
      break;
    case lang_output_section_statement_enum:
      build_bfdextra_table_statement_list (s->output_section_statement.
                                           children.head,
                                           &s->output_section_statement);
      break;
    case lang_group_statement_enum:
      build_bfdextra_table_statement_list (s->group_statement.children.head,
                                           os);
      break;
    }
}

static void
build_bfdextra_table_statement_list (lang_statement_union_type * s,
                                     lang_output_section_statement_type * os)
{
  while (s != NULL)
    {
      build_bfdextra_table_statement (s, os);
      s = s->header.next;
    }
}

static void
build_bfdextra_table_statements (void)
{
  bfd_extra_list_init (&bfdextra_list);
  build_bfdextra_table_statement_list (statement_list.head,
                                       abs_output_section);
}

static void
object_component_size_reset (object_component_size * current)
{
  memset (current, 0, sizeof (object_component_size));
  return;
}

static void
object_component_size_add (object_component_size * current,
                           object_component_size adden)
{
  current->code_size += adden.code_size;
  current->rodata_size += adden.rodata_size;
  current->rwdata_size += adden.rwdata_size;
  current->zidata_size += adden.zidata_size;
  current->debug_size += adden.debug_size;
  current->code_pad += adden.code_pad;
  current->rodata_pad += adden.rodata_pad;
  current->rwdata_pad += adden.rwdata_pad;
  current->zidata_pad += adden.zidata_pad;
  current->debug_pad += adden.debug_pad;
  current->code_gen += adden.code_gen;
  current->rodata_gen += adden.rodata_gen;
  current->rwdata_gen += adden.rwdata_gen;
  current->zidata_gen += adden.zidata_gen;
  current->debug_gen += adden.debug_gen;
}

/*Output the input object file's code size, ro data size, rw data size,
 * zi data size, debug size in output file in order. */
static object_component_size
print_object_component_sizes (bfd * input_bfd)
{
  object_component_size ibfd_component_size;
  asection *asec;

  ibfd_component_size.code_size = 0;
  ibfd_component_size.rodata_size = 0;
  ibfd_component_size.rwdata_size = 0;
  ibfd_component_size.zidata_size = 0;
  ibfd_component_size.debug_size = 0;
  ibfd_component_size.code_pad = 0;
  ibfd_component_size.rodata_pad = 0;
  ibfd_component_size.rwdata_pad = 0;
  ibfd_component_size.zidata_pad = 0;
  ibfd_component_size.debug_pad = 0;
  ibfd_component_size.code_gen = 0;
  ibfd_component_size.rodata_gen = 0;
  ibfd_component_size.rwdata_gen = 0;
  ibfd_component_size.zidata_gen = 0;
  ibfd_component_size.debug_gen = 0;
  /*Traversal each section in the file */
  for (asec = input_bfd->sections; asec != NULL; asec = asec->next)
    {
      /*Judge the section is whether the Discarded input section, if it is,
       * don't calculate the size;if not, judge the section's attribute and
       * add the size to the group.*/
      if ((asec->output_section != NULL
           && asec->output_section->owner == link_info.output_bfd)
          || ((asec->flags & (SEC_LINKER_CREATED | SEC_KEEP)) != 0))
        {
          if ((asec->flags & SEC_CODE) != 0)
            ibfd_component_size.code_size += asec->size;
          else if ((asec->flags & SEC_DEBUGGING) != 0)
            ibfd_component_size.debug_size += asec->size;
          else if ((asec->flags & SEC_DATA) != 0)
            {
              if ((asec->flags & SEC_READONLY) != 0)
                ibfd_component_size.rodata_size += asec->size;
              else
                ibfd_component_size.rwdata_size += asec->size;
            }
          else if (strstr (asec->output_section->name, ".bss"))
            ibfd_component_size.zidata_size += asec->size;
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
  fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   %s\n",
           (int) ibfd_component_size.code_size,
           (int) ibfd_component_size.rodata_size,
           (int) ibfd_component_size.rwdata_size,
           (int) ibfd_component_size.zidata_size,
           (int) ibfd_component_size.debug_size, _(input_bfd->filename));

  return ibfd_component_size;
}


static void
print_image_component_sizes (void)
{
  bfd *abfd;
  /*Each component's total size */
  object_component_size current_component_size;
  object_component_size_reset (&current_component_size);

  int total_code_size = 0;
  int total_rodata_size = 0;
  int total_rwdata_size = 0;
  int total_zidata_size = 0;
  int total_debug_size = 0;

  archive_printed_list_init (&archprinted_list);

  /*Build section pad table */
  build_bfdextra_table_statements ();

  object_component_size abfd_component_size;
  fprintf (csky_config.cskymap_file, _("\nImage component sizes\n\n"));
  /*Output the Object file's component sizes at first */
  /*FIXME Each column alignment. */
  fprintf (csky_config.cskymap_file,
           _
           ("       Code    RO Data    RW Data    ZI Data      Debug   Object Name\n\n"));
  /*Traversal each input file */
  for (abfd = link_info.input_bfds; abfd != (bfd *) NULL;
       abfd = abfd->link.next)
    {
      abfd_component_size = find_bfd_extra (&bfdextra_list, abfd->id);
      object_component_size_add (&current_component_size,
                                 abfd_component_size);
      /*Judge the file is whether the object file, if it is,
       * call the print_object_component_sizes() to output the component sizes;
       * if not, don't do any thing. */
      if (abfd->format == bfd_object && abfd->my_archive == NULL)
        {
          abfd_component_size = print_object_component_sizes (abfd);
          object_component_size_add (&current_component_size,
                                     abfd_component_size);
        }
      else
        {
          /*Do nothing */
        }
    }
  fprintf (csky_config.cskymap_file, _("\t"));
  /*FIXME the line size can be macro */
  print_dividing_line (csky_config.cskymap_file, "-", IN_DIVID_LENGTH);
  /*Output Each component's total size */
  fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   Object Totals\n",
           current_component_size.code_size,
           current_component_size.rodata_size,
           current_component_size.rwdata_size,
           current_component_size.zidata_size,
           current_component_size.debug_size);
  fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   Pad\n",
           current_component_size.code_pad,
           current_component_size.rodata_pad,
           current_component_size.rwdata_pad,
           current_component_size.zidata_pad,
           current_component_size.debug_pad);
  fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   LD_GEN\n\n\t",
           current_component_size.code_gen,
           current_component_size.rodata_gen,
           current_component_size.rwdata_gen,
           current_component_size.zidata_gen,
           current_component_size.debug_gen);
  print_dividing_line (csky_config.cskymap_file, "-", IN_DIVID_LENGTH);

  total_code_size += (current_component_size.code_size +
                      current_component_size.code_pad +
                      current_component_size.code_gen);
  total_rodata_size += (current_component_size.rodata_size +
                        current_component_size.rodata_pad +
                        current_component_size.rodata_gen);
  total_rwdata_size += (current_component_size.rwdata_size +
                        current_component_size.rwdata_pad +
                        current_component_size.rwdata_gen);
  total_zidata_size += (current_component_size.zidata_size +
                        current_component_size.zidata_pad +
                        current_component_size.zidata_gen);
  total_debug_size += (current_component_size.debug_size +
                       current_component_size.debug_pad +
                       current_component_size.debug_gen);

  /*Output the Archive file's component sizes at next */
  bfd *current_archive = NULL;
  int map_uncomplete = 1;
  while (map_uncomplete)
    {
      map_uncomplete = 0;
      object_component_size_reset (&current_component_size);

      /*Traversal each input file */
      for (abfd = link_info.input_bfds; abfd != (bfd *) NULL;
           abfd = abfd->link.next)
        {
          abfd_component_size = find_bfd_extra (&bfdextra_list, abfd->id);
          object_component_size_add (&current_component_size,
                                     abfd_component_size);
          /*Judge the file is whether the Archive file, if it is,
           * Traversal theeach object file in the Archive file and call the
           * print_object_component_sizes() to output the component sizes;
           * if not, don't do any thing. */
          if (abfd->format == bfd_object && abfd->my_archive != NULL)
            {
              if (check_archive_has_printed (&archprinted_list,
                                             abfd->my_archive) == 0)
                {
                  if (current_archive == NULL)
                    current_archive = abfd->my_archive;
                  if (current_archive == abfd->my_archive)
                    {
                      if (!map_uncomplete)
                        {
                          fprintf (csky_config.cskymap_file,
                                   "    [Library Name]: %s\n\t",
                                   current_archive->filename);
                          print_dividing_line (csky_config.cskymap_file, "-",
                                               IN_DIVID_LENGTH);
                          /*FIXME Each column alignment. */
                          fprintf (csky_config.cskymap_file,
                                   _
                                   ("       Code    RO Data    RW Data    ZI Data      Debug   Library Member Name\n\n"));
                          map_uncomplete = 1;
                        }
                      abfd_component_size =
                        print_object_component_sizes (abfd);
                      object_component_size_add (&current_component_size,
                                                 abfd_component_size);
                    }
                }
            }
          else
            {
              /*Do nothing */
            }
        }
      if (current_archive != NULL)
        {
          /*Output Each component's total size */
          fprintf (csky_config.cskymap_file, _("\t"));
          print_dividing_line (csky_config.cskymap_file, "-",
                               IN_DIVID_LENGTH);
          fprintf (csky_config.cskymap_file,
                   "%11d%11d%11d%11d%11d   Library Totals\n",
                   current_component_size.code_size,
                   current_component_size.rodata_size,
                   current_component_size.rwdata_size,
                   current_component_size.zidata_size,
                   current_component_size.debug_size);
          fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   Pad\n",
                   current_component_size.code_pad,
                   current_component_size.rodata_pad,
                   current_component_size.rwdata_pad,
                   current_component_size.zidata_pad,
                   current_component_size.debug_pad);
          fprintf (csky_config.cskymap_file,
                   "%11d%11d%11d%11d%11d   LD_GEN\n\n\t",
                   current_component_size.code_gen,
                   current_component_size.rodata_gen,
                   current_component_size.rwdata_gen,
                   current_component_size.zidata_gen,
                   current_component_size.debug_gen);
          print_dividing_line (csky_config.cskymap_file, "-",
                               IN_DIVID_LENGTH);

          archive_printed cur_archive;
          cur_archive.bfd_id = current_archive->id;
          archive_printed_list_insert (&archprinted_list, cur_archive);
          current_archive = NULL;

          total_code_size += (current_component_size.code_size +
                              current_component_size.code_pad +
                              current_component_size.code_gen);
          total_rodata_size += (current_component_size.rodata_size +
                                current_component_size.rodata_pad +
                                current_component_size.rodata_gen);
          total_rwdata_size += (current_component_size.rwdata_size +
                                current_component_size.rwdata_pad +
                                current_component_size.rwdata_gen);
          total_zidata_size += (current_component_size.zidata_size +
                                current_component_size.zidata_pad +
                                current_component_size.zidata_gen);
          total_debug_size += (current_component_size.debug_size +
                               current_component_size.debug_pad +
                               current_component_size.debug_gen);

        }
    }
  fprintf (csky_config.cskymap_file, _("\n"));
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);

  fprintf (csky_config.cskymap_file,
           _
           ("\n\n       Code    RO Data    RW Data    ZI Data      Debug\n"));
  fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   %s\n",
           total_code_size, total_rodata_size, total_rwdata_size,
           total_zidata_size, total_debug_size, "Grand Totals");
  fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   %s\n",
           total_code_size, total_rodata_size, total_rwdata_size,
           total_zidata_size, total_debug_size, "Elf Image Totals");
  fprintf (csky_config.cskymap_file, "%11d%11d%11d%11d%11d   %s\n\n",
           total_code_size, total_rodata_size, total_rwdata_size, 0, 0,
           "ROM Totals");
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);

  fprintf (csky_config.cskymap_file,
           "\nTotal\tRO\tSize (Code + RO Data)\t\t\t\t%d (  %.2fkB)\n",
           total_code_size + total_rodata_size,
           (double) (total_code_size + total_rodata_size) / 1024);
  fprintf (csky_config.cskymap_file,
           "Total\tRW\tSize (RW Data + ZI Data)\t\t\t%d (  %.2fkB)\n",
           total_rwdata_size + total_zidata_size,
           (double) (total_rwdata_size + total_zidata_size) / 1024);
  fprintf (csky_config.cskymap_file,
           "Total\tROM\tSize (Code + RO Data + RW Data)\t\t%d (  %.2fkB)\n\n",
           total_code_size + total_rodata_size + total_rwdata_size,
           (double) (total_code_size + total_rodata_size +
                     total_rwdata_size) / 1024);
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);

}

lang_memory_region_type *
csky_get_lang_memory_region (void)
{
  union lang_statement_union *p = lang_output_section_statement.head;
  lang_output_section_statement_type *s = NULL;
  lang_memory_region_type *region = NULL;
  for (s = &p->output_section_statement; s != NULL; s = s->next)
    {
      region = s->region;
      if (region != NULL)
        return region;
    }

  return NULL;
}

void
csky_lang_map (void)
{
  if (csky_config.cskymap_filename == NULL)
    return;

  csky_config.cskymap_file = fopen (csky_config.cskymap_filename, FOPEN_WT);
  if (csky_config.cskymap_file == (FILE *) NULL)
    {
      bfd_set_error (bfd_error_system_call);
      einfo (_("%P%F: cannot open map file %s: %E\n"),
             csky_config.cskymap_filename);
      return;
    }

  lang_memory_region_list = csky_get_lang_memory_region ();
  statement_list = *stat_ptr;

  fprintf (csky_config.cskymap_file, "Csky GNU Linker\n\n");
  print_dividing_line (csky_config.cskymap_file, "=", OUT_DIVID_LENGTH);

  print_section_cross_references ();
  print_discard_sections ();
  print_image_symbol_table ();
  print_memory_map ();
  print_image_component_sizes ();

  bfd_extra_list_clean (&bfdextra_list);
  archive_printed_list_clean (&archprinted_list);
  fclose (csky_config.cskymap_file);
}
