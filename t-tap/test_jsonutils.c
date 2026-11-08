/*****************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*
*****************************************************************************/

/*
 * The JSON CGIs pass object names (host names, service descriptions, ...)
 * as the *format* of json_object_append_string()/json_array_append_string()
 * together with percent_escapes, relying on the escaping to turn every '%'
 * of the name into "%%" before it is formatted. json_array_append_string()
 * used to format first and escape afterwards, so a name ending in '%' was
 * dropped (vasprintf() fails) and one containing e.g. "%s" was undefined
 * behavior. This checks that names come through unchanged either way.
 */

#include <stdlib.h>
#include <string.h>

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/cgiutils.h"
#include "../include/jsonutils.h"
#include "tap.h"

/* jsonutils.c's json_result() reads it; nothing here calls that */
time_t program_start = 0;

extern const json_escape percent_escapes;

/* an empty value may be stored as NULL; it is printed as "" either way */
static const char *str(const char *s) {
	return (NULL == s) ? "" : s;
	}

static const char *names[] = {
	"plain",
	"disk /var 50%",
	"100% full",
	"%",
	"%%",
	"a %s b %d c %n",
	"",
	};
#define NAME_COUNT (sizeof(names) / sizeof(names[0]))

int
main() {
	json_object *obj;
	json_array *arr;
	unsigned x;

	plan_tests(2 * NAME_COUNT + 5);

	arr = json_new_array();
	for(x = 0; x < NAME_COUNT; x++)
		json_array_append_string(arr, &percent_escapes, (char *)names[x]);
	ok(arr->member_count == NAME_COUNT, "json_array_append_string() keeps every value (%u of %u)", arr->member_count, (unsigned)NAME_COUNT);
	for(x = 0; x < NAME_COUNT && x < arr->member_count; x++)
		ok(!strcmp(str(arr->members[x]->value.string), names[x]), "array value '%s' unchanged", names[x]);
	for(; x < NAME_COUNT; x++)
		ok(0, "array value '%s' missing", names[x]);
	json_free_object(arr, 1);

	obj = json_new_object();
	for(x = 0; x < NAME_COUNT; x++)
		json_object_append_string(obj, "k", &percent_escapes, (char *)names[x]);
	for(x = 0; x < NAME_COUNT; x++)
		ok(x < obj->member_count && !strcmp(str(obj->members[x]->value.string), names[x]), "object value '%s' unchanged", names[x]);
	json_free_object(obj, 1);

	/* without escapes the value is a real format with arguments */
	arr = json_new_array();
	json_array_append_string(arr, NULL, "%s-%d", "x", 42);
	ok(arr->member_count == 1 && !strcmp(arr->members[0]->value.string, "x-42"), "unescaped format is formatted with its arguments");
	/* with escapes, the format's own text is escaped but arguments are not */
	json_array_append_string(arr, &percent_escapes, "%s", "50%");
	ok(arr->member_count == 2 && !strcmp(arr->members[1]->value.string, "%s"), "escaped format is taken literally");
	json_array_append_string(arr, &percent_escapes, NULL);
	ok(arr->member_count == 2, "NULL format adds nothing");
	json_free_object(arr, 1);

	obj = json_new_object();
	json_array_append_string(obj, &percent_escapes, "%%");
	ok(obj->member_count == 1 && !strcmp(obj->members[0]->value.string, "%%"), "\"%%%%\" stays two percent signs");
	json_free_object(obj, 1);

	return exit_status();
	}
