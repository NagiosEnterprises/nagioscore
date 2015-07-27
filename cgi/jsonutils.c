/**************************************************************************
 *
 * JSONUTILS.C -  Utilities for Nagios CGIs for returning JSON-formatted 
 *                object data
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/comments.h"

#include "../include/cgiutils.h"
#include "../include/getcgi.h"
#include "../include/cgiauth.h"
#include "../include/jsonutils.h"

/* Multiplier to increment the buffer in json_escape_string() to avoid frequent
	repeated reallocations */
#define BUF_REALLOC_MULTIPLIER 16

const char *result_types[] = {
	"Success",
	"Unable to Allocate Memory",
	"Unable to Open File for Reading",
	"Option Invalid",
	"Option Missing",
	"Option Value Missing",
	"Option Value Invalid",
	"Option Ignored"
	};

const string_value_mapping svm_format_options[] = {
	{ "whitespace", JSON_FORMAT_WHITESPACE, 
		"Pad with whitespace to increase readability" },
	{ "enumerate", JSON_FORMAT_ENUMERATE, 
		"Use textual representations of enumerated values rather than "
		"raw numeric values" },
	{ "bitmask", JSON_FORMAT_BITMASK, 
		"Use textual representations of bitmask values rather than "
		"raw numeric values" },
	{ "duration", JSON_FORMAT_DURATION, 
		"Use textual representations (xd xh xm xs) of duration values rather "
		"than raw number of seconds" },
#if 0
	{ "datetime", JSON_FORMAT_DATETIME, 
		"Format date/time values according to the supplied strftime format "
		"or '%%Y-%%m-%%d %%H:%%M:%%S' if no format specified" },
	{ "date", JSON_FORMAT_DATE, 
		"Format dates according to the supplied strftime format or "
		"default Javascript format (number of ms since the beginning of the "
		"Unix epoch) if no format specified" },
	{ "time", JSON_FORMAT_TIME, 
		"Format times according the supplied strftime format or "
		"'%%H:%%M:%%S' in for format specified" },
#endif
	{ NULL, -1, NULL },
	};

const string_value_mapping query_statuses[] = {
	{ "alpha", QUERY_STATUS_ALPHA, "Alpha" },
	{ "beta", QUERY_STATUS_BETA, "Beta" },
	{ "released", QUERY_STATUS_RELEASED, "Released" },
	{ "deprecated", QUERY_STATUS_DEPRECATED, "Deprecated" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_host_statuses[] = {
#ifdef JSON_NAGIOS_4X
	{ "up", SD_HOST_UP, "HOST_UP" },
	{ "down", SD_HOST_DOWN, "HOST_DOWN" },
	{ "unreachable", SD_HOST_UNREACHABLE, "HOST_UNREACHABLE" },
#else
	{ "up", HOST_UP, "HOST_UP" },
	{ "down", HOST_DOWN, "HOST_DOWN" },
	{ "unreachable", HOST_UNREACHABLE, "HOST_UNREACHABLE" },
#endif
	{ "pending", HOST_PENDING, "HOST_PENDING" },
	{ NULL, -1, NULL },
	};

/* Hard-coded values used because the HOST_UP/DOWN/UNREACHABLE
	macros are host status (and include PENDING), not host state */
const string_value_mapping svm_host_states[] = {
	{ "up", 0, "HOST_UP" },
	{ "down", 1, "HOST_DOWN" },
	{ "unreachable", 2, "HOST_UNREACHABLE" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_service_statuses[] = {
	{ "ok", SERVICE_OK, "SERVICE_OK" },
	{ "warning", SERVICE_WARNING, "SERVICE_WARNING" },
	{ "critical", SERVICE_CRITICAL, "SERVICE_CRITICAL" },
	{ "unknown", SERVICE_UNKNOWN, "SERVICE_UNKNOWN" },
	{ "pending", SERVICE_PENDING, "SERVICE_PENDING" },
	{ NULL, -1, NULL },
	};

/* Hard-coded values used because the SERVICE_OK/WARNING/CRITICAL/UNKNOWN
	macros are service status (and include PENDING), not service state */
const string_value_mapping svm_service_states[] = {
	{ "ok", 0, "SERVICE_OK" },
	{ "warning", 1, "SERVICE_WARNING" },
	{ "critical", 2, "SERVICE_CRITICAL" },
	{ "unknown", 3, "SERVICE_UNKNOWN" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_check_options[] = {
	{ "force_execution", CHECK_OPTION_FORCE_EXECUTION, "FORCE_EXECUTION" },
	{ "freshness_check", CHECK_OPTION_FRESHNESS_CHECK, "FRESHNESS_CHECK" },
	{ "ophan_check", CHECK_OPTION_ORPHAN_CHECK, "ORPHAN_CHECK" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_host_check_types[] = {
	{ "active", HOST_CHECK_ACTIVE, "ACTIVE" },
	{ "passive", HOST_CHECK_PASSIVE, "PASSIVE" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_service_check_types[] = {
	{ "active", SERVICE_CHECK_ACTIVE, "ACTIVE" },
	{ "passive", SERVICE_CHECK_PASSIVE, "PASSIVE" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_state_types[] = {
	{ "soft", SOFT_STATE, "SOFT" },
	{ "hard", HARD_STATE, "HARD" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_acknowledgement_types[] = {
	{ "none", ACKNOWLEDGEMENT_NONE, "NONE" },
	{ "normal", ACKNOWLEDGEMENT_NORMAL, "NORMAL" },
	{ "sticky", ACKNOWLEDGEMENT_STICKY, "STICKY" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_comment_types[] = {
	{ "host", HOST_COMMENT, "Host Comment" },
	{ "service", SERVICE_COMMENT, "Service Comment" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_comment_entry_types[] = {
	{ "user", USER_COMMENT, "User Comment" },
	{ "downtime", DOWNTIME_COMMENT, "Downtime Comment" },
	{ "flapping", FLAPPING_COMMENT, "Flapping Comment" },
	{ "acknowledgement", ACKNOWLEDGEMENT_COMMENT, "Acknowledgement Comment" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_downtime_types[] = {
	{ "service", SERVICE_DOWNTIME, "Service Downtime" },
	{ "host", HOST_DOWNTIME, "Host Downtime" },
	{ "any", ANY_DOWNTIME, "Any Downtime" },
	{ NULL, -1, NULL },
	};

#ifdef JSON_NAGIOS_4X
const string_value_mapping svm_option_types[] = {
	{ "up", OPT_UP, "Up" },
	{ "down", OPT_DOWN, "Down" },
	{ "unreachable", OPT_UNREACHABLE, "Unreachable" },
	{ "ok", OPT_OK, "OK" },
	{ "unkwown", OPT_UNKNOWN, "Unknown" },
	{ "warning", OPT_WARNING, "Warning" },
	{ "critical", OPT_CRITICAL, "Critical" },
	{ "recovery", OPT_RECOVERY, "Recovery" },
	{ "pending", OPT_PENDING, "Pending" },
	{ "flapping", OPT_FLAPPING, "Flapping" },
	{ "downtime", OPT_DOWNTIME, "Downtime" },
	{ NULL, -1, NULL },
	};
#endif

const string_value_mapping parent_host_extras[] = {
	{ "none", 0, "Hosts that are directly reachable by the Nagios Core host" },
	{ NULL, -1, NULL },
	};

const string_value_mapping child_host_extras[] = {
	{ "none", 0, "Hosts that have no child hosts" },
	{ NULL, -1, NULL },
	};

const string_value_mapping parent_service_extras[] = {
	{ "none", 0, "Services that have no parent services" },
	{ NULL, -1, NULL },
	};

const string_value_mapping child_service_extras[] = {
	{ "none", 0, "Services that have no child services" },
	{ NULL, -1, NULL },
	};

const char *dayofweek[7] = { "Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday" };
const char *month[12] = { "January", "February", "March", "April", "May",
		"June", "July", "August", "September", "October", "November",
		"December" };

static const json_escape_pair string_escape_pairs[] = {
	{ L"\\", L"\\\\" },
	{ L"\x01", L"\\u0001" },
	{ L"\x02", L"\\u0002" },
	{ L"\x03", L"\\u0003" },
	{ L"\x04", L"\\u0004" },
	{ L"\x05", L"\\u0004" },
	{ L"\x06", L"\\u0006" },
	{ L"\a", L"\\a" },
	{ L"\b", L"\\b" },
	{ L"\t", L"\\t" },
	{ L"\n", L"\\n" },
	{ L"\v", L"\\v" },
	{ L"\f", L"\\f" },
	{ L"\r", L"\\r" },
	{ L"\x0e", L"\\u000e" },
	{ L"\x0f", L"\\u000f" },
	{ L"\x10", L"\\u0010" },
	{ L"\x11", L"\\u0011" },
	{ L"\x12", L"\\u0012" },
	{ L"\x13", L"\\u0013" },
	{ L"\x14", L"\\u0014" },
	{ L"\x15", L"\\u0015" },
	{ L"\x16", L"\\u0016" },
	{ L"\x17", L"\\u0017" },
	{ L"\x18", L"\\u0018" },
	{ L"\x19", L"\\u0019" },
	{ L"\x1a", L"\\u001a" },
	{ L"\x1b", L"\\u001b" },
	{ L"\x1c", L"\\u001c" },
	{ L"\x1d", L"\\u001d" },
	{ L"\x1e", L"\\u001e" },
	{ L"\x1f", L"\\u001f" },
	{ L"\"", L"\\\"" },
};

static const json_escape string_escapes = {
	(sizeof(string_escape_pairs) / sizeof(string_escape_pairs[0])),
	string_escape_pairs
};

const json_escape_pair percent_escape_pairs[] = {
	{ L"%", L"%%" },
};

const json_escape percent_escapes = {
	(sizeof(percent_escape_pairs) / sizeof(percent_escape_pairs[0])),
	percent_escape_pairs
};

extern char main_config_file[MAX_FILENAME_LENGTH];
extern time_t program_start;

static json_object_member * json_object_add_member(json_object *);

json_object *json_new_object(void) {
	json_object *new;
	new = calloc(1, sizeof(json_object));
	return new;
	}

void json_free_object(json_object *obj, int free_children) {

	int x;
	json_object_member **mpp;

	if(1 == free_children) {
		for(x = 0, mpp = obj->members; x < obj->member_count; x++, mpp++) {
			json_free_member(*mpp, free_children);
			}
		}
	free(obj->members);
	free(obj);
	}

json_array *json_new_array(void) {
	return (json_array *)json_new_object();
	}

void json_free_member(json_object_member *mp, int free_children) {

	if(NULL != mp->key) free(mp->key);

	switch(mp->type) {
	case JSON_TYPE_OBJECT:
	case JSON_TYPE_ARRAY:
		if(NULL != mp->value.object) {
			json_free_object(mp->value.object, free_children);
			}
		break;
	case JSON_TYPE_STRING:
		if(NULL != mp->value.string) {
			free(mp->value.string);
			}
		break;
	case JSON_TYPE_INTEGER:
	case JSON_TYPE_REAL:
	case JSON_TYPE_TIME_T:
	case JSON_TYPE_BOOLEAN:
		break;
	default:
		break;
		}

	free(mp);
	}

/* Adds a member to a JSON object and returns a pointer to the new member.
	Returns NULL on failure. */
static json_object_member * json_object_add_member(json_object *obj) {

	if(0 == obj->member_count) {
		obj->members = calloc(1, sizeof(json_object_member *)); 
		if(NULL == obj->members) {
			obj->member_count = 0;
			return NULL;
			}
		}
	else {
		obj->members = realloc(obj->members, 
				((obj->member_count + 1) * sizeof(json_object_member *)));
		if(NULL == obj->members) {
			obj->member_count = 0;
			return NULL;
			}
		}
	obj->members[ obj->member_count] = calloc(1, sizeof(json_object_member));
	if(NULL == obj->members[ obj->member_count]) {
		return NULL;
		}
	obj->member_count++;

	return obj->members[ obj->member_count - 1];
	}

void json_object_append_object(json_object *obj, char *key, json_object *value) {
	json_object_member *mp;

	if(NULL == obj) return;
	if(NULL == value) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_OBJECT;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	mp->value.object = value;
	}

void json_array_append_object(json_object *obj, json_object *value) {
	json_object_append_object(obj, NULL, value);
	}

void json_object_append_array(json_object *obj, char *key, json_array *value) {
	json_object_member *mp;

	if(NULL == obj) return;
	if(NULL == value) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_ARRAY;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	mp->value.object = value;
	}

void json_array_append_array(json_array *obj, json_array *value) {
	json_object_append_array((json_object *)obj, NULL, value);
	}

void json_object_append_integer(json_object *obj, char *key, int value) {
	json_object_member *mp;

	if(NULL == obj) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_INTEGER;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	mp->value.integer = value;
	}

void json_array_append_integer(json_object *obj, int value) {
	json_object_append_integer(obj, NULL, value);
	}

void json_object_append_real(json_object *obj, char *key, double value) {
	json_object_member *mp;

	if(NULL == obj) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_REAL;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	mp->value.real = value;
	}

void json_array_append_real(json_array *obj, double value) {
	json_object_append_real(obj, NULL, value);
	}

void json_object_append_time(json_object *obj, char *key, unsigned long value) {

	unsigned hours;
	unsigned minutes;
	unsigned seconds;

	hours = (unsigned)(value / 3600);
	value -= hours * 3600;
	minutes = (unsigned)(value / 60);
	value -= minutes * 60;
	seconds = value;

	json_object_append_string(obj, key, NULL, "%02u:%02u:%02u", hours, minutes,
			seconds);
	}

void json_array_append_time(json_array *obj, unsigned long value) {
	json_object_append_time(obj, NULL, value);
	}

void json_object_append_time_t(json_object *obj, char *key, time_t value) {
	json_object_member *mp;

	if(NULL == obj) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_TIME_T;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	mp->value.time = value;
	}

void json_set_time_t(json_object_member *mp, time_t value) {
	if(NULL == mp) return;
	mp->value.time = value;
	}

void json_object_append_string(json_object *obj, char *key,
		const json_escape *format_escapes, char *format, ...) {
	json_object_member *mp;
	va_list a_list;
	int		result;
	char	*escaped_format;
	char	*buf;

	if(NULL == obj) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_STRING;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	if((NULL != format_escapes) && (NULL != format)) {
		escaped_format = json_escape_string(format, format_escapes);
		}
	else {
		escaped_format = format;
		}
	if(NULL != escaped_format) {
		va_start(a_list, format);
		result = vasprintf(&buf, escaped_format, a_list);
		va_end(a_list);
		if(result >= 0) {
			mp->value.string = buf;
			}
		}
	if((NULL != format_escapes) && (NULL != escaped_format)) {
		/* free only if format_escapes were passed and the escaping succeeded */
		free(escaped_format);
		}
	}

void json_array_append_string(json_object *obj,
		const json_escape *format_escapes, char *format, ...) {

	va_list a_list;
	int		result;
	char	*buf;

	va_start( a_list, format);
	result = vasprintf(&buf, format, a_list);
	va_end( a_list);
	if(result >= 0) {
		json_object_append_string(obj, NULL, format_escapes, buf);
		}
	}

void json_object_append_boolean(json_object *obj, char *key, int value) {
	json_object_member *mp;

	if(NULL == obj) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_BOOLEAN;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	mp->value.boolean = value;
	}

void json_array_append_boolean(json_object *obj, int value) {
	json_object_append_boolean(obj, NULL, value);
	}

void json_object_append_duration(json_object *obj, char *key, 
		unsigned long value) {
	json_object_member *mp;

	if(NULL == obj) return;

	if((mp = json_object_add_member(obj)) == NULL) {
		return;
		}
	mp->type = JSON_TYPE_DURATION;
	if(NULL != key) {
		mp->key = strdup(key);
		if(NULL == mp->key) {
			obj->member_count--;
			return;
			}
		}
	mp->value.unsigned_integer = value;
	}

void json_array_append_duration(json_object *obj, unsigned long value) {
	json_object_append_duration(obj, NULL, value);
	}

/*
	Fetch an object member based on the path. The path is a dot-separated
	list of nodes. Nodes may be either a key or a zero-based array index.

	For example to return the query_time key in the result object, the
	path would be "result.query_time". To find the 2nd host host in
	the list of hosts for a hostlist query, the path would be
	"data.hostlist.1"
*/

json_object_member *json_get_object_member(json_object *root, char *path) {

	char *dot;
	char node[1024];
	int x;
	json_object_member **mpp;

	/* Parse the path to get the first node */
	dot = strchr(path, '.');
	if(NULL == dot) {	/* single node path */
		strcpy(node, path);
		}
	else {
		strncpy(node, path, (dot - path));
		node[dot - path] = '\0';
		}

	/* Loop over the members of the passed root looking for the node name */
	for(x = 0, mpp = root->members; x < root->member_count; x++, mpp++) {
		if(!strcmp((*mpp)->key, node)) {
			if(NULL == dot) { /* return this node */
				return *mpp;
				}
			else {
				switch((*mpp)->type) {
				case JSON_TYPE_OBJECT:
					return json_get_object_member((*mpp)->value.object, dot + 1);
					break;
				case JSON_TYPE_ARRAY:
					return json_get_array_member((*mpp)->value.object, dot + 1);
					break;
				default:
					/* It should never happen that we want the child of a
						childless node */
					return NULL;
					break;
					}
				}
			}
		}

	return NULL;
	}

json_object_member *json_get_array_member(json_object *root, char *path) {

	char *dot;
	char node[1024];
	int index;
	json_object_member *mp;

	/* Parse the path to get the first node */
	dot = strchr(path, '.');
	if(NULL == dot) {	/* single node path */
		strcpy(node, path);
		}
	else {
		strncpy(node, path, (dot - path));
		node[dot - path] = '\0';
		}
	index = (int)strtol(node, NULL, 10);

	/* Verify that we have a reasonable index */
	if(index < 0 || index >= root->member_count) {
		return NULL;
		}

	/* Find the requested member and deal with it appropriately */
	mp = root->members[ index];
	if(NULL == dot) { /* return this node */
		return mp;
		}
	else {
		switch(mp->type) {
		case JSON_TYPE_OBJECT:
			return json_get_object_member(mp->value.object, dot + 1);
			break;
		case JSON_TYPE_ARRAY:
			return json_get_array_member(mp->value.object, dot + 1);
			break;
		default:
			/* It should never happen that we want the child of a
				childless node */
			return NULL;
			break;
			}
		}

	return NULL;
	}

void json_object_print(json_object *obj, int padding, int whitespace,
		char *strftime_format, unsigned format_options) {
	int x;
	json_object_member **mpp;

	//indentf(padding, whitespace, "{%s", (whitespace ? "\n" : ""));
	printf( "{%s", (whitespace ? "\n" : ""));
	padding++;
	for(x = 0, mpp = obj->members; x < obj->member_count; x++, mpp++) {
		json_member_print(*mpp, padding, whitespace, strftime_format, 
				format_options);
		if(x != obj->member_count - 1) printf(",");
		if(whitespace) printf("\n");
		}
	padding--;
	indentf(padding, whitespace, "}");
}

void json_array_print(json_array *obj, int padding, int whitespace,
		char *strftime_format, unsigned format_options) {
	int x;
	json_object_member **mpp;

	printf( "[%s", (whitespace ? "\n" : ""));
	padding++;
	for(x = 0, mpp = obj->members; x < obj->member_count; x++, mpp++) {
		json_member_print(*mpp, padding, whitespace, strftime_format, 
				format_options);
		if(x != obj->member_count - 1) printf(",");
		if(whitespace) printf("\n");
		}
	padding--;
	indentf(padding, whitespace, "]");
	}

void json_member_print(json_object_member *mp, int padding, int whitespace, 
		char *strftime_format, unsigned format_options) {

	char *buf = NULL;

	switch(mp->type) {
	case JSON_TYPE_OBJECT:
		if(NULL != mp->key) {
			buf = json_escape_string(mp->key, &string_escapes);
			indentf(padding, whitespace, "\"%s\": ", buf);
			if(NULL != buf) free(buf);
			}
		else {
			indentf(padding, whitespace, "");
			}
		json_object_print(mp->value.object, padding, whitespace, 
				strftime_format, format_options);
		break;
	case JSON_TYPE_ARRAY:
		if(NULL != mp->key) {
			buf = json_escape_string(mp->key, &string_escapes);
			indentf(padding, whitespace, "\"%s\": ", buf);
			if(NULL != buf) free(buf);
			}
		else {
			indentf(padding, whitespace, "");
			}
		json_array_print(mp->value.object, padding, whitespace, strftime_format,
				format_options);
		break;
	case JSON_TYPE_INTEGER:
		json_int(padding, whitespace, mp->key, mp->value.integer);
		break;
	case JSON_TYPE_REAL:
		json_float(padding, whitespace, mp->key, mp->value.real);
		break;
	case JSON_TYPE_TIME_T:
		json_time_t(padding, whitespace, mp->key, mp->value.time, 
				strftime_format);
		break;
	case JSON_TYPE_STRING:
		json_string(padding, whitespace, mp->key, mp->value.string);
		break;
	case JSON_TYPE_BOOLEAN:
		json_boolean(padding, whitespace, mp->key, mp->value.boolean);
		break;
	case JSON_TYPE_DURATION:
		json_duration(padding, whitespace, mp->key, mp->value.unsigned_integer,
				format_options & JSON_FORMAT_DURATION);
		break;
	default:
		break;
		}
	}

void indentf(int padding, int whitespace, char *format, ...) {
	va_list a_list;
	int padvar;

	if( whitespace > 0) {
		for(padvar = 0; padvar < padding; padvar++) printf( "  ");
		}
	va_start( a_list, format);
	vprintf(format, a_list);
	va_end( a_list);
	}

json_object * json_result(time_t query_time, char *cgi, char *query,
		int query_status, time_t last_data_update, authdata *authinfo, int type,
		char *message, ...) {

	json_object *json_result;
	va_list a_list;
	char	*buf;


	json_result = json_new_object();
	json_object_append_time_t(json_result, "query_time", query_time);
	json_object_append_string(json_result, "cgi", &percent_escapes, cgi);
	if(NULL != authinfo) {
		json_object_append_string(json_result, "user", &percent_escapes,
				authinfo->username);
		}
	if(NULL != query) {
		json_object_append_string(json_result, "query", &percent_escapes,
				query);
		json_object_append_string(json_result, "query_status", &percent_escapes,
				svm_get_string_from_value(query_status, query_statuses));
		}
	json_object_append_time_t(json_result, "program_start", program_start);
	if(last_data_update != (time_t)-1) {
		json_object_append_time_t(json_result, "last_data_update",
				last_data_update);
		}
	json_object_append_integer(json_result, "type_code", type);
	json_object_append_string(json_result, "type_text", &percent_escapes,
			(char *)result_types[ type]);
	va_start( a_list, message);
	if(vasprintf(&buf, message, a_list) == -1) {
		buf = NULL;
		}
	va_end( a_list);
	json_object_append_string(json_result, "message", &percent_escapes, buf);
	if(NULL != buf) free(buf);

	return json_result;
}

json_object *json_help(option_help *help) {

	json_object *json_data = json_new_object();
	json_object *json_options = json_new_object();
	json_object *json_option;
	json_array *json_required;
	json_array *json_optional;
	json_object *json_validvalues;
	json_object *json_validvalue;
	int x;
	char **	stpp;
	string_value_mapping *svmp;

	while(NULL != help->name) {
		json_option = json_new_object();
		json_object_append_string(json_option, "label", &percent_escapes,
				(char *)help->label);
		json_object_append_string(json_option, "type", &percent_escapes,
				(char *)help->type);

		json_required = json_new_array();
		for(x = 0, stpp = (char **)help->required; 
				(( x < sizeof( help->required) / 
				sizeof( help->required[ 0])) && ( NULL != *stpp)); 
				x++, stpp++) {
			json_array_append_string(json_required, &percent_escapes, *stpp);
			}
		json_object_append_array(json_option, "required",
				json_required);

		json_optional = json_new_array();
		for(x = 0, stpp = (char **)help->optional; 
				(( x < sizeof( help->optional) / 
				sizeof( help->optional[ 0])) && ( NULL != *stpp)); 
				x++, stpp++) {
			json_array_append_string(json_optional, &percent_escapes, *stpp);
			}
		json_object_append_array(json_option, "optional",
				json_optional);

		json_object_append_string(json_option, "depends_on", 
				&percent_escapes, (char *)help->depends_on);
		json_object_append_string(json_option, "description", 
				&percent_escapes, (char *)help->description);
		if( NULL != help->valid_values) {
			json_validvalues = json_new_object();
			for(svmp = (string_value_mapping *)help->valid_values; 
					NULL != svmp->string; svmp++) {
				if( NULL != svmp->description) {
					json_validvalue = json_new_object();
					json_object_append_string(json_validvalue, "description", 
							&percent_escapes, svmp->description);
					json_object_append_object(json_validvalues, svmp->string, 
							json_validvalue);
					}
				else {
					json_array_append_string(json_validvalues, &percent_escapes,
							svmp->string);
					}
				}
			json_object_append_object(json_option, "valid_values", 
					json_validvalues);
			}
		json_object_append_object(json_options, (char *)help->name, json_option);
		help++;
		}

	json_object_append_object(json_data, "options", json_options);

	return json_data;
	}

int passes_start_and_count_limits(int start, int max, int current, int counted) {

	int result = FALSE;

	if(start > 0) {
		/* The user requested we start at a specific index */
		if(current >= start) {
			if(max > 0) {
				/* The user requested a limit on the number of items returned */
				if(counted < max) {
					result = TRUE;
					}
				}
			else {
				/* The user did not request a limit on the number of items 
					returned */
				result = TRUE;
				}
			}
		}
	else {
		/* The user did not request we start at a specific index */
		if(max > 0) {
			/* The user requested a limit on the number of items returned */
			if(counted < max) {
				result = TRUE;
				}
			}
		else {
			/* The user did not request a limit on the number of items 
					returned */
			result = TRUE;
			}
		}
	return result;
	}

void json_string(int padding, int whitespace, char *key, char *value) {

	char *keybuf = NULL;
	char *valbuf = NULL;

	valbuf = json_escape_string(value, &string_escapes);

	if( NULL == key) {
		indentf(padding, whitespace, "\"%s\"",
				(( NULL == valbuf) ? "" : valbuf));
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s\"%s\"", keybuf, 
				(( whitespace> 0) ? " " : ""),
				(( NULL == valbuf) ? "" : valbuf));
		}
	if(NULL != keybuf) free(keybuf);
	if(NULL != valbuf) free(valbuf);
	}

void json_boolean(int padding, int whitespace, char *key, int value) {

	char *keybuf = NULL;

	if( NULL == key) {
		indentf(padding, whitespace, "%s", 
				(( 0 == value) ? "false" : "true"));
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s%s", keybuf, 
				(( whitespace > 0) ? " " : ""),
				(( 0 == value) ? "false" : "true"));
		}
	if(NULL != keybuf) free(keybuf);
	}

void json_int(int padding, int whitespace, char *key, int value) {

	char *keybuf = NULL;

	if( NULL == key) {
		indentf(padding, whitespace, "%d", value); 
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s%d", keybuf, 
				(( whitespace > 0) ? " " : ""), value); 
		}
	if(NULL != keybuf) free(keybuf);
	}

void json_unsigned(int padding, int whitespace, char *key, 
		unsigned long long value) {

	char *keybuf = NULL;

	if( NULL == key) {
		indentf(padding, whitespace, "%llu", value);
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s%llu", keybuf, 
				(( whitespace > 0) ? " " : ""), value);
		}
	if(NULL != keybuf) free(keybuf);
	}

void json_float(int padding, int whitespace, char *key, double value) {

	char *keybuf = NULL;

	if( NULL == key) {
		indentf(padding, whitespace, "%.2f", value);
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s%.2f", keybuf, 
				(( whitespace > 0) ? " " : ""), value);
		}
	if(NULL != keybuf) free(keybuf);
	}

void json_time(int padding, int whitespace, char *key, unsigned long value) {

	char *keybuf = NULL;
	unsigned hours;
	unsigned minutes;
	unsigned seconds;

	hours = (unsigned)(value / 3600);
	value -= hours * 3600;
	minutes = (unsigned)(value / 60);
	value -= minutes * 60;
	seconds = value;

	if( NULL == key) {
		indentf(padding, whitespace, "\"%02u:%02u:%02u\"", hours, minutes,
				seconds);
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s\"%02u:%02u:%02u\"", keybuf, 
				(( whitespace > 0) ? " " : ""), hours, minutes,
				seconds);
		}
	if(NULL != keybuf) free(keybuf);
	}

void json_time_t(int padding, int whitespace, char *key, time_t value, 
		char *format) {

	char		*keybuf = NULL;
	char		buf[1024];
	struct tm	*tmp_tm;

	if(NULL == format) {
		snprintf(buf, sizeof(buf)-1, "%llu%s", (unsigned long long)value, 
				((unsigned long long)value > 0 ? "000" : ""));
		}
	else {
		tmp_tm = localtime(&value);
		buf[ 0] = '"';
		strftime(buf+1, sizeof(buf)-3, format, tmp_tm);
		strcat(buf, "\"");
		}

	if(NULL == key) {
		indentf(padding, whitespace, "%s", buf);
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s%s", keybuf, 
				(( whitespace > 0) ? " " : ""), buf);
		}
	if(NULL != keybuf) free(keybuf);
	}

void json_duration(int padding, int whitespace, char *key, unsigned long value,
		int format_duration) {

	char		*keybuf = NULL;
	char		buf[1024];
	int			days = 0;
	int			hours = 0;
	int			minutes = 0;
	int			seconds = 0;

	if(0 == format_duration) {
		snprintf(buf, sizeof(buf)-1, "%lu", (unsigned long)value);
		}
	else {
		days = (unsigned)(value / 86400);
		value -= days * 86400;
		hours = (unsigned)(value / 3600);
		value -= hours * 3600;
		minutes = (unsigned)(value / 60);
		value -= minutes * 60;
		seconds = value;
		snprintf(buf, sizeof(buf)-1, "%ud %uh %um %us", days, hours, minutes, 
				seconds);
		}

	if( NULL == key) {
		indentf(padding, whitespace, "%s", buf);
		}
	else {
		keybuf = json_escape_string(key, &string_escapes);
		indentf(padding, whitespace, "\"%s\":%s%s%s%s", keybuf, 
				(( whitespace > 0) ? " " : ""), (format_duration ? "\"" : ""),
				buf, (format_duration ? "\"" : ""));
		}
	if(NULL != keybuf) free(keybuf);
	}

void json_enumeration(json_object *json_parent, unsigned format_options, 
		char *key, int value, const string_value_mapping *map) {

	string_value_mapping *svmp;

	if(format_options & JSON_FORMAT_ENUMERATE) {
		for(svmp = (string_value_mapping *)map; NULL != svmp->string; svmp++) {
			if( value == svmp->value) {
				json_object_append_string(json_parent, key, &percent_escapes,
						svmp->string);
				break;
				}
			}
			if( NULL == svmp->string) {
				json_object_append_string(json_parent, key, NULL,
						"Unknown value %d", svmp->value);
				}
		}
	else {
		json_object_append_integer(json_parent, key, value);
		}
	}

void json_bitmask(json_object *json_parent, unsigned format_options, char *key, 
		int value, const string_value_mapping *map) {

	json_array *json_bitmask_array;
	string_value_mapping *svmp;

	if(format_options & JSON_FORMAT_BITMASK) {
		json_bitmask_array = json_new_array();
		for(svmp = (string_value_mapping *)map; NULL != svmp->string; svmp++) {
			if( value & svmp->value) {
				json_array_append_string(json_bitmask_array, &percent_escapes,
						svmp->string);
				}
			}
		json_object_append_array(json_parent, key, json_bitmask_array);
		}
	else {
		json_object_append_integer(json_parent, key, value);
		}
	}

int parse_bitmask_cgivar(char *cgi, char *query, int query_status,
		json_object *json_parent, time_t query_time, authdata *authinfo,
		char *key, char *value, const string_value_mapping *svm,
		unsigned *var) {

	int result = RESULT_SUCCESS;
	char *option;
	char *saveptr;
	string_value_mapping *svmp;

	if(value == NULL) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_MISSING,
				"No value specified for %s option.", key));
		return RESULT_OPTION_VALUE_MISSING;
		}

	option = strtok_r(value, " ", &saveptr);
	while(NULL != option) {	
		for(svmp = (string_value_mapping *)svm; NULL != svmp->string; svmp++) {
			if( !strcmp( svmp->string, option)) {
					*var |= svmp->value;
					break;
				}
			}
		if( NULL == svmp->string) {
			json_object_append_object(json_parent, "result", 
					json_result(query_time, cgi, query, query_status,
					(time_t)-1, authinfo, RESULT_OPTION_VALUE_INVALID,
					"The %s option value '%s' is invalid.", key, option));
			result = RESULT_OPTION_VALUE_INVALID;
			break;
			}
		option = strtok_r(NULL, " ", &saveptr);
		}
	return result;
	}

int parse_enumeration_cgivar(char *cgi, char *query, int query_status,
		json_object *json_parent, time_t query_time, authdata *authinfo,
		char *key, char *value, const string_value_mapping *svm, int *var) {

	string_value_mapping *svmp;

	if(value == NULL) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_MISSING,
				"No value specified for %s option.", key));
		return RESULT_OPTION_VALUE_MISSING;
		}

	for(svmp = (string_value_mapping *)svm; NULL != svmp->string; svmp++) {
		if( !strcmp( svmp->string, value)) {
				*var = svmp->value;
				break;
			}
		}
	if( NULL == svmp->string) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_INVALID,
				"The %s option value '%s' is invalid.", key, value));
		return RESULT_OPTION_VALUE_INVALID;
		} 

	return RESULT_SUCCESS;
	}


int parse_string_cgivar(char *cgi, char *query, int query_status,
		json_object *json_parent, time_t query_time, authdata *authinfo,
		char *key, char *value, char **var) {

	if(value == NULL) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_MISSING,
				"No value specified for %s option.", key));
		return RESULT_OPTION_VALUE_MISSING;
		}

	if(NULL == (*var = strdup( value))) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_MEMORY_ALLOCATION_ERROR,
				"Unable to allocate memory for %s option.", key));
		return RESULT_MEMORY_ALLOCATION_ERROR;
		}

	return RESULT_SUCCESS;
	}


int parse_time_cgivar(char *cgi, char *query, int query_status,
		json_object *json_parent, time_t query_time, authdata *authinfo,
		char *key, char *value, time_t *var) {

	long long templl;

	if(value == NULL) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_MISSING,
				"No value specified for %s option.", key));
		return RESULT_OPTION_VALUE_MISSING;
		}

	if('+' == value[0]) {
		templl = strtoll(&(value[1]), NULL, 10);
		*var = (time_t)((long long)query_time + templl);
		}
	else if('-' == value[0]) {
		templl = strtoll(&(value[1]), NULL, 10);
		*var = (time_t)((long long)query_time - templl);
		}
	else {
		templl = strtoll(value, NULL, 10);
		*var = (time_t)templl;
		}

	return RESULT_SUCCESS;
	}


int parse_boolean_cgivar(char *cgi, char *query, int query_status,
		json_object *json_parent, time_t query_time, authdata *authinfo,
		char *key, char *value, int *var) {

	if(value == NULL) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_MISSING,
				"No value specified for %s option.", key));
		return ERROR;
		}

	if(!strcmp(value, "true")) {
		*var = 1;
		}
	else if(!strcmp(value, "false")) {
		*var = 0;
		}
	else {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_INVALID,
				"Value for %s option must be 'true' or 'false'.", key));
		return RESULT_OPTION_VALUE_INVALID;
		}

	return RESULT_SUCCESS;
	}


int parse_int_cgivar(char *cgi, char *query, int query_status,
		json_object *json_parent, time_t query_time, authdata *authinfo,
		char *key, char *value, int *var) {

	if(value == NULL) {
		json_object_append_object(json_parent, "result", 
				json_result(query_time, cgi, query, query_status,
				(time_t)-1, authinfo, RESULT_OPTION_VALUE_MISSING,
				"No value specified for %s option.", key));
		return RESULT_OPTION_VALUE_MISSING;
		}

	*var = atoi(value);
	return RESULT_SUCCESS;
	}

int get_query_status(const int statuses[][2], int query) {
	int x;

	for(x = 0; -1 != statuses[x][0]; x++) {
		if(statuses[x][0] == query) return statuses[x][1];
		}
	return -1;
	}

char *svm_get_string_from_value(int value, const string_value_mapping *svm) {

	string_value_mapping *svmp;

	for(svmp = (string_value_mapping *)svm; NULL != svmp->string; svmp++) {
		if(svmp->value == value) return svmp->string;
		}
	return NULL;
	}

char *svm_get_description_from_value(int value, const string_value_mapping *svm) {

	string_value_mapping *svmp;

	for(svmp = (string_value_mapping *)svm; NULL != svmp->string; svmp++) {
		if(svmp->value == value) return svmp->description;
		}
	return NULL;
	}

/* Thanks to Jerry Coffin for posting the basis of this function on Stack
	Overflow */
time_t compile_time(const char *date, const char *time) {

	char buf[5];
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;

    struct tm t;
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(date, "%s %d %d", buf, &day, &year);
    sscanf(time, "%d:%d:%d", &hour, &minute, &second);

    month = (strstr(months, buf) - months) / 3;

    t.tm_year = year - 1900;
    t.tm_mon = month;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = -1;

    return mktime(&t);
}

/* Escape a string based on the values in the escapes parameter */
char *json_escape_string(const char *src, const json_escape *escapes) {

	wchar_t *wdest;		/* wide character version of the output string */
	size_t	wdest_size;	/* number of available wchars in wdest */
	size_t	wdest_len;	/* number of wchars in wdest */
	int x;
	json_escape_pair	*escp;		/* pointer to current escape pair */
	size_t	from_len;
	size_t	to_len;
	wchar_t	*fromp;		/* pointer to a found "from" string */
	long	offset;		/* offset from beginning of wdest to a "from" string */
	size_t	wchars;		/* number of wide characters to move */
	size_t	dest_len;	/* length of ouput string "dest" */
	char	*dest;		/* buffer containing the escaped version of src */

	/* Make sure we're passed valid parameters */
	if((NULL == src) || (NULL == escapes)) {
		return NULL;
		}

	/* Make a wide string copy of src */
	wdest_len = mbstowcs(NULL, src, 0);
	if(wdest_len <= 0) return NULL;
	if((wdest = calloc(wdest_len + 1, sizeof(wchar_t))) == NULL) {
		return NULL;
		}
	if(mbstowcs(wdest, src, wdest_len) != wdest_len) {
		free(wdest);
		return NULL;
		}
	wdest_size = wdest_len;

	/* Process each escape pair */
	for(x = 0, escp = (json_escape_pair *)escapes->pairs; x < escapes->count;
			x++, escp++) {
		from_len = wcslen(escp->from);
		to_len = wcslen(escp->to);
		fromp = wdest;
		while((fromp = wcsstr(fromp, escp->from)) != NULL) {
			offset = fromp - wdest;
			if(from_len < to_len) {
				if((wdest_size - wdest_len) < (to_len - from_len)) {
					/* If more room is needed, realloc and update variables */
					wdest_size += (to_len - from_len) * BUF_REALLOC_MULTIPLIER;
					wdest = realloc(wdest, (wdest_size + 1) * sizeof(wchar_t));
					if(NULL == wdest) return NULL;
					fromp = wdest + offset;
					}
				wchars = wdest_len - offset - from_len + 1;
				wmemmove(fromp + to_len, fromp + from_len, wchars);
				wcsncpy(fromp, escp->to, to_len);
				wdest_len += (to_len - from_len);
				fromp += to_len;
				}
			else {
				wchars = wdest_len - offset - to_len;
				memmove(fromp + to_len, fromp + from_len,
						wchars * sizeof(wchar_t));
				wcsncpy(fromp, escp->to, to_len);
				fromp += (from_len - to_len);
				wdest_len -= (from_len - to_len);
				}
			}
		}

	/* Covert the wide string back to a multibyte string */
	dest_len = wcstombs(NULL, wdest, 0);
	if(0 == dest_len) return NULL;
	if((dest = calloc(dest_len + 1, sizeof(char))) == NULL) {
		return NULL;
		}
	if(wcstombs(dest, wdest, dest_len) != dest_len) {
		free(dest);
		return NULL;
		}

	return dest;
	}
