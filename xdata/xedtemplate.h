/*****************************************************************************
 *
 * XEDTEMPLATE.H - Template-based extended information data header file
 *
 * Copyright (c) 2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-03-2001
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


#ifndef _XEDTEMPLATE_H
#define _XEDTEMPLATE_H



/*********** GENERAL DEFINITIONS ************/

#define MAX_XEDTEMPLATE_INPUT_BUFFER    8196

#define XEDTEMPLATE_NONE                0
#define XEDTEMPLATE_HOSTEXTINFO         1
#define XEDTEMPLATE_SERVICEEXTINFO      2



/********** STRUCTURE DEFINITIONS **********/


/* HOSTEXTINFO TEMPLATE STRUCTURE */
typedef struct xedtemplate_hostextinfo_struct{
	char       *template;
	char       *name;
	char       *host_name;
	char       *hostgroup;
	char       *notes_url;
	char       *icon_image;
	char       *icon_image_alt;
	char       *vrml_image;
	char       *gd2_image;
	int        x_2d;
	int        y_2d;
	double     x_3d;
	double     y_3d;
	double     z_3d;
	
	int        have_2d_coords;
	int        have_3d_coords;

	int        has_been_resolved;
	int        register_object;
	struct xedtemplate_hostextinfo_struct *next;
        }xedtemplate_hostextinfo;



/* SERVICEEXTINFO TEMPLATE STRUCTURE */
typedef struct xedtemplate_serviceextinfo_struct{
	char       *template;
	char       *name;
	char       *host_name;
	char       *hostgroup;
	char       *service_description;
	char       *notes_url;
	char       *icon_image;
	char       *icon_image_alt;

	int        has_been_resolved;
	int        register_object;
	struct xedtemplate_serviceextinfo_struct *next;
        }xedtemplate_serviceextinfo;



/********* FUNCTION DEFINITIONS **********/

int xedtemplate_read_extended_object_config_data(char *,int);  /* top level routine for reading in extended info */
int xedtemplate_process_config_file(char *,int);               /* process data in a specific config file */
int xedtemplate_process_config_dir(char *,int);                /* process all config files in a specific directory */

void xedtemplate_strip(char *);

int xedtemplate_begin_object_definition(char *);
int xedtemplate_add_object_property(char *);
int xedtemplate_end_object_definition(void);

int xedtemplate_duplicate_objects(void);
int xedtemplate_resolve_objects(void);
int xedtemplate_register_objects(void);
int xedtemplate_free_memory(void);

int xedtemplate_duplicate_hostextinfo(xedtemplate_hostextinfo *,char *);
int xedtemplate_duplicate_serviceextinfo(xedtemplate_serviceextinfo *,char *);

int xedtemplate_resolve_hostextinfo(xedtemplate_hostextinfo *);
int xedtemplate_resolve_serviceextinfo(xedtemplate_serviceextinfo *);

xedtemplate_hostextinfo *xedtemplate_find_hostextinfo(char *);
xedtemplate_serviceextinfo *xedtemplate_find_serviceextinfo(char *);

int xedtemplate_register_hostextinfo(xedtemplate_hostextinfo *);
int xedtemplate_register_serviceextinfo(xedtemplate_serviceextinfo *);


#endif
