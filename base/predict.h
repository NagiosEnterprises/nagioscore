/*****************************************************************************
 *
 * PREDICT.H - Include file for failure prediction routines
 *
 * Copyright (c) 2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   07-26-2001
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

#include "../common/objects.h"



int initialize_prediction_data(char *);            /* initialize prediction data */
int cleanup_prediction_data(char *);               /* cleans up prediction data */

void submit_service_prediction_data(service *);    /* passes service check data to prediction failure logic */
void submit_host_prediction_data(host *);          /* passes host check data to prediction failure logic */

