/*****************************************************************************
 *
 * SRETENTION.H - Header for state retention routines
 *
 * Copyright (c) 1999-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   06-29-2001
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


int save_state_information(char *,int);            /* saves all host and state information */
int read_initial_state_information(char *);        /* reads in initial host and state information */


int set_service_state_information(char *,char *,int,char *,unsigned long,int,unsigned long,unsigned long,unsigned long,unsigned long,unsigned long,int,int,int,int,int,int,int,int,int,int,unsigned long);
int set_host_state_information(char *,int,char *,unsigned long,int,unsigned long,unsigned long,unsigned long,unsigned long,int,int,int,int,int,int,int,unsigned long);
int set_program_state_information(int,int,int,int,int,int,int,int);

service * get_service_state_information(service *,char **,char **,int *,char **,unsigned long *,int *,unsigned long *,unsigned long *,unsigned long *,unsigned long *,unsigned long *,int *,int *,int *,int *,int *,int *,int *,int *,int *,int *,unsigned long *);
host * get_host_state_information(host *,char **,int *,char **,unsigned long *,int *,unsigned long *,unsigned long *,unsigned long *,unsigned long *,int *,int *,int *,int *,int *,int *,int *,unsigned long *);
int get_program_state_information(int *,int *,int *,int *,int *,int *,int *,int *);
