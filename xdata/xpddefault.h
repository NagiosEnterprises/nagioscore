/*****************************************************************************
 *
 * XPDDEFAULT.H - Include file for default performance data routines
 *
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

#ifndef _XPDDEFAULT_H
#define _XPDDEFAULT_H

#include "../include/objects.h"


int xpddefault_initialize_performance_data(const char *);
int xpddefault_cleanup_performance_data(void);

int xpddefault_update_service_performance_data(service *);
int xpddefault_update_host_performance_data(host *);

int xpddefault_run_service_performance_data_command(nagios_macros *mac, service *);
int xpddefault_run_host_performance_data_command(nagios_macros *mac, host *);

int xpddefault_update_service_performance_data_file(nagios_macros *mac, service *);
int xpddefault_update_host_performance_data_file(nagios_macros *mac, host *);

int xpddefault_preprocess_file_templates(char *);

int xpddefault_open_host_perfdata_file(void);
int xpddefault_open_service_perfdata_file(void);
int xpddefault_close_host_perfdata_file(void);
int xpddefault_close_service_perfdata_file(void);

int xpddefault_process_host_perfdata_file(void);
int xpddefault_process_service_perfdata_file(void);

#endif
