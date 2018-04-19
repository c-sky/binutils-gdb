#ifndef _CSKY_CALLGRAPH_BFD_H_
#define _CSKY_CALLGRAPH_BFD_H_ 1
#include <elf-bfd.h>

/* The address of function discarded by linker will be set
 * to this value in '.csky_stack_size' section. */
#define SEC_DISCARDED 0xffffffff

typedef struct func_vec
{
  struct callgraph_hash_entry *hash_entry;
  struct func_vec *next;
} func_vec;

typedef struct add_ref
{
  asection *section;
  struct address_ref *next;
} add_ref;

typedef struct add_ref_list
{
  unsigned int ref_count;
  struct add_ref *head;
  struct add_ref *tail;
} add_ref_list;

typedef struct callgraph_hash_entry
{
  struct bfd_hash_entry root;
  unsigned int id;
  int stack_size;
  int max_depth;
  struct callgraph_hash_entry *max_depth_route;
  bfd_boolean max_in_cycle;
  bfd_vma start_addr;
  unsigned int function_size;
  asection *section;
  struct func_vec *calls;
  struct func_vec *call_bys;
  struct add_ref_list add_refs;
  bfd_boolean has_traversed;
} callgraph_hash_entry;

/*Each file has a bfd_func_link struct, a link tale will be built that include
 * every input file, bfd_head and bfd_tail point to this link table. And in
 * bfd_func_link, func_head and func_tail point to a link table that include
 * every function in the file, each function is represented by struct
 * func_hash_bind, the hash_entry in func_hash_bind point to the hash_entry
 * created by function name in the hash table. */
typedef struct callgraph_hash_table
{
  struct bfd_hash_table table;
  struct bfd_func_link *bfd_head;
  struct bfd_func_link *bfd_tail;
  unsigned int last_id;
} callgraph_hash_table;

typedef struct func_hash_bind
{
  callgraph_hash_entry *hash_entry;
  struct func_hash_bind *next;
} func_hash_bind;

typedef struct bfd_func_link
{
  func_hash_bind *func_head;
  func_hash_bind *func_tail;
  long sorted_symcount;
  bfd *abfd;
  struct bfd_func_link *next;
} bfd_func_link;

typedef struct func_link_list
{
  struct bfd_func_link *head;
  struct bfd_func_link *tail;
} func_link_list;


extern callgraph_hash_table func_hash_table;


extern func_hash_bind *find_func_with_addr (bfd_func_link * abfd_func_link,
    func_hash_bind * current_func,
    bfd_vma address,
    unsigned int do_start_addr);

extern void func_vec_insert (func_vec ** list,
                             callgraph_hash_entry * func_hash_entry);
extern void func_vec_clean (func_vec ** list);

extern bfd_boolean callgraph_hash_table_init (struct callgraph_hash_table
    *table,
    struct callgraph_hash_entry
    *(*newfunc) (struct
                 bfd_hash_entry *,
                 struct
                 bfd_hash_table *,
                 const char *),
    unsigned int entsize);
extern struct callgraph_hash_entry *callgraph_hash_lookup (struct
    callgraph_hash_table
    *table,
    const char *string,
    bfd_boolean create,
    bfd_boolean copy);
extern void callgraph_hash_traverse (struct callgraph_hash_table *table,
                                     bfd_boolean (*func) (struct
                                         callgraph_hash_entry
                                         *, void *),
                                     void *info);
extern struct callgraph_hash_entry *callgraph_hash_newfunc (struct
    bfd_hash_entry
    *entry,
    struct
    bfd_hash_table
    *table,
    const char
    *string);
extern bfd_boolean creat_csky_callgraph_table (struct elf_link_hash_entry *h,
    void *data);
extern void reset_has_traversed (callgraph_hash_table * table);
extern void callgraph_hash_table_clean (callgraph_hash_table * table);

extern void sym_hash_bind_list_init (bfd_func_link * list);
extern void sym_hash_bind_list_insert (bfd_func_link * list,
                                       func_hash_bind elem);
extern void sym_hash_bind_list_clean (bfd_func_link * list);
extern void sym_hash_bind_list_copy (func_hash_bind * start_elem,
                                     bfd_func_link * list);
extern func_hash_bind *func_stack_search (bfd_func_link * stack,
    struct callgraph_hash_entry
    *func_entry);
extern void func_stack_pop (bfd_func_link * stack);
#define func_stack_push(stack, elem) sym_hash_bind_list_insert(stack, elem)

extern void bfd_list_init (callgraph_hash_table * list);
extern void bfd_list_insert (callgraph_hash_table * list, bfd_func_link elem);
extern bfd_func_link *bfd_list_search (callgraph_hash_table list, bfd * abfd);
extern void bfd_list_clean (callgraph_hash_table * list);

extern void func_link_list_init (func_link_list * list);
extern void func_link_list_insert (func_link_list * list, bfd_func_link elem);
extern void func_link_list_clean (func_link_list * list);

extern void add_ref_list_init (add_ref_list * list);
extern void add_ref_list_insert (add_ref_list * list, add_ref elem);
extern void add_ref_list_clean (add_ref_list * list);
#endif
