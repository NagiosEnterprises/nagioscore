/*****************************************************************************
 *
 * EDATA.H - Header for etended object data routines
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   04-20-2002
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

#ifndef _EDATA_H
#define _EDATA_H


#define READ_EXTENDED_HOST_INFO     1
#define READ_EXTENDED_SERVICE_INFO  2

#define READ_ALL_EXTENDED_DATA      READ_EXTENDED_HOST_INFO | READ_EXTENDED_SERVICE_INFO


/* EXTENDED HOST INFO structure */
typedef struct hostextinfo_struct{
	char *host_name;
	char *notes_url;
	char *icon_image;
	char *vrml_image;
	char *gd2_icon_image;
	char *icon_image_alt;
	int have_2d_coords;
	int x_2d;
	int y_2d;
	int have_3d_coords;
	double x_3d;
	double y_3d;
	double z_3d;
	int should_be_drawn;
	struct hostextinfo_struct *next;
        }hostextinfo;

/* EXTENDED SERVICE INFO structure */
typedef struct serviceextinfo_struct{
	char *host_name;
	char *description;
	char *notes_url;
	char *icon_image;
	char *icon_image_alt;
	struct serviceextinfo_struct *next;
        }serviceextinfo;


int read_extended_object_config_data(char *,int);               /* top level function for reading extended data */

int add_extended_host_info(char *,char *,char *,char *,char *,char *,int,int,double,double,double,int,int); 
int add_extended_service_info(char *,char *,char *,char *,char *);

hostextinfo *find_hostextinfo(char *);				/* given a host name, return a pointer to the extended host information */
serviceextinfo *find_serviceextinfo(char *,char *);             /* find an extended service info struct for a specific service */

void free_extended_data(void);                                  /* frees memory allocated to the extended host/service information */

#endif
