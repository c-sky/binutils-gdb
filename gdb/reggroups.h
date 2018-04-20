/* Register groupings for GDB, the GNU debugger.

   Copyright (C) 2002-2016 Free Software Foundation, Inc.

   Contributed by Red Hat.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef REGGROUPS_H
#define REGGROUPS_H

struct gdbarch;

#ifndef CSKYGDB_CONFIG
struct reggroup;

enum reggroup_type { USER_REGGROUP, INTERNAL_REGGROUP };

#else /* CSKYGDB_CONFIG */

enum reggroup_type { USER_REGGROUP, INTERNAL_REGGROUP };

struct reggroup
{
  const char *name;
  enum reggroup_type type;
};

/* A linked list of groups for the given architecture.  */

struct reggroup_el
{
  struct reggroup *group;
  struct reggroup_el *next;
};
#endif /* CSKYGDB_CONFIG */


/* Pre-defined, user visible, register groups.  */
extern struct reggroup *const general_reggroup;
extern struct reggroup *const float_reggroup;
extern struct reggroup *const system_reggroup;
extern struct reggroup *const vector_reggroup;
extern struct reggroup *const all_reggroup;

/* Pre-defined, internal, register groups.  */
extern struct reggroup *const save_reggroup;
extern struct reggroup *const restore_reggroup;

/* Create a new local register group.  */
extern struct reggroup *reggroup_new (const char *name,
				      enum reggroup_type type);

/* Add a register group (with attribute values) to the pre-defined list.  */
extern void reggroup_add (struct gdbarch *gdbarch, struct reggroup *group);

/* Register group attributes.  */
extern const char *reggroup_name (struct reggroup *reggroup);
extern enum reggroup_type reggroup_type (struct reggroup *reggroup);

/* Iterators for the architecture's register groups.  Pass in NULL, returns
   the first (for next), or last (for prev) group.  Pass in a group,
   returns the next or previous group, or NULL when either the end or the
   beginning of the group list is reached.  */
extern struct reggroup *reggroup_next (struct gdbarch *gdbarch,
				       struct reggroup *last);
extern struct reggroup *reggroup_prev (struct gdbarch *gdbarch,
				       struct reggroup *curr);

/* Is REGNUM a member of REGGROUP?  */
extern int default_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
					struct reggroup *reggroup);
#ifdef CSKYGDB_CONFIG
/* For csky reggroup add */
void csky_reggroup_name_add (struct reggroup_el **list, const char *name);
#endif

#endif
