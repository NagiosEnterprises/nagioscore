/*****************************************************************************
 *
 * NEBMODS.H - Include file for event broker modules
 *
 * Copyright (c) 2002-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-14-2003
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#ifndef _NEBMODS_H
#define _NEBMODS_H

#include "config.h"
#include "nebcallbacks.h"
#include "nebmodules.h"



/***** MODULE STRUCTURES *****/

/* NEB module callback list struct */
typedef struct nebcallback_struct{
	void            *callback_func;
	int             priority;
	struct nebcallback_struct *next;
        }nebcallback;



/***** MODULE FUNCTIONS *****/

int neb_init_modules(void);
int neb_deinit_modules(void);
int neb_load_all_modules(void);
int neb_load_module(nebmodule *);
int neb_free_module_list(void);
int neb_unload_all_modules(int,int);
int neb_unload_module(nebmodule *,int,int);
int neb_add_module(char *,char *,int);


/***** CALLBACK FUNCTIONS *****/
int neb_init_callback_list(void);
int neb_free_callback_list(void);
int neb_make_callbacks(int,void *);

#endif


