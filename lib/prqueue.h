/*
 * Copyright 2010 Volkan Yazıcı <volkan.yazici@gmail.com>
 * Copyright 2006-2010 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
#ifndef LIBNAGIOS_PQUEUE_H_INCLUDED
#define LIBNAGIOS_PQUEUE_H_INCLUDED
#include <stdio.h>

/**
 * @file  prqueue.h
 * @brief Priority Queue function declarations
 *
 * This priority queue library was originally written by Volkan Yazici
 * <volkan.yazici@gmail.com>. It was lated adapted for Nagios by
 * Andreas Ericsson <ae@op5.se>. Changes compared to the original
 * version are pretty much limited to changing prqueue_pri_t to be
 * an unsigned long long instead of a double, since ULL comparisons
 * are 107 times faster on my 64-bit laptop.
 *
 * @{
 */


/** priority data type (used to be double, but ull is 107 times faster) */
typedef unsigned long long prqueue_pri_t;

/** callback functions to get/set/compare the priority of an element */
typedef prqueue_pri_t (*prqueue_get_pri_f)(void *a);
typedef void (*prqueue_set_pri_f)(void *a, prqueue_pri_t pri);
typedef int (*prqueue_cmp_pri_f)(prqueue_pri_t next, prqueue_pri_t curr);


/** callback functions to get/set the position of an element */
typedef unsigned int (*prqueue_get_pos_f)(void *a);
typedef void (*prqueue_set_pos_f)(void *a, unsigned int pos);


/** debug callback function to print a entry */
typedef void (*prqueue_print_entry_f)(FILE *out, void *a);


/** the priority queue handle */
typedef struct prqueue_t
{
    unsigned int size;       /**< number of elements in this queue */
    unsigned int avail;      /**< slots available in this queue */
    unsigned int step;       /**< growth stepping setting */
    prqueue_cmp_pri_f cmppri; /**< callback to compare nodes */
    prqueue_get_pri_f getpri; /**< callback to get priority of a node */
    prqueue_set_pri_f setpri; /**< callback to set priority of a node */
    prqueue_get_pos_f getpos; /**< callback to get position of a node */
    prqueue_set_pos_f setpos; /**< callback to set position of a node */
    void **d;                /**< The actual queue in binary heap form */
} prqueue_t;


/**
 * initialize the queue
 *
 * @param n the initial estimate of the number of queue items for which memory
 *          should be preallocated
 * @param cmppri The callback function to run to compare two elements
 *    This callback should return 0 for 'lower' and non-zero
 *    for 'higher', or vice versa if reverse priority is desired
 * @param setpri the callback function to run to assign a score to an element
 * @param getpri the callback function to run to set a score to an element
 * @param getpos the callback function to get the current element's position
 * @param setpos the callback function to set the current element's position
 *
 * @return the handle or NULL for insufficient memory
 */
prqueue_t *
prqueue_init(unsigned int n,
            prqueue_cmp_pri_f cmppri,
            prqueue_get_pri_f getpri,
            prqueue_set_pri_f setpri,
            prqueue_get_pos_f getpos,
            prqueue_set_pos_f setpos);


/**
 * free all memory used by the queue
 * @param q the queue
 */
void prqueue_free(prqueue_t *q);


/**
 * return the size of the queue.
 * @param q the queue
 */
unsigned int prqueue_size(prqueue_t *q);


/**
 * insert an item into the queue.
 * @param q the queue
 * @param d the item
 * @return 0 on success
 */
int prqueue_insert(prqueue_t *q, void *d);


/**
 * move an existing entry to a different priority
 * @param q the queue
 * @param new_pri the new priority
 * @param d the entry
 */
void
prqueue_change_priority(prqueue_t *q,
                       prqueue_pri_t new_pri,
                       void *d);


/**
 * pop the highest-ranking item from the queue.
 * @param q the queue
 * @return NULL on error, otherwise the entry
 */
void *prqueue_pop(prqueue_t *q);


/**
 * remove an item from the queue.
 * @param q the queue
 * @param d the entry
 * @return 0 on success
 */
int prqueue_remove(prqueue_t *q, void *d);


/**
 * access highest-ranking item without removing it.
 * @param q the queue
 * @return NULL on error, otherwise the entry
 */
void *prqueue_peek(prqueue_t *q);


/**
 * print the queue
 * @internal
 * DEBUG function only
 * @param q the queue
 * @param out the output handle
 * @param the callback function to print the entry
 */
void
prqueue_print(prqueue_t *q, FILE *out, prqueue_print_entry_f print);


/**
 * dump the queue and it's internal structure
 * @internal
 * debug function only
 * @param q the queue
 * @param out the output handle
 * @param the callback function to print the entry
 */
void prqueue_dump(prqueue_t *q, FILE *out, prqueue_print_entry_f print);


/**
 * checks that the pq is in the right order, etc
 * @internal
 * debug function only
 * @param q the queue
 */
int prqueue_is_valid(prqueue_t *q);

#endif
/** @} */
