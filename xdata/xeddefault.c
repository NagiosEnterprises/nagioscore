/*****************************************************************************
 *
 * XEDDEFAULT.C - Default external extended information routines for Nagios
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


/*********** COMMON HEADER FILES ***********/

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"

#include "../cgi/cgiutils.h"
#include "../cgi/edata.h"


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#include "xeddefault.h"



/******************************************************************/
/****************** DEFAULT DATA INPUT FUNCTION *******************/
/******************************************************************/


/* read extended host information from the CGI config file */
int xeddefault_read_extended_object_config_data(char *config_file, int options){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;

	/* open the config file for reading */
	fp=fopen(config_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the CGI config file */
	for(fgets(input_buffer,sizeof(input_buffer)-1,fp);!feof(fp);fgets(input_buffer,sizeof(input_buffer)-1,fp)){

		/* skip blank lines and comments */
		if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
			continue;

		strip(input_buffer);

		/* extended host data entries */
		if(strstr(input_buffer,"hostextinfo[")==input_buffer && (options & READ_EXTENDED_HOST_INFO))
			xeddefault_read_extended_host_config_data(input_buffer);

		/* extended service data entries */
		if(strstr(input_buffer,"serviceextinfo[")==input_buffer && (options & READ_EXTENDED_SERVICE_INFO))
			xeddefault_read_extended_service_config_data(input_buffer);
	        }

	fclose(fp);

	return OK;
        }


/* read an extended host info definition */
int xeddefault_read_extended_host_config_data(char *buffer){
	char *temp_ptr;
	char *delim;
	char *host_name;
	char *notes_url;
	char *icon_image;
	char *vrml_image;
	char *gd2_icon_image;
	char *icon_image_alt;
	int x_2d=-1;
	int y_2d=-1;
	double x_3d=-1.0;
	double y_3d=-1.0;
	double z_3d=-1.0;
	int have_2d_coords=TRUE;
	int have_3d_coords=TRUE;


	/* get the extended host info data */
	temp_ptr=my_strtok(buffer,"[");
	host_name=my_strtok(NULL,"]");
	if(host_name==NULL)
		return ERROR;
	temp_ptr=my_strtok(NULL,"=");
	notes_url=my_strtok(NULL,";");
	icon_image=my_strtok(NULL,";");
	vrml_image=my_strtok(NULL,";");
	gd2_icon_image=my_strtok(NULL,";");
	icon_image_alt=my_strtok(NULL,";");

	/* assume we have coordinates until proven otherwise */
	have_2d_coords=TRUE;
	have_3d_coords=TRUE;

	/* get 2-D coordinates */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr!=NULL){
		x_2d=atoi(temp_ptr);
		delim=strchr(temp_ptr,',');
		if(delim!=NULL)
			y_2d=atoi(delim+1);
		else
			have_2d_coords=FALSE;
	        }
	else
		have_2d_coords=FALSE;
	if(have_2d_coords==FALSE){
		x_2d=-1;
		y_2d=-1;
	        }
			
	/* get 3-D coordinates */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr!=NULL){
		x_3d=strtod(temp_ptr,NULL);
		delim=strchr(temp_ptr,',');
		if(delim!=NULL){
			y_3d=strtod(delim+1,NULL);
			delim=strchr(delim+1,',');
			if(delim!=NULL)
				z_3d=strtod(delim+1,NULL);
			else
				have_3d_coords=FALSE;
		        }
		else
			have_3d_coords=FALSE;
	        }
	else
		have_3d_coords=FALSE;
	if(have_3d_coords==FALSE){
		x_3d=-1.0;
		y_3d=-1.0;
		z_3d=-1.0;
	        }
			
	/* add the extended host info */
	add_extended_host_info(host_name,notes_url,icon_image,vrml_image,gd2_icon_image,icon_image_alt,x_2d,y_2d,x_3d,y_3d,z_3d,have_2d_coords,have_3d_coords);

	return OK;
        }


/* read an extended service info definition */
int xeddefault_read_extended_service_config_data(char *buffer){
	char *temp_ptr;
	char *host_name;
	char *description;
	char *notes_url;
	char *icon_image;
	char *icon_image_alt;

	/* get the extended service info data */
	temp_ptr=my_strtok(buffer,"[");
	host_name=my_strtok(NULL,";");
	if(host_name==NULL)
		return ERROR;
	description=my_strtok(NULL,"]");
	if(description==NULL)
		return ERROR;
	temp_ptr=my_strtok(NULL,"=");
	notes_url=my_strtok(NULL,";");
	icon_image=my_strtok(NULL,";");
	icon_image_alt=my_strtok(NULL,";");

	/* add the extended service info */
	add_extended_service_info(host_name,description,notes_url,icon_image,icon_image_alt);

	return OK;
        }
