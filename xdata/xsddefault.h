/*****************************************************************************
 *
 * XSDDEFAULT.H - Header file for default status data routines
 *
 * Copyright (c) 1999-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   06-22-2001
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
int xsddefault_initialize_status_data(char *);
int xsddefault_cleanup_status_data(char *,int);
int xsddefault_begin_aggregated_dump(void);
int xsddefault_end_aggregated_dump(void);
int xsddefault_update_program_status(time_t,int,int,time_t,time_t,int,int,int,int,int,int,int,int,int);
int xsddefault_update_host_status(char *,char *,time_t,time_t,time_t,int,unsigned long,unsigned long,unsigned long,time_t,int,int,int,int,int,int,double,int,int,int,char *,int);
int xsddefault_update_service_status(char *,char *,char *,time_t,int,int,int,time_t,time_t,int,int,int,int,int,time_t,int,char *,unsigned long,unsigned long,unsigned long,unsigned long,time_t,int,int,int,int,int,int,double,int,int,int,int,char *,int);
#endif

#ifdef NSCGI
int xsddefault_read_status_data(char *,int);
int xsddefault_add_program_status(char *);
int xsddefault_add_host_status(char *);
int xsddefault_add_service_status(char *);
#endif


int xsddefault_grab_config_info(char *);
void xsddefault_grab_config_directives(char *);
