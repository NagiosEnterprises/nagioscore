/*****************************************************************************
 *
 * COMMENTS.C - Comment functions for Nagios
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/comments.h"
#include "../include/objects.h"
#include "../xdata/xcddefault.h"

#ifdef NSCORE
#include "../include/nagios.h"
#include "../include/broker.h"
#endif

#ifdef NSCGI
#include "../include/cgiutils.h"
#endif


nagios_comment     *comment_list = NULL;
int	    defer_comment_sorting = 0;
nagios_comment     **comment_hashlist = NULL;




#ifdef NSCORE

/******************************************************************/
/**************** INITIALIZATION/CLEANUP FUNCTIONS ****************/
/******************************************************************/


/* initializes comment data */
int initialize_comment_data(void) {
	return xcddefault_initialize_comment_data();
	}


/******************************************************************/
/****************** COMMENT OUTPUT FUNCTIONS **********************/
/******************************************************************/


/* adds a new host or service comment */
int add_new_comment(int type, int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id) {
	int result;
	unsigned long new_comment_id = 0L;

	if(type == HOST_COMMENT)
		result = add_new_host_comment(entry_type, host_name, entry_time, author_name, comment_data, persistent, source, expires, expire_time, &new_comment_id);
	else
		result = add_new_service_comment(entry_type, host_name, svc_description, entry_time, author_name, comment_data, persistent, source, expires, expire_time, &new_comment_id);

	/* add an event to expire comment data if necessary... */
	if(expires == TRUE)
		schedule_new_event(EVENT_EXPIRE_COMMENT, FALSE, expire_time, FALSE, 0, NULL, TRUE, (void *)new_comment_id, NULL, 0);

	/* save comment id */
	if(comment_id != NULL)
		*comment_id = new_comment_id;

	return result;
	}


/* adds a new host comment */
int add_new_host_comment(int entry_type, char *host_name, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id) {
	int result;
	unsigned long new_comment_id = 0L;

	result = xcddefault_add_new_host_comment(entry_type, host_name, entry_time, author_name, comment_data, persistent, source, expires, expire_time, &new_comment_id);

	/* save comment id */
	if(comment_id != NULL)
		*comment_id = new_comment_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_comment_data(NEBTYPE_COMMENT_ADD, NEBFLAG_NONE, NEBATTR_NONE, HOST_COMMENT, entry_type, host_name, NULL, entry_time, author_name, comment_data, persistent, source, expires, expire_time, new_comment_id, NULL);
#endif

	return result;
	}


/* adds a new service comment */
int add_new_service_comment(int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id) {
	int result;
	unsigned long new_comment_id = 0L;

	result = xcddefault_add_new_service_comment(entry_type, host_name, svc_description, entry_time, author_name, comment_data, persistent, source, expires, expire_time, &new_comment_id);

	/* save comment id */
	if(comment_id != NULL)
		*comment_id = new_comment_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_comment_data(NEBTYPE_COMMENT_ADD, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_COMMENT, entry_type, host_name, svc_description, entry_time, author_name, comment_data, persistent, source, expires, expire_time, new_comment_id, NULL);
#endif

	return result;
	}



/******************************************************************/
/***************** COMMENT DELETION FUNCTIONS *********************/
/******************************************************************/


/* deletes a host or service comment */
int delete_comment(int type, unsigned long comment_id) {
	nagios_comment *this_comment = NULL;
	nagios_comment *last_comment = NULL;
	nagios_comment *next_comment = NULL;
	int hashslot = 0;
	nagios_comment *this_hash = NULL;
	nagios_comment *last_hash = NULL;

	/* find the comment we should remove */
	for(this_comment = comment_list, last_comment = comment_list; this_comment != NULL; this_comment = next_comment) {
		next_comment = this_comment->next;

		/* we found the comment we should delete */
		if(this_comment->comment_id == comment_id && this_comment->comment_type == type)
			break;

		last_comment = this_comment;
		}

	if(this_comment == NULL)
		return ERROR;

	/* remove the comment from the list in memory */
#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_comment_data(NEBTYPE_COMMENT_DELETE, NEBFLAG_NONE, NEBATTR_NONE, type, this_comment->entry_type, this_comment->host_name, this_comment->service_description, this_comment->entry_time, this_comment->author, this_comment->comment_data, this_comment->persistent, this_comment->source, this_comment->expires, this_comment->expire_time, comment_id, NULL);
#endif

	/* first remove from chained hash list */
	hashslot = hashfunc(this_comment->host_name, NULL, COMMENT_HASHSLOTS);
	last_hash = NULL;
	for(this_hash = comment_hashlist[hashslot]; this_hash; this_hash = this_hash->nexthash) {
		if(this_hash == this_comment) {
			if(last_hash)
				last_hash->nexthash = this_hash->nexthash;
			else {
				if(this_hash->nexthash)
					comment_hashlist[hashslot] = this_hash->nexthash;
				else
					comment_hashlist[hashslot] = NULL;
				}
			break;
			}
			last_hash = this_hash;
		}

	/* then removed from linked list */
	if(comment_list == this_comment)
		comment_list = this_comment->next;
	else
		last_comment->next = next_comment;

	/* free memory */
	my_free(this_comment->host_name);
	my_free(this_comment->service_description);
	my_free(this_comment->author);
	my_free(this_comment->comment_data);
	my_free(this_comment);

	return OK;
	}


/* deletes a host comment */
int delete_host_comment(unsigned long comment_id) {
	int result = OK;

	/* delete the comment from memory */
	result = delete_comment(HOST_COMMENT, comment_id);

	return result;
	}



/* deletes a service comment */
int delete_service_comment(unsigned long comment_id) {
	int result = OK;

	/* delete the comment from memory */
	result = delete_comment(SERVICE_COMMENT, comment_id);

	return result;
	}


/* deletes all comments for a particular host or service */
int delete_all_comments(int type, char *host_name, char *svc_description) {
	int result = OK;

	if(type == HOST_COMMENT)
		result = delete_all_host_comments(host_name);
	else
		result = delete_all_service_comments(host_name, svc_description);

	return result;
	}


/* deletes all comments for a particular host */
int delete_all_host_comments(char *host_name) {
	int result = OK;
	nagios_comment *temp_comment = NULL;
	nagios_comment *next_comment = NULL;

	if(host_name == NULL)
		return ERROR;

	/* delete host comments from memory */
	for(temp_comment = get_first_comment_by_host(host_name); temp_comment != NULL; temp_comment = next_comment) {
		next_comment = get_next_comment_by_host(host_name, temp_comment);
		if(temp_comment->comment_type == HOST_COMMENT)
			delete_comment(HOST_COMMENT, temp_comment->comment_id);
		}

	return result;
	}


/* deletes all non-persistent acknowledgement comments for a particular host */
int delete_host_acknowledgement_comments(host *hst) {
	int result = OK;
	nagios_comment *temp_comment = NULL;
	nagios_comment *next_comment = NULL;

	if(hst == NULL)
		return ERROR;

	/* delete comments from memory */
	temp_comment = get_first_comment_by_host(hst->name);
	while (temp_comment) {
		next_comment = get_next_comment_by_host(hst->name, temp_comment);
		if(temp_comment->comment_type == HOST_COMMENT && temp_comment->entry_type == ACKNOWLEDGEMENT_COMMENT && temp_comment->persistent == FALSE) {
			delete_comment(HOST_COMMENT, temp_comment->comment_id);
			}
		temp_comment = next_comment;
		}

	return result;
	}


/* deletes all comments for a particular service */
int delete_all_service_comments(char *host_name, char *svc_description) {
	int result = OK;
	nagios_comment *temp_comment = NULL;
	nagios_comment *next_comment = NULL;

	if(host_name == NULL || svc_description == NULL)
		return ERROR;

	/* delete service comments from memory */
	for(temp_comment = comment_list; temp_comment != NULL; temp_comment = next_comment) {
		next_comment = temp_comment->next;
		if(temp_comment->comment_type == SERVICE_COMMENT && !strcmp(temp_comment->host_name, host_name) && !strcmp(temp_comment->service_description, svc_description))
			delete_comment(SERVICE_COMMENT, temp_comment->comment_id);
		}

	return result;
	}


/* deletes all non-persistent acknowledgement comments for a particular service */
int delete_service_acknowledgement_comments(service *svc) {
	int result = OK;
	nagios_comment *temp_comment = NULL;
	nagios_comment *next_comment = NULL;

	if(svc == NULL)
		return ERROR;

	/* delete comments from memory */
	for(temp_comment = comment_list; temp_comment != NULL; temp_comment = next_comment) {
		next_comment = temp_comment->next;
		if(temp_comment->comment_type == SERVICE_COMMENT && !strcmp(temp_comment->host_name, svc->host_name) && !strcmp(temp_comment->service_description, svc->description)  && temp_comment->entry_type == ACKNOWLEDGEMENT_COMMENT && temp_comment->persistent == FALSE)
			delete_comment(SERVICE_COMMENT, temp_comment->comment_id);
		}

	return result;
	}


/* checks for an expired comment (and removes it) */
int check_for_expired_comment(unsigned long comment_id) {
	nagios_comment *temp_comment = NULL;

	/* check all comments */
	for(temp_comment = comment_list; temp_comment != NULL; temp_comment = temp_comment->next) {

		/* delete the now expired comment */
		if(temp_comment->comment_id == comment_id && temp_comment->expires == TRUE && temp_comment->expire_time < time(NULL)) {
			delete_comment(temp_comment->comment_type, comment_id);
			break;
			}
		}

	return OK;
	}


#endif





/******************************************************************/
/****************** CHAINED HASH FUNCTIONS ************************/
/******************************************************************/

/* adds comment to hash list in memory */
int add_comment_to_hashlist(nagios_comment *new_comment) {
	nagios_comment *temp_comment = NULL;
	nagios_comment *lastpointer = NULL;
	int hashslot = 0;

	/* initialize hash list */
	if(comment_hashlist == NULL) {
		int i;

		comment_hashlist = (nagios_comment **)malloc(sizeof(nagios_comment *) * COMMENT_HASHSLOTS);
		if(comment_hashlist == NULL)
			return 0;

		for(i = 0; i < COMMENT_HASHSLOTS; i++)
			comment_hashlist[i] = NULL;
		}

	if(!new_comment)
		return 0;

	hashslot = hashfunc(new_comment->host_name, NULL, COMMENT_HASHSLOTS);
	lastpointer = NULL;
	for(temp_comment = comment_hashlist[hashslot]; temp_comment && compare_hashdata(temp_comment->host_name, NULL, new_comment->host_name, NULL) < 0; temp_comment = temp_comment->nexthash) {
		if(compare_hashdata(temp_comment->host_name, NULL, new_comment->host_name, NULL) >= 0)
			break;
		lastpointer = temp_comment;
		}

	/* multiples are allowed */
	if(lastpointer)
		lastpointer->nexthash = new_comment;
	else
		comment_hashlist[hashslot] = new_comment;
	new_comment->nexthash = temp_comment;

	return 1;
	}



/******************************************************************/
/******************** ADDITION FUNCTIONS **************************/
/******************************************************************/


/* adds a host comment to the list in memory */
int add_host_comment(int entry_type, char *host_name, time_t entry_time, char *author, char *comment_data, unsigned long comment_id, int persistent, int expires, time_t expire_time, int source) {
	int result = OK;

	result = add_comment(HOST_COMMENT, entry_type, host_name, NULL, entry_time, author, comment_data, comment_id, persistent, expires, expire_time, source);

	return result;
	}



/* adds a service comment to the list in memory */
int add_service_comment(int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, unsigned long comment_id, int persistent, int expires, time_t expire_time, int source) {
	int result = OK;

	result = add_comment(SERVICE_COMMENT, entry_type, host_name, svc_description, entry_time, author, comment_data, comment_id, persistent, expires, expire_time, source);

	return result;
	}



/* adds a comment to the list in memory */
int add_comment(int comment_type, int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, unsigned long comment_id, int persistent, int expires, time_t expire_time, int source) {
	nagios_comment *new_comment = NULL;
	nagios_comment *last_comment = NULL;
	nagios_comment *temp_comment = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(host_name == NULL || author == NULL || comment_data == NULL || (comment_type == SERVICE_COMMENT && svc_description == NULL))
		return ERROR;

	/* allocate memory for the comment */
	if((new_comment = (nagios_comment *)calloc(1, sizeof(nagios_comment))) == NULL)
		return ERROR;

	/* duplicate vars */
	if((new_comment->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;
	if(comment_type == SERVICE_COMMENT) {
		if((new_comment->service_description = (char *)strdup(svc_description)) == NULL)
			result = ERROR;
		}
	if((new_comment->author = (char *)strdup(author)) == NULL)
		result = ERROR;
	if((new_comment->comment_data = (char *)strdup(comment_data)) == NULL)
		result = ERROR;

	new_comment->comment_type = comment_type;
	new_comment->entry_type = entry_type;
	new_comment->source = source;
	new_comment->entry_time = entry_time;
	new_comment->comment_id = comment_id;
	new_comment->persistent = (persistent == TRUE) ? TRUE : FALSE;
	new_comment->expires = (expires == TRUE) ? TRUE : FALSE;
	new_comment->expire_time = expire_time;

	/* add comment to hash list */
	if(result == OK) {
		if(!add_comment_to_hashlist(new_comment))
			result = ERROR;
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_comment->comment_data);
		my_free(new_comment->author);
		my_free(new_comment->service_description);
		my_free(new_comment->host_name);
		my_free(new_comment);
		return ERROR;
		}

	if(defer_comment_sorting) {
		new_comment->next = comment_list;
		comment_list = new_comment;
		}
	else {
		/* add new comment to comment list, sorted by comment id */
		last_comment = comment_list;
		for(temp_comment = comment_list; temp_comment != NULL; temp_comment = temp_comment->next) {
			if(new_comment->comment_id < temp_comment->comment_id) {
				new_comment->next = temp_comment;
				if(temp_comment == comment_list)
					comment_list = new_comment;
				else
					last_comment->next = new_comment;
				break;
				}
			else
				last_comment = temp_comment;
			}
		if(comment_list == NULL) {
			new_comment->next = NULL;
			comment_list = new_comment;
			}
		else if(temp_comment == NULL) {
			new_comment->next = NULL;
			last_comment->next = new_comment;
			}
		}

#ifdef NSCORE
#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_comment_data(NEBTYPE_COMMENT_LOAD, NEBFLAG_NONE, NEBATTR_NONE, comment_type, entry_type, host_name, svc_description, entry_time, author, comment_data, persistent, source, expires, expire_time, comment_id, NULL);
#endif
#endif

	return OK;
	}

static int comment_compar(const void *p1, const void *p2) {
	nagios_comment *c1 = *(nagios_comment **)p1;
	nagios_comment *c2 = *(nagios_comment **)p2;
	return c1->comment_id - c2->comment_id;
	}

int sort_comments(void) {
	nagios_comment **array, *temp_comment;
	unsigned long i = 0, unsorted_comments = 0;

	if(!defer_comment_sorting)
		return OK;
	defer_comment_sorting = 0;

	temp_comment = comment_list;
	while(temp_comment != NULL) {
		temp_comment = temp_comment->next;
		unsorted_comments++;
		}

	if(!unsorted_comments)
		return OK;

	if(!(array = malloc(sizeof(*array) * unsorted_comments)))
		return ERROR;
	while(comment_list) {
		array[i++] = comment_list;
		comment_list = comment_list->next;
		}

	qsort((void *)array, i, sizeof(*array), comment_compar);
	comment_list = temp_comment = array[0];
	for(i = 1; i < unsorted_comments; i++) {
		temp_comment->next = array[i];
		temp_comment = temp_comment->next;
		}
	temp_comment->next = NULL;
	my_free(array);
	return OK;
	}

/******************************************************************/
/********************* CLEANUP FUNCTIONS **************************/
/******************************************************************/

/* frees memory allocated for the comment data */
void free_comment_data(void) {
	nagios_comment *this_comment = NULL;
	nagios_comment *next_comment = NULL;

	/* free memory for the comment list */
	for(this_comment = comment_list; this_comment != NULL; this_comment = next_comment) {
		next_comment = this_comment->next;
		my_free(this_comment->host_name);
		my_free(this_comment->service_description);
		my_free(this_comment->author);
		my_free(this_comment->comment_data);
		my_free(this_comment);
		}

	/* free hash list and reset list pointer */
	my_free(comment_hashlist);
	comment_hashlist = NULL;
	comment_list = NULL;

	return;
	}




/******************************************************************/
/********************* UTILITY FUNCTIONS **************************/
/******************************************************************/

/* get the number of comments associated with a particular host */
int number_of_host_comments(char *host_name) {
	nagios_comment *temp_comment = NULL;
	int total_comments = 0;

	if(host_name == NULL)
		return 0;

	for(temp_comment = get_first_comment_by_host(host_name); temp_comment != NULL; temp_comment = get_next_comment_by_host(host_name, temp_comment)) {
		if(temp_comment->comment_type == HOST_COMMENT)
			total_comments++;
		}

	return total_comments;
	}


/* get the number of comments associated with a particular service */
int number_of_service_comments(char *host_name, char *svc_description) {
	nagios_comment *temp_comment = NULL;
	int total_comments = 0;

	if(host_name == NULL || svc_description == NULL)
		return 0;

	for(temp_comment = get_first_comment_by_host(host_name); temp_comment != NULL; temp_comment = get_next_comment_by_host(host_name, temp_comment)) {
		if(temp_comment->comment_type == SERVICE_COMMENT && !strcmp(temp_comment->service_description, svc_description))
			total_comments++;
		}

	return total_comments;
	}



/******************************************************************/
/********************* TRAVERSAL FUNCTIONS ************************/
/******************************************************************/

nagios_comment *get_first_comment_by_host(char *host_name) {

	return get_next_comment_by_host(host_name, NULL);
	}


nagios_comment *get_next_comment_by_host(char *host_name, nagios_comment *start) {
	nagios_comment *temp_comment = NULL;

	if(host_name == NULL || comment_hashlist == NULL)
		return NULL;

	if(start == NULL)
		temp_comment = comment_hashlist[hashfunc(host_name, NULL, COMMENT_HASHSLOTS)];
	else
		temp_comment = start->nexthash;

	for(; temp_comment && compare_hashdata(temp_comment->host_name, NULL, host_name, NULL) < 0; temp_comment = temp_comment->nexthash);

	if(temp_comment && compare_hashdata(temp_comment->host_name, NULL, host_name, NULL) == 0)
		return temp_comment;

	return NULL;
	}



/******************************************************************/
/********************** SEARCH FUNCTIONS **************************/
/******************************************************************/

/* find a service comment by id */
nagios_comment *find_service_comment(unsigned long comment_id) {

	return find_comment(comment_id, SERVICE_COMMENT);
	}


/* find a host comment by id */
nagios_comment *find_host_comment(unsigned long comment_id) {

	return find_comment(comment_id, HOST_COMMENT);
	}


/* find a comment by id */
nagios_comment *find_comment(unsigned long comment_id, int comment_type) {
	nagios_comment *temp_comment = NULL;

	for(temp_comment = comment_list; temp_comment != NULL; temp_comment = temp_comment->next) {
		if(temp_comment->comment_id == comment_id && temp_comment->comment_type == comment_type)
			return temp_comment;
		}

	return NULL;
	}
