/*****************************************************************************
 *
 * XSDDEFAULT.H - Header file for default status data routines
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

#ifndef NAGIOS_XSDDEFAULT_H_INCLUDED
#define NAGIOS_XSDDEFAULT_H_INCLUDED

#ifdef NSCORE
int xsddefault_initialize_status_data(const char *);
int xsddefault_cleanup_status_data(int);
int xsddefault_save_status_data(void);
#endif

#ifdef NSCGI

#define XSDDEFAULT_NO_DATA               0
#define XSDDEFAULT_INFO_DATA             1
#define XSDDEFAULT_PROGRAMSTATUS_DATA    2
#define XSDDEFAULT_HOSTSTATUS_DATA       3
#define XSDDEFAULT_SERVICESTATUS_DATA    4
#define XSDDEFAULT_CONTACTSTATUS_DATA    5
#define XSDDEFAULT_HOSTCOMMENT_DATA      6
#define XSDDEFAULT_SERVICECOMMENT_DATA   7
#define XSDDEFAULT_HOSTDOWNTIME_DATA     8
#define XSDDEFAULT_SERVICEDOWNTIME_DATA  9

int xsddefault_read_status_data(const char *, int);
#endif

#endif
