/*****************************************************************************
 *
 * XODDEFAULT.H - Header file for default object config data input routines
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   01-05-2002
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


#define MAX_XODDEFAULT_VAR_TYPE	32	/* max. length of a variable type ('service', 'host', etc.) */
#define MAX_XODDEFAULT_VAR_NAME	128	/* max. length of a variable name */


int xoddefault_read_config_data(char *,int);
int xoddefault_process_config_file(char *,int);
int xoddefault_process_config_dir(char *,int);

int xoddefault_add_timeperiod(char *,char *,int);
int xoddefault_add_host(char *,char *,int);
int xoddefault_add_hostgroup(char *,char *,int);
int xoddefault_add_contact(char *,char *,int);
int xoddefault_add_contactgroup(char *,char *,int);
int xoddefault_add_service(char *,char *,int);
int xoddefault_add_command(char *,char *,int);
int xoddefault_add_serviceescalation(char *,char *,int);
int xoddefault_add_hostgroupescalation(char *,char *,int);
int xoddefault_add_servicedependency(char *,char *,int);

int xoddefault_parse_input(char *,char *,char *,int,int);       	/* used to parse config file input */
int xoddefault_parse_variable(char *,char *,char *,int,int);    	/* used to parse variable name/value pairs */
