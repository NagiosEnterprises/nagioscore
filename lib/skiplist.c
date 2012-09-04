/************************************************************************
 *
 * SKIPLIST.C - Skiplist functions for use in Nagios event/object lists
 *
 *
 * Notes:
 *
 * These function implement a slightly modified skiplist from that
 * described by William Pugh (ftp://ftp.cs.umd.edu/pub/skipLists/skiplists.pdf).
 * The structures and function were modified to allow the list to act
 * like a priority queue for the Nagios event list/queue(s).  Multiple nodes with
 * the same key value are allowed on the list to accomodate multiple events
 * occurring at the same (second) point in time.  Implemented peek() and pop()
 * functions to allow for quick event queue processing, and a method to delete
 * a specific list item, based on its pointer, rather than its data value.  Again,
 * this is useful for the Nagios event queue.
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

#include <unistd.h>
#include <stdlib.h>
#include "skiplist.h"


typedef struct skiplistnode_struct {
	void *data;
	struct skiplistnode_struct *forward[1]; /* this must be the last element of the struct, as we allocate # of elements during runtime*/
	} skiplistnode;

struct skiplist_struct {
	int current_level;
	int max_levels;
	float level_probability;
	unsigned long items;
	int allow_duplicates;
	int append_duplicates;
	int (*compare_function)(void *, void *);
	skiplistnode *head;
	};

unsigned long skiplist_num_items(skiplist *list) {
	return list ? list->items : 0;
	}

static skiplistnode *skiplist_new_node(skiplist *list, int node_levels) {
	skiplistnode *newnode = NULL;
	register int x = 0;

	if(list == NULL)
		return NULL;

	if(node_levels < 0 || node_levels > list->max_levels)
		return NULL;

	/* allocate memory for node + variable number of level pointers */
	if((newnode = (skiplistnode *)malloc(sizeof(skiplistnode) + (node_levels * sizeof(skiplistnode *))))) {

		/* initialize forward pointers */
		for(x = 0; x < node_levels; x++)
			newnode->forward[x] = NULL;

		/* initialize data pointer */
		newnode->data = NULL;
		}

	return newnode;
	}


skiplist *skiplist_new(int max_levels, float level_probability, int allow_duplicates, int append_duplicates, int (*compare_function)(void *, void *)) {
	skiplist *newlist = NULL;

	/* alloc memory for new list structure */
	if((newlist = (skiplist *)malloc(sizeof(skiplist)))) {

		/* initialize levels, etc. */
		newlist->current_level = 0;
		newlist->max_levels = max_levels;
		newlist->level_probability = level_probability;
		newlist->allow_duplicates = allow_duplicates;
		newlist->append_duplicates = append_duplicates;
		newlist->items = 0;
		newlist->compare_function = compare_function;

		/* initialize head node */
		newlist->head = skiplist_new_node(newlist, max_levels);
		}

	return newlist;
	}


static int skiplist_random_level(skiplist *list) {
	int level = 0;
	float r = 0.0;

	if(list == NULL)
		return -1;

	for(level = 0; level < list->max_levels; level++) {
		r = ((float)rand() / (float)RAND_MAX);
		if(r > list->level_probability)
			break;
		}

	return (level >= list->max_levels) ? list->max_levels - 1 : level;
	}


int skiplist_insert(skiplist *list, void *data) {
	skiplistnode **update = NULL;
	skiplistnode *thisnode = NULL;
	skiplistnode *nextnode = NULL;
	skiplistnode *newnode = NULL;
	int level = 0;
	int x = 0;

	if(list == NULL || data == NULL) {
		return SKIPLIST_ERROR_ARGS;
		}

	/* check to make sure we don't have duplicates */
	/* NOTE: this could made be more efficient */
	if(list->allow_duplicates == FALSE) {
		if(skiplist_find_first(list, data, NULL))
			return SKIPLIST_ERROR_DUPLICATE;
		}

	/* initialize update vector */
	if((update = (skiplistnode **)malloc(sizeof(skiplistnode *) * list->max_levels)) == NULL) {
		return SKIPLIST_ERROR_MEMORY;
		}
	for(x = 0; x < list->max_levels; x++)
		update[x] = NULL;

	/* find proper position for insert, remember pointers  with an update vector */
	thisnode = list->head;
	for(level = list->current_level; level >= 0; level--) {

		while((nextnode = thisnode->forward[level])) {
			if(list->append_duplicates == TRUE) {
				if(list->compare_function(nextnode->data, data) > 0)
					break;
				}
			else {
				if(list->compare_function(nextnode->data, data) >= 0)
					break;
				}
			thisnode = nextnode;
			}

		update[level] = thisnode;
		}

	/* get a random level the new node should be inserted at */
	level = skiplist_random_level(list);

	/* we're adding a new level... */
	if(level > list->current_level) {
		/*printf("NEW LEVEL!\n");*/
		list->current_level++;
		level = list->current_level;
		update[level] = list->head;
		}

	/* create a new node */
	if((newnode = skiplist_new_node(list, level)) == NULL) {
		/*printf("NODE ERROR\n");*/
		free(update);
		return SKIPLIST_ERROR_MEMORY;
		}
	newnode->data = data;

	/* update pointers to insert node at proper location */
	do {
		thisnode = update[level];
		newnode->forward[level] = thisnode->forward[level];
		thisnode->forward[level] = newnode;

		}
	while(--level >= 0);

	/* update counters */
	list->items++;

	/* free memory */
	free(update);

	return SKIPLIST_OK;
	}



int skiplist_empty(skiplist *list) {
	skiplistnode *this = NULL;
	skiplistnode *next = NULL;
	int level = 0;

	if(list == NULL)
		return ERROR;

	/* free all list nodes (but not header) */
	for(this = list->head->forward[0]; this != NULL; this = next) {
		next = this->forward[0];
		free(this);
		}

	/* reset level pointers */
	for(level = list->current_level; level >= 0; level--)
		list->head->forward[level] = NULL;

	/* reset list level */
	list->current_level = 0;

	/* reset items */
	list->items = 0;

	return OK;
	}



int skiplist_free(skiplist **list) {
	skiplistnode *this = NULL;
	skiplistnode *next = NULL;

	if(list == NULL)
		return ERROR;
	if(*list == NULL)
		return OK;

	/* free header and all list nodes */
	for(this = (*list)->head; this != NULL; this = next) {
		next = this->forward[0];
		free(this);
		}

	/* free list structure */
	free(*list);
	*list = NULL;

	return OK;
	}



/* get first item in list */
void *skiplist_peek(skiplist *list) {

	if(list == NULL)
		return NULL;

	/* return first item */
	return list->head->forward[0]->data;
	}



/* get/remove first item in list */
void *skiplist_pop(skiplist *list) {
	skiplistnode *thisnode = NULL;
	void *data = NULL;
	int level = 0;

	if(list == NULL)
		return NULL;

	/* get first item */
	thisnode = list->head->forward[0];
	if(thisnode == NULL)
		return NULL;

	/* get data for first item */
	data = thisnode->data;

	/* remove first item from queue - update forward links from head to first node */
	for(level = 0; level <= list->current_level; level++) {
		if(list->head->forward[level] == thisnode)
			list->head->forward[level] = thisnode->forward[level];
		}

	/* free deleted node */
	free(thisnode);

	/* adjust items */
	list->items--;

	return data;
	}



/* get first item in list */
void *skiplist_get_first(skiplist *list, void **node_ptr) {
	skiplistnode *thisnode = NULL;

	if(list == NULL)
		return NULL;

	/* get first node */
	thisnode = list->head->forward[0];

	/* return pointer to node */
	if(node_ptr)
		*node_ptr = (void *)thisnode;

	if(thisnode)
		return thisnode->data;
	else
		return NULL;
	}



/* get next item in list */
void *skiplist_get_next(void **node_ptr) {
	skiplistnode *thisnode = NULL;
	skiplistnode *nextnode = NULL;

	if(node_ptr == NULL || *node_ptr == NULL)
		return NULL;

	thisnode = (skiplistnode *)(*node_ptr);
	nextnode = thisnode->forward[0];

	*node_ptr = (void *)nextnode;

	if(nextnode)
		return nextnode->data;
	else
		return NULL;
	}



/* first first item in list */
void *skiplist_find_first(skiplist *list, void *data, void **node_ptr) {
	skiplistnode *thisnode = NULL;
	skiplistnode *nextnode = NULL;
	int level = 0;

	if(list == NULL || data == NULL)
		return NULL;

	thisnode = list->head;
	for(level = list->current_level; level >= 0; level--) {
		while((nextnode = thisnode->forward[level])) {
			if(list->compare_function(nextnode->data, data) >= 0)
				break;
			thisnode = nextnode;
			}
		}

	/* we found it! */
	if(nextnode && list->compare_function(nextnode->data, data) == 0) {
		if(node_ptr)
			*node_ptr = (void *)nextnode;
		return nextnode->data;
		}
	else {
		if(node_ptr)
			*node_ptr = NULL;
		}

	return NULL;
	}



/* find next match */
void *skiplist_find_next(skiplist *list, void *data, void **node_ptr) {
	skiplistnode *thisnode = NULL;
	skiplistnode *nextnode = NULL;

	if(list == NULL || data == NULL || node_ptr == NULL)
		return NULL;
	if(*node_ptr == NULL)
		return NULL;

	thisnode = (skiplistnode *)(*node_ptr);
	nextnode = thisnode->forward[0];

	if(nextnode) {
		if(list->compare_function(nextnode->data, data) == 0) {
			*node_ptr = (void *)nextnode;
			return nextnode->data;
			}
		}

	*node_ptr = NULL;
	return NULL;
	}



/* delete first matching item from list */
int skiplist_delete_first(skiplist *list, void *data) {
	skiplistnode **update = NULL;
	skiplistnode *thisnode = NULL;
	skiplistnode *nextnode = NULL;
	int level = 0;
	int top_level = 0;
	int deleted = FALSE;
	int x = 0;

	if(list == NULL || data == NULL)
		return ERROR;

	/* initialize update vector */
	if((update = (skiplistnode **)malloc(sizeof(skiplistnode *) * list->max_levels)) == NULL)
		return ERROR;
	for(x = 0; x < list->max_levels; x++)
		update[x] = NULL;

	/* find location in list */
	thisnode = list->head;
	for(top_level = level = list->current_level; level >= 0; level--) {
		while((nextnode = thisnode->forward[level])) {
			if(list->compare_function(nextnode->data, data) >= 0)
				break;
			thisnode = nextnode;
			}
		update[level] = thisnode;
		}

	/* we found a match! */
	if(list->compare_function(nextnode->data, data) == 0) {

		/* adjust level pointers to bypass (soon to be) removed node */
		for(level = 0; level <= top_level; level++) {

			thisnode = update[level];
			if(thisnode->forward[level] != nextnode)
				break;

			thisnode->forward[level] = nextnode->forward[level];
			}

		/* free node memory */
		free(nextnode);

		/* adjust top/current level of list is necessary */
		while(list->head->forward[top_level] == NULL && top_level > 0)
			top_level--;
		list->current_level = top_level;

		/* adjust items */
		list->items--;

		deleted = TRUE;
		}

	/* free memory */
	free(update);

	return deleted;
	}



/* delete all matching items from list */
int skiplist_delete(skiplist *list, void *data) {
	int deleted = 0;
	int total_deleted = 0;

	/* NOTE: there is a more efficient way to do this... */
	while((deleted = skiplist_delete_first(list, data)) == 1)
		total_deleted++;

	return total_deleted;
	}



/* delete specific node from list */
int skiplist_delete_node(skiplist *list, void *node_ptr) {
	void *data = NULL;
	skiplistnode **update = NULL;
	skiplistnode *thenode = NULL;
	skiplistnode *thisnode = NULL;
	skiplistnode *nextnode = NULL;
	int level = 0;
	int top_level = 0;
	int deleted = FALSE;
	int x = 0;

	if(list == NULL || node_ptr == NULL)
		return ERROR;

	/* we'll need the data from the node to first find the node */
	thenode = (skiplistnode *)node_ptr;
	data = thenode->data;

	/* initialize update vector */
	if((update = (skiplistnode **)malloc(sizeof(skiplistnode *) * list->max_levels)) == NULL)
		return ERROR;
	for(x = 0; x < list->max_levels; x++)
		update[x] = NULL;

	/* find location in list */
	thisnode = list->head;
	for(top_level = level = list->current_level; level >= 0; level--) {
		while((nextnode = thisnode->forward[level])) {

			/* next node would be too far */
			if(list->compare_function(nextnode->data, data) > 0)
				break;
			/* this is the exact node we want */
			if(list->compare_function(nextnode->data, data) == 0 && nextnode == thenode)
				break;

			thisnode = nextnode;
			}
		update[level] = thisnode;
		}

	/* we found a match! (value + pointers match) */
	if(nextnode && list->compare_function(nextnode->data, data) == 0 && nextnode == thenode) {

		/* adjust level pointers to bypass (soon to be) removed node */
		for(level = 0; level <= top_level; level++) {

			thisnode = update[level];
			if(thisnode->forward[level] != nextnode)
				break;

			thisnode->forward[level] = nextnode->forward[level];
			}

		/* free node memory */
		free(nextnode);

		/* adjust top/current level of list is necessary */
		while(list->head->forward[top_level] == NULL && top_level > 0)
			top_level--;
		list->current_level = top_level;

		/* adjust items */
		list->items--;

		deleted = TRUE;
		}

	/* free memory */
	free(update);

	return deleted;
	}



