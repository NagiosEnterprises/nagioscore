/************************************************************************
 *
 * SKIPLIST.H - Skiplist data structures and functions
 *
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************/

#ifndef _SKIPLIST_H
#define _SKIPLIST_H
#include "lnag-utils.h"

#define SKIPLIST_OK              0 /**< A ok */
#define SKIPLIST_ERROR_ARGS      1 /**< Bad arguments */
#define SKIPLIST_ERROR_MEMORY    2 /**< Memory error */
#define SKIPLIST_ERROR_DUPLICATE 3 /**< Trying to insert non-unique item */

NAGIOS_BEGIN_DECL

struct skiplist_struct;
typedef struct skiplist_struct skiplist;

unsigned long skiplist_num_items(skiplist *list);
skiplist *skiplist_new(int max_levels, float level_probability, int allow_duplicates, int append_duplicates, int (*compare_function)(void *, void *));
int skiplist_insert(skiplist *list, void *data);
int skiplist_empty(skiplist *list);
int skiplist_free(skiplist **list);
void *skiplist_peek(skiplist *);
void *skiplist_pop(skiplist *);
void *skiplist_get_first(skiplist *list, void **node_ptr);
void *skiplist_get_next(void **node_ptr);
void *skiplist_find_first(skiplist *list, void *data, void **node_ptr);
void *skiplist_find_next(skiplist *list, void *data, void **node_ptr);
int skiplist_delete(skiplist *list, void *data);
int skiplist_delete_first(skiplist *list, void *data);
int skiplist_delete_all(skiplist *list, void *data);
int skiplist_delete_node(skiplist *list, void *node_ptr);

NAGIOS_END_DECL
#endif
