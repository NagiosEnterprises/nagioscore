/*****************************************************************************
 *
 * LIFO.C - LIFO functions for NetSaint CGIs
 *
 * Copyright (c) 1999-2000 Ethan Galstad (netsaint@netsaint.org)
 * Last Modified:   09-27-2000
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

#include "../common/config.h"

#include "lifo.h"


lifo *lifo_list=NULL;



int read_file_into_lifo(char *filename){
	char input_buffer[1024];
	FILE *fp;
	int lifo_result;

	fp=fopen(filename,"r");
	if(fp==NULL)
		return LIFO_ERROR_FILE;

	while(fgets(input_buffer,(int)(sizeof(input_buffer)-1),fp)){

		if(feof(fp))
			break;

		lifo_result=push_lifo(input_buffer);

		if(lifo_result!=LIFO_OK){
			free_lifo_memory();
			fclose(fp);
			return lifo_result;
		        }
	        }

	fclose(fp);

	return LIFO_OK;
        }


void free_lifo_memory(void){
	lifo *temp_lifo;
	lifo *next_lifo;

	if(lifo_list==NULL)
		return;

	temp_lifo=lifo_list;
	while(temp_lifo!=NULL){
		next_lifo=temp_lifo->next;
		if(temp_lifo->data!=NULL)
			free((void *)temp_lifo->data);
		free((void *)temp_lifo);
		temp_lifo=next_lifo;
	        }

	return;
        }


int push_lifo(char *buffer){
	lifo *temp_lifo;
	int bytes_allocated;

	temp_lifo=(lifo *)malloc(sizeof(lifo));
	if(temp_lifo==NULL)
		return LIFO_ERROR_MEMORY;

	bytes_allocated=(int)(strlen(buffer)+1);
	temp_lifo->data=(char *)malloc(bytes_allocated);
	if(temp_lifo->data==NULL){
		free(temp_lifo);
		return LIFO_ERROR_MEMORY;
	        }

	if(buffer==NULL)
		strcpy(temp_lifo->data,"");
	else{
		strncpy(temp_lifo->data,buffer,bytes_allocated);
		temp_lifo->data[bytes_allocated-1]='\x0';
	        }

	/* add item to front of lifo... */
	temp_lifo->next=lifo_list;
	lifo_list=temp_lifo;

	return LIFO_OK;
        }



int pop_lifo(char *buffer,int buffer_length){
	lifo *temp_lifo;

	if(buffer==NULL)
		return LIFO_ERROR_DATA;

	if(lifo_list==NULL){
		strcpy(buffer,"");
		return LIFO_ERROR_DATA;
	        }

	if(lifo_list->data==NULL){
		strcpy(buffer,"");
	        }
	else{
		strncpy(buffer,lifo_list->data,buffer_length);
		buffer[buffer_length-1]='\x0';
	        }

	temp_lifo=lifo_list->next;

	if(lifo_list->data!=NULL)
		free((void *)lifo_list->data);
	free((void *)lifo_list);

	lifo_list=temp_lifo;

	return LIFO_OK;
        }

