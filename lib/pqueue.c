/*
 * Copyright 2010 Volkan Yazıcı <volkan.yazici@gmail.com>
 * Copyright 2006-2010 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pqueue.h"


#define left(i)   (unsigned long)((i) << 1)
#define right(i)  (unsigned long)(((i) << 1) + 1)
#define parent(i) (unsigned long)((i) >> 1)


pqueue_t *
pqueue_init(unsigned int n,
            pqueue_cmp_pri_f cmppri,
            pqueue_get_pri_f getpri,
            pqueue_set_pri_f setpri,
            pqueue_get_pos_f getpos,
            pqueue_set_pos_f setpos)
{
	pqueue_t *q;

	if (!(q = calloc(1, sizeof(pqueue_t)))) {
		return NULL;
	}

	/* Need to allocate n+1 elements since element 0 isn't used. */
	if (!(q->d = calloc(n + 1, sizeof(void *)))) {
		free(q);
		return NULL;
	}

	q->size = 1;
	q->avail = q->step = (n + 1); /* see comment above about n+1 */
	q->cmppri = cmppri;
	q->setpri = setpri;
	q->getpri = getpri;
	q->getpos = getpos;
	q->setpos = setpos;

	return q;
}


void
pqueue_free(pqueue_t *q)
{
	free(q->d);
	free(q);
}


unsigned int
pqueue_size(pqueue_t *q)
{
	/* queue element 0 exists but doesn't count since it isn't used. */
	return (q->size - 1);
}


static void
bubble_up(pqueue_t *q, unsigned int i)
{
	unsigned int parent_node;
	void *moving_node = q->d[i];
	pqueue_pri_t moving_pri = q->getpri(moving_node);

	for (parent_node = parent(i);
	        ((i > 1) && q->cmppri(q->getpri(q->d[parent_node]), moving_pri));
	        i = parent_node, parent_node = parent(i)) {
		q->d[i] = q->d[parent_node];
		q->setpos(q->d[i], i);
	}

	q->d[i] = moving_node;
	q->setpos(moving_node, i);
}


static unsigned int
maxchild(pqueue_t *q, unsigned int i)
{
	unsigned int child_node = left(i);

	if (child_node >= q->size) {
		return 0;
	}

	if ((child_node + 1) < q->size && q->cmppri(q->getpri(q->d[child_node]), q->getpri(q->d[child_node + 1]))) {
		child_node++;    /* use right child instead of left */
	}

	return child_node;
}


static void
percolate_down(pqueue_t *q, unsigned int i)
{
	unsigned int child_node;
	void *moving_node = q->d[i];
	pqueue_pri_t moving_pri = q->getpri(moving_node);

	while ((child_node = maxchild(q, i)) &&
	        q->cmppri(moving_pri, q->getpri(q->d[child_node]))) {
		q->d[i] = q->d[child_node];
		q->setpos(q->d[i], i);
		i = child_node;
	}

	q->d[i] = moving_node;
	q->setpos(moving_node, i);
}


int
pqueue_insert(pqueue_t *q, void *d)
{
	void *tmp;
	unsigned int i;
	unsigned int newsize;

	if (!q) {
		return 1;
	}

	/* allocate more memory if necessary */
	if (q->size >= q->avail) {
		newsize = q->size + q->step;
		if (!(tmp = realloc(q->d, sizeof(void *) * newsize))) {
			return 1;
		}
		q->d = tmp;
		q->avail = newsize;
	}

	/* insert item */
	i = q->size++;
	q->d[i] = d;
	bubble_up(q, i);

	return 0;
}


void
pqueue_change_priority(pqueue_t *q, pqueue_pri_t new_pri, void *d)
{
	unsigned int posn;
	pqueue_pri_t old_pri = q->getpri(d);

	q->setpri(d, new_pri);
	posn = q->getpos(d);
	if (q->cmppri(old_pri, new_pri)) {
		bubble_up(q, posn);
	} else {
		percolate_down(q, posn);
	}
}


int
pqueue_remove(pqueue_t *q, void *d)
{
	unsigned int posn = q->getpos(d);
	q->d[posn] = q->d[--q->size];
	if (q->cmppri(q->getpri(d), q->getpri(q->d[posn]))) {
		bubble_up(q, posn);
	} else {
		percolate_down(q, posn);
	}

	return 0;
}


void *
pqueue_pop(pqueue_t *q)
{
	void *head;

	if (!q || q->size == 1) {
		return NULL;
	}

	head = q->d[1];
	q->d[1] = q->d[--q->size];
	percolate_down(q, 1);

	return head;
}


void *
pqueue_peek(pqueue_t *q)
{
	if (!q || q->size == 1) {
		return NULL;
	}
	return q->d[1];
}

#if 0
void
pqueue_dump(pqueue_t *q, FILE *out, pqueue_print_entry_f print)
{
	int i;

	fprintf(out, "posn\tleft\tright\tparent\tmaxchild\t...\n");
	for (i = 1; i < q->size ; i++) {
		fprintf(out, "%d\t%d\t%d\t%d\t%ul\t",
		        i,
		        left(i), right(i), parent(i),
		        (unsigned int)maxchild(q, i));
		print(out, q->d[i]);
	}
}


static void
set_pos(void *d, unsigned int val)
{
	/* do nothing */
}


static void
set_pri(void *d, pqueue_pri_t pri)
{
	/* do nothing */
}


void
pqueue_print(pqueue_t *q, FILE *out, pqueue_print_entry_f print)
{
	pqueue_t *dup;
	void *e;

	dup = pqueue_init(q->size, q->cmppri, q->getpri, set_pri,
	                  q->getpos, set_pos);
	dup->size = q->size;
	dup->avail = q->avail;
	dup->step = q->step;

	memcpy(dup->d, q->d, (q->size * sizeof(void *)));

	while ((e = pqueue_pop(dup))) {
		print(out, e);
	}

	pqueue_free(dup);
}
#endif

static int
subtree_is_valid(pqueue_t *q, int pos)
{
	if (left(pos) < q->size) {
		/* has a left child */
		if (q->cmppri(q->getpri(q->d[pos]), q->getpri(q->d[left(pos)]))) {
			return 0;
		}
		if (!subtree_is_valid(q, left(pos))) {
			return 0;
		}
	}
	if (right(pos) < q->size) {
		/* has a right child */
		if (q->cmppri(q->getpri(q->d[pos]), q->getpri(q->d[right(pos)]))) {
			return 0;
		}
		if (!subtree_is_valid(q, right(pos))) {
			return 0;
		}
	}
	return 1;
}


int
pqueue_is_valid(pqueue_t *q)
{
	return subtree_is_valid(q, 1);
}
