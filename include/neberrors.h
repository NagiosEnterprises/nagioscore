/*****************************************************************************
 *
 * NEBERRORS.H - Event broker errors
 *
 * Copyright (c) 2003 Ethan Galstad (nagios@nagios.org)
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

#ifndef _NEBERRORS_H
#define _NEBERRORS_H


/***** GENERIC DEFINES *****/

#define NEB_OK                      0
#define NEB_ERROR                   -1

#define NEB_TRUE                    1
#define NEB_FALSE                   0



/***** GENERIC ERRORS *****/

#define NEBERROR_NOMEM              100     /* memory could not be allocated */



/***** CALLBACK ERRORS *****/

#define NEBERROR_NOCALLBACKFUNC     200     /* no callback function was specified */
#define NEBERROR_NOCALLBACKLIST     201     /* callback list not initialized */
#define NEBERROR_CALLBACKBOUNDS     202     /* callback type was out of bounds */
#define NEBERROR_CALLBACKNOTFOUND   203     /* the callback could not be found */



/***** MODULE ERRORS *****/

#define NEBERROR_NOMODULE           300     /* no module was specified */



/***** MODULE INFO ERRORS *****/

#define NEBERROR_MODINFOBOUNDS      400     /* module info index was out of bounds */


#endif
