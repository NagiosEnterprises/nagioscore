/*****************************************************************************
 *
 * MACROS.C - Common macro functions for Nagios
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
#include "../include/macros.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/comments.h"
#ifdef NSCORE
#include "../include/nagios.h"
#else
#include "../include/cgiutils.h"
#endif

static char *macro_x_names[MACRO_X_COUNT]; /* the macro names */
char *macro_user[MAX_USER_MACROS]; /* $USERx$ macros */

struct macro_key_code {
	char *name;  /* macro key name */
	char *value; /* macro value */
	int code;  /* numeric macro code, usable in case statements */
	int options; /* Options for how the macro can be escaped */
};

static struct macro_key_code macro_keys[MACRO_X_COUNT];

/*
 * These point to their corresponding pointer arrays in global_macros
 * AFTER macros have been initialized.
 *
 * They really only exist so that eventbroker modules that reference
 * them won't need to be re-compiled, although modules that rely
 * on their values after having run a certain command will require an
 * update
 */
static char **macro_x = NULL;

/*
 * scoped to this file to prevent (unintentional) mischief,
 * but see base/notifications.c for how to use it
 */
static nagios_macros global_macros;


nagios_macros *get_global_macros(void) {
	return &global_macros;
	}

/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/*
 * locate a macro key based on its name by using a binary search
 * over all keys. O(log(n)) complexity and a vast improvement over
 * the previous linear scan
 */
static const struct macro_key_code *find_macro_key(const char *name) {
	unsigned int high, low = 0;
	int value;
	struct macro_key_code *key;

	high = MACRO_X_COUNT;
	while (high - low > 0) {
		unsigned int mid = low + ((high - low) / 2);
		key = &macro_keys[mid];
		value = strcmp(name, key->name);
		if (value == 0) {
			return key;
			}
		if (value > 0)
			low = mid + 1;
		else
			high = mid;
		}
	return NULL;
	}


/*
 * replace macros in notification commands with their values,
 * the thread-safe version
 */
int process_macros_r(nagios_macros *mac, char *input_buffer, char **output_buffer, int options) {
	char *temp_buffer = NULL;
	char *save_buffer = NULL;
	char *buf_ptr = NULL;
	char *delim_ptr = NULL;
	int in_macro = FALSE;
	char *selected_macro = NULL;
	char *original_macro = NULL;
	int result = OK;
	int free_macro = FALSE;
	int macro_options = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_macros_r()\n");

	if(output_buffer == NULL)
		return ERROR;

	*output_buffer = (char *)strdup("");

	if(input_buffer == NULL)
		return ERROR;

	in_macro = FALSE;

	log_debug_info(DEBUGL_MACROS, 1, "**** BEGIN MACRO PROCESSING ***********\n");
	log_debug_info(DEBUGL_MACROS, 1, "Processing: '%s'\n", input_buffer);

	/* use a duplicate of original buffer, so we don't modify the original */
	save_buffer = buf_ptr = (input_buffer ? strdup(input_buffer) : NULL);

	while(buf_ptr) {

		/* save pointer to this working part of buffer */
		temp_buffer = buf_ptr;

		/* find the next delimiter - terminate preceding string and advance buffer pointer for next run */
		if((delim_ptr = strchr(buf_ptr, '$'))) {
			delim_ptr[0] = '\x0';
			buf_ptr = (char *)delim_ptr + 1;
			}
		/* no delimiter found - we already have the last of the buffer */
		else
			buf_ptr = NULL;

		log_debug_info(DEBUGL_MACROS, 2, "  Processing part: '%s'\n", temp_buffer);

		selected_macro = NULL;

		/* we're in plain text... */
		if(in_macro == FALSE) {

			/* add the plain text to the end of the already processed buffer */
			*output_buffer = (char *)realloc(*output_buffer, strlen(*output_buffer) + strlen(temp_buffer) + 1);
			strcat(*output_buffer, temp_buffer);

			log_debug_info(DEBUGL_MACROS, 2, "  Not currently in macro.  Running output (%lu): '%s'\n", (unsigned long)strlen(*output_buffer), *output_buffer);
			in_macro = TRUE;
			}

		/* looks like we're in a macro, so process it... */
		else {
			/* grab the macro value */
			free_macro = FALSE;
			result = grab_macro_value_r(mac, temp_buffer, &selected_macro, &macro_options, &free_macro);
			log_debug_info(DEBUGL_MACROS, 2, "  Processed '%s', Free: %d\n", temp_buffer, free_macro);

			/* an error occurred - we couldn't parse the macro, so continue on */
			if(result == ERROR) {
				log_debug_info(DEBUGL_MACROS, 0, " WARNING: An error occurred processing macro '%s'!\n", temp_buffer);
				if(free_macro == TRUE)
					my_free(selected_macro);
				}

			if (result == OK)
				; /* do nothing special if things worked out ok */
			/* an escaped $ is done by specifying two $$ next to each other */
			else if(!strcmp(temp_buffer, "")) {
				log_debug_info(DEBUGL_MACROS, 2, "  Escaped $.  Running output (%lu): '%s'\n", (unsigned long)strlen(*output_buffer), *output_buffer);
				*output_buffer = (char *)realloc(*output_buffer, strlen(*output_buffer) + 2);
				strcat(*output_buffer, "$");
				}

			/* a non-macro, just some user-defined string between two $s */
			else {
				log_debug_info(DEBUGL_MACROS, 2, "  Non-macro.  Running output (%lu): '%s'\n", (unsigned long)strlen(*output_buffer), *output_buffer);

				/* add the plain text to the end of the already processed buffer */
				*output_buffer = (char *)realloc(*output_buffer, strlen(*output_buffer) + strlen(temp_buffer) + 3);
				strcat(*output_buffer, "$");
				strcat(*output_buffer, temp_buffer);
				strcat(*output_buffer, "$");
				}

			/* insert macro */
			if(selected_macro != NULL) {
				log_debug_info(DEBUGL_MACROS, 2, "  Processed '%s', Free: %d,  Cleaning options: %d\n", temp_buffer, free_macro, options);

				/* URL encode the macro if requested - this allocates new memory */
				if(options & URL_ENCODE_MACRO_CHARS) {
					original_macro = selected_macro;
					selected_macro = get_url_encoded_string(selected_macro);
					if(free_macro == TRUE) {
						my_free(original_macro);
						}
					free_macro = TRUE;
					}

				/* some macros should sometimes be cleaned */
				if(macro_options & options & (STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS)) {
					char *cleaned_macro = NULL;

					/* add the (cleaned) processed macro to the end of the already processed buffer */
					if(selected_macro != NULL && (cleaned_macro = clean_macro_chars(selected_macro, options)) != NULL) {
						*output_buffer = (char *)realloc(*output_buffer, strlen(*output_buffer) + strlen(cleaned_macro) + 1);
						strcat(*output_buffer, cleaned_macro);
						if(*cleaned_macro)
							my_free(cleaned_macro);

						log_debug_info(DEBUGL_MACROS, 2, "  Cleaned macro.  Running output (%lu): '%s'\n", (unsigned long)strlen(*output_buffer), *output_buffer);
						}
					}

				/* others are not cleaned */
				else {
					/* add the processed macro to the end of the already processed buffer */
					if(selected_macro != NULL) {
						*output_buffer = (char *)realloc(*output_buffer, strlen(*output_buffer) + strlen(selected_macro) + 1);
						strcat(*output_buffer, selected_macro);

						log_debug_info(DEBUGL_MACROS, 2, "  Uncleaned macro.  Running output (%lu): '%s'\n", (unsigned long)strlen(*output_buffer), *output_buffer);
						}
					}

				/* free memory if necessary (if we URL encoded the macro or we were told to do so by grab_macro_value()) */
				if(free_macro == TRUE)
					my_free(selected_macro);

				log_debug_info(DEBUGL_MACROS, 2, "  Just finished macro.  Running output (%lu): '%s'\n", (unsigned long)strlen(*output_buffer), *output_buffer);
				}

			in_macro = FALSE;
			}
		}

	/* free copy of input buffer */
	my_free(save_buffer);

	log_debug_info(DEBUGL_MACROS, 1, "  Done.  Final output: '%s'\n", *output_buffer);
	log_debug_info(DEBUGL_MACROS, 1, "**** END MACRO PROCESSING *************\n");

	return OK;
	}

int process_macros(char *input_buffer, char **output_buffer, int options) {
	return process_macros_r(&global_macros, input_buffer, output_buffer, options);
	}

/******************************************************************/
/********************** MACRO GRAB FUNCTIONS **********************/
/******************************************************************/

/* grab macros that are specific to a particular host */
int grab_host_macros_r(nagios_macros *mac, host *hst) {
	/* clear host-related macros */
	clear_host_macros_r(mac);
	clear_hostgroup_macros_r(mac);

	/* save pointer to host */
	mac->host_ptr = hst;
	mac->hostgroup_ptr = NULL;

	if(hst == NULL)
		return ERROR;

	/* save pointer to host's first/primary hostgroup */
	if(hst->hostgroups_ptr)
		mac->hostgroup_ptr = (hostgroup *)hst->hostgroups_ptr->object_ptr;

	return OK;
	}

int grab_host_macros(host *hst) {
	return grab_host_macros_r(&global_macros, hst);
	}


/* grab hostgroup macros */
int grab_hostgroup_macros_r(nagios_macros *mac, hostgroup *hg) {
	/* clear hostgroup macros */
	clear_hostgroup_macros_r(mac);

	/* save the hostgroup pointer for later */
	mac->hostgroup_ptr = hg;

	if(hg == NULL)
		return ERROR;

	return OK;
	}

int grab_hostgroup_macros(hostgroup *hg) {
	return grab_hostgroup_macros_r(&global_macros, hg);
	}


/* grab macros that are specific to a particular service */
int grab_service_macros_r(nagios_macros *mac, service *svc) {

	/* clear service-related macros */
	clear_service_macros_r(mac);
	clear_servicegroup_macros_r(mac);

	/* save pointer for later */
	mac->service_ptr = svc;
	mac->servicegroup_ptr = NULL;

	if(svc == NULL)
		return ERROR;

	/* save first/primary servicegroup pointer for later */
	if(svc->servicegroups_ptr)
		mac->servicegroup_ptr = (servicegroup *)svc->servicegroups_ptr->object_ptr;

	return OK;
	}

int grab_service_macros(service *svc) {
	return grab_service_macros_r(&global_macros, svc);
	}


/* grab macros that are specific to a particular servicegroup */
int grab_servicegroup_macros_r(nagios_macros *mac, servicegroup *sg) {
	/* clear servicegroup macros */
	clear_servicegroup_macros_r(mac);

	/* save the pointer for later */
	mac->servicegroup_ptr = sg;

	if(sg == NULL)
		return ERROR;

	return OK;
	}

int grab_servicegroup_macros(servicegroup *sg) {
	return grab_servicegroup_macros_r(&global_macros, sg);
	}


/* grab macros that are specific to a particular contact */
int grab_contact_macros_r(nagios_macros *mac, contact *cntct) {
	/* clear contact-related macros */
	clear_contact_macros_r(mac);
	clear_contactgroup_macros_r(mac);

	/* save pointer to contact for later */
	mac->contact_ptr = cntct;
	mac->contactgroup_ptr = NULL;

	if(cntct == NULL)
		return ERROR;

	/* save pointer to first/primary contactgroup for later */
	if(cntct->contactgroups_ptr)
		mac->contactgroup_ptr = (contactgroup *)cntct->contactgroups_ptr->object_ptr;

	return OK;
	}

int grab_contact_macros(contact *cntct) {
	return grab_contact_macros_r(&global_macros, cntct);
	}


/******************************************************************/
/******************* MACRO GENERATION FUNCTIONS *******************/
/******************************************************************/

/* this is the big one */
int grab_macro_value_r(nagios_macros *mac, char *macro_buffer, char **output, int *clean_options, int *free_macro) {
	char *buf = NULL;
	char *ptr = NULL;
	char *macro_name = NULL;
	char *arg[2] = {NULL, NULL};
	contact *temp_contact = NULL;
	contactgroup *temp_contactgroup = NULL;
	contactsmember *temp_contactsmember = NULL;
	char *temp_buffer = NULL;
	int delimiter_len = 0;
	int x, result = OK;
	const struct macro_key_code *mkey;

	/* for the early cases, this is the default */
	*free_macro = FALSE;

	if(output == NULL)
		return ERROR;

	/* clear the old macro value */
	my_free(*output);

	if(macro_buffer == NULL || free_macro == NULL)
		return ERROR;

	if(clean_options)
		*clean_options = 0;

	/*
	 * We handle argv and user macros first, since those are by far
	 * the most commonly accessed ones (3.4 and 1.005 per check,
	 * respectively). Since neither of them requires that we copy
	 * the original buffer, we can also get away with some less
	 * code for these simple cases.
	 */
	if(strstr(macro_buffer, "ARG") == macro_buffer) {

		/* which arg do we want? */
		x = atoi(macro_buffer + 3);

		if(x <= 0 || x > MAX_COMMAND_ARGUMENTS) {
			return ERROR;
			}

		/* use a pre-computed macro value */
		*output = mac->argv[x - 1];
		return OK;
		}

	if(strstr(macro_buffer, "USER") == macro_buffer) {

		/* which macro do we want? */
		x = atoi(macro_buffer + 4);

		if(x <= 0 || x > MAX_USER_MACROS) {
			return ERROR;
			}

		/* use a pre-computed macro value */
		*output = macro_user[x - 1];
		return OK;
		}

	/* most frequently used "x" macro gets a shortcut */
	if(mac->host_ptr && !strcmp(macro_buffer, "HOSTADDRESS")) {
		if(mac->host_ptr->address)
			*output = mac->host_ptr->address;
		return OK;
		}

	/* work with a copy of the original buffer */
	if((buf = (char *)strdup(macro_buffer)) == NULL)
		return ERROR;

	/* macro name is at start of buffer */
	macro_name = buf;

	/* see if there's an argument - if so, this is most likely an on-demand macro */
	if((ptr = strchr(buf, ':'))) {

		ptr[0] = '\x0';
		ptr++;

		/* save the first argument - host name, hostgroup name, etc. */
		arg[0] = ptr;

		/* try and find a second argument */
		if((ptr = strchr(ptr, ':'))) {

			ptr[0] = '\x0';
			ptr++;

			/* save second argument - service description or delimiter */
			arg[1] = ptr;
			}
		}

	if ((mkey = find_macro_key(macro_name))) {
		log_debug_info(DEBUGL_MACROS, 2, "  macros[%d] (%s) match.\n", mkey->code, macro_x_names[mkey->code]);

		/* get the macro value */
		result = grab_macrox_value_r(mac, mkey->code, arg[0], arg[1], output, free_macro);

		/* Return the macro attributes */

		if(clean_options) {
			*clean_options = mkey->options;
			}
		}
	/***** CONTACT ADDRESS MACROS *****/
	/* NOTE: the code below should be broken out into a separate function */
	else if(strstr(macro_name, "CONTACTADDRESS") == macro_name) {

		/* which address do we want? */
		x = atoi(macro_name + 14) - 1;

		/* regular macro */
		if(arg[0] == NULL) {

			/* use the saved pointer */
			if((temp_contact = mac->contact_ptr) == NULL) {
				my_free(buf);
				return ERROR;
				}

			/* get the macro value by reference, so no need to free() */
			*free_macro = FALSE;
			result = grab_contact_address_macro(x, temp_contact, output);
			}

		/* on-demand macro */
		else {

			/* on-demand contact macro with a contactgroup and a delimiter */
			if(arg[1] != NULL) {

				if((temp_contactgroup = find_contactgroup(arg[0])) == NULL)
					return ERROR;

				delimiter_len = strlen(arg[1]);

				/* concatenate macro values for all contactgroup members */
				for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {

					if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
						continue;

					/* get the macro value for this contact */
					grab_contact_address_macro(x, temp_contact, &temp_buffer);

					if(temp_buffer == NULL)
						continue;

					/* add macro value to already running macro */
					if(*output == NULL)
						*output = (char *)strdup(temp_buffer);
					else {
						if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1)) == NULL)
							continue;
						strcat(*output, arg[1]);
						strcat(*output, temp_buffer);
						}
					my_free(temp_buffer);
					}
				}

			/* else on-demand contact macro */
			else {

				/* find the contact */
				if((temp_contact = find_contact(arg[0])) == NULL) {
					my_free(buf);
					return ERROR;
					}

				/* get the macro value */
				result = grab_contact_address_macro(x, temp_contact, output);
				}
			}
		}

	/***** CUSTOM VARIABLE MACROS *****/
	else if(macro_name[0] == '_') {

		/* get the macro value */
		result = grab_custom_macro_value_r(mac, macro_name, arg[0], arg[1], output);
		}

	/* no macro matched... */
	else {
		log_debug_info(DEBUGL_MACROS, 0, " WARNING: Could not find a macro matching '%s'!\n", macro_name);
		result = ERROR;
		}

	/* free memory */
	my_free(buf);

	return result;
	}

int grab_macro_value(char *macro_buffer, char **output, int *clean_options, int *free_macro) {
	return grab_macro_value_r(&global_macros, macro_buffer, output, clean_options, free_macro);
	}


int grab_macrox_value_r(nagios_macros *mac, int macro_type, char *arg1, char *arg2, char **output, int *free_macro) {
	host *temp_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	hostsmember *temp_hostsmember = NULL;
	service *temp_service = NULL;
	servicegroup *temp_servicegroup = NULL;
	servicesmember *temp_servicesmember = NULL;
	contact *temp_contact = NULL;
	contactgroup *temp_contactgroup = NULL;
	contactsmember *temp_contactsmember = NULL;
	char *temp_buffer = NULL;
	int result = OK;
	int delimiter_len = 0;
	int free_sub_macro = FALSE;
#ifdef NSCORE
	register int x;
	int authorized = TRUE;
	int problem = TRUE;
	int hosts_up = 0;
	int hosts_down = 0;
	int hosts_unreachable = 0;
	int hosts_down_unhandled = 0;
	int hosts_unreachable_unhandled = 0;
	int host_problems = 0;
	int host_problems_unhandled = 0;
	int services_ok = 0;
	int services_warning = 0;
	int services_unknown = 0;
	int services_critical = 0;
	int services_warning_unhandled = 0;
	int services_unknown_unhandled = 0;
	int services_critical_unhandled = 0;
	int service_problems = 0;
	int service_problems_unhandled = 0;
#endif


	if(output == NULL || free_macro == NULL)
		return ERROR;

	*free_macro = FALSE;

	/* handle the macro */
	switch(macro_type) {

			/***************/
			/* HOST MACROS */
			/***************/
		case MACRO_HOSTGROUPNAMES:
			*free_macro = TRUE;
		case MACRO_HOSTNAME:
		case MACRO_HOSTALIAS:
		case MACRO_HOSTADDRESS:
		case MACRO_LASTHOSTCHECK:
		case MACRO_LASTHOSTSTATECHANGE:
		case MACRO_HOSTOUTPUT:
		case MACRO_HOSTPERFDATA:
		case MACRO_HOSTSTATE:
		case MACRO_HOSTSTATEID:
		case MACRO_HOSTATTEMPT:
		case MACRO_HOSTEXECUTIONTIME:
		case MACRO_HOSTLATENCY:
		case MACRO_HOSTDURATION:
		case MACRO_HOSTDURATIONSEC:
		case MACRO_HOSTDOWNTIME:
		case MACRO_HOSTSTATETYPE:
		case MACRO_HOSTPERCENTCHANGE:
		case MACRO_HOSTACKAUTHOR:
		case MACRO_HOSTACKCOMMENT:
		case MACRO_LASTHOSTUP:
		case MACRO_LASTHOSTDOWN:
		case MACRO_LASTHOSTUNREACHABLE:
		case MACRO_HOSTCHECKCOMMAND:
		case MACRO_HOSTDISPLAYNAME:
		case MACRO_HOSTACTIONURL:
		case MACRO_HOSTNOTESURL:
		case MACRO_HOSTNOTES:
		case MACRO_HOSTCHECKTYPE:
		case MACRO_LONGHOSTOUTPUT:
		case MACRO_HOSTNOTIFICATIONNUMBER:
		case MACRO_HOSTNOTIFICATIONID:
		case MACRO_HOSTEVENTID:
		case MACRO_LASTHOSTEVENTID:
		case MACRO_HOSTACKAUTHORNAME:
		case MACRO_HOSTACKAUTHORALIAS:
		case MACRO_MAXHOSTATTEMPTS:
		case MACRO_TOTALHOSTSERVICES:
		case MACRO_TOTALHOSTSERVICESOK:
		case MACRO_TOTALHOSTSERVICESWARNING:
		case MACRO_TOTALHOSTSERVICESUNKNOWN:
		case MACRO_TOTALHOSTSERVICESCRITICAL:
		case MACRO_HOSTPROBLEMID:
		case MACRO_LASTHOSTPROBLEMID:
		case MACRO_LASTHOSTSTATE:
		case MACRO_LASTHOSTSTATEID:
		case MACRO_HOSTIMPORTANCE:
		case MACRO_HOSTANDSERVICESIMPORTANCE:

			/* a standard host macro */
			if(arg2 == NULL) {

				/* find the host for on-demand macros */
				if(arg1) {
					if((temp_host = find_host(arg1)) == NULL)
						return ERROR;
					}

				/* else use saved host pointer */
				else if((temp_host = mac->host_ptr) == NULL)
					return ERROR;

				/* get the host macro value */
				result = grab_standard_host_macro_r(mac, macro_type, temp_host, output, free_macro);
				}

			/* a host macro with a hostgroup name and delimiter */
			else {

				if((temp_hostgroup = find_hostgroup(arg1)) == NULL)
					return ERROR;

				delimiter_len = strlen(arg2);

				/* concatenate macro values for all hostgroup members */
				for(temp_hostsmember = temp_hostgroup->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

					if((temp_host = temp_hostsmember->host_ptr) == NULL)
						continue;

					/* get the macro value for this host */
					grab_standard_host_macro_r(mac, macro_type, temp_host, &temp_buffer, &free_sub_macro);

					if(temp_buffer == NULL)
						continue;

					/* add macro value to already running macro */
					if(*output == NULL)
						*output = (char *)strdup(temp_buffer);
					else {
						if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1)) == NULL)
							continue;
						strcat(*output, arg2);
						strcat(*output, temp_buffer);
						}
					if(free_sub_macro == TRUE)
						my_free(temp_buffer);
					}
				}
			break;

			/********************/
			/* HOSTGROUP MACROS */
			/********************/
		case MACRO_HOSTGROUPMEMBERS:
			*free_macro = TRUE;
		case MACRO_HOSTGROUPNAME:
		case MACRO_HOSTGROUPALIAS:
		case MACRO_HOSTGROUPNOTES:
		case MACRO_HOSTGROUPNOTESURL:
		case MACRO_HOSTGROUPACTIONURL:

			/* a standard hostgroup macro */
			/* use the saved hostgroup pointer */
			if(arg1 == NULL) {
				if((temp_hostgroup = mac->hostgroup_ptr) == NULL)
					return ERROR;
				}

			/* else find the hostgroup for on-demand macros */
			else {
				if((temp_hostgroup = find_hostgroup(arg1)) == NULL)
					return ERROR;
				}

			/* get the hostgroup macro value */
			result = grab_standard_hostgroup_macro_r(mac, macro_type, temp_hostgroup, output);
			break;

			/******************/
			/* SERVICE MACROS */
			/******************/
		case MACRO_SERVICEGROUPNAMES:
			*free_macro = TRUE;
		case MACRO_SERVICEDESC:
		case MACRO_SERVICESTATE:
		case MACRO_SERVICESTATEID:
		case MACRO_SERVICEATTEMPT:
		case MACRO_LASTSERVICECHECK:
		case MACRO_LASTSERVICESTATECHANGE:
		case MACRO_SERVICEOUTPUT:
		case MACRO_SERVICEPERFDATA:
		case MACRO_SERVICEEXECUTIONTIME:
		case MACRO_SERVICELATENCY:
		case MACRO_SERVICEDURATION:
		case MACRO_SERVICEDURATIONSEC:
		case MACRO_SERVICEDOWNTIME:
		case MACRO_SERVICESTATETYPE:
		case MACRO_SERVICEPERCENTCHANGE:
		case MACRO_SERVICEACKAUTHOR:
		case MACRO_SERVICEACKCOMMENT:
		case MACRO_LASTSERVICEOK:
		case MACRO_LASTSERVICEWARNING:
		case MACRO_LASTSERVICEUNKNOWN:
		case MACRO_LASTSERVICECRITICAL:
		case MACRO_SERVICECHECKCOMMAND:
		case MACRO_SERVICEDISPLAYNAME:
		case MACRO_SERVICEACTIONURL:
		case MACRO_SERVICENOTESURL:
		case MACRO_SERVICENOTES:
		case MACRO_SERVICECHECKTYPE:
		case MACRO_LONGSERVICEOUTPUT:
		case MACRO_SERVICENOTIFICATIONNUMBER:
		case MACRO_SERVICENOTIFICATIONID:
		case MACRO_SERVICEEVENTID:
		case MACRO_LASTSERVICEEVENTID:
		case MACRO_SERVICEACKAUTHORNAME:
		case MACRO_SERVICEACKAUTHORALIAS:
		case MACRO_MAXSERVICEATTEMPTS:
		case MACRO_SERVICEISVOLATILE:
		case MACRO_SERVICEPROBLEMID:
		case MACRO_LASTSERVICEPROBLEMID:
		case MACRO_LASTSERVICESTATE:
		case MACRO_LASTSERVICESTATEID:
		case MACRO_SERVICEIMPORTANCE:

			/* use saved service pointer */
			if(arg1 == NULL && arg2 == NULL) {

				if((temp_service = mac->service_ptr) == NULL)
					return ERROR;

				result = grab_standard_service_macro_r(mac, macro_type, temp_service, output, free_macro);
				}

			/* else and ondemand macro... */
			else {

				/* if first arg is blank, it means use the current host name */
				if(arg1 == NULL || arg1[0] == '\x0') {

					if(mac->host_ptr == NULL)
						return ERROR;

					if((temp_service = find_service(mac->host_ptr->name, arg2))) {

						/* get the service macro value */
						result = grab_standard_service_macro_r(mac, macro_type, temp_service, output, free_macro);
						}
					}

				/* on-demand macro with both host and service name */
				else if((temp_service = find_service(arg1, arg2))) {

					/* get the service macro value */
					result = grab_standard_service_macro_r(mac, macro_type, temp_service, output, free_macro);
					}

				/* else we have a service macro with a servicegroup name and a delimiter... */
				else if(arg1 && arg2) {

					if((temp_servicegroup = find_servicegroup(arg1)) == NULL)
						return ERROR;

					delimiter_len = strlen(arg2);
					*free_macro = TRUE;

					/* concatenate macro values for all servicegroup members */
					for(temp_servicesmember = temp_servicegroup->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {

						if((temp_service = temp_servicesmember->service_ptr) == NULL)
							continue;

						/* get the macro value for this service */
						grab_standard_service_macro_r(mac, macro_type, temp_service, &temp_buffer, &free_sub_macro);

						if(temp_buffer == NULL)
							continue;

						/* add macro value to already running macro */
						if(*output == NULL)
							*output = (char *)strdup(temp_buffer);
						else {
							if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1)) == NULL)
								continue;
							strcat(*output, arg2);
							strcat(*output, temp_buffer);
							}
						if(free_sub_macro == TRUE)
							my_free(temp_buffer);
						}
					}
				else
					return ERROR;
				}
			break;

			/***********************/
			/* SERVICEGROUP MACROS */
			/***********************/
		case MACRO_SERVICEGROUPMEMBERS:
		case MACRO_SERVICEGROUPNOTES:
		case MACRO_SERVICEGROUPNOTESURL:
		case MACRO_SERVICEGROUPACTIONURL:
			*free_macro = TRUE;
		case MACRO_SERVICEGROUPNAME:
		case MACRO_SERVICEGROUPALIAS:
			/* a standard servicegroup macro */
			/* use the saved servicegroup pointer */
			if(arg1 == NULL) {
				if((temp_servicegroup = mac->servicegroup_ptr) == NULL)
					return ERROR;
				}

			/* else find the servicegroup for on-demand macros */
			else {
				if((temp_servicegroup = find_servicegroup(arg1)) == NULL)
					return ERROR;
				}

			/* get the servicegroup macro value */
			result = grab_standard_servicegroup_macro_r(mac, macro_type, temp_servicegroup, output);
			break;

			/******************/
			/* CONTACT MACROS */
			/******************/
		case MACRO_CONTACTGROUPNAMES:
			*free_macro = TRUE;
		case MACRO_CONTACTNAME:
		case MACRO_CONTACTALIAS:
		case MACRO_CONTACTEMAIL:
		case MACRO_CONTACTPAGER:
			/* a standard contact macro */
			if(arg2 == NULL) {

				/* find the contact for on-demand macros */
				if(arg1) {
					if((temp_contact = find_contact(arg1)) == NULL)
						return ERROR;
					}

				/* else use saved contact pointer */
				else if((temp_contact = mac->contact_ptr) == NULL)
					return ERROR;

				/* get the contact macro value */
				result = grab_standard_contact_macro_r(mac, macro_type, temp_contact, output);
				}

			/* a contact macro with a contactgroup name and delimiter */
			else {

				if((temp_contactgroup = find_contactgroup(arg1)) == NULL)
					return ERROR;

				delimiter_len = strlen(arg2);
				*free_macro = TRUE;

				/* concatenate macro values for all contactgroup members */
				for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {

					if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
						continue;

					/* get the macro value for this contact */
					grab_standard_contact_macro_r(mac, macro_type, temp_contact, &temp_buffer);

					if(temp_buffer == NULL)
						continue;

					/* add macro value to already running macro */
					if(*output == NULL)
						*output = (char *)strdup(temp_buffer);
					else {
						if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1)) == NULL)
							continue;
						strcat(*output, arg2);
						strcat(*output, temp_buffer);
						}
					my_free(temp_buffer);
					}
				}
			break;

			/***********************/
			/* CONTACTGROUP MACROS */
			/***********************/
		case MACRO_CONTACTGROUPMEMBERS:
			*free_macro = TRUE;
		case MACRO_CONTACTGROUPNAME:
		case MACRO_CONTACTGROUPALIAS:
			/* a standard contactgroup macro */
			/* use the saved contactgroup pointer */
			if(arg1 == NULL) {
				if((temp_contactgroup = mac->contactgroup_ptr) == NULL)
					return ERROR;
				}

			/* else find the contactgroup for on-demand macros */
			else {
				if((temp_contactgroup = find_contactgroup(arg1)) == NULL)
					return ERROR;
				}

			/* get the contactgroup macro value */
			result = grab_standard_contactgroup_macro(macro_type, temp_contactgroup, output);
			break;

			/***********************/
			/* NOTIFICATION MACROS */
			/***********************/
		case MACRO_NOTIFICATIONTYPE:
		case MACRO_NOTIFICATIONNUMBER:
		case MACRO_NOTIFICATIONRECIPIENTS:
		case MACRO_NOTIFICATIONISESCALATED:
		case MACRO_NOTIFICATIONAUTHOR:
		case MACRO_NOTIFICATIONAUTHORNAME:
		case MACRO_NOTIFICATIONAUTHORALIAS:
		case MACRO_NOTIFICATIONCOMMENT:

			/* notification macros have already been pre-computed */
			*output = mac->x[macro_type];
			*free_macro = FALSE;
			break;

			/********************/
			/* DATE/TIME MACROS */
			/********************/
		case MACRO_LONGDATETIME:
		case MACRO_SHORTDATETIME:
		case MACRO_DATE:
		case MACRO_TIME:
			*free_macro = TRUE;
		case MACRO_TIMET:
		case MACRO_ISVALIDTIME:
		case MACRO_NEXTVALIDTIME:

			/* calculate macros */
			result = grab_datetime_macro_r(mac, macro_type, arg1, arg2, output);
			break;

			/*****************/
			/* STATIC MACROS */
			/*****************/
		case MACRO_ADMINEMAIL:
		case MACRO_ADMINPAGER:
		case MACRO_MAINCONFIGFILE:
		case MACRO_STATUSDATAFILE:
		case MACRO_RETENTIONDATAFILE:
		case MACRO_OBJECTCACHEFILE:
		case MACRO_TEMPFILE:
		case MACRO_LOGFILE:
		case MACRO_RESOURCEFILE:
		case MACRO_COMMANDFILE:
		case MACRO_HOSTPERFDATAFILE:
		case MACRO_SERVICEPERFDATAFILE:
		case MACRO_PROCESSSTARTTIME:
		case MACRO_TEMPPATH:
		case MACRO_EVENTSTARTTIME:

			/* no need to do any more work - these are already precomputed for us */
			*output = global_macros.x[macro_type];
			*free_macro = FALSE;
			break;

			/******************/
			/* SUMMARY MACROS */
			/******************/
		case MACRO_TOTALHOSTSUP:
		case MACRO_TOTALHOSTSDOWN:
		case MACRO_TOTALHOSTSUNREACHABLE:
		case MACRO_TOTALHOSTSDOWNUNHANDLED:
		case MACRO_TOTALHOSTSUNREACHABLEUNHANDLED:
		case MACRO_TOTALHOSTPROBLEMS:
		case MACRO_TOTALHOSTPROBLEMSUNHANDLED:
		case MACRO_TOTALSERVICESOK:
		case MACRO_TOTALSERVICESWARNING:
		case MACRO_TOTALSERVICESCRITICAL:
		case MACRO_TOTALSERVICESUNKNOWN:
		case MACRO_TOTALSERVICESWARNINGUNHANDLED:
		case MACRO_TOTALSERVICESCRITICALUNHANDLED:
		case MACRO_TOTALSERVICESUNKNOWNUNHANDLED:
		case MACRO_TOTALSERVICEPROBLEMS:
		case MACRO_TOTALSERVICEPROBLEMSUNHANDLED:

#ifdef NSCORE
			/* generate summary macros if needed */
			if(mac->x[MACRO_TOTALHOSTSUP] == NULL) {

				/* get host totals */
				for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

					/* filter totals based on contact if necessary */
					if(mac->contact_ptr != NULL)
						authorized = is_contact_for_host(temp_host, mac->contact_ptr);

					if(authorized == TRUE) {
						problem = TRUE;

						if(temp_host->current_state == HOST_UP && temp_host->has_been_checked == TRUE)
							hosts_up++;
						else if(temp_host->current_state == HOST_DOWN) {
							if(temp_host->scheduled_downtime_depth > 0)
								problem = FALSE;
							if(temp_host->problem_has_been_acknowledged == TRUE)
								problem = FALSE;
							if(temp_host->checks_enabled == FALSE)
								problem = FALSE;
							if(problem == TRUE)
								hosts_down_unhandled++;
							hosts_down++;
							}
						else if(temp_host->current_state == HOST_UNREACHABLE) {
							if(temp_host->scheduled_downtime_depth > 0)
								problem = FALSE;
							if(temp_host->problem_has_been_acknowledged == TRUE)
								problem = FALSE;
							if(temp_host->checks_enabled == FALSE)
								problem = FALSE;
							if(problem == TRUE)
								hosts_down_unhandled++;
							hosts_unreachable++;
							}
						}
					}

				host_problems = hosts_down + hosts_unreachable;
				host_problems_unhandled = hosts_down_unhandled + hosts_unreachable_unhandled;

				/* get service totals */
				for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

					/* filter totals based on contact if necessary */
					if(mac->contact_ptr != NULL)
						authorized = is_contact_for_service(temp_service, mac->contact_ptr);

					if(authorized == TRUE) {
						problem = TRUE;

						if(temp_service->current_state == STATE_OK && temp_service->has_been_checked == TRUE)
							services_ok++;
						else if(temp_service->current_state == STATE_WARNING) {
							temp_host = find_host(temp_service->host_name);
							if(temp_host != NULL && (temp_host->current_state == HOST_DOWN || temp_host->current_state == HOST_UNREACHABLE))
								problem = FALSE;
							if(temp_service->scheduled_downtime_depth > 0)
								problem = FALSE;
							if(temp_service->problem_has_been_acknowledged == TRUE)
								problem = FALSE;
							if(temp_service->checks_enabled == FALSE)
								problem = FALSE;
							if(problem == TRUE)
								services_warning_unhandled++;
							services_warning++;
							}
						else if(temp_service->current_state == STATE_UNKNOWN) {
							temp_host = find_host(temp_service->host_name);
							if(temp_host != NULL && (temp_host->current_state == HOST_DOWN || temp_host->current_state == HOST_UNREACHABLE))
								problem = FALSE;
							if(temp_service->scheduled_downtime_depth > 0)
								problem = FALSE;
							if(temp_service->problem_has_been_acknowledged == TRUE)
								problem = FALSE;
							if(temp_service->checks_enabled == FALSE)
								problem = FALSE;
							if(problem == TRUE)
								services_unknown_unhandled++;
							services_unknown++;
							}
						else if(temp_service->current_state == STATE_CRITICAL) {
							temp_host = find_host(temp_service->host_name);
							if(temp_host != NULL && (temp_host->current_state == HOST_DOWN || temp_host->current_state == HOST_UNREACHABLE))
								problem = FALSE;
							if(temp_service->scheduled_downtime_depth > 0)
								problem = FALSE;
							if(temp_service->problem_has_been_acknowledged == TRUE)
								problem = FALSE;
							if(temp_service->checks_enabled == FALSE)
								problem = FALSE;
							if(problem == TRUE)
								services_critical_unhandled++;
							services_critical++;
							}
						}
					}

				service_problems = services_warning + services_critical + services_unknown;
				service_problems_unhandled = services_warning_unhandled + services_critical_unhandled + services_unknown_unhandled;

				/* these macros are time-intensive to compute, and will likely be used together, so save them all for future use */
				for(x = MACRO_TOTALHOSTSUP; x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED; x++)
					my_free(mac->x[x]);
				asprintf(&mac->x[MACRO_TOTALHOSTSUP], "%d", hosts_up);
				asprintf(&mac->x[MACRO_TOTALHOSTSDOWN], "%d", hosts_down);
				asprintf(&mac->x[MACRO_TOTALHOSTSUNREACHABLE], "%d", hosts_unreachable);
				asprintf(&mac->x[MACRO_TOTALHOSTSDOWNUNHANDLED], "%d", hosts_down_unhandled);
				asprintf(&mac->x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED], "%d", hosts_unreachable_unhandled);
				asprintf(&mac->x[MACRO_TOTALHOSTPROBLEMS], "%d", host_problems);
				asprintf(&mac->x[MACRO_TOTALHOSTPROBLEMSUNHANDLED], "%d", host_problems_unhandled);
				asprintf(&mac->x[MACRO_TOTALSERVICESOK], "%d", services_ok);
				asprintf(&mac->x[MACRO_TOTALSERVICESWARNING], "%d", services_warning);
				asprintf(&mac->x[MACRO_TOTALSERVICESCRITICAL], "%d", services_critical);
				asprintf(&mac->x[MACRO_TOTALSERVICESUNKNOWN], "%d", services_unknown);
				asprintf(&mac->x[MACRO_TOTALSERVICESWARNINGUNHANDLED], "%d", services_warning_unhandled);
				asprintf(&mac->x[MACRO_TOTALSERVICESCRITICALUNHANDLED], "%d", services_critical_unhandled);
				asprintf(&mac->x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED], "%d", services_unknown_unhandled);
				asprintf(&mac->x[MACRO_TOTALSERVICEPROBLEMS], "%d", service_problems);
				asprintf(&mac->x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED], "%d", service_problems_unhandled);
				}

			/* return only the macro the user requested */
			*output = mac->x[macro_type];

			/* tell caller to NOT free memory when done */
			*free_macro = FALSE;
#endif
			break;

		default:
			log_debug_info(DEBUGL_MACROS, 0, "UNHANDLED MACRO #%d! THIS IS A BUG!\n", macro_type);
			return ERROR;
			break;
		}

	return result;
	}

int grab_macrox_value(int macro_type, char *arg1, char *arg2, char **output, int *free_macro) {
	return grab_macrox_value_r(&global_macros, macro_type, arg1, arg2, output, free_macro);
	}


/* calculates the value of a custom macro */
int grab_custom_macro_value_r(nagios_macros *mac, char *macro_name, char *arg1, char *arg2, char **output) {
	host *temp_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	hostsmember *temp_hostsmember = NULL;
	service *temp_service = NULL;
	servicegroup *temp_servicegroup = NULL;
	servicesmember *temp_servicesmember = NULL;
	contact *temp_contact = NULL;
	contactgroup *temp_contactgroup = NULL;
	contactsmember *temp_contactsmember = NULL;
	int delimiter_len = 0;
	char *temp_buffer = NULL;
	int result = OK;

	if(macro_name == NULL || output == NULL)
		return ERROR;

	/***** CUSTOM HOST MACRO *****/
	if(strstr(macro_name, "_HOST") == macro_name) {

		/* a standard host macro */
		if(arg2 == NULL) {

			/* find the host for on-demand macros */
			if(arg1) {
				if((temp_host = find_host(arg1)) == NULL)
					return ERROR;
				}

			/* else use saved host pointer */
			else if((temp_host = mac->host_ptr) == NULL)
				return ERROR;

			/* get the host macro value */
			result = grab_custom_object_macro_r(mac, macro_name + 5, temp_host->custom_variables, output);
			}

		/* a host macro with a hostgroup name and delimiter */
		else {
			if((temp_hostgroup = find_hostgroup(arg1)) == NULL)
				return ERROR;

			delimiter_len = strlen(arg2);

			/* concatenate macro values for all hostgroup members */
			for(temp_hostsmember = temp_hostgroup->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

				if((temp_host = temp_hostsmember->host_ptr) == NULL)
					continue;

				/* get the macro value for this host */
				grab_custom_macro_value_r(mac, macro_name, temp_host->name, NULL, &temp_buffer);

				if(temp_buffer == NULL)
					continue;

				/* add macro value to already running macro */
				if(*output == NULL)
					*output = (char *)strdup(temp_buffer);
				else {
					if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1)) == NULL)
						continue;
					strcat(*output, arg2);
					strcat(*output, temp_buffer);
					}
				my_free(temp_buffer);
				}
			}
		}

	/***** CUSTOM SERVICE MACRO *****/
	else if(strstr(macro_name, "_SERVICE") == macro_name) {

		/* use saved service pointer */
		if(arg1 == NULL && arg2 == NULL) {

			if((temp_service = mac->service_ptr) == NULL)
				return ERROR;

			/* get the service macro value */
			result = grab_custom_object_macro_r(mac, macro_name + 8, temp_service->custom_variables, output);
			}

		/* else and ondemand macro... */
		else {

			/* if first arg is blank, it means use the current host name */
			if(mac->host_ptr == NULL)
				return ERROR;
			if((temp_service = find_service((mac->host_ptr) ? mac->host_ptr->name : NULL, arg2))) {

				/* get the service macro value */
				result = grab_custom_object_macro_r(mac, macro_name + 8, temp_service->custom_variables, output);
				}

			/* else we have a service macro with a servicegroup name and a delimiter... */
			else {

				if((temp_servicegroup = find_servicegroup(arg1)) == NULL)
					return ERROR;

				delimiter_len = strlen(arg2);

				/* concatenate macro values for all servicegroup members */
				for(temp_servicesmember = temp_servicegroup->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {

					if((temp_service = temp_servicesmember->service_ptr) == NULL)
						continue;

					/* get the macro value for this service */
					grab_custom_macro_value_r(mac, macro_name, temp_service->host_name, temp_service->description, &temp_buffer);

					if(temp_buffer == NULL)
						continue;

					/* add macro value to already running macro */
					if(*output == NULL)
						*output = (char *)strdup(temp_buffer);
					else {
						if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1)) == NULL)
							continue;
						strcat(*output, arg2);
						strcat(*output, temp_buffer);
						}
					my_free(temp_buffer);
					}
				}
			}
		}

	/***** CUSTOM CONTACT VARIABLE *****/
	else if(strstr(macro_name, "_CONTACT") == macro_name) {

		/* a standard contact macro */
		if(arg2 == NULL) {

			/* find the contact for on-demand macros */
			if(arg1) {
				if((temp_contact = find_contact(arg1)) == NULL)
					return ERROR;
				}

			/* else use saved contact pointer */
			else if((temp_contact = mac->contact_ptr) == NULL)
				return ERROR;

			/* get the contact macro value */
			result = grab_custom_object_macro_r(mac, macro_name + 8, temp_contact->custom_variables, output);
			}

		/* a contact macro with a contactgroup name and delimiter */
		else {

			if((temp_contactgroup = find_contactgroup(arg1)) == NULL)
				return ERROR;

			delimiter_len = strlen(arg2);

			/* concatenate macro values for all contactgroup members */
			for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {

				if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
					continue;

				/* get the macro value for this contact */
				grab_custom_macro_value_r(mac, macro_name, temp_contact->name, NULL, &temp_buffer);

				if(temp_buffer == NULL)
					continue;

				/* add macro value to already running macro */
				if(*output == NULL)
					*output = (char *)strdup(temp_buffer);
				else {
					if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1)) == NULL)
						continue;
					strcat(*output, arg2);
					strcat(*output, temp_buffer);
					}
				my_free(temp_buffer);
				}
			}
		}

	else
		return ERROR;

	return result;
	}

int grab_custom_macro_value(char *macro_name, char *arg1, char *arg2, char **output) {
	return grab_custom_macro_value_r(&global_macros, macro_name, arg1, arg2, output);
	}


/* calculates a date/time macro */
int grab_datetime_macro_r(nagios_macros *mac, int macro_type, char *arg1, char *arg2, char **output) {
	time_t current_time = 0L;
	timeperiod *temp_timeperiod = NULL;
#ifdef NSCORE
	time_t test_time = 0L;
	time_t next_valid_time = 0L;
#endif

	if(output == NULL)
		return ERROR;

	/* get the current time */
	time(&current_time);

	/* parse args, do prep work */
	switch(macro_type) {

		case MACRO_ISVALIDTIME:
		case MACRO_NEXTVALIDTIME:

			/* find the timeperiod */
			if((temp_timeperiod = find_timeperiod(arg1)) == NULL)
				return ERROR;

			/* what timestamp should we use? */
#ifdef NSCORE
			if(arg2)
				test_time = (time_t)strtoul(arg2, NULL, 0);
			else
				test_time = current_time;
#endif
			break;

		default:
			break;
		}

	/* calculate the value */
	switch(macro_type) {

		case MACRO_LONGDATETIME:
			if(*output == NULL)
				*output = (char *)malloc(MAX_DATETIME_LENGTH);
			if(*output)
				get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, LONG_DATE_TIME);
			break;

		case MACRO_SHORTDATETIME:
			if(*output == NULL)
				*output = (char *)malloc(MAX_DATETIME_LENGTH);
			if(*output)
				get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, SHORT_DATE_TIME);
			break;

		case MACRO_DATE:
			if(*output == NULL)
				*output = (char *)malloc(MAX_DATETIME_LENGTH);
			if(*output)
				get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, SHORT_DATE);
			break;

		case MACRO_TIME:
			if(*output == NULL)
				*output = (char *)malloc(MAX_DATETIME_LENGTH);
			if(*output)
				get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, SHORT_TIME);
			break;

		case MACRO_TIMET:
			*output = (char *)mkstr("%lu", (unsigned long)current_time);
			break;

#ifdef NSCORE
		case MACRO_ISVALIDTIME:
			*output = (char *)mkstr("%d", (check_time_against_period(test_time, temp_timeperiod) == OK) ? 1 : 0);
			break;

		case MACRO_NEXTVALIDTIME:
			get_next_valid_time(test_time, &next_valid_time, temp_timeperiod);
			if(next_valid_time == test_time && check_time_against_period(test_time, temp_timeperiod) == ERROR)
				next_valid_time = (time_t)0L;
			*output = (char *)mkstr("%lu", (unsigned long)next_valid_time);
			break;
#endif

		default:
			return ERROR;
			break;
		}

	return OK;
	}

int grab_datetime_macro(int macro_type, char *arg1, char *arg2, char **output) {
	return grab_datetime_macro_r(&global_macros, macro_type, arg1, arg2, output);
	}


/* calculates a host macro */
int grab_standard_host_macro_r(nagios_macros *mac, int macro_type, host *temp_host, char **output, int *free_macro) {
	char *temp_buffer = NULL;
#ifdef NSCORE
	hostgroup *temp_hostgroup = NULL;
	servicesmember *temp_servicesmember = NULL;
	service *temp_service = NULL;
	objectlist *temp_objectlist = NULL;
	time_t current_time = 0L;
	unsigned long duration = 0L;
	int days = 0;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	char *buf1 = NULL;
	char *buf2 = NULL;
	int total_host_services = 0;
	int total_host_services_ok = 0;
	int total_host_services_warning = 0;
	int total_host_services_unknown = 0;
	int total_host_services_critical = 0;
#endif

	if(temp_host == NULL || output == NULL || free_macro == NULL)
		return ERROR;

	/* get the macro */
	switch(macro_type) {

		case MACRO_HOSTNAME:
			*output = temp_host->name;
			break;
		case MACRO_HOSTDISPLAYNAME:
			if(temp_host->display_name)
				*output = temp_host->display_name;
			break;
		case MACRO_HOSTALIAS:
			*output = temp_host->alias;
			break;
		case MACRO_HOSTADDRESS:
			*output = temp_host->address;
			break;
#ifdef NSCORE
		case MACRO_HOSTSTATE:
			*output = (char *)host_state_name(temp_host->current_state);
			break;
		case MACRO_HOSTSTATEID:
			*output = (char *)mkstr("%d", temp_host->current_state);
			break;
		case MACRO_LASTHOSTSTATE:
			*output = (char *)host_state_name(temp_host->last_state);
			break;
		case MACRO_LASTHOSTSTATEID:
			*output = (char *)mkstr("%d", temp_host->last_state);
			break;
		case MACRO_HOSTCHECKTYPE:
			*output = (char *)check_type_name(temp_host->check_type);
			break;
		case MACRO_HOSTSTATETYPE:
			*output = (char *)state_type_name(temp_host->state_type);
			break;
		case MACRO_HOSTOUTPUT:
			if(temp_host->plugin_output)
				*output = temp_host->plugin_output;
			break;
		case MACRO_LONGHOSTOUTPUT:
			if(temp_host->long_plugin_output)
				*output = temp_host->long_plugin_output;
			break;
		case MACRO_HOSTPERFDATA:
			if(temp_host->perf_data)
				*output = temp_host->perf_data;
			break;
#endif
		case MACRO_HOSTCHECKCOMMAND:
			if(temp_host->check_command)
				*output = temp_host->check_command;
			break;
#ifdef NSCORE
		case MACRO_HOSTATTEMPT:
			*output = (char *)mkstr("%d", temp_host->current_attempt);
			break;
		case MACRO_MAXHOSTATTEMPTS:
			*output = (char *)mkstr("%d", temp_host->max_attempts);
			break;
		case MACRO_HOSTDOWNTIME:
			*output = (char *)mkstr("%d", temp_host->scheduled_downtime_depth);
			break;
		case MACRO_HOSTPERCENTCHANGE:
			*output = (char *)mkstr("%.2f", temp_host->percent_state_change);
			break;
		case MACRO_HOSTDURATIONSEC:
		case MACRO_HOSTDURATION:
			time(&current_time);
			duration = (unsigned long)(current_time - temp_host->last_state_change);

			if(macro_type == MACRO_HOSTDURATIONSEC)
				*output = (char *)mkstr("%lu", duration);
			else {

				days = duration / 86400;
				duration -= (days * 86400);
				hours = duration / 3600;
				duration -= (hours * 3600);
				minutes = duration / 60;
				duration -= (minutes * 60);
				seconds = duration;
				*output = (char *)mkstr("%dd %dh %dm %ds", days, hours, minutes, seconds);
				}
			break;
		case MACRO_HOSTEXECUTIONTIME:
			*output = (char *)mkstr("%.3f", temp_host->execution_time);
			break;
		case MACRO_HOSTLATENCY:
			*output = (char *)mkstr("%.3f", temp_host->latency);
			break;
		case MACRO_LASTHOSTCHECK:
			*output = (char *)mkstr("%lu", (unsigned long)temp_host->last_check);
			break;
		case MACRO_LASTHOSTSTATECHANGE:
			*output = (char *)mkstr("%lu", (unsigned long)temp_host->last_state_change);
			break;
		case MACRO_LASTHOSTUP:
			*output = (char *)mkstr("%lu", (unsigned long)temp_host->last_time_up);
			break;
		case MACRO_LASTHOSTDOWN:
			*output = (char *)mkstr("%lu", (unsigned long)temp_host->last_time_down);
			break;
		case MACRO_LASTHOSTUNREACHABLE:
			*output = (char *)mkstr("%lu", (unsigned long)temp_host->last_time_unreachable);
			break;
		case MACRO_HOSTNOTIFICATIONNUMBER:
			*output = (char *)mkstr("%d", temp_host->current_notification_number);
			break;
		case MACRO_HOSTNOTIFICATIONID:
			*output = (char *)mkstr("%lu", temp_host->current_notification_id);
			break;
		case MACRO_HOSTEVENTID:
			*output = (char *)mkstr("%lu", temp_host->current_event_id);
			break;
		case MACRO_LASTHOSTEVENTID:
			*output = (char *)mkstr("%lu", temp_host->last_event_id);
			break;
		case MACRO_HOSTPROBLEMID:
			*output = (char *)mkstr("%lu", temp_host->current_problem_id);
			break;
		case MACRO_LASTHOSTPROBLEMID:
			*output = (char *)mkstr("%lu", temp_host->last_problem_id);
			break;
#endif
		case MACRO_HOSTACTIONURL:
			if(temp_host->action_url)
				*output = temp_host->action_url;
			break;
		case MACRO_HOSTNOTESURL:
			if(temp_host->notes_url)
				*output = temp_host->notes_url;
			break;
		case MACRO_HOSTNOTES:
			if(temp_host->notes)
				*output = temp_host->notes;
			break;
#ifdef NSCORE
		case MACRO_HOSTGROUPNAMES:
			/* find all hostgroups this host is associated with */
			for(temp_objectlist = temp_host->hostgroups_ptr; temp_objectlist != NULL; temp_objectlist = temp_objectlist->next) {

				if((temp_hostgroup = (hostgroup *)temp_objectlist->object_ptr) == NULL)
					continue;

				asprintf(&buf1, "%s%s%s", (buf2) ? buf2 : "", (buf2) ? "," : "", temp_hostgroup->group_name);
				my_free(buf2);
				buf2 = buf1;
				}
			if(buf2) {
				*output = (char *)strdup(buf2);
				my_free(buf2);
				}
			break;
		case MACRO_TOTALHOSTSERVICES:
		case MACRO_TOTALHOSTSERVICESOK:
		case MACRO_TOTALHOSTSERVICESWARNING:
		case MACRO_TOTALHOSTSERVICESUNKNOWN:
		case MACRO_TOTALHOSTSERVICESCRITICAL:

			/* generate host service summary macros (if they haven't already been computed) */
			if(mac->x[MACRO_TOTALHOSTSERVICES] == NULL) {

				for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
					if((temp_service = temp_servicesmember->service_ptr) == NULL)
						continue;

					total_host_services++;

					switch(temp_service->current_state) {
						case STATE_OK:
							total_host_services_ok++;
							break;
						case STATE_WARNING:
							total_host_services_warning++;
							break;
						case STATE_UNKNOWN:
							total_host_services_unknown++;
							break;
						case STATE_CRITICAL:
							total_host_services_critical++;
							break;
						default:
							break;
						}
					}

				/* these macros are time-intensive to compute, and will likely be used together, so save them all for future use */
				mac->x[MACRO_TOTALHOSTSERVICES] = 
						(char *)mkstr("%d", total_host_services);
				mac->x[MACRO_TOTALHOSTSERVICESOK] = 
						(char *)mkstr("%d", total_host_services_ok);
				mac->x[MACRO_TOTALHOSTSERVICESWARNING] = 
						(char *)mkstr("%d", total_host_services_warning);
				mac->x[MACRO_TOTALHOSTSERVICESUNKNOWN] = 
						(char *)mkstr("%d", total_host_services_unknown);
				mac->x[MACRO_TOTALHOSTSERVICESCRITICAL] = 
						(char *)mkstr("%d", total_host_services_critical);
				}

			/* return only the macro the user requested */
			*output = mac->x[macro_type];
			break;
		case MACRO_HOSTIMPORTANCE:
			*output = (char *)mkstr("%u", temp_host->hourly_value);
			break;
		case MACRO_HOSTANDSERVICESIMPORTANCE:
			*output = (char *)mkstr("%u", temp_host->hourly_value +
					host_services_value(temp_host));
			break;
#endif

			/***************/
			/* MISC MACROS */
			/***************/
		case MACRO_HOSTACKAUTHOR:
		case MACRO_HOSTACKAUTHORNAME:
		case MACRO_HOSTACKAUTHORALIAS:
		case MACRO_HOSTACKCOMMENT:
			/* no need to do any more work - these are already precomputed elsewhere */
			/* NOTE: these macros won't work as on-demand macros */
			*output = mac->x[macro_type];
			break;

		default:
			log_debug_info(DEBUGL_MACROS, 0, "UNHANDLED HOST MACRO #%d! THIS IS A BUG!\n", macro_type);
			return ERROR;
			break;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type) {
		case MACRO_HOSTACTIONURL:
		case MACRO_HOSTNOTESURL:
			*free_macro = TRUE;
			process_macros_r(mac, *output, &temp_buffer, URL_ENCODE_MACRO_CHARS);
			*output = temp_buffer;
			break;
		case MACRO_HOSTNOTES:
			*free_macro = TRUE;
			process_macros_r(mac, *output, &temp_buffer, 0);
			*output = temp_buffer;
			break;
		default:
			break;
		}

	return OK;
	}

int grab_standard_host_macro(int macro_type, host *temp_host, char **output, int *free_macro) {
	return grab_standard_host_macro_r(&global_macros, macro_type, temp_host, output, free_macro);
	}


/* computes a hostgroup macro */
int grab_standard_hostgroup_macro_r(nagios_macros *mac, int macro_type, hostgroup *temp_hostgroup, char **output) {
	hostsmember *temp_hostsmember = NULL;
	char *temp_buffer = NULL;
	unsigned int	temp_len = 0;
	unsigned int	init_len = 0;

	if(temp_hostgroup == NULL || output == NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type) {
		case MACRO_HOSTGROUPNAME:
			*output = temp_hostgroup->group_name;
			break;
		case MACRO_HOSTGROUPALIAS:
			if(temp_hostgroup->alias)
				*output = temp_hostgroup->alias;
			break;
		case MACRO_HOSTGROUPMEMBERS:
			/* make the calculations for total string length */
			for(temp_hostsmember = temp_hostgroup->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
				if(temp_hostsmember->host_name == NULL)
					continue;
				if(temp_len == 0) {
					temp_len += strlen(temp_hostsmember->host_name) + 1;
					}
				else {
					temp_len += strlen(temp_hostsmember->host_name) + 2;
					}
				}
			if(!temp_len) {
				/* empty group, so return the nul string */
				*output = calloc(1, 1);
				return OK;
				}

			/* allocate or reallocate the memory buffer */
			if(*output == NULL) {
				*output = (char *)malloc(temp_len);
				}
			else {
				init_len = strlen(*output);
				temp_len += init_len;
				*output = (char *)realloc(*output, temp_len);
				}
			/* now fill in the string with the member names */
			for(temp_hostsmember = temp_hostgroup->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
				if(temp_hostsmember->host_name == NULL)
					continue;
				temp_buffer = *output + init_len;
				if(init_len == 0) {  /* If our buffer didn't contain anything, we just need to write "%s,%s" */
					init_len += sprintf(temp_buffer, "%s", temp_hostsmember->host_name);
					}
				else {
					init_len += sprintf(temp_buffer, ",%s", temp_hostsmember->host_name);
					}
				}
			break;
		case MACRO_HOSTGROUPACTIONURL:
			if(temp_hostgroup->action_url)
				*output = temp_hostgroup->action_url;
			break;
		case MACRO_HOSTGROUPNOTESURL:
			if(temp_hostgroup->notes_url)
				*output = temp_hostgroup->notes_url;
			break;
		case MACRO_HOSTGROUPNOTES:
			if(temp_hostgroup->notes)
				*output = temp_hostgroup->notes;
			break;
		default:
			log_debug_info(DEBUGL_MACROS, 0, "UNHANDLED HOSTGROUP MACRO #%d! THIS IS A BUG!\n", macro_type);
			return ERROR;
			break;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type) {
		case MACRO_HOSTGROUPACTIONURL:
		case MACRO_HOSTGROUPNOTESURL:
			process_macros_r(mac, *output, &temp_buffer, URL_ENCODE_MACRO_CHARS);
			*output = temp_buffer;
			break;
		case MACRO_HOSTGROUPNOTES:
			process_macros_r(mac, *output, &temp_buffer, 0);
			*output = temp_buffer;
			break;
		default:
			break;
		}

	return OK;
	}

int grab_standard_hostgroup_macro(int macro_type, hostgroup *temp_hostgroup, char **output) {
	return grab_standard_hostgroup_macro_r(&global_macros, macro_type, temp_hostgroup, output);
	}


/* computes a service macro */
int grab_standard_service_macro_r(nagios_macros *mac, int macro_type, service *temp_service, char **output, int *free_macro) {
	char *temp_buffer = NULL;
#ifdef NSCORE
	servicegroup *temp_servicegroup = NULL;
	objectlist *temp_objectlist = NULL;
	time_t current_time = 0L;
	unsigned long duration = 0L;
	int days = 0;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	char *buf1 = NULL;
	char *buf2 = NULL;
#endif

	if(temp_service == NULL || output == NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type) {
		case MACRO_SERVICEDESC:
			*output = temp_service->description;
			break;
		case MACRO_SERVICEDISPLAYNAME:
			if(temp_service->display_name)
				*output = temp_service->display_name;
			break;
#ifdef NSCORE
		case MACRO_SERVICEOUTPUT:
			if(temp_service->plugin_output)
				*output = temp_service->plugin_output;
			break;
		case MACRO_LONGSERVICEOUTPUT:
			if(temp_service->long_plugin_output)
				*output = temp_service->long_plugin_output;
			break;
		case MACRO_SERVICEPERFDATA:
			if(temp_service->perf_data)
				*output = temp_service->perf_data;
			break;
#endif
		case MACRO_SERVICECHECKCOMMAND:
			if(temp_service->check_command)
				*output = temp_service->check_command;
			break;
#ifdef NSCORE
		case MACRO_SERVICECHECKTYPE:
			*output = (char *)check_type_name(temp_service->check_type);
			break;
		case MACRO_SERVICESTATETYPE:
			*output = (char *)state_type_name(temp_service->state_type);
			break;
		case MACRO_SERVICESTATE:
			*output = (char *)service_state_name(temp_service->current_state);
			break;
		case MACRO_SERVICESTATEID:
			*output = (char *)mkstr("%d", temp_service->current_state);
			break;
		case MACRO_LASTSERVICESTATE:
			*output = (char *)service_state_name(temp_service->last_state);
			break;
		case MACRO_LASTSERVICESTATEID:
			*output = (char *)mkstr("%d", temp_service->last_state);
			break;
#endif
		case MACRO_SERVICEISVOLATILE:
			*output = (char *)mkstr("%d", temp_service->is_volatile);
			break;
#ifdef NSCORE
		case MACRO_SERVICEATTEMPT:
			*output = (char *)mkstr("%d", temp_service->current_attempt);
			break;
		case MACRO_MAXSERVICEATTEMPTS:
			*output = (char *)mkstr("%d", temp_service->max_attempts);
			break;
		case MACRO_SERVICEEXECUTIONTIME:
			*output = (char *)mkstr("%.3f", temp_service->execution_time);
			break;
		case MACRO_SERVICELATENCY:
			*output = (char *)mkstr("%.3f", temp_service->latency);
			break;
		case MACRO_LASTSERVICECHECK:
			*output = (char *)mkstr("%lu", (unsigned long)temp_service->last_check);
			break;
		case MACRO_LASTSERVICESTATECHANGE:
			*output = (char *)mkstr("%lu", (unsigned long)temp_service->last_state_change);
			break;
		case MACRO_LASTSERVICEOK:
			*output = (char *)mkstr("%lu", (unsigned long)temp_service->last_time_ok);
			break;
		case MACRO_LASTSERVICEWARNING:
			*output = (char *)mkstr("%lu", (unsigned long)temp_service->last_time_warning);
			break;
		case MACRO_LASTSERVICEUNKNOWN:
			*output = (char *)mkstr("%lu", (unsigned long)temp_service->last_time_unknown);
			break;
		case MACRO_LASTSERVICECRITICAL:
			*output = (char *)mkstr("%lu", (unsigned long)temp_service->last_time_critical);
			break;
		case MACRO_SERVICEDOWNTIME:
			*output = (char *)mkstr("%d", temp_service->scheduled_downtime_depth);
			break;
		case MACRO_SERVICEPERCENTCHANGE:
			*output = (char *)mkstr("%.2f", temp_service->percent_state_change);
			break;
		case MACRO_SERVICEDURATIONSEC:
		case MACRO_SERVICEDURATION:

			time(&current_time);
			duration = (unsigned long)(current_time - temp_service->last_state_change);

			/* get the state duration in seconds */
			if(macro_type == MACRO_SERVICEDURATIONSEC)
				*output = (char *)mkstr("%lu", duration);

			/* get the state duration */
			else {
				days = duration / 86400;
				duration -= (days * 86400);
				hours = duration / 3600;
				duration -= (hours * 3600);
				minutes = duration / 60;
				duration -= (minutes * 60);
				seconds = duration;
				*output = (char *)mkstr("%dd %dh %dm %ds", days, hours, minutes, seconds);
				}
			break;
		case MACRO_SERVICENOTIFICATIONNUMBER:
			*output = (char *)mkstr("%d", temp_service->current_notification_number);
			break;
		case MACRO_SERVICENOTIFICATIONID:
			*output = (char *)mkstr("%lu", temp_service->current_notification_id);
			break;
		case MACRO_SERVICEEVENTID:
			*output = (char *)mkstr("%lu", temp_service->current_event_id);
			break;
		case MACRO_LASTSERVICEEVENTID:
			*output = (char *)mkstr("%lu", temp_service->last_event_id);
			break;
		case MACRO_SERVICEPROBLEMID:
			*output = (char *)mkstr("%lu", temp_service->current_problem_id);
			break;
		case MACRO_LASTSERVICEPROBLEMID:
			*output = (char *)mkstr("%lu", temp_service->last_problem_id);
			break;
#endif
		case MACRO_SERVICEACTIONURL:
			if(temp_service->action_url)
				*output = temp_service->action_url;
			break;
		case MACRO_SERVICENOTESURL:
			if(temp_service->notes_url)
				*output = temp_service->notes_url;
			break;
		case MACRO_SERVICENOTES:
			if(temp_service->notes)
				*output = temp_service->notes;
			break;

#ifdef NSCORE
		case MACRO_SERVICEGROUPNAMES:
			/* find all servicegroups this service is associated with */
			for(temp_objectlist = temp_service->servicegroups_ptr; temp_objectlist != NULL; temp_objectlist = temp_objectlist->next) {

				if((temp_servicegroup = (servicegroup *)temp_objectlist->object_ptr) == NULL)
					continue;

				asprintf(&buf1, "%s%s%s", (buf2) ? buf2 : "", (buf2) ? "," : "", temp_servicegroup->group_name);
				my_free(buf2);
				buf2 = buf1;
				}
			if(buf2) {
				*output = (char *)strdup(buf2);
				my_free(buf2);
				}
			break;
		case MACRO_SERVICEIMPORTANCE:
			*output = (char *)mkstr("%u", temp_service->hourly_value);
			break;
#endif

			/***************/
			/* MISC MACROS */
			/***************/
		case MACRO_SERVICEACKAUTHOR:
		case MACRO_SERVICEACKAUTHORNAME:
		case MACRO_SERVICEACKAUTHORALIAS:
		case MACRO_SERVICEACKCOMMENT:
			/* no need to do any more work - these are already precomputed elsewhere */
			/* NOTE: these macros won't work as on-demand macros */
			*output = mac->x[macro_type];
			*free_macro = FALSE;
			break;

		default:
			log_debug_info(DEBUGL_MACROS, 0, "UNHANDLED SERVICE MACRO #%d! THIS IS A BUG!\n", macro_type);
			return ERROR;
			break;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type) {
		case MACRO_SERVICEACTIONURL:
		case MACRO_SERVICENOTESURL:
			process_macros_r(mac, *output, &temp_buffer, URL_ENCODE_MACRO_CHARS);
			*output = temp_buffer;
			break;
		case MACRO_SERVICENOTES:
			process_macros_r(mac, *output, &temp_buffer, 0);
			*output = temp_buffer;
			break;
		default:
			break;
		}

	return OK;
	}

int grab_standard_service_macro(int macro_type, service *temp_service, char **output, int *free_macro) {
	return grab_standard_service_macro_r(&global_macros, macro_type, temp_service, output, free_macro);
	}


/* computes a servicegroup macro */
int grab_standard_servicegroup_macro_r(nagios_macros *mac, int macro_type, servicegroup *temp_servicegroup, char **output) {
	servicesmember *temp_servicesmember = NULL;
	char *temp_buffer = NULL;
	unsigned int	temp_len = 0;
	unsigned int	init_len = 0;

	if(temp_servicegroup == NULL || output == NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type) {
		case MACRO_SERVICEGROUPNAME:
			*output = temp_servicegroup->group_name;
			break;
		case MACRO_SERVICEGROUPALIAS:
			if(temp_servicegroup->alias)
				*output = temp_servicegroup->alias;
			break;
		case MACRO_SERVICEGROUPMEMBERS:
			/* make the calculations for total string length */
			for(temp_servicesmember = temp_servicegroup->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
				if(temp_servicesmember->host_name == NULL || temp_servicesmember->service_description == NULL)
					continue;
				if(temp_len == 0) {
					temp_len += strlen(temp_servicesmember->host_name) + strlen(temp_servicesmember->service_description) + 2;
					}
				else {
					temp_len += strlen(temp_servicesmember->host_name) + strlen(temp_servicesmember->service_description) + 3;
					}
				}
			if(!temp_len) {
				/* empty group, so return the nul string */
				*output = calloc(1, 1);
				return OK;
				}
			/* allocate or reallocate the memory buffer */
			if(*output == NULL) {
				*output = (char *)malloc(temp_len);
				}
			else {
				init_len = strlen(*output);
				temp_len += init_len;
				*output = (char *)realloc(*output, temp_len);
				}
			/* now fill in the string with the group members */
			for(temp_servicesmember = temp_servicegroup->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
				if(temp_servicesmember->host_name == NULL || temp_servicesmember->service_description == NULL)
					continue;
				temp_buffer = *output + init_len;
				if(init_len == 0) {  /* If our buffer didn't contain anything, we just need to write "%s,%s" */
					init_len += sprintf(temp_buffer, "%s,%s", temp_servicesmember->host_name, temp_servicesmember->service_description);
					}
				else {   /* Now we need to write ",%s,%s" */
					init_len += sprintf(temp_buffer, ",%s,%s", temp_servicesmember->host_name, temp_servicesmember->service_description);
					}
				}
			break;
		case MACRO_SERVICEGROUPACTIONURL:
			if(temp_servicegroup->action_url)
				*output = temp_servicegroup->action_url;
			break;
		case MACRO_SERVICEGROUPNOTESURL:
			if(temp_servicegroup->notes_url)
				*output = temp_servicegroup->notes_url;
			break;
		case MACRO_SERVICEGROUPNOTES:
			if(temp_servicegroup->notes)
				*output = temp_servicegroup->notes;
			break;
		default:
			log_debug_info(DEBUGL_MACROS, 0, "UNHANDLED SERVICEGROUP MACRO #%d! THIS IS A BUG!\n", macro_type);
			return ERROR;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type) {
		case MACRO_SERVICEGROUPACTIONURL:
		case MACRO_SERVICEGROUPNOTESURL:
			process_macros_r(mac, *output, &temp_buffer, URL_ENCODE_MACRO_CHARS);
			*output = temp_buffer;
			break;
		case MACRO_SERVICEGROUPNOTES:
			process_macros_r(mac, *output, &temp_buffer, 0);
			*output = temp_buffer;
			break;
		default:
			break;
		}

	return OK;
	}

int grab_standard_servicegroup_macro(int macro_type, servicegroup *temp_servicegroup, char **output) {
	return grab_standard_servicegroup_macro_r(&global_macros, macro_type, temp_servicegroup, output);
	}


/* computes a contact macro */
int grab_standard_contact_macro_r(nagios_macros *mac, int macro_type, contact *temp_contact, char **output) {
#ifdef NSCORE
	contactgroup *temp_contactgroup = NULL;
	objectlist *temp_objectlist = NULL;
	char *buf1 = NULL;
	char *buf2 = NULL;
#endif

	if(temp_contact == NULL || output == NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type) {
		case MACRO_CONTACTNAME:
			*output = temp_contact->name;
			break;
		case MACRO_CONTACTALIAS:
			*output = temp_contact->alias;
			break;
		case MACRO_CONTACTEMAIL:
			if(temp_contact->email)
				*output = temp_contact->email;
			break;
		case MACRO_CONTACTPAGER:
			if(temp_contact->pager)
				*output = temp_contact->pager;
			break;
#ifdef NSCORE
		case MACRO_CONTACTGROUPNAMES:
			/* get the contactgroup names */
			/* find all contactgroups this contact is a member of */
			for(temp_objectlist = temp_contact->contactgroups_ptr; temp_objectlist != NULL; temp_objectlist = temp_objectlist->next) {

				if((temp_contactgroup = (contactgroup *)temp_objectlist->object_ptr) == NULL)
					continue;

				asprintf(&buf1, "%s%s%s", (buf2) ? buf2 : "", (buf2) ? "," : "", temp_contactgroup->group_name);
				my_free(buf2);
				buf2 = buf1;
				}
			if(buf2) {
				*output = (char *)strdup(buf2);
				my_free(buf2);
				}
			break;
#endif
		default:
			log_debug_info(DEBUGL_MACROS, 0, "UNHANDLED CONTACT MACRO #%d! THIS IS A BUG!\n", macro_type);
			return ERROR;
		}

	return OK;
	}

int grab_standard_contact_macro(int macro_type, contact *temp_contact, char **output) {
	return grab_standard_contact_macro_r(&global_macros, macro_type, temp_contact, output);
	}


/* computes a contact address macro */
int grab_contact_address_macro(int macro_num, contact *temp_contact, char **output) {
	if(macro_num < 0 || macro_num >= MAX_CONTACT_ADDRESSES)
		return ERROR;

	if(temp_contact == NULL || output == NULL)
		return ERROR;

	/* get the macro */
	if(temp_contact->address[macro_num])
		*output = temp_contact->address[macro_num];

	return OK;
	}



/* computes a contactgroup macro */
int grab_standard_contactgroup_macro(int macro_type, contactgroup *temp_contactgroup, char **output) {
	contactsmember *temp_contactsmember = NULL;

	if(temp_contactgroup == NULL || output == NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type) {
		case MACRO_CONTACTGROUPNAME:
			*output = temp_contactgroup->group_name;
			break;
		case MACRO_CONTACTGROUPALIAS:
			if(temp_contactgroup->alias)
				*output = temp_contactgroup->alias;
			break;
		case MACRO_CONTACTGROUPMEMBERS:
			/* get the member list */
			for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
				if(temp_contactsmember->contact_name == NULL)
					continue;
				if(*output == NULL)
					*output = (char *)strdup(temp_contactsmember->contact_name);
				else if((*output = (char *)realloc(*output, strlen(*output) + strlen(temp_contactsmember->contact_name) + 2))) {
					strcat(*output, ",");
					strcat(*output, temp_contactsmember->contact_name);
					}
				}
			break;
		default:
			log_debug_info(DEBUGL_MACROS, 0, "UNHANDLED CONTACTGROUP MACRO #%d! THIS IS A BUG!\n", macro_type);
			return ERROR;
		}

	return OK;
	}


/* computes a custom object macro */
int grab_custom_object_macro_r(nagios_macros *mac, char *macro_name, customvariablesmember *vars, char **output) {
	customvariablesmember *temp_customvariablesmember = NULL;
	int result = ERROR;

	if(macro_name == NULL || vars == NULL || output == NULL)
		return ERROR;

	/* get the custom variable */
	for(temp_customvariablesmember = vars; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {

		if(temp_customvariablesmember->variable_name == NULL)
			continue;

		if(!strcmp(macro_name, temp_customvariablesmember->variable_name)) {
			if(temp_customvariablesmember->variable_value)
				*output = temp_customvariablesmember->variable_value;
			result = OK;
			break;
			}
		}

	return result;
	}

int grab_custom_object_macro(char *macro_name, customvariablesmember *vars, char **output) {
	return grab_custom_object_macro_r(&global_macros, macro_name, vars, output);
	}


/******************************************************************/
/********************* MACRO STRING FUNCTIONS *********************/
/******************************************************************/

/* cleans illegal characters in macros before output */
char *clean_macro_chars(char *macro, int options) {
	register int x = 0;
	register int y = 0;
	register int ch = 0;
	register int len = 0;
	char *ret = NULL;

	if(macro == NULL || !*macro)
		return "";

	len = (int)strlen(macro);
	ret = strdup(macro);

	/* strip illegal characters out of macro */
	if(options & STRIP_ILLEGAL_MACRO_CHARS) {
		for(y = 0, x = 0; x < len; x++) {
			ch = macro[x] & 0xff;

			/* illegal chars are skipped */
			if(!illegal_output_char_map[ch])
				ret[y++] = ret[x];
			}

		ret[y++] = '\x0';
		}

	return ret;
	}



/* encodes a string in proper URL format */
char *get_url_encoded_string(char *input) {
	/* From RFC 3986:
	segment       = *pchar

	[...]

	pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"

	query         = *( pchar / "/" / "?" )

	fragment      = *( pchar / "/" / "?" )

	pct-encoded   = "%" HEXDIG HEXDIG

	unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
	reserved      = gen-delims / sub-delims
	gen-delims    = ":" / "/" / "?" / "#" / "[" / "]" / "@"
	sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
	                 / "*" / "+" / "," / ";" / "="

	Encode everything but "unreserved", to be on safe side.

	Another note:
	nowhere in the RFC states that + is interpreted as space. Therefore, encode
	space as %20 (as all other characters that should be escaped)
	*/

	register int x = 0;
	register int y = 0;
	char *encoded_url_string = NULL;


	/* bail if no input */
	if(input == NULL)
		return NULL;

	/* allocate enough memory to escape all characters if necessary */
	if((encoded_url_string = (char *)malloc((strlen(input) * 3) + 1)) == NULL)
		return NULL;

	/* check/encode all characters */
	for(x = 0, y = 0; input[x]; x++) {

		/* alpha-numeric characters and a few other characters don't get encoded */
		if(((char)input[x] >= '0' && (char)input[x] <= '9') ||
		   ((char)input[x] >= 'A' && (char)input[x] <= 'Z') ||
		   ((char)input[x] >= 'a' && (char)input[x] <= 'z') ||
		   (char)input[x] == '.' ||
		   (char)input[x] == '-' ||
		   (char)input[x] == '_' ||
		   (char)input[x] == '~')
		{
			encoded_url_string[y++] = input[x];
		}

		/* anything else gets represented by its hex value */
		else {
			sprintf(&encoded_url_string[y], "%%%02X", (unsigned int)(input[x] & 0xFF));
			y += 3;
			}
		}

	/* terminate encoded string */
	encoded_url_string[y] = '\x0';

	return encoded_url_string;
	}


static int macro_key_cmp(const void *a_, const void *b_)
{
	struct macro_key_code *a = (struct macro_key_code *)a_;
	struct macro_key_code *b = (struct macro_key_code *)b_;

	return strcmp(a->name, b->name);
}


/******************************************************************/
/***************** MACRO INITIALIZATION FUNCTIONS *****************/
/******************************************************************/

/* initializes global macros */
int init_macros(void) {
	int x;
	init_macrox_names();

	for(x = 0; x < 32; x++)
		illegal_output_char_map[x] = 1;
	illegal_output_char_map[127] = 1;

	/*
	 * non-volatile macros are free()'d when they're set.
	 * We must do this in order to not lose the constant
	 * ones when we get SIGHUP or a RESTART_PROGRAM event
	 * from the command fifo. Otherwise a memset() would
	 * have been better.
	 */
	clear_volatile_macros_r(&global_macros);

	/* backwards compatibility hack */
	macro_x = global_macros.x;

	/*
	 * Now build an ordered list of X macro names so we can
	 * do binary lookups later and avoid a ton of strcmp()'s
	 * for each and every check that gets run. A hash table
	 * is actually slower, since the most frequently used
	 * keys are so long and a binary lookup is completed in
	 * 7 steps for up to ~200 keys, worst case.
	 */
	for (x = 0; x < MACRO_X_COUNT; x++) {
		macro_keys[x].code = x;
		macro_keys[x].name = macro_x_names[x];

		/* This tells which escaping is possible to do on the macro */
		macro_keys[x].options = URL_ENCODE_MACRO_CHARS;
		switch(x) {
		case MACRO_HOSTOUTPUT:
		case MACRO_LONGHOSTOUTPUT:
		case MACRO_HOSTPERFDATA:
		case MACRO_HOSTACKAUTHOR:
		case MACRO_HOSTACKCOMMENT:
		case MACRO_SERVICEOUTPUT:
		case MACRO_LONGSERVICEOUTPUT:
		case MACRO_SERVICEPERFDATA:
		case MACRO_SERVICEACKAUTHOR:
		case MACRO_SERVICEACKCOMMENT:
			macro_keys[x].options |= STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
			break;
		}
	}

	qsort(macro_keys, x, sizeof(struct macro_key_code), macro_key_cmp);
	return OK;
	}

/*
 * initializes the names of macros, using this nifty little macro
 * which ensures we never add any typos to the list
 */
#define add_macrox_name(name) macro_x_names[MACRO_##name] = strdup(#name)
int init_macrox_names(void) {
	register int x = 0;

	/* initialize macro names */
	for(x = 0; x < MACRO_X_COUNT; x++)
		macro_x_names[x] = NULL;

	/* initialize each macro name */
	add_macrox_name(HOSTNAME);
	add_macrox_name(HOSTALIAS);
	add_macrox_name(HOSTADDRESS);
	add_macrox_name(SERVICEDESC);
	add_macrox_name(SERVICESTATE);
	add_macrox_name(SERVICESTATEID);
	add_macrox_name(SERVICEATTEMPT);
	add_macrox_name(SERVICEISVOLATILE);
	add_macrox_name(LONGDATETIME);
	add_macrox_name(SHORTDATETIME);
	add_macrox_name(DATE);
	add_macrox_name(TIME);
	add_macrox_name(TIMET);
	add_macrox_name(LASTHOSTCHECK);
	add_macrox_name(LASTSERVICECHECK);
	add_macrox_name(LASTHOSTSTATECHANGE);
	add_macrox_name(LASTSERVICESTATECHANGE);
	add_macrox_name(HOSTOUTPUT);
	add_macrox_name(SERVICEOUTPUT);
	add_macrox_name(HOSTPERFDATA);
	add_macrox_name(SERVICEPERFDATA);
	add_macrox_name(CONTACTNAME);
	add_macrox_name(CONTACTALIAS);
	add_macrox_name(CONTACTEMAIL);
	add_macrox_name(CONTACTPAGER);
	add_macrox_name(ADMINEMAIL);
	add_macrox_name(ADMINPAGER);
	add_macrox_name(HOSTSTATE);
	add_macrox_name(HOSTSTATEID);
	add_macrox_name(HOSTATTEMPT);
	add_macrox_name(NOTIFICATIONTYPE);
	add_macrox_name(NOTIFICATIONNUMBER);
	add_macrox_name(NOTIFICATIONISESCALATED);
	add_macrox_name(HOSTEXECUTIONTIME);
	add_macrox_name(SERVICEEXECUTIONTIME);
	add_macrox_name(HOSTLATENCY);
	add_macrox_name(SERVICELATENCY);
	add_macrox_name(HOSTDURATION);
	add_macrox_name(SERVICEDURATION);
	add_macrox_name(HOSTDURATIONSEC);
	add_macrox_name(SERVICEDURATIONSEC);
	add_macrox_name(HOSTDOWNTIME);
	add_macrox_name(SERVICEDOWNTIME);
	add_macrox_name(HOSTSTATETYPE);
	add_macrox_name(SERVICESTATETYPE);
	add_macrox_name(HOSTPERCENTCHANGE);
	add_macrox_name(SERVICEPERCENTCHANGE);
	add_macrox_name(HOSTGROUPNAME);
	add_macrox_name(HOSTGROUPALIAS);
	add_macrox_name(SERVICEGROUPNAME);
	add_macrox_name(SERVICEGROUPALIAS);
	add_macrox_name(HOSTACKAUTHOR);
	add_macrox_name(HOSTACKCOMMENT);
	add_macrox_name(SERVICEACKAUTHOR);
	add_macrox_name(SERVICEACKCOMMENT);
	add_macrox_name(LASTSERVICEOK);
	add_macrox_name(LASTSERVICEWARNING);
	add_macrox_name(LASTSERVICEUNKNOWN);
	add_macrox_name(LASTSERVICECRITICAL);
	add_macrox_name(LASTHOSTUP);
	add_macrox_name(LASTHOSTDOWN);
	add_macrox_name(LASTHOSTUNREACHABLE);
	add_macrox_name(SERVICECHECKCOMMAND);
	add_macrox_name(HOSTCHECKCOMMAND);
	add_macrox_name(MAINCONFIGFILE);
	add_macrox_name(STATUSDATAFILE);
	add_macrox_name(HOSTDISPLAYNAME);
	add_macrox_name(SERVICEDISPLAYNAME);
	add_macrox_name(RETENTIONDATAFILE);
	add_macrox_name(OBJECTCACHEFILE);
	add_macrox_name(TEMPFILE);
	add_macrox_name(LOGFILE);
	add_macrox_name(RESOURCEFILE);
	add_macrox_name(COMMANDFILE);
	add_macrox_name(HOSTPERFDATAFILE);
	add_macrox_name(SERVICEPERFDATAFILE);
	add_macrox_name(HOSTACTIONURL);
	add_macrox_name(HOSTNOTESURL);
	add_macrox_name(HOSTNOTES);
	add_macrox_name(SERVICEACTIONURL);
	add_macrox_name(SERVICENOTESURL);
	add_macrox_name(SERVICENOTES);
	add_macrox_name(TOTALHOSTSUP);
	add_macrox_name(TOTALHOSTSDOWN);
	add_macrox_name(TOTALHOSTSUNREACHABLE);
	add_macrox_name(TOTALHOSTSDOWNUNHANDLED);
	add_macrox_name(TOTALHOSTSUNREACHABLEUNHANDLED);
	add_macrox_name(TOTALHOSTPROBLEMS);
	add_macrox_name(TOTALHOSTPROBLEMSUNHANDLED);
	add_macrox_name(TOTALSERVICESOK);
	add_macrox_name(TOTALSERVICESWARNING);
	add_macrox_name(TOTALSERVICESCRITICAL);
	add_macrox_name(TOTALSERVICESUNKNOWN);
	add_macrox_name(TOTALSERVICESWARNINGUNHANDLED);
	add_macrox_name(TOTALSERVICESCRITICALUNHANDLED);
	add_macrox_name(TOTALSERVICESUNKNOWNUNHANDLED);
	add_macrox_name(TOTALSERVICEPROBLEMS);
	add_macrox_name(TOTALSERVICEPROBLEMSUNHANDLED);
	add_macrox_name(PROCESSSTARTTIME);
	add_macrox_name(HOSTCHECKTYPE);
	add_macrox_name(SERVICECHECKTYPE);
	add_macrox_name(LONGHOSTOUTPUT);
	add_macrox_name(LONGSERVICEOUTPUT);
	add_macrox_name(TEMPPATH);
	add_macrox_name(HOSTNOTIFICATIONNUMBER);
	add_macrox_name(SERVICENOTIFICATIONNUMBER);
	add_macrox_name(HOSTNOTIFICATIONID);
	add_macrox_name(SERVICENOTIFICATIONID);
	add_macrox_name(HOSTEVENTID);
	add_macrox_name(LASTHOSTEVENTID);
	add_macrox_name(SERVICEEVENTID);
	add_macrox_name(LASTSERVICEEVENTID);
	add_macrox_name(HOSTGROUPNAMES);
	add_macrox_name(SERVICEGROUPNAMES);
	add_macrox_name(HOSTACKAUTHORNAME);
	add_macrox_name(HOSTACKAUTHORALIAS);
	add_macrox_name(SERVICEACKAUTHORNAME);
	add_macrox_name(SERVICEACKAUTHORALIAS);
	add_macrox_name(MAXHOSTATTEMPTS);
	add_macrox_name(MAXSERVICEATTEMPTS);
	add_macrox_name(TOTALHOSTSERVICES);
	add_macrox_name(TOTALHOSTSERVICESOK);
	add_macrox_name(TOTALHOSTSERVICESWARNING);
	add_macrox_name(TOTALHOSTSERVICESUNKNOWN);
	add_macrox_name(TOTALHOSTSERVICESCRITICAL);
	add_macrox_name(HOSTGROUPNOTES);
	add_macrox_name(HOSTGROUPNOTESURL);
	add_macrox_name(HOSTGROUPACTIONURL);
	add_macrox_name(SERVICEGROUPNOTES);
	add_macrox_name(SERVICEGROUPNOTESURL);
	add_macrox_name(SERVICEGROUPACTIONURL);
	add_macrox_name(HOSTGROUPMEMBERS);
	add_macrox_name(SERVICEGROUPMEMBERS);
	add_macrox_name(CONTACTGROUPNAME);
	add_macrox_name(CONTACTGROUPALIAS);
	add_macrox_name(CONTACTGROUPMEMBERS);
	add_macrox_name(CONTACTGROUPNAMES);
	add_macrox_name(NOTIFICATIONRECIPIENTS);
	add_macrox_name(NOTIFICATIONAUTHOR);
	add_macrox_name(NOTIFICATIONAUTHORNAME);
	add_macrox_name(NOTIFICATIONAUTHORALIAS);
	add_macrox_name(NOTIFICATIONCOMMENT);
	add_macrox_name(EVENTSTARTTIME);
	add_macrox_name(HOSTPROBLEMID);
	add_macrox_name(LASTHOSTPROBLEMID);
	add_macrox_name(SERVICEPROBLEMID);
	add_macrox_name(LASTSERVICEPROBLEMID);
	add_macrox_name(ISVALIDTIME);
	add_macrox_name(NEXTVALIDTIME);
	add_macrox_name(LASTHOSTSTATE);
	add_macrox_name(LASTHOSTSTATEID);
	add_macrox_name(LASTSERVICESTATE);
	add_macrox_name(LASTSERVICESTATEID);
	add_macrox_name(HOSTIMPORTANCE);
	add_macrox_name(SERVICEIMPORTANCE);
	add_macrox_name(HOSTANDSERVICESIMPORTANCE);

	return OK;
	}


/******************************************************************/
/********************* MACRO CLEANUP FUNCTIONS ********************/
/******************************************************************/

/* free memory associated with the macrox names */
int free_macrox_names(void) {
	register int x = 0;

	/* free each macro name */
	for(x = 0; x < MACRO_X_COUNT; x++)
		my_free(macro_x_names[x]);

	return OK;
	}



/* clear argv macros - used in commands */
int clear_argv_macros_r(nagios_macros *mac) {
	register int x = 0;

	/* command argument macros */
	for(x = 0; x < MAX_COMMAND_ARGUMENTS; x++)
		my_free(mac->argv[x]);

	return OK;
	}

int clear_argv_macros(void) {
	return clear_argv_macros_r(&global_macros);
	}

/*
 * copies non-volatile macros from global macro_x to **dest, which
 * must be large enough to hold at least MACRO_X_COUNT entries.
 * We use a shortlived macro to save up on typing
 */
#define cp_macro(name) dest[MACRO_##name] = global_macros.x[MACRO_##name]
void copy_constant_macros(char **dest) {
	cp_macro(ADMINEMAIL);
	cp_macro(ADMINPAGER);
	cp_macro(MAINCONFIGFILE);
	cp_macro(STATUSDATAFILE);
	cp_macro(RETENTIONDATAFILE);
	cp_macro(OBJECTCACHEFILE);
	cp_macro(TEMPFILE);
	cp_macro(LOGFILE);
	cp_macro(RESOURCEFILE);
	cp_macro(COMMANDFILE);
	cp_macro(HOSTPERFDATAFILE);
	cp_macro(SERVICEPERFDATAFILE);
	cp_macro(PROCESSSTARTTIME);
	cp_macro(TEMPPATH);
	cp_macro(EVENTSTARTTIME);
	}
#undef cp_macro

static void clear_custom_vars(customvariablesmember **vars) {
	customvariablesmember *this_customvariablesmember = NULL;
	customvariablesmember *next_customvariablesmember = NULL;

	for(this_customvariablesmember = *vars;
			this_customvariablesmember != NULL;
			this_customvariablesmember = next_customvariablesmember) {
		next_customvariablesmember = this_customvariablesmember->next;
		my_free(this_customvariablesmember->variable_name);
		my_free(this_customvariablesmember->variable_value);
		my_free(this_customvariablesmember);
		}
	*vars = NULL;
	}

/* clear all macros that are not "constant" (i.e. they change throughout the course of monitoring) */
int clear_volatile_macros_r(nagios_macros *mac) {
	register int x = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "clear_volatile_macros_r()\n");

	clear_host_macros_r(mac);
	clear_service_macros_r(mac);
	clear_summary_macros_r(mac);
	clear_hostgroup_macros_r(mac);
	clear_servicegroup_macros_r(mac);
	clear_contactgroup_macros_r(mac);
	clear_contact_macros_r(mac);
	clear_datetime_macros_r(mac);

	/* contact address macros */
	for(x = 0; x < MAX_CONTACT_ADDRESSES; x++)
		my_free(mac->contactaddress[x]);

	/* clear on-demand macro */
	my_free(mac->ondemand);

	/* clear ARGx macros */
	clear_argv_macros_r(mac);

	return OK;
	}


int clear_volatile_macros(void) {
	return clear_volatile_macros_r(&global_macros);
	}


/* clear datetime macros */
int clear_datetime_macros_r(nagios_macros *mac) {
	my_free(mac->x[MACRO_LONGDATETIME]);
	my_free(mac->x[MACRO_SHORTDATETIME]);
	my_free(mac->x[MACRO_DATE]);
	my_free(mac->x[MACRO_TIME]);

	return OK;
	}

/* clear service macros */
int clear_service_macros_r(nagios_macros *mac) {

	/* these are recursive but persistent. what to do? */
	my_free(mac->x[MACRO_SERVICEACTIONURL]);
	my_free(mac->x[MACRO_SERVICENOTESURL]);
	my_free(mac->x[MACRO_SERVICENOTES]);

	my_free(mac->x[MACRO_SERVICEGROUPNAMES]);

	/* clear custom service variables */
	clear_custom_vars(&(mac->custom_service_vars));

	/* clear pointers */
	mac->service_ptr = NULL;

	return OK;
	}

int clear_service_macros(void) {
	return clear_service_macros_r(&global_macros);
	}

/* clear host macros */
int clear_host_macros_r(nagios_macros *mac) {

	/* these are recursive but persistent. what to do? */
	my_free(mac->x[MACRO_HOSTACTIONURL]);
	my_free(mac->x[MACRO_HOSTNOTESURL]);
	my_free(mac->x[MACRO_HOSTNOTES]);

	/* numbers or by necessity autogenerated strings */
	my_free(mac->x[MACRO_HOSTGROUPNAMES]);

	/* clear custom host variables */
	clear_custom_vars(&(mac->custom_host_vars));

	/* clear pointers */
	mac->host_ptr = NULL;

	return OK;
	}

int clear_host_macros(void) {
	return clear_host_macros_r(&global_macros);
	}


/* clear hostgroup macros */
int clear_hostgroup_macros_r(nagios_macros *mac) {

	/* recursive but persistent. what to do? */
	my_free(mac->x[MACRO_HOSTGROUPACTIONURL]);
	my_free(mac->x[MACRO_HOSTGROUPNOTESURL]);
	my_free(mac->x[MACRO_HOSTGROUPNOTES]);

	/* generated */
	my_free(mac->x[MACRO_HOSTGROUPMEMBERS]);

	/* clear pointers */
	mac->hostgroup_ptr = NULL;

	return OK;
	}

int clear_hostgroup_macros(void) {
	return clear_hostgroup_macros_r(&global_macros);
	}


/* clear servicegroup macros */
int clear_servicegroup_macros_r(nagios_macros *mac) {
	/* recursive but persistent. what to do? */
	my_free(mac->x[MACRO_SERVICEGROUPACTIONURL]);
	my_free(mac->x[MACRO_SERVICEGROUPNOTESURL]);
	my_free(mac->x[MACRO_SERVICEGROUPNOTES]);

	/* generated */
	my_free(mac->x[MACRO_SERVICEGROUPMEMBERS]);

	/* clear pointers */
	mac->servicegroup_ptr = NULL;

	return OK;
	}

int clear_servicegroup_macros(void) {
	return clear_servicegroup_macros_r(&global_macros);
	}


/* clear contact macros */
int clear_contact_macros_r(nagios_macros *mac) {

	/* generated */
	my_free(mac->x[MACRO_CONTACTGROUPNAMES]);

	/* clear custom contact variables */
	clear_custom_vars(&(mac->custom_contact_vars));

	/* clear pointers */
	mac->contact_ptr = NULL;

	return OK;
	}

int clear_contact_macros(void) {
	return clear_contact_macros_r(&global_macros);
	}


/* clear contactgroup macros */
int clear_contactgroup_macros_r(nagios_macros *mac) {
	/* generated */
	my_free(mac->x[MACRO_CONTACTGROUPMEMBERS]);

	/* clear pointers */
	mac->contactgroup_ptr = NULL;

	return OK;
	}

int clear_contactgroup_macros(void) {
	return clear_contactgroup_macros_r(&global_macros);
	}

/* clear summary macros */
int clear_summary_macros_r(nagios_macros *mac) {
	register int x;

	for(x = MACRO_TOTALHOSTSUP; x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED; x++)
		my_free(mac->x[x]);

	return OK;
	}

int clear_summary_macros(void) {
	return clear_summary_macros_r(&global_macros);
	}


/******************************************************************/
/****************** ENVIRONMENT MACRO FUNCTIONS *******************/
/******************************************************************/

#ifdef NSCORE

/* sets or unsets all macro environment variables */
int set_all_macro_environment_vars_r(nagios_macros *mac, int set) {
	if(enable_environment_macros == FALSE)
		return ERROR;

	set_macrox_environment_vars_r(mac, set);
	set_argv_macro_environment_vars_r(mac, set);
	set_custom_macro_environment_vars_r(mac, set);
	set_contact_address_environment_vars_r(mac, set);

	return OK;
	}

int set_all_macro_environment_vars(int set) {
	return set_all_macro_environment_vars_r(&global_macros, set);
	}


/* sets or unsets macrox environment variables */
int set_macrox_environment_vars_r(nagios_macros *mac, int set) {
	register int x = 0;
	int free_macro = FALSE;

	/* set each of the macrox environment variables */
	for(x = 0; x < MACRO_X_COUNT; x++) {

		free_macro = FALSE;

		/* large installations don't get all macros */
		if(use_large_installation_tweaks == TRUE) {
			/*
			 * member macros tend to overflow the
			 * environment on large installations
			 */
			if(x == MACRO_SERVICEGROUPMEMBERS || x == MACRO_HOSTGROUPMEMBERS)
				continue;

			/* summary macros are CPU intensive to compute */
			if(x >= MACRO_TOTALHOSTSUP && x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED)
				continue;
			}

		/* generate the macro value if it hasn't already been done */
		/* THIS IS EXPENSIVE */
		if(set == TRUE) {

			if(mac->x[x] == NULL)
				grab_macrox_value_r(mac, x, NULL, NULL, &mac->x[x], &free_macro);
			}

		/* set the value */
		set_macro_environment_var(macro_x_names[x], mac->x[x], set);
		}

	return OK;
	}

int set_macrox_environment_vars(int set) {
	return set_macrox_environment_vars_r(&global_macros, set);
	}


/* sets or unsets argv macro environment variables */
int set_argv_macro_environment_vars_r(nagios_macros *mac, int set) {
	char *macro_name = NULL;
	register int x = 0;

	/* set each of the argv macro environment variables */
	for(x = 0; x < MAX_COMMAND_ARGUMENTS; x++) {
		asprintf(&macro_name, "ARG%d", x + 1);
		set_macro_environment_var(macro_name, mac->argv[x], set);
		my_free(macro_name);
		}

	return OK;
	}

int set_argv_macro_environment_vars(int set) {
	return set_argv_macro_environment_vars_r(&global_macros, set);
	}


/* sets or unsets custom host/service/contact macro environment variables */
int set_custom_macro_environment_vars_r(nagios_macros *mac, int set) {
	customvariablesmember *temp_customvariablesmember = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	contact *temp_contact = NULL;
	char *customvarname = NULL;

	/***** CUSTOM HOST VARIABLES *****/
	/* generate variables and save them for later */
	if((temp_host = mac->host_ptr) && set == TRUE) {
		for(temp_customvariablesmember = temp_host->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
			asprintf(&customvarname, "_HOST%s", temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&mac->custom_host_vars, customvarname, temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember = mac->custom_host_vars; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
		set_macro_environment_var(temp_customvariablesmember->variable_name, clean_macro_chars(temp_customvariablesmember->variable_value, STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS), set);
		}

	/***** CUSTOM SERVICE VARIABLES *****/
	/* generate variables and save them for later */
	if((temp_service = mac->service_ptr) && set == TRUE) {
		for(temp_customvariablesmember = temp_service->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
			asprintf(&customvarname, "_SERVICE%s", temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&mac->custom_service_vars, customvarname, temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember = mac->custom_service_vars; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name, clean_macro_chars(temp_customvariablesmember->variable_value, STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS), set);

	/***** CUSTOM CONTACT VARIABLES *****/
	/* generate variables and save them for later */
	if((temp_contact = mac->contact_ptr) && set == TRUE) {
		for(temp_customvariablesmember = temp_contact->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
			asprintf(&customvarname, "_CONTACT%s", temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&mac->custom_contact_vars, customvarname, temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember = mac->custom_contact_vars; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name, clean_macro_chars(temp_customvariablesmember->variable_value, STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS), set);

	return OK;
	}

int set_custom_macro_environment_vars(int set) {
	return set_custom_macro_environment_vars_r(&global_macros, set);
	}


/* sets or unsets contact address environment variables */
int set_contact_address_environment_vars_r(nagios_macros *mac, int set) {
	char *varname = NULL;
	register int x;

	/* these only get set during notifications */
	if(mac->contact_ptr == NULL)
		return OK;

	for(x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
		asprintf(&varname, "CONTACTADDRESS%d", x);
		set_macro_environment_var(varname, mac->contact_ptr->address[x], set);
		my_free(varname);
		}

	return OK;
	}

int set_contact_address_environment_vars(int set) {
	return set_contact_address_environment_vars_r(&global_macros, set);
	}


/* sets or unsets a macro environment variable */
int set_macro_environment_var(char *name, char *value, int set) {
	char *env_macro_name = NULL;

	/* we won't mess with null variable names */
	if(name == NULL)
		return ERROR;

	/* create environment var name */
	asprintf(&env_macro_name, "%s%s", MACRO_ENV_VAR_PREFIX, name);

	/* set or unset the environment variable */
	set_environment_var(env_macro_name, value, set);

	/* free allocated memory */
	my_free(env_macro_name);

	return OK;
	}

static int add_macrox_environment_vars_r(nagios_macros *, struct kvvec *);
static int add_argv_macro_environment_vars_r(nagios_macros *, struct kvvec *);
static int add_custom_macro_environment_vars_r(nagios_macros *, struct kvvec *);
static int add_contact_address_environment_vars_r(nagios_macros *,
		struct kvvec *);

struct kvvec * macros_to_kvv(nagios_macros *mac) {

	struct kvvec *kvvp;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "macros_to_kvv()\n");

	/* If we're not supposed to export macros as environment variables,
		just return */
	if(FALSE == enable_environment_macros) return NULL;

	/* Create the kvvec to hold the macros */
	if((kvvp = calloc(1, sizeof(struct kvvec))) == NULL) return NULL;
	if(!kvvec_init(kvvp, MACRO_X_COUNT + MAX_COMMAND_ARGUMENTS + MAX_CONTACT_ADDRESSES + 4)) return NULL;

	add_macrox_environment_vars_r(mac, kvvp);
	add_argv_macro_environment_vars_r(mac, kvvp);
	add_custom_macro_environment_vars_r(mac, kvvp);
	add_contact_address_environment_vars_r(mac, kvvp);

	return kvvp;
	}

/* adds macrox environment variables */
static int add_macrox_environment_vars_r(nagios_macros *mac,
		struct kvvec *kvvp) {
	register int x = 0;
	int free_macro = FALSE;
	char *envname;

	log_debug_info(DEBUGL_FUNCTIONS, 1, "add_macrox_environment_vars_r()\n");

	/* set each of the macrox environment variables */
	for(x = 0; x < MACRO_X_COUNT; x++) {

		log_debug_info(DEBUGL_MACROS, 2, "Processing macro %d of %d\n", x,
				MACRO_X_COUNT);
		free_macro = FALSE;

		/* large installations don't get all macros */
		if(use_large_installation_tweaks == TRUE) {
			/*
			 * member macros tend to overflow the
			 * environment on large installations
			 */
			if(x == MACRO_SERVICEGROUPMEMBERS ||
					x == MACRO_HOSTGROUPMEMBERS) continue;

			/* summary macros are CPU intensive to compute */
			if(x >= MACRO_TOTALHOSTSUP &&
					x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED) continue;
			}

		/* generate the macro value if it hasn't already been done */
		/* THIS IS EXPENSIVE */
		if(mac->x[x] == NULL) {
			log_debug_info(DEBUGL_MACROS, 2, "Grabbing value for macro: %s\n",
					macro_x_names[x]);

			grab_macrox_value_r(mac, x, NULL, NULL, &mac->x[x], &free_macro);
			}

		/* add the value to the kvvec */
		log_debug_info(DEBUGL_MACROS, 2, "Adding macro \"%s\" with value \"%s\" to kvvec\n",
					macro_x_names[x], mac->x[x]);
		/* Allocate memory for each environment variable name, but not the 
			values because when kvvec_destroy() is called, it is called with
			KVVEC_FREE_KEYS */
		asprintf(&envname, "%s%s", MACRO_ENV_VAR_PREFIX, macro_x_names[x]);
		kvvec_addkv(kvvp, envname, mac->x[x]);
		}

	log_debug_info(DEBUGL_FUNCTIONS, 2, "add_macrox_environment_vars_r() end\n");
	return OK;
	}

/* adds argv macro environment variables */
static int add_argv_macro_environment_vars_r(nagios_macros *mac,
		struct kvvec *kvvp) {
	char *macro_name = NULL;
	register int x = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 1, "add_argv_macro_environment_vars_r()\n");

	/* set each of the argv macro environment variables */
	for(x = 0; x < MAX_COMMAND_ARGUMENTS; x++) {
		/* Allocate memory for each environment variable name, but not the 
			values because when kvvec_destroy() is called, it is called with
			KVVEC_FREE_KEYS */
		asprintf(&macro_name, "%sARG%d", MACRO_ENV_VAR_PREFIX, x + 1);
		kvvec_addkv(kvvp, macro_name, mac->argv[x]);
		}

	return OK;
	}

/* adds custom host/service/contact macro environment variables */
static int add_custom_macro_environment_vars_r(nagios_macros *mac,
		struct kvvec *kvvp) {

	customvariablesmember *temp_customvariablesmember = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	contact *temp_contact = NULL;
	char *customvarname = NULL;
	char *customvarvalue = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 1, "add_custom_macro_environment_vars_r()\n");

	/***** CUSTOM HOST VARIABLES *****/
	/* generate variables and save them for later */
	temp_host = mac->host_ptr;
	if(temp_host) {
		for(temp_customvariablesmember = temp_host->custom_variables;
				temp_customvariablesmember != NULL;
				temp_customvariablesmember = temp_customvariablesmember->next) {
			asprintf(&customvarname, "%s_HOST%s", MACRO_ENV_VAR_PREFIX,
					temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&mac->custom_host_vars, customvarname,
					temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember = mac->custom_host_vars;
			temp_customvariablesmember != NULL;
			temp_customvariablesmember = temp_customvariablesmember->next) {
		customvarvalue = 
				clean_macro_chars(temp_customvariablesmember->variable_value,
				STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
		if(customvarvalue && *customvarvalue) {
			my_free(temp_customvariablesmember->variable_value);
			temp_customvariablesmember->variable_value = customvarvalue;
			/* Allocate memory for each environment variable name, but not the 
				values because when kvvec_destroy() is called, it is called with
				KVVEC_FREE_KEYS */
			kvvec_addkv(kvvp, strdup(temp_customvariablesmember->variable_name),
					customvarvalue);
			}
		}

	/***** CUSTOM SERVICE VARIABLES *****/
	/* generate variables and save them for later */
	temp_service = mac->service_ptr;
	if(temp_service) {
		for(temp_customvariablesmember = temp_service->custom_variables;
				temp_customvariablesmember != NULL;
				temp_customvariablesmember = temp_customvariablesmember->next) {
			asprintf(&customvarname, "%s_SERVICE%s", MACRO_ENV_VAR_PREFIX,
					temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&mac->custom_service_vars,
					customvarname, temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember = mac->custom_service_vars;
			temp_customvariablesmember != NULL;
			temp_customvariablesmember = temp_customvariablesmember->next) {
		customvarvalue =
				clean_macro_chars(temp_customvariablesmember->variable_value,
				STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
		if(customvarvalue && *customvarvalue) {
			my_free(temp_customvariablesmember->variable_value);
			temp_customvariablesmember->variable_value = customvarvalue;
			/* Allocate memory for each environment variable name, but not the 
				values because when kvvec_destroy() is called, it is called with
				KVVEC_FREE_KEYS */
			kvvec_addkv(kvvp, strdup(temp_customvariablesmember->variable_name),
					customvarvalue);
			}
		}

	/***** CUSTOM CONTACT VARIABLES *****/
	/* generate variables and save them for later */
	temp_contact = mac->contact_ptr;
	if(temp_contact) {
		for(temp_customvariablesmember = temp_contact->custom_variables;
				temp_customvariablesmember != NULL;
				temp_customvariablesmember = temp_customvariablesmember->next) {
			asprintf(&customvarname, "%s_CONTACT%s", MACRO_ENV_VAR_PREFIX,
					temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&mac->custom_contact_vars,
					customvarname, temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember = mac->custom_contact_vars;
			temp_customvariablesmember != NULL;
			temp_customvariablesmember = temp_customvariablesmember->next) {
		customvarvalue = 
				clean_macro_chars(temp_customvariablesmember->variable_value,
				STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
		if(customvarvalue && *customvarvalue) {
			my_free(temp_customvariablesmember->variable_value);
			temp_customvariablesmember->variable_value = customvarvalue;
			/* Allocate memory for each environment variable name, but not the 
				values because when kvvec_destroy() is called, it is called with
				KVVEC_FREE_KEYS */
			kvvec_addkv(kvvp, strdup(temp_customvariablesmember->variable_name),
					customvarvalue);
			}
		}

	return OK;
	}

/* add contact address environment variables */
static int add_contact_address_environment_vars_r(nagios_macros *mac,
		struct kvvec *kvvp) {

	char *varname = NULL;
	register int x;

	log_debug_info(DEBUGL_FUNCTIONS, 1, "add_contact_address_environment_vars_r()\n");

	/* these only get set during notifications */
	if(mac->contact_ptr == NULL)
		return OK;

	asprintf(&varname, "%sCONTACTNAME", MACRO_ENV_VAR_PREFIX);
	kvvec_addkv(kvvp, varname, mac->contact_ptr->name);
    asprintf(&varname, "%sCONTACTALIAS", MACRO_ENV_VAR_PREFIX);
    kvvec_addkv(kvvp, varname, mac->contact_ptr->alias);
    asprintf(&varname, "%sCONTACTEMAIL", MACRO_ENV_VAR_PREFIX);
    kvvec_addkv(kvvp, varname, mac->contact_ptr->email);
    asprintf(&varname, "%sCONTACTPAGER", MACRO_ENV_VAR_PREFIX);
    kvvec_addkv(kvvp, varname, mac->contact_ptr->pager);

	for(x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
		/* Allocate memory for each environment variable name, but not the 
			values because when kvvec_destroy() is called, it is called with
			KVVEC_FREE_KEYS */
		asprintf(&varname, "%sCONTACTADDRESS%d", MACRO_ENV_VAR_PREFIX, x);
		kvvec_addkv(kvvp, varname, mac->contact_ptr->address[x]);
		}

	return OK;
	}

#endif
