/*****************************************************************************
 *
 * XPDDEFAULT.H - Include file for default performance data routines
 *
 * Copyright (c) 2001-2003 Ethan Galstad (nagios@nagios.org)
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

#include "../include/objects.h"


int xpddefault_initialize_performance_data(char *);
int xpddefault_cleanup_performance_data(char *);
int xpddefault_grab_config_info(char *);

int xpddefault_update_service_performance_data(service *);
int xpddefault_update_host_performance_data(host *);
