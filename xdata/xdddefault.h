/*****************************************************************************
 *
 * XDDDEFAULT.H - Header file for default scheduled downtime data routines
 *
 * Copyright (c) 2001-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 02-16-2002
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

#ifdef NSCORE
int xdddefault_initialize_downtime_data(char *);
int xdddefault_create_downtime_file(void);
int xdddefault_validate_downtime_data(void);
int xdddefault_cleanup_downtime_data(char *);

int xdddefault_save_host_downtime(char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int *);
int xdddefault_save_service_downtime(char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int *);

int xdddefault_delete_host_downtime(int);
int xdddefault_delete_service_downtime(int);
int xdddefault_delete_downtime(int,int);
#endif


int xdddefault_grab_config_info(char *);
void xdddefault_grab_config_directives(char *);
int xdddefault_read_downtime_data(char *);
