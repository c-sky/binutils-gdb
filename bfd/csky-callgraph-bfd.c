#include <sysdep.h>
#include <bfd.h>
#include <bfdlink.h>
#include <csky-callgraph-bfd.h>

/*do_start_addr : 1 ---- find function with function start address
 *                0 ---- find which function the address belong to*/
func_hash_bind *
find_func_with_addr (bfd_func_link * abfd_func_link,
                     func_hash_bind * current_func,
                     bfd_vma address, unsigned int do_start_addr)
{
  if (do_start_addr != 0 && address == SEC_DISCARDED)
    return NULL;

  if (current_func == NULL || current_func->hash_entry->start_addr > address)
    current_func = abfd_func_link->func_head;
  else
    {
      /*Do nothing */
    }

  while (current_func != NULL)
    {
      if (do_start_addr != 0)
        {
          if (current_func->hash_entry->start_addr == address)
            break;
        }
      else
        {
          bfd_vma func_end = current_func->hash_entry->start_addr
                             + current_func->hash_entry->function_size;
          if (address >= current_func->hash_entry->start_addr
              && address < func_end)
            break;
        }

      current_func = current_func->next;
    }

  return current_func;
}

void
func_vec_insert (func_vec ** list, callgraph_hash_entry * func_hash_entry)
{
  func_vec *p_elem = NULL, *current = NULL, *tail = NULL;
  p_elem = (func_vec *) malloc (sizeof (func_vec));
  memset (p_elem, 0, sizeof (func_vec));

  p_elem->hash_entry = func_hash_entry;
  p_elem->next = NULL;

  if (*list == NULL)
    {
      *list = p_elem;
    }
  else
    {
      current = *list;
      while (current != NULL)
        {
          tail = current;
          if (tail->hash_entry == func_hash_entry)
            return;
          current = current->next;
        }
      tail->next = p_elem;
    }

}

void
func_vec_clean (func_vec ** list)
{
  func_vec *p_current = *list;
  func_vec *p_next = NULL;

  while (p_current != NULL)
    {
      p_next = p_current->next;
      free (p_current);
      p_current = p_next;
    }
  list = NULL;

  return;
}

bfd_boolean
callgraph_hash_table_init (struct callgraph_hash_table * table,
                           struct callgraph_hash_entry *
                           (*newfunc) (struct bfd_hash_entry *,
                                       struct bfd_hash_table *, const char *),
                           unsigned int entsize)
{
  table->last_id = 0;
  bfd_list_init (table);
  return bfd_hash_table_init (&table->table, newfunc, entsize);
}


struct callgraph_hash_entry *
callgraph_hash_lookup (struct callgraph_hash_table *table,
                       const char *string,
                       bfd_boolean create, bfd_boolean copy)
{
  struct callgraph_hash_entry *ret;

  ret = ((struct callgraph_hash_entry *)
         bfd_hash_lookup (&table->table, string, create, copy));

  return ret;
}

void
callgraph_hash_traverse (struct callgraph_hash_table *table,
                         bfd_boolean (*func) (struct callgraph_hash_entry *,
                             void *), void *info)
{
  bfd_hash_traverse (&table->table,
                     (bfd_boolean (*)(struct bfd_hash_entry *, void *)) func,
                     info);
}

struct callgraph_hash_entry *
callgraph_hash_newfunc (struct bfd_hash_entry *entry,
                        struct bfd_hash_table *table, const char *string)
{
  if (entry == NULL)
    {
      entry = (struct bfd_hash_entry *)
              bfd_hash_allocate (table, sizeof (callgraph_hash_entry));
      if (entry == NULL)
        return entry;
    }

  entry = bfd_hash_newfunc (entry, table, string);
  if (entry)
    {
      struct callgraph_hash_entry *h = (struct callgraph_hash_entry *) entry;

      h->id = ((callgraph_hash_table *) table)->last_id++;
      h->stack_size = 0;
      h->max_depth = 0;
      h->max_depth_route = NULL;
      h->max_in_cycle = FALSE;
      h->start_addr = 0;
      h->function_size = 0;
      h->section = NULL;
      h->calls = NULL;
      h->call_bys = NULL;
      h->has_traversed = FALSE;
      add_ref_list_init (&h->add_refs);
    }

  return entry;
}

bfd_boolean
creat_csky_callgraph_table (struct elf_link_hash_entry * h, void *data)
{
  callgraph_hash_entry *ret;

  ret = callgraph_hash_lookup ((callgraph_hash_table *) data,
                               h->root.root.string, 1, 0);

  if (ret == NULL)
    return FALSE;
  else
    return TRUE;
}

static bfd_boolean
reset_entry_has_traversed (struct callgraph_hash_entry *h, void *data)
{
  h->has_traversed = FALSE;
  return TRUE;
}

void
reset_has_traversed (callgraph_hash_table * table)
{
  callgraph_hash_traverse (table, reset_entry_has_traversed, (void *) NULL);
}


void
sym_hash_bind_list_init (bfd_func_link * list)
{
  list->func_head = NULL;
  list->func_tail = NULL;
}

void
sym_hash_bind_list_insert (bfd_func_link * list, func_hash_bind elem)
{
  func_hash_bind *p_elem = NULL;
  p_elem = (func_hash_bind *) malloc (sizeof (func_hash_bind));
  memset (p_elem, 0, sizeof (func_hash_bind));

  p_elem->hash_entry = elem.hash_entry;
  p_elem->next = NULL;

  if (list->func_head == NULL)
    {
      list->func_head = p_elem;
      list->func_tail = p_elem;
    }
  else
    {
      list->func_tail->next = p_elem;
      list->func_tail = p_elem;
    }

  return;
}

void
sym_hash_bind_list_clean (bfd_func_link * list)
{
  func_hash_bind *p_current = list->func_head;
  func_hash_bind *p_next = NULL;

  while (p_current != NULL)
    {
      p_next = p_current->next;
      free (p_current);
      p_current = p_next;
    }
  list->func_head = NULL;
  list->func_tail = NULL;

  return;
}

void
bfd_list_init (callgraph_hash_table * list)
{
  list->bfd_head = NULL;
  list->bfd_tail = NULL;
}

void
bfd_list_insert (callgraph_hash_table * list, bfd_func_link elem)
{
  bfd_func_link *p_elem = NULL;
  p_elem = (bfd_func_link *) malloc (sizeof (bfd_func_link));
  memset (p_elem, 0, sizeof (bfd_func_link));

  p_elem->func_head = elem.func_head;
  p_elem->func_tail = elem.func_tail;
  p_elem->sorted_symcount = elem.sorted_symcount;
  p_elem->abfd = elem.abfd;
  p_elem->next = NULL;

  if (list->bfd_head == NULL)
    {
      list->bfd_head = p_elem;
      list->bfd_tail = p_elem;
    }
  else
    {
      list->bfd_tail->next = p_elem;
      list->bfd_tail = p_elem;
    }

  return;
}

bfd_func_link *
bfd_list_search (callgraph_hash_table list, bfd * abfd)
{
  bfd_func_link *p_elem = list.bfd_head;

  while (p_elem != NULL)
    {
      if (p_elem->abfd == abfd)
        break;
      else
        p_elem = p_elem->next;
    }

  return p_elem;
}

void
bfd_list_clean (callgraph_hash_table * list)
{
  bfd_func_link *p_current = list->bfd_head;
  bfd_func_link *p_next = NULL;

  while (p_current != NULL)
    {
      p_next = p_current->next;
      free (p_current);
      p_current = p_next;
    }
  list->bfd_head = NULL;
  list->bfd_tail = NULL;

  return;
}

void
func_link_list_init (func_link_list * list)
{
  list->head = NULL;
  list->tail = NULL;
}

void
func_link_list_insert (func_link_list * list, bfd_func_link elem)
{
  bfd_func_link *p_elem = NULL;
  p_elem = (bfd_func_link *) malloc (sizeof (bfd_func_link));
  memset (p_elem, 0, sizeof (bfd_func_link));

  p_elem->func_head = elem.func_head;
  p_elem->func_tail = elem.func_tail;
  p_elem->sorted_symcount = elem.sorted_symcount;
  p_elem->abfd = elem.abfd;
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

void
func_link_list_clean (func_link_list * list)
{
  bfd_func_link *p_current = list->head;
  bfd_func_link *p_next = NULL;

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

void
add_ref_list_init (add_ref_list * list)
{
  list->ref_count = 0;
  list->head = NULL;
  list->tail = NULL;
}

void
add_ref_list_insert (add_ref_list * list, add_ref elem)
{
  add_ref *p_elem = NULL;

  list->ref_count++;
  /*if the seciton has been add to the list,
   * just add the count and return. */
  p_elem = list->head;
  while (p_elem != NULL)
    {
      if (p_elem->section == elem.section)
        return;
      else
        p_elem = p_elem->next;
    }
  /*if the section has no been add to the list,
   * add it to the list. */
  p_elem = (add_ref *) malloc (sizeof (add_ref));
  memset (p_elem, 0, sizeof (add_ref));

  p_elem->section = elem.section;
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

void
add_ref_list_clean (add_ref_list * list)
{
  add_ref *p_current = list->head;
  add_ref *p_next = NULL;

  while (p_current != NULL)
    {
      p_next = p_current->next;
      free (p_current);
      p_current = p_next;
    }
  list->head = NULL;
  list->tail = NULL;
  list->ref_count = 0;

  return;
}

void
sym_hash_bind_list_copy (func_hash_bind * start_elem, bfd_func_link * list)
{
  func_hash_bind *current = start_elem;

  while (current != NULL)
    {
      sym_hash_bind_list_insert (list, *current);
      current = current->next;
    }

  return;
}

func_hash_bind *
func_stack_search (bfd_func_link * stack,
                   struct callgraph_hash_entry * func_entry)
{
  func_hash_bind *current = stack->func_head;

  while (current != NULL)
    {
      if (current->hash_entry == func_entry)
        return current;
      current = current->next;
    }

  return NULL;
}

void
func_stack_pop (bfd_func_link * stack)
{
  if (stack->func_head == NULL)
    return;
  else if (stack->func_head == stack->func_tail)
    {
      free (stack->func_head);
      stack->func_head = NULL;
      stack->func_tail = NULL;
      return;
    }
  else
    {
      func_hash_bind *current = stack->func_head;

      while (current->next != stack->func_tail)
        current = current->next;

      free (stack->func_tail);
      current->next = NULL;
      stack->func_tail = current;
      return;
    }
}

static bfd_boolean
clean_func_reference (struct callgraph_hash_entry *h, void *data)
{
  if (h->calls != NULL)
    func_vec_clean (&h->calls);
  if (h->call_bys != NULL)
    func_vec_clean (&h->call_bys);
  add_ref_list_clean (&h->add_refs);
  return TRUE;
}

void
callgraph_hash_table_clean (callgraph_hash_table * table)
{
  bfd_func_link *p_current = table->bfd_head;
  bfd_func_link *p_next = NULL;

  callgraph_hash_traverse (table, clean_func_reference, (void *) NULL);

  while (p_current != NULL)
    {
      p_next = p_current->next;
      sym_hash_bind_list_clean (p_current);
      free (p_current);
      p_current = p_next;
    }
  table->bfd_head = NULL;
  table->bfd_tail = NULL;

  bfd_hash_table_free (&table->table);

  return;
}
