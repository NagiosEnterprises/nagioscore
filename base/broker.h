/*****************************************************************************
 *
 * BROKER.H - Event broker includes for Nagios
 *
 * Copyright (c) 2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-15-2002
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

#ifndef _BROKER_H
#define _BROKER_H


/****** EVENT TYPES ************************/

#define NEBTYPE_HELLO                         1
#define NEBTYPE_GOODBYE                       2
#define NEBTYPE_INFO                          3

#define NEBTYPE_PROCESS_START                 10
#define NEBTYPE_PROCESS_DAEMON                11
#define NEBTYPE_PROCESS_RESTART               12
#define NEBTYPE_PROCESS_SHUTDOWN              13

#define NEBTYPE_TIMEDEVENT_ADD                20
#define NEBTYPE_TIMEDEVENT_REMOVE             21
#define NEBTYPE_TIMEDEVENT_EXECUTE            22
#define NEBTYPE_TIMEDEVENT_DELAY              23
#define NEBTYPE_TIMEDEVENT_SKIP               24
#define NEBTYPE_TIMEDEVENT_SLEEP              25

#define NEBTYPE_LOG_DATA                      30
#define NEBTYPE_LOG_ROTATION                  31

#define NEBTYPE_SYSTEM_COMMAND                40

#define NEBTYPE_EVENTHANDLER_HOST             50
#define NEBTYPE_EVENTHANDLER_GLOBAL_HOST      51
#define NEBTYPE_EVENTHANDLER_SERVICE          52
#define NEBTYPE_EVENTHANDLER_GLOBAL_SERVICE   53

#define NEBTYPE_NOTIFICATION_HOST             60
#define NEBTYPE_NOTIFICATION_SERVICE          61
#define NEBTYPE_NOTIFICATION_CONTACT          62

#define NEBTYPE_SERVICECHECK_INITIATE         70
#define NEBTYPE_SERVICECHECK_PROCESSED        71
#define NEBTYPE_HOSTCHECK_INITIATE            75        /* a check of the route to the host has been initiated */
#define NEBTYPE_HOSTCHECK_RAW                 76        /* the "raw" result of a host check */
#define NEBTYPE_HOSTCHECK_PROCESSED           77        /* the processed/final result of a host check */




/****** EVENT FLAGS ************************/

#define NEBFLAG_NONE                          0
#define NEBFLAG_PROCESS_INITIATED             1         /* event was initiated by Nagios process */
#define NEBFLAG_USER_INITIATED                2         /* event was initiated by a user request */




/****** EVENT ATTRIBUTES *******************/

#define NEBATTR_NONE                          0

#define NEBATTR_BROKERFILE_ERROR              1

#define NEBATTR_BUFFER_OVERFLOW               1

#define NEBATTR_SHUTDOWN_NORMAL               1
#define NEBATTR_SHUTDOWN_ABNORMAL             2
#define NEBATTR_RESTART_NORMAL                4
#define NEBATTR_RESTART_ABNORMAL              8

#define NEBATTR_HOSTCHECK_ACTIVE              1
#define NEBATTR_HOSTCHECK_PASSIVE             2

#define NEBATTR_SERVICECHECK_ACTIVE           1
#define NEBATTR_SERVICECHECK_PASSIVE          2

#define NEBATTR_EARLY_COMMAND_TIMEOUT         512

#endif
