/*****************************************************************************
 *
 * XEDDEFAULT.H - Header file for default extended information routines
 *
 * Copyright (c) 1999-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   05-07-2001
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


int xeddefault_read_extended_object_config_data(char *,int);        /* reads in extended host information */

int xeddefault_read_extended_host_config_data(char *);
int xeddefault_read_extended_service_config_data(char *);
