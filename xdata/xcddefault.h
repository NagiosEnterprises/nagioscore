/*****************************************************************************
 *
 * XCDDEFAULT.H - Header file for default comment data routines
 *
 * Copyright (c) 2000-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-16-2002
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

#ifdef NSCORE
int xcddefault_initialize_comment_data(char *);
int xcddefault_create_comment_file(void);
int xcddefault_validate_comment_data(void);
int xcddefault_cleanup_comment_data(char *);
int xcddefault_save_host_comment(char *,time_t,char *,char *,int,int *);
int xcddefault_save_service_comment(char *,char *,time_t,char *,char *,int,int *);
int xcddefault_delete_host_comment(int);
int xcddefault_delete_service_comment(int);
int xcddefault_delete_all_host_comments(char *);
int xcddefault_delete_all_service_comments(char *,char *);
#endif

#ifdef NSCGI
int xcddefault_read_comment_data(char *);
#endif

int xcddefault_grab_config_info(char *);
void xcddefault_grab_config_directives(char *);
