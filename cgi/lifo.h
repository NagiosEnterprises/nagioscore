/************************************************************************
 *
 * LIFO Header File
 * Written By: Ethan Galstad (nagios@nagios.org)
 * Last Modified: 09-27-2000
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
 ************************************************************************/


#define LIFO_OK			0
#define LIFO_ERROR_MEMORY	1
#define LIFO_ERROR_FILE		2
#define LIFO_ERROR_DATA		3

/* LIFO data structure */
typedef struct lifo_struct{
	char *data;
	struct lifo_struct *next;
        }lifo;


int read_file_into_lifo(char *);
void free_lifo_memory(void);
int push_lifo(char *);
int pop_lifo(char *,int);


