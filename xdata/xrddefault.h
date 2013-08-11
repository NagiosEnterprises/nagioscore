/*****************************************************************************
 *
 * XRDDEFAULT.H - Header file for default state retention routines
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

#ifndef _XRDDEFAULT_H
#define _XRDDEFAULT_H


#define XRDDEFAULT_NO_DATA               0
#define XRDDEFAULT_INFO_DATA             1
#define XRDDEFAULT_PROGRAMSTATUS_DATA    2
#define XRDDEFAULT_HOSTSTATUS_DATA       3
#define XRDDEFAULT_SERVICESTATUS_DATA    4
#define XRDDEFAULT_CONTACTSTATUS_DATA    5
#define XRDDEFAULT_HOSTCOMMENT_DATA      6
#define XRDDEFAULT_SERVICECOMMENT_DATA   7
#define XRDDEFAULT_HOSTDOWNTIME_DATA     8
#define XRDDEFAULT_SERVICEDOWNTIME_DATA  9

int xrddefault_initialize_retention_data(const char *);
int xrddefault_cleanup_retention_data(void);
int xrddefault_save_state_information(void);        /* saves all host and service state information */
int xrddefault_read_state_information(void);        /* reads in initial host and service state information */

#endif
