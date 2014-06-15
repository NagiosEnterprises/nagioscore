/**************************************************************************
 *
 * JSONUTILS.H -  Utility information for Nagios CGI for that return 
 *                JSON-formatted data
 *
 * Copyright (c) 2013 Nagios Enterprises, LLC
 * Last Modified: 04-13-2013
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
 *************************************************************************/

#ifndef JSONUTILS_H_INCLUDED
#define JSONUTILS_H_INCLUDED

/* JSON object definitions */
#define JSON_TYPE_INVALID	0
#define JSON_TYPE_OBJECT	1
#define JSON_TYPE_ARRAY		2
#define JSON_TYPE_INTEGER	3
#define JSON_TYPE_REAL		4
#define JSON_TYPE_TIME_T	5
#define JSON_TYPE_STRING	6
#define JSON_TYPE_BOOLEAN	7
#define JSON_TYPE_DURATION	8

struct json_object_struct;

typedef struct json_object_member_struct {
	char 		*key;
	unsigned	type;
	union {
		long long			integer;
		unsigned long long	unsigned_integer;
		time_t				time;
		double				real;
		char *				string;
		unsigned			boolean;
		struct json_object_struct	*object;
		}		value;
	} json_object_member;

typedef struct json_object_struct {
	unsigned member_count;
	json_object_member **members;
	} json_object;

typedef json_object json_array;

/* Mapping from CGI query string option to value */
typedef struct string_value_mapping_struct {
	char 		*string;		/* String to map from */
	unsigned	value;			/* Value to map to */
	char 		*description;	/* Description of the meaning of the 
									string/value */
	} string_value_mapping;

typedef struct option_help_struct {
	const char *					name; 			/* name used in query */
	const char *					label;			/* short label for name */
	const char *					type;			/* option type */
	/* If the type is of the form nagios:<cgibase>/<objtype>, value values
		for this option will be obtained by sending the query
		'http://<whatever>/<cgibase>.cgi?objecttype=<objtype>' */
	const char *					required[25];	/* optiontypes required 
														for */
	const char *					optional[25];	/* optiontypes optional 
														for */
	const char *					depends_on;		/* valid values dependent on
														other option */
	const char *					description;	/* longer description */
	const string_value_mapping *	valid_values;	/* list of valid values */
	} option_help;

/* String escaping structures */
typedef struct json_escape_pair_struct {
	const wchar_t *from;
	const wchar_t *to;
}	json_escape_pair;

typedef struct json_escape_struct {
	const int				count;
	const json_escape_pair	*pairs;
}	json_escape;

/* Output Format Version */
#define OUTPUT_FORMAT_VERSION	0

/* Result Types */
#define RESULT_SUCCESS	0
#define RESULT_MEMORY_ALLOCATION_ERROR	1
#define RESULT_FILE_OPEN_READ_ERROR	2
#define RESULT_OPTION_INVALID	3
#define RESULT_OPTION_MISSING	4
#define RESULT_OPTION_VALUE_MISSING	5
#define RESULT_OPTION_VALUE_INVALID	6
#define RESULT_OPTION_IGNORED	7

/* Boolean Values - Used when selecting true, false, or both */
#define BOOLEAN_TRUE	1
#define BOOLEAN_FALSE	2
#define BOOLEAN_EITHER  (BOOLEAN_TRUE | BOOLEAN_FALSE)

/* Formatting Options */
#define JSON_FORMAT_WHITESPACE	(1<<0)
#define JSON_FORMAT_ENUMERATE	(1<<1)
#define JSON_FORMAT_BITMASK		(1<<2)
#define JSON_FORMAT_DURATION	(1<<3)
#if 0
#define JSON_FORMAT_DATETIME	(1<<3)
#define JSON_FORMAT_DATE		(1<<4)
#define JSON_FORMAT_TIME		(1<<5)
#endif

/* Possible Query Statuses */
#define QUERY_STATUS_ALPHA		0
#define QUERY_STATUS_BETA		1
#define QUERY_STATUS_RELEASED	2
#define QUERY_STATUS_DEPRECATED	3

extern const string_value_mapping svm_format_options[];
extern const string_value_mapping svm_host_statuses[];
extern const string_value_mapping svm_host_states[];
extern const string_value_mapping svm_service_statuses[];
extern const string_value_mapping svm_service_states[];
extern const string_value_mapping svm_check_options[];
extern const string_value_mapping svm_host_check_types[];
extern const string_value_mapping svm_service_check_types[];
extern const string_value_mapping svm_state_types[];
extern const string_value_mapping svm_acknowledgement_types[];
extern const string_value_mapping svm_comment_types[];
extern const string_value_mapping svm_comment_entry_types[];
extern const string_value_mapping svm_downtime_types[];
extern const string_value_mapping parent_host_extras[];
extern const string_value_mapping child_host_extras[];
extern const string_value_mapping parent_service_extras[];
extern const string_value_mapping child_service_extras[];
#ifdef JSON_NAGIOS_4X
extern const string_value_mapping svm_option_types[];
#endif

extern const char *dayofweek[];
extern const char *month[];

extern json_object *json_new_object(void);
extern void json_free_object(json_object *, int);
extern json_array *json_new_array(void);
extern void json_free_member(json_object_member *, int);
extern void json_object_append_object(json_object *, char *, json_object *);
extern void json_object_append_array(json_object *, char *, json_array *);
extern void json_object_append_integer(json_object *, char *, int);
extern void json_object_append_real(json_object *, char *, double);
extern void json_object_append_time(json_object *, char *, unsigned long);
extern void json_object_append_time_t(json_object *, char *, time_t);
extern void json_set_time_t(json_object_member *, time_t);
extern void json_object_append_string(json_object *, char *,
		const json_escape *, char *, ...);
extern void json_object_append_boolean(json_object *, char *, int);
extern void json_array_append_object(json_array *, json_object *);
extern void json_array_append_array(json_array *, json_array *);
extern void json_array_append_integer(json_array *, int);
extern void json_array_append_real(json_array *, double);
extern void json_array_append_time(json_array *, unsigned long);
extern void json_array_append_time_t(json_array *, time_t);
extern void json_array_append_string(json_array *, const json_escape *,
		char *, ...);
extern void json_array_append_boolean(json_array *, int);
extern void json_object_append_duration(json_object *, char *, unsigned long);
extern void json_array_append_duration(json_object *, unsigned long);
extern json_object_member *json_get_object_member(json_object *, char *);
extern json_object_member *json_get_array_member(json_object *, char *);
extern void json_object_print(json_object *, int, int, char *, unsigned);
extern void json_array_print(json_array *, int, int, char *, unsigned);
extern void json_member_print(json_object_member *, int, int, char *, unsigned);

extern json_object *json_result(time_t, char *, char *, int, time_t, authdata *,
		int, char *, ...);
extern json_object *json_help(option_help *);
extern int passes_start_and_count_limits(int, int, int, int);
extern void indentf(int, int, char *, ...);
extern void json_string(int, int, char *, char *);
extern void json_boolean(int, int, char *, int);
extern void json_int(int, int, char *, int);
extern void json_unsigned(int, int, char *, unsigned long long);
extern void json_float(int, int, char *, double);
extern void json_time(int, int, char *, unsigned long);
extern void json_time_t(int, int, char *, time_t, char *);
extern void json_duration(int, int, char *, unsigned long, int);

extern void json_enumeration(json_object *, unsigned, char *, int, 
		const string_value_mapping *);
extern void json_bitmask(json_object *, unsigned, char *, int, 
		const string_value_mapping *);

extern int parse_bitmask_cgivar(char *, char *, int, json_object *, time_t,
		authdata *, char *, char *, const string_value_mapping *, unsigned *);
extern int parse_enumeration_cgivar(char *, char *, int, json_object *,
		time_t, authdata *, char *, char *, const string_value_mapping *,
		int *);
extern int parse_string_cgivar(char *, char *, int, json_object *, time_t,
		authdata *, char *, char *, char **);
extern int parse_time_cgivar(char *, char *, int, json_object *, time_t,
		authdata *, char *, char *, time_t *);
extern int parse_boolean_cgivar(char *, char *, int, json_object *, time_t,
		authdata *, char *, char *, int *);
extern int parse_int_cgivar(char *, char *, int, json_object *, time_t,
		authdata *, char *, char *, int *);

extern int get_query_status(const int[][2], int);
extern char *svm_get_string_from_value(int, const string_value_mapping *);
extern char *svm_get_description_from_value(int, const string_value_mapping *);

extern time_t compile_time(const char *, const char *);
extern char *json_escape_string(const char *, const json_escape *);
#endif
