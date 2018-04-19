fragment <<EOF

#include "ldctor.h"
#include "elf/csky.h"

/* To use branch stub or not*/
extern bfd_boolean use_branch_stub;

/* Fake input file for stubs.  */
static lang_input_statement_type *stub_file;

/* Whether we need to call gldcsky_layout_sections_again.  */
static int need_laying_out = 0;

/* Maximum size of a group of input sections that can be handled by
   one stub section.  A value of +/-1 indicates the bfd back-end
   should use a suitable default size.  */
static bfd_signed_vma group_size = 1;

struct hook_stub_info
{
  lang_statement_list_type add;
  asection *input_section;
};

/* Local functions.  */
void
 csky_lang_map (void);
void
csky_ldmul_before_finish (void);
void
 csky_ldmul_before_write(void);


/* Extern functions.  */
void
elf32_csky_next_input_section (struct bfd_link_info *info,
                               asection *isec);
int
elf32_csky_setup_section_lists (bfd *output_bfd,
                                struct bfd_link_info *info);
bfd_boolean
elf32_csky_size_stubs (bfd *output_bfd,
                       bfd *stub_bfd,
                       struct bfd_link_info *info,
                       bfd_signed_vma group_size,
                       asection *(*add_stub_section) (const char*, asection*),
                       void (*layout_sections_again) (void));
bfd_boolean
elf32_csky_build_stubs (struct bfd_link_info *info);


/* Traverse the linker tree to find the spot where the stub goes.  */
static bfd_boolean
hook_in_stub (struct hook_stub_info *info, lang_statement_union_type **lp)
{
  lang_statement_union_type *l;
  bfd_boolean ret;

  for (; (l = *lp) != NULL; lp = &l->header.next)
  {
    switch (l->header.type)
    {
    case lang_constructors_statement_enum:
      ret = hook_in_stub (info, &constructor_list.head);
      if (ret)
        {
          return ret;
        }
      break;

    case lang_output_section_statement_enum:
      ret = hook_in_stub (info,
                   &l->output_section_statement.children.head);
      if (ret)
        {
          return ret;
        }
      break;

    case lang_wild_statement_enum:
      ret = hook_in_stub (info, &l->wild_statement.children.head);
      if (ret)
        {
          return ret;
        }
      break;

#if ENABLE_ANY
    case lang_any_statement_enum:
      {
        lang_wild_statement_type *any_wildcard =
           &l->any_statement.children.head->wild_statement;
        if (any_wildcard != NULL)
          {
            ret = hook_in_stub(info, &any_wildcard->children.head);
            if (ret)
              return ret;
          }
        break;
      }
#endif

    case lang_group_statement_enum:
      ret = hook_in_stub (info, &l->group_statement.children.head);
      if (ret)
        {
          return ret;
        }
      break;

    case lang_input_section_enum:
      if (l->input_section.section == info->input_section)
        {
          /* We've found our section.  Insert the stub immediately
             after its associated input section.  */
          *(info->add.tail) = l->header.next;
          l->header.next = info->add.head;
          return TRUE;
        }
      break;

    case lang_data_statement_enum:
    case lang_reloc_statement_enum:
    case lang_object_symbols_statement_enum:
    case lang_output_statement_enum:
    case lang_target_statement_enum:
    case lang_input_statement_enum:
    case lang_assignment_statement_enum:
    case lang_padding_statement_enum:
    case lang_address_statement_enum:
    case lang_fill_statement_enum:
      break;

    default:
      FAIL ();
      break;
    }
  }
  return FALSE;
}
EOF

case ${target} in
    csky-*-linux-* | csky-*-uclinux-*)
fragment <<EOF
/* This is a convenient point to tell BFD about target specific flags.
   After the output has been created, but before inputs are read.  */
static void
csky_elf_create_output_section_statements (void)
{
  use_branch_stub = FALSE;
}
EOF
    break
    ;;
    *)
fragment <<EOF
/* This is a convenient point to tell BFD about target specific flags.
   After the output has been created, but before inputs are read.  */
static void
csky_elf_create_output_section_statements (void)
{
  /* If don't use branch stub, just do not emit stub_file*/
  if (use_branch_stub == FALSE)
    {
      return;
    }
  stub_file = lang_add_input_file ("linker stubs", lang_input_file_is_fake_enum, NULL);
  stub_file->the_bfd = bfd_create ("linker stubs", link_info.output_bfd);
  if (stub_file->the_bfd == NULL
      || !bfd_set_arch_mach (stub_file->the_bfd,
          bfd_get_arch (link_info.output_bfd),
          bfd_get_mach (link_info.output_bfd)))
  {
    einfo ("%X%P: can not create BFD %E\n");
    return;
  }

  stub_file->the_bfd->flags |= BFD_LINKER_CREATED;
  ldlang_add_file (stub_file);
}
EOF
    ;;
esac

fragment <<EOF
/* Call-back for elf32_csky_size_stubs.  */

/* Create a new stub section, and arrange for it to be linked
   immediately after INPUT_SECTION.  */
static asection *
elf32_csky_add_stub_section (const char *stub_sec_name,
              asection *input_section)
{
  asection *stub_sec;
  flagword flags;
  asection *output_section;
  const char *secname;
  lang_output_section_statement_type *os;
  struct hook_stub_info info;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE
      | SEC_HAS_CONTENTS | SEC_RELOC | SEC_IN_MEMORY | SEC_KEEP);
  stub_sec = bfd_make_section_anyway_with_flags (stub_file->the_bfd,
                      stub_sec_name, flags);
  if (stub_sec == NULL)
    {
      goto err_ret;
    }

  bfd_set_section_alignment (stub_file->the_bfd, stub_sec, 3);

  output_section = input_section->output_section;
  secname = bfd_get_section_name (output_section->owner, output_section);
  os = lang_output_section_find (secname);

  info.input_section = input_section;
  lang_list_init (&info.add);
  lang_add_section (&info.add, stub_sec, NULL, os);

  if (info.add.head == NULL)
    {
      goto err_ret;
    }

  if (hook_in_stub (&info, &os->children.head))
    {
      return stub_sec;
    }

err_ret:
  einfo ("%X%P: can not make stub section: %E\n");
  return NULL;
}

/* Another call-back for elf_csky_size_stubs.  */
static void
gldcsky_layout_sections_again (void)
{
  /* If we have changed sizes of the stub sections, then we need
     to recalculate all the section offsets.  This may mean we need to
     add even more stubs.  */
  gld${EMULATION_NAME}_map_segments (TRUE);
  need_laying_out = -1;
}

static void
build_section_lists (lang_statement_union_type *statement)
{
  if (statement->header.type == lang_input_section_enum)
  {
    asection *i = statement->input_section.section;

    if (i->sec_info_type != SEC_INFO_TYPE_JUST_SYMS
        && (i->flags & SEC_EXCLUDE) == 0
        && i->output_section != NULL
        && i->output_section->owner == link_info.output_bfd)
      {
        elf32_csky_next_input_section (& link_info, i);
      }
  }
}

static void
gld${EMULATION_NAME}_after_allocation (void)
{
  /* bfd_elf32_discard_info just plays with debugging sections,
     ie. doesn't affect any code, so we can delay resizing the
     sections.  It's likely we'll resize everything in the process of
     adding stubs.  */
  if (bfd_elf_discard_info (link_info.output_bfd, & link_info))
    {
      need_laying_out = 1;
    }

  /* If generating a relocatable output file, then we don't
     have to examine the relocs.  */

  if (stub_file != NULL && !bfd_link_relocatable(&link_info))
  {
    int  ret = elf32_csky_setup_section_lists (link_info.output_bfd, & link_info);

    if (ret != 0)
      {
        if (ret < 0)
          {
            einfo ("%X%P: could not compute sections lists for stub generation: %E\n");
            return;
          }

        lang_for_each_statement (build_section_lists);

        /* Call into the BFD backend to do the real work.  */
        if (! elf32_csky_size_stubs (link_info.output_bfd,
                        stub_file->the_bfd,
                        & link_info,
                        group_size,
                        & elf32_csky_add_stub_section,
                        & gldcsky_layout_sections_again))
          {
            einfo ("%X%P: cannot size stub section: %E\n");
            return;
          }
      }
  }

  if (need_laying_out != -1)
    {
      gld${EMULATION_NAME}_map_segments (need_laying_out);
    }
}

static void
gld${EMULATION_NAME}_finish (void)
{
  if (stub_file != NULL && ! bfd_link_relocatable(&link_info))
  {
    /* Now build the linker stubs.  */
    if (stub_file->the_bfd->sections != NULL)
      {
        if (! elf32_csky_build_stubs (& link_info))
          {
            einfo ("%X%P: can not build stubs: %E\n");
          }
      }
  }
  finish_default ();
}

/* Avoid processing the fake stub_file in vercheck, stat_needed and
   check_needed routines.  */

static void (*real_func) (lang_input_statement_type *);

static void csky_for_each_input_file_wrapper (lang_input_statement_type *l)
{
  if (l != stub_file)
    {
      (*real_func) (l);
    }
}

static void
csky_lang_for_each_input_file (void (*func) (lang_input_statement_type *))
{
  real_func = func;
  lang_for_each_input_file (&csky_for_each_input_file_wrapper);
}

#define lang_for_each_input_file csky_lang_for_each_input_file

EOF

# The following is about csky map file.
if ! cat ${srcdir}/emultempl/csky_map.c >> e${EMULATION_NAME}.c
then
  echo >&2 "Missing ${srcdir}/emultempl/csky_map.c"
  exit 1
fi

# The following is about csky callgraph
if ! cat ${srcdir}/emultempl/ld_csky.h >> ld_csky.h
then
  echo >&2 "Missing ${srcdir}/emultempl/ld_csky.h.c"
  exit 1
fi
if ! cat ${srcdir}/emultempl/csky_callgraph.c >> e${EMULATION_NAME}.c
then
  echo >&2 "Missing ${srcdir}/emultempl/csky_map.c"
  exit 1
fi

fragment <<EOF
/* Add callgraph & ckmap.  */
void
csky_ldmul_before_write(void)
{
  if (csky_config.do_callgraph || csky_config.cskymap_filename)
    build_func_hash ();

  if (csky_config.do_callgraph)
    {
      int str_len = 0;
      if (csky_config.callgraph_filename == NULL)
      {
        /*The default callgraph file name is "output_filename"+".htm",
         * so the length = strlen (output_filename) + strlen (".htm") + 1,
         * 1 is the end mark '\0' */
        str_len = strlen (output_filename) + 5;
        csky_config.callgraph_filename = (char *)malloc (str_len * sizeof (char));
        strcpy (csky_config.callgraph_filename, output_filename);
        if (csky_config.callgraph_fmt == html)
          strcat (csky_config.callgraph_filename, ".htm");
        else
          strcat (csky_config.callgraph_filename, ".txt");
      }
      else
      {
        /*do nothing */
      }
    csky_config.callgraph_file = fopen (csky_config.callgraph_filename, FOPEN_WT);
    if (csky_config.callgraph_file == (FILE *) NULL)
      {
        bfd_set_error (bfd_error_system_call);
        einfo (_("%P%F: cannot open map file %s: %E\n"),
         csky_config.callgraph_filename);
      }
    }
}
void
csky_ldmul_before_finish (void)
{
  if (csky_config.cskymap_filename != NULL)
    csky_lang_map ();

  if (csky_config.callgraph_file)
    {
      set_func_stack_size ();
      dump_callgraph ();
    }

  if (csky_config.callgraph_file || csky_config.cskymap_filename)
    call_graph_free ();

}

EOF


# This code gets inserted into the generic elf32.sc linker script
# and allows us to define our own command line switches.
case ${target} in
    csky-*-linux-* | csky-*-uclinux-*)

PARSE_AND_LIST_PROLOGUE='
#define OPTION_BASE_FILE                    300
#define OPTION_CSKY_MAP                     301
#define OPTION_CALLGRAPH                    302
#define OPTION_CALLGRAPH_FILE               303
#define OPTION_CALLGRAPH_OUTPUT             304
#define OPTION_ANY_CONTINGENCY              305
#define OPTION_ANY_SORTORDER                306
'

PARSE_AND_LIST_LONGOPTS='
  {"base-file",        required_argument, NULL, OPTION_BASE_FILE},
  {"ckmap",            required_argument, NULL, OPTION_CSKY_MAP},
  {"callgraph",        no_argument,       NULL, OPTION_CALLGRAPH},
  {"callgraph_file",   required_argument, NULL, OPTION_CALLGRAPH_FILE},
  {"callgraph_output", required_argument, NULL, OPTION_CALLGRAPH_OUTPUT},
#if ENABLE_ANY
  {"any_contingency",  required_argument, NULL, OPTION_ANY_CONTINGENCY},
  {"any_sort_order",   required_argument, NULL, OPTION_ANY_SORTORDER},
#endif
'
PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("  --base_file <basefile>\n"));
  fprintf (file, _("\t\t\tGenerate a base file for relocatable DLLs\n"));
  fprintf (file, _("  --ckmap=<mapfile>\n"));
  fprintf (file, _("\t\t\tOutput map information. \n"));
  fprintf (file, _("  --callgraph\n"));
  fprintf (file, _("\t\t\tOutput callgraph file. \n"));
  fprintf (file, _("  --callgraph_file=<filename>\n"));
  fprintf (file, _("\t\t\tControl the the filename of output callgraph. \n"));
  fprintf (file, _("  --callgraph_output=<fmt>\n"));
  fprintf (file, _("\t\t\tControl the the format of output callgraph, html(default) or txt. \n"));
#if ENABLE_ANY
  fprintf (file, _("  --any_contingency=N\n"));
  fprintf (file, _("\t\t\tPermits N/100 extra space in any execution regions containing\n"\
                   "\t\t\t.ANY sections for linker-generated content such as\n"\
                   "\t\t\tveneers and alignment padding.\n"));
  fprintf (file, _("  --any_sort_order=[cmdline|descending_size]"));
  fprintf (file, _("\t\t\tControls the sort order of input sections that are placed using\n"\
                   "\t\t\tthe .ANY module selector.\n"));
#endif
'
PARSE_AND_LIST_ARGS_CASES='
  case OPTION_BASE_FILE:
    link_info.base_file = fopen (optarg, FOPEN_WB);
    if (link_info.base_file == NULL)
      {
        /* xgettext:c-format */
        fprintf (stderr, _("%s: Cannot open base file %s\n"),
        program_name, optarg);
        xexit (1);
      }
    break;
  case OPTION_CSKY_MAP:
    csky_config.cskymap_filename = optarg;
    break;
  case OPTION_CALLGRAPH:
    csky_config.do_callgraph = TRUE;
    break;
  case OPTION_CALLGRAPH_FILE:
    csky_config.do_callgraph = TRUE;
    csky_config.callgraph_filename = optarg;
    break;
  case OPTION_CALLGRAPH_OUTPUT:
    if (strcmp (optarg, "html") == 0)
      csky_config.callgraph_fmt = html;
    else if (strcmp (optarg, "text") == 0)
      csky_config.callgraph_fmt = text;
    else
      einfo (_("%P%F: unsupported file format: %s\n"), optarg);
    break;
#if ENABLE_ANY
  case OPTION_ANY_CONTINGENCY:
    {
      const char *end;

      any_contingency = bfd_scan_vma (optarg, &end, 0);
      if (*end)
        einfo (_("%P%F: invalid number `%s'\''\n"), optarg);
      else if (any_contingency < 0 || any_contingency > 100)
        einfo (_("%P%F: the any_contingency number must between 0-100.\n"));
      break;
    }
  case OPTION_ANY_SORTORDER:
    {
      if (strcasecmp (optarg, "cmdline") == 0)
        any_sort_order = ANY_SORT_ORDER_CMDLINE;
      else if (strcasecmp (optarg, "descending_size") == 0)
        any_sort_order = ANY_SORT_ORDER_DESCENDING_SIZE;
      else
        einfo (_("%P%F: Unrecognized any sort order: %s\n"), optarg);
      break;
    }
#endif
'
    break
    ;;

    *)
PARSE_AND_LIST_PROLOGUE='
#define OPTION_BASE_FILE                    300
#define OPTION_BRANCH_STUB                  301
#define OPTION_NO_BRANCH_STUB               302
#define OPTION_STUBGROUP_SIZE               303
#define OPTION_CSKY_MAP                     304
#define OPTION_CALLGRAPH                    305
#define OPTION_CALLGRAPH_FILE               306
#define OPTION_CALLGRAPH_OUTPUT             307
#define OPTION_ANY_CONTINGENCY              308
#define OPTION_ANY_SORTORDER                309
'

PARSE_AND_LIST_LONGOPTS='
  {"base-file",        required_argument, NULL, OPTION_BASE_FILE},
  {"branch-stub",      no_argument,       NULL, OPTION_BRANCH_STUB},
  {"no-branch-stub",   no_argument,       NULL, OPTION_NO_BRANCH_STUB},
  {"stub-group-size",  required_argument, NULL, OPTION_STUBGROUP_SIZE},
  {"ckmap",            required_argument, NULL, OPTION_CSKY_MAP},
  {"callgraph",        no_argument,       NULL, OPTION_CALLGRAPH},
  {"callgraph_file",   required_argument, NULL, OPTION_CALLGRAPH_FILE},
  {"callgraph_output", required_argument, NULL, OPTION_CALLGRAPH_OUTPUT},
#if ENABLE_ANY
  {"any_contingency",  required_argument, NULL, OPTION_ANY_CONTINGENCY},
  {"any_sort_order",   required_argument, NULL, OPTION_ANY_SORTORDER},
#endif
'
PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("  --base_file <basefile>\n"));
  fprintf (file, _("\t\t\tGenerate a base file for relocatable DLLs\n"));
  fprintf (file, _("  --[no-]branch-stub\n"));
  fprintf (file, _("\t\t\tDisable/enable linker to use stub to expand branch instruction\n\
\t\t\twhich cannot reach the target. \n"));
  fprintf (file, _("  --ckmap=<mapfile>\n"));
  fprintf (file, _("  --stub-group-size=N\n"));
  fprintf (file, _("\t\t\tMaximum size of a group of input sections that can be\n\
\t\t\thandled by one stub section.  A negative value\n\
\t\t\tlocates all stubs after their branches (with a\n\
\t\t\tgroup size of -N), while a positive value allows\n\
\t\t\ttwo groups of input sections, one before, and one\n\
\t\t\tafter each stub section.  Values of +/-1 indicate\n\
\t\t\tthe linker should choose suitable defaults.\n"));
  fprintf (file, _("  --callgraph\n"));
  fprintf (file, _("\t\t\tOutput callgraph file. \n"));
  fprintf (file, _("  --callgraph_file=<filename>\n"));
  fprintf (file, _("\t\t\tControl the the filename of output callgraph. \n"));
  fprintf (file, _("  --callgraph_output=<fmt>\n"));
  fprintf (file, _("\t\t\tControl the the format of output callgraph, html(default) or txt. \n"));
#if ENABLE_ANY
  fprintf (file, _("  --any_contingency=N\n"));
  fprintf (file, _("\t\t\tPermits N/100 extra space in any execution regions containing\n"\
                   "\t\t\t.ANY sections for linker-generated content such as\n"\
                   "\t\t\tveneers and alignment padding.\n"));
  fprintf (file, _("  --any_sort_order=[cmdline|descending_size]"));
  fprintf (file, _("\t\t\tControls the sort order of input sections that are placed using\n"\
                   "\t\t\tthe .ANY module selector.\n"));
#endif

'

PARSE_AND_LIST_ARGS_CASES='
  case OPTION_BASE_FILE:
    link_info.base_file = fopen (optarg, FOPEN_WB);
    if (link_info.base_file == NULL)
      {
        /* xgettext:c-format */
        fprintf (stderr, _("%s: Cannot open base file %s\n"),
        program_name, optarg);
        xexit (1);
      }
    break;

  case OPTION_BRANCH_STUB:
    use_branch_stub = TRUE;
    break;
  case OPTION_NO_BRANCH_STUB:
    use_branch_stub = FALSE;
    break;

  case OPTION_STUBGROUP_SIZE:
    {
      const char *end;

      group_size = bfd_scan_vma (optarg, &end, 0);
      if (*end)
        einfo (_("%P%F: invalid number `%s'\''\n"), optarg);
    }
    break;

  case OPTION_CSKY_MAP:
    csky_config.cskymap_filename = optarg;
    break;
  case OPTION_CALLGRAPH:
    csky_config.do_callgraph = TRUE;
    break;
  case OPTION_CALLGRAPH_FILE:
    csky_config.do_callgraph = TRUE;
    csky_config.callgraph_filename = optarg;
    break;
  case OPTION_CALLGRAPH_OUTPUT:
    if (strcmp (optarg, "html") == 0)
      csky_config.callgraph_fmt = html;
    else if (strcmp (optarg, "text") == 0)
      csky_config.callgraph_fmt = text;
    else
      einfo (_("%P%F: unsupported file format: %s\n"), optarg);
    break;
#if ENABLE_ANY
  case OPTION_ANY_CONTINGENCY:
    {
      const char *end;

      any_contingency = bfd_scan_vma (optarg, &end, 0);
      if (*end)
        einfo (_("%P%F: invalid number `%s'\''\n"), optarg);
      else if (any_contingency < 0 || any_contingency > 100)
        einfo (_("%P%F: the any_contingency number must between 0-100.\n"));
      break;
    }
  case OPTION_ANY_SORTORDER:
    {
      if (strcasecmp (optarg, "cmdline") == 0)
        any_sort_order = ANY_SORT_ORDER_CMDLINE;
      else if (strcasecmp (optarg, "descending_size") == 0)
        any_sort_order = ANY_SORT_ORDER_DESCENDING_SIZE;
      else
        einfo (_("%P%F: Unrecognized any sort order: %s\n"), optarg);
      break;
    }
#endif
'

    break;
    ;;
esac

LDEMUL_AFTER_ALLOCATION=gld${EMULATION_NAME}_after_allocation
LDEMUL_CREATE_OUTPUT_SECTION_STATEMENTS=csky_elf_create_output_section_statements
LDEMUL_FINISH=gld${EMULATION_NAME}_finish
LDEMUL_ARCH_MAP=csky_map
LDEMUL_BEFORE_WRITE=csky_ldmul_before_write
LDEMUL_BEFORE_FINISH=csky_ldmul_before_finish
