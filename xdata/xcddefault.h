/*****************************************************************************
 *
 * XCDDEFAULT.H - Header file for default comment data routines
 *
 * Copyright (c) 2000-2006 Ethan Galstad (egalstad@nagios.org)
 * Last Modified:   12-26-2006
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
 *
 *****************************************************************************/

#ifndef _XCDDEFAULT_H
#define _XCDDEFAULT_H

#ifdef NSCORE
int xcddefault_initialize_comment_data(char *);
int xcddefault_cleanup_comment_data(char *);
int xcddefault_save_comment_data(void);
int xcddefault_add_new_host_comment(int,char *,time_t,char *,char *,int,int,int,time_t,unsigned long *);
int xcddefault_add_new_service_comment(int,char *,char *,time_t,char *,char *,int,int,int,time_t,unsigned long *);
int xcddefault_delete_host_comment(unsigned long);
int xcddefault_delete_service_comment(unsigned long);
#endif

#endif
