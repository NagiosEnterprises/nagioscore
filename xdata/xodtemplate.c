/*****************************************************************************
 *
 * XODTEMPLATE.C - Template-based object configuration data input routines
 *
 *
 * Description:
 *
 * Routines for parsing and resolving template-based object definitions.
 * Basic steps involved in this in the daemon are as follows:
 *
 *    1) Read
 *    2) Resolve
 *    3) Duplicate
 *    4) Recombobulate
 *    5) Cache
 *    7) Register
 *    8) Cleanup
 *
 * The steps involved for the CGIs differ a bit, since they read the cached
 * definitions which are already resolved, recombobulated and duplicated.  In
 * otherwords, they've already been "flattened"...
 *
 *    1) Read
 *    2) Register
 *    3) Cleanup
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


/*********** COMMON HEADER FILES ***********/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/locations.h"
#include "../include/macros.h"

/**** CORE OR CGI SPECIFIC HEADER FILES ****/

#ifdef NSCORE
#include "../include/nagios.h"
#endif

#ifdef NSCGI
#include "../include/cgiutils.h"
#endif

/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xodtemplate.h"

#define XOD_NEW   0 /* not seen */
#define XOD_SEEN  1 /* seen, but not yet loopy */
#define XOD_LOOPY 2 /* loopy */
#define XOD_OK    3 /* not loopy */

xodtemplate_timeperiod *xodtemplate_timeperiod_list = NULL;
xodtemplate_command *xodtemplate_command_list = NULL;
xodtemplate_contactgroup *xodtemplate_contactgroup_list = NULL;
xodtemplate_hostgroup *xodtemplate_hostgroup_list = NULL;
xodtemplate_servicegroup *xodtemplate_servicegroup_list = NULL;
xodtemplate_servicedependency *xodtemplate_servicedependency_list = NULL;
xodtemplate_serviceescalation *xodtemplate_serviceescalation_list = NULL;
xodtemplate_contact *xodtemplate_contact_list = NULL;
xodtemplate_host *xodtemplate_host_list = NULL;
xodtemplate_service *xodtemplate_service_list = NULL;
xodtemplate_hostdependency *xodtemplate_hostdependency_list = NULL;
xodtemplate_hostescalation *xodtemplate_hostescalation_list = NULL;
xodtemplate_hostextinfo *xodtemplate_hostextinfo_list = NULL;
xodtemplate_serviceextinfo *xodtemplate_serviceextinfo_list = NULL;

xodtemplate_timeperiod *xodtemplate_timeperiod_list_tail = NULL;
xodtemplate_command *xodtemplate_command_list_tail = NULL;
xodtemplate_contactgroup *xodtemplate_contactgroup_list_tail = NULL;
xodtemplate_hostgroup *xodtemplate_hostgroup_list_tail = NULL;
xodtemplate_servicegroup *xodtemplate_servicegroup_list_tail = NULL;
xodtemplate_servicedependency *xodtemplate_servicedependency_list_tail = NULL;
xodtemplate_serviceescalation *xodtemplate_serviceescalation_list_tail = NULL;
xodtemplate_contact *xodtemplate_contact_list_tail = NULL;
xodtemplate_host *xodtemplate_host_list_tail = NULL;
xodtemplate_service *xodtemplate_service_list_tail = NULL;
xodtemplate_hostdependency *xodtemplate_hostdependency_list_tail = NULL;
xodtemplate_hostescalation *xodtemplate_hostescalation_list_tail = NULL;
xodtemplate_hostextinfo *xodtemplate_hostextinfo_list_tail = NULL;
xodtemplate_serviceextinfo *xodtemplate_serviceextinfo_list_tail = NULL;


skiplist *xobject_template_skiplists[NUM_XOBJECT_SKIPLISTS];
skiplist *xobject_skiplists[NUM_XOBJECT_SKIPLISTS];


void *xodtemplate_current_object = NULL;
int xodtemplate_current_object_type = XODTEMPLATE_NONE;

int xodtemplate_current_config_file = 0;
char **xodtemplate_config_files = NULL;

int presorted_objects = FALSE;

/* xodtemplate id / object counter */
static struct object_count xodcount;

/* reusable bitmaps for expanding objects */
static bitmap *host_map = NULL, *contact_map = NULL;
static bitmap *service_map = NULL, *parent_map = NULL;

/* These variables are defined in base/utils.c, but as CGIs do not need these
   we just fake the values for this file */
#ifdef NSCGI
#define use_precached_objects TRUE
#define use_regexp_matches FALSE
#define use_true_regexp_matching FALSE
#define test_scheduling FALSE
#endif

/*
 * simple inheritance macros. o = object, t = template, v = variable
 * Note that these can be used for inter-object inheritance as well,
 * so long as the variable names are identical.
 */
#define xod_inherit(o, t, v) \
	do { \
		if(o->have_##v == FALSE && t->have_##v == TRUE) { \
			o->v = t->v; \
			o->have_##v = TRUE; \
		} \
	} while(0)

#define xod_inherit_str_nohave(o, t, v) \
	do { \
		if(o->v == NULL && t->v != NULL) { \
			o->v = (char *)strdup(t->v); \
		} \
	} while(0)

#define xod_inherit_str(o, t, v) \
	do { \
		if(o->have_##v == FALSE && t->have_##v == TRUE) { \
			xod_inherit_str_nohave(o, t, v); \
			o->have_##v = TRUE; \
		} \
	} while(0)



/* returns the name of a numbered config file */
static const char *xodtemplate_config_file_name(int cfgfile) {
	if(cfgfile <= xodtemplate_current_config_file)
		return xodtemplate_config_files[cfgfile - 1];

	return "?";
	}



/******************************************************************/
/************* TOP-LEVEL CONFIG DATA INPUT FUNCTION ***************/
/******************************************************************/

#ifndef NSCGI
static void xodtemplate_free_template_skiplists(void) {
	int x = 0;

	for(x = 0; x < NUM_XOBJECT_SKIPLISTS; x++) {
		skiplist_free(&xobject_template_skiplists[x]);
		}
	}
#endif

/* process all config files - both core and CGIs pass in name of main config file */
int xodtemplate_read_config_data(const char *main_config_file, int options) {
#ifdef NSCORE
	char *cfgfile = NULL;
	char *config_base_dir = NULL;
	char *input = NULL;
	char *var = NULL;
	char *val = NULL;
	double runtime[11];
	mmapfile *thefile = NULL;
#endif
	struct timeval tv[12];
	int result = OK;


	if(main_config_file == NULL) {
#ifdef NSCORE
		printf("Error: No main config file passed to object routines!\n");
#endif
		return ERROR;
		}

	timing_point("Reading config data from '%s'\n", main_config_file);

	/* initialize variables */
	xodtemplate_timeperiod_list = NULL;
	xodtemplate_command_list = NULL;
	xodtemplate_contactgroup_list = NULL;
	xodtemplate_hostgroup_list = NULL;
	xodtemplate_servicegroup_list = NULL;
	xodtemplate_servicedependency_list = NULL;
	xodtemplate_serviceescalation_list = NULL;
	xodtemplate_contact_list = NULL;
	xodtemplate_host_list = NULL;
	xodtemplate_service_list = NULL;
	xodtemplate_hostdependency_list = NULL;
	xodtemplate_hostescalation_list = NULL;
	xodtemplate_hostextinfo_list = NULL;
	xodtemplate_serviceextinfo_list = NULL;

	/* initialize skiplists */
	xodtemplate_init_xobject_skiplists();

	xodtemplate_current_object = NULL;
	xodtemplate_current_object_type = XODTEMPLATE_NONE;

	/* allocate memory for 256 config files (increased dynamically) */
	xodtemplate_current_config_file = 0;
	xodtemplate_config_files = (char **)malloc(256 * sizeof(char *));
	if(xodtemplate_config_files == NULL) {
#ifdef NSCORE
		printf("Unable to allocate memory!\n");
#endif
		return ERROR;
		}

	/* are the objects we're reading already pre-sorted? */
	presorted_objects = FALSE;
#ifdef NSCORE
	presorted_objects = (use_precached_objects == TRUE) ? TRUE : FALSE;
#endif

#ifdef NSCORE
	if(test_scheduling == TRUE)
		gettimeofday(&tv[0], NULL);

	/* only process the precached object file as long as we're not regenerating it and we're not verifying the config */
	if(use_precached_objects == TRUE)
		result = xodtemplate_process_config_file(object_precache_file, options);

	/* process object config files normally... */
	else {
		/* determine the directory of the main config file */
		if((cfgfile = (char *)strdup(main_config_file)) == NULL) {
			my_free(xodtemplate_config_files);
			printf("Unable to allocate memory!\n");
			return ERROR;
			}
		config_base_dir = (char *)strdup(dirname(cfgfile));
		my_free(cfgfile);

		/* open the main config file for reading (we need to find all the config files to read) */
		if((thefile = mmap_fopen(main_config_file)) == NULL) {
			my_free(config_base_dir);
			my_free(xodtemplate_config_files);
			printf("Unable to open main config file '%s'\n", main_config_file);
			return ERROR;
			}

		/* daemon reads all config files/dirs specified in the main config file */
		/* read in all lines from the main config file */
		while(1) {

			/* free memory */
			my_free(input);

			/* get the next line */
			if((input = mmap_fgets_multiline(thefile)) == NULL)
				break;

			/* strip input */
			strip(input);

			/* skip blank lines and comments */
			if(input[0] == '#' || input[0] == ';' || input[0] == '\x0')
				continue;

			if((var = strtok(input, "=")) == NULL)
				continue;

			if((val = strtok(NULL, "\n")) == NULL)
				continue;

			/* process a single config file */
			if(!strcmp(var, "xodtemplate_config_file") || !strcmp(var, "cfg_file")) {

				if(config_base_dir != NULL && val[0] != '/') {
					asprintf(&cfgfile, "%s/%s", config_base_dir, val);
					}
				else
					cfgfile = strdup(val);

				/* process the config file... */
				result = xodtemplate_process_config_file(cfgfile, options);

				my_free(cfgfile);

				/* if there was an error processing the config file, break out of loop */
				if(result == ERROR)
					break;
				}

			/* process all files in a config directory */
			else if(!strcmp(var, "xodtemplate_config_dir") || !strcmp(var, "cfg_dir")) {

				if(config_base_dir != NULL && val[0] != '/') {
					asprintf(&cfgfile, "%s/%s", config_base_dir, val);
					}
				else
					cfgfile = strdup(val);

				/* strip trailing / if necessary */
				if(cfgfile != NULL && cfgfile[strlen(cfgfile) - 1] == '/')
					cfgfile[strlen(cfgfile) - 1] = '\x0';

				/* process the config directory... */
				result = xodtemplate_process_config_dir(cfgfile, options);

				my_free(cfgfile);

				/* if there was an error processing the config file, break out of loop */
				if(result == ERROR)
					break;
				}
			}

		/* free memory and close the file */
		my_free(config_base_dir);
		my_free(input);
		mmap_fclose(thefile);
		}

	if(test_scheduling == TRUE)
		gettimeofday(&tv[1], NULL);
#endif

#ifdef NSCGI
	/* CGIs process only one file - the cached objects file */
	result = xodtemplate_process_config_file(object_cache_file, options);
#endif
	timing_point("Done parsing config files\n");

#ifdef NSCORE

	/* only perform intensive operations if we're not using the precached object file */
	if(use_precached_objects == FALSE) {

		/* resolve objects definitions */
		if(result == OK)
			result = xodtemplate_resolve_objects();
		timing_point("Done resolving objects\n");

		/* these are no longer needed */
		xodtemplate_free_template_skiplists();

		if(test_scheduling == TRUE)
			gettimeofday(&tv[2], NULL);

		/* cleanup some additive inheritance stuff... */
		xodtemplate_clean_additive_strings();
		}
#endif

	/* do the meat and potatoes stuff... */
	host_map = bitmap_create(xodcount.hosts);
	contact_map = bitmap_create(xodcount.contacts);
	if(!host_map || !contact_map) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to create bitmaps for resolving objects\n");
		return ERROR;
		}

	if(result == OK)
		result = xodtemplate_recombobulate_contactgroups();
	if(test_scheduling == TRUE)
		gettimeofday(&tv[3], NULL);
	timing_point("Done recombobulating contactgroups\n");

	if(result == OK)
		result = xodtemplate_recombobulate_hostgroups();
	if(test_scheduling == TRUE)
		gettimeofday(&tv[4], NULL);
	timing_point("Done recombobulating hostgroups\n");

#ifndef NSCGI
	if(use_precached_objects == FALSE) {
		if(result == OK)
			result = xodtemplate_duplicate_services();
		if(test_scheduling == TRUE)
			gettimeofday(&tv[5], NULL);
		timing_point("Created %u services (dupes possible)\n", xodcount.services);
		}
#endif

	/* now we have an accurate service count */
	service_map = bitmap_create(xodcount.services);
	if(!service_map) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Failed to create service map\n");
		return ERROR;
		}

	if(result == OK)
		result = xodtemplate_recombobulate_servicegroups();
	if(test_scheduling == TRUE)
		gettimeofday(&tv[6], NULL);
	timing_point("Done recombobulating servicegroups\n");

#ifndef NSCGI
	if(use_precached_objects == FALSE) {
		if(result == OK)
			result = xodtemplate_duplicate_objects();
		if(test_scheduling == TRUE)
			gettimeofday(&tv[7], NULL);

		/* NOTE: some missing defaults (notification options, etc.) are also applied here */
		if(result == OK)
			result = xodtemplate_inherit_object_properties();
		timing_point("Done propagating inherited object properties\n");
		if(test_scheduling == TRUE)
			gettimeofday(&tv[8], NULL);
		}
#endif

	if(test_scheduling == TRUE)
		gettimeofday(&tv[9], NULL);


	/* register objects */

/* Commented out because functions above (xodtemplate_duplicate_objects
	in this particular case) return an error without printing any error
	messages, so nothing will say what was wrong. */
/*	if(result == OK) */
		result |= xodtemplate_register_objects();
	if(test_scheduling == TRUE)
		gettimeofday(&tv[10], NULL);

	/* cleanup */
	xodtemplate_free_memory();
#ifdef NSCORE
	if(test_scheduling == TRUE) {
		gettimeofday(&tv[11], NULL);

		runtime[0] = (double)((double)(tv[1].tv_sec - tv[0].tv_sec) + (double)((tv[1].tv_usec - tv[0].tv_usec) / 1000.0) / 1000.0);
		if(use_precached_objects == FALSE) {
			runtime[1] = (double)((double)(tv[2].tv_sec - tv[1].tv_sec) + (double)((tv[2].tv_usec - tv[1].tv_usec) / 1000.0) / 1000.0);
			runtime[2] = (double)((double)(tv[3].tv_sec - tv[2].tv_sec) + (double)((tv[3].tv_usec - tv[2].tv_usec) / 1000.0) / 1000.0);
			runtime[3] = (double)((double)(tv[4].tv_sec - tv[3].tv_sec) + (double)((tv[4].tv_usec - tv[3].tv_usec) / 1000.0) / 1000.0);
			runtime[4] = (double)((double)(tv[5].tv_sec - tv[4].tv_sec) + (double)((tv[5].tv_usec - tv[4].tv_usec) / 1000.0) / 1000.0);
			runtime[5] = (double)((double)(tv[6].tv_sec - tv[5].tv_sec) + (double)((tv[6].tv_usec - tv[5].tv_usec) / 1000.0) / 1000.0);
			runtime[6] = (double)((double)(tv[7].tv_sec - tv[6].tv_sec) + (double)((tv[7].tv_usec - tv[6].tv_usec) / 1000.0) / 1000.0);
			runtime[7] = (double)((double)(tv[8].tv_sec - tv[7].tv_sec) + (double)((tv[8].tv_usec - tv[7].tv_usec) / 1000.0) / 1000.0);
			runtime[8] = (double)((double)(tv[10].tv_sec - tv[9].tv_sec) + (double)((tv[10].tv_usec - tv[9].tv_usec) / 1000.0) / 1000.0);
			}
		else {
			runtime[1] = 0.0;
			runtime[2] = 0.0;
			runtime[3] = 0.0;
			runtime[4] = 0.0;
			runtime[5] = 0.0;
			runtime[6] = 0.0;
			runtime[7] = 0.0;
			runtime[8] = (double)((double)(tv[10].tv_sec - tv[1].tv_sec) + (double)((tv[10].tv_usec - tv[1].tv_usec) / 1000.0) / 1000.0);
			}
		runtime[9] = (double)((double)(tv[11].tv_sec - tv[10].tv_sec) + (double)((tv[11].tv_usec - tv[10].tv_usec) / 1000.0) / 1000.0);
		runtime[10] = (double)((double)(tv[11].tv_sec - tv[0].tv_sec) + (double)((tv[11].tv_usec - tv[0].tv_usec) / 1000.0) / 1000.0);

		printf("Timing information on object configuration processing is listed\n");
		printf("below.  You can use this information to see if precaching your\n");
		printf("object configuration would be useful.\n\n");

		printf("Object Config Source: %s\n\n", (use_precached_objects == TRUE) ? "Pre-cached config file" : "Config files (uncached)");

		printf("OBJECT CONFIG PROCESSING TIMES      (* = Potential for precache savings with -u option)\n");
		printf("----------------------------------\n");
		printf("Read:                 %.6lf sec\n", runtime[0]);
		printf("Resolve:              %.6lf sec  *\n", runtime[1]);
		printf("Recomb Contactgroups: %.6lf sec  *\n", runtime[2]);
		printf("Recomb Hostgroups:    %.6lf sec  *\n", runtime[3]);
		printf("Dup Services:         %.6lf sec  *\n", runtime[4]);
		printf("Recomb Servicegroups: %.6lf sec  *\n", runtime[5]);
		printf("Duplicate:            %.6lf sec  *\n", runtime[6]);
		printf("Inherit:              %.6lf sec  *\n", runtime[7]);
		printf("Register:             %.6lf sec\n", runtime[8]);
		printf("Free:                 %.6lf sec\n", runtime[9]);
		printf("                      ============\n");
		printf("TOTAL:                %.6lf sec  ", runtime[10]);
		if(use_precached_objects == FALSE)
			printf("* = %.6lf sec (%.2f%%) estimated savings", (runtime[10] - runtime[9] - runtime[8] - runtime[0]) / 4, ((runtime[10] - runtime[9] - runtime[8] - runtime[0]) / runtime[10]) * 25.0);
		printf("\n");
		printf("\n\n");
		}
#endif

	bitmap_destroy(contact_map);
	bitmap_destroy(host_map);
	bitmap_destroy(service_map);

	return result;
	}


/* process all files in a specific config directory */
int xodtemplate_process_config_dir(char *dirname, int options) {
	char file[MAX_FILENAME_LENGTH];
	DIR *dirp = NULL;
	struct dirent *dirfile = NULL;
	int result = OK;
	register int x = 0;
	struct stat stat_buf;

#ifdef NSCORE
	if(verify_config >= 2)
		printf("Processing object config directory '%s'...\n", dirname);
#endif

	/* open the directory for reading */
	dirp = opendir(dirname);
	if(dirp == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not open config directory '%s' for reading.\n", dirname);
		return ERROR;
		}

	/* process all files in the directory... */
	while((dirfile = readdir(dirp)) != NULL) {

		/* skip hidden files and directories, and current and parent dir */
		if(dirfile->d_name[0] == '.')
			continue;

		/* create /path/to/file */
		snprintf(file, sizeof(file), "%s/%s", dirname, dirfile->d_name);
		file[sizeof(file) - 1] = '\x0';

		/* process this if it's a non-hidden config file... */
		if(stat(file, &stat_buf) == -1) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Could not open config directory member '%s' for reading.\n", file);
			closedir(dirp);
			return ERROR;
			}

		switch(stat_buf.st_mode & S_IFMT) {

			case S_IFREG:
				x = strlen(dirfile->d_name);
				if(x <= 4 || strcmp(dirfile->d_name + (x - 4), ".cfg"))
					break;

				/* process the config file */
				result = xodtemplate_process_config_file(file, options);

				if(result == ERROR) {
					closedir(dirp);
					return ERROR;
					}

				break;

			case S_IFDIR:
				/* recurse into subdirectories... */
				result = xodtemplate_process_config_dir(file, options);

				if(result == ERROR) {
					closedir(dirp);
					return ERROR;
					}

				break;

			default:
				/* everything else we ignore */
				break;
			}
		}

	closedir(dirp);

	return result;
	}


/* process data in a specific config file */
int xodtemplate_process_config_file(char *filename, int options) {
	mmapfile *thefile = NULL;
	char *input = NULL;
	register int in_definition = FALSE;
	register int current_line = 0;
	int result = OK;
	register int x = 0;
	register int y = 0;
	char *ptr = NULL;


#ifdef NSCORE
	if(verify_config >= 2)
		printf("Processing object config file '%s'...\n", filename);
#endif

	/* save config file name */
	xodtemplate_config_files[xodtemplate_current_config_file++] = (char *)strdup(filename);

	/* reallocate memory for config files */
	if(!(xodtemplate_current_config_file % 256)) {
		xodtemplate_config_files = (char **)realloc(xodtemplate_config_files, (xodtemplate_current_config_file + 256) * sizeof(char *));
		if(xodtemplate_config_files == NULL)
			return ERROR;
		}

	/* open the config file for reading */
	if((thefile = mmap_fopen(filename)) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Cannot open config file '%s' for reading: %s\n", filename, strerror(errno));
		return ERROR;
		}

	/* read in all lines from the config file */
	while(1) {

		/* free memory */
		my_free(input);

		/* read the next line */
		if((input = mmap_fgets_multiline(thefile)) == NULL)
			break;

		current_line = thefile->current_line;

		/* grab data before comment delimiter - faster than a strtok() and strncpy()... */
		for(x = 0; input[x] != '\x0'; x++) {
			if(input[x] == ';') {
				if(x == 0)
					break;
				else if(input[x - 1] != '\\')
					break;
				}
			}
		input[x] = '\x0';

		/* strip input */
		strip(input);

		/* skip empty lines */
		if(input[0] == '\x0' || input[0] == '#')
			continue;

		/* this is the start of an object definition */
		if(strstr(input, "define") == input) {

			/* get the type of object we're defining... */
			for(x = 6; input[x] != '\x0'; x++)
				if(input[x] != ' ' && input[x] != '\t')
					break;
			for(y = 0; input[x] != '\x0'; x++) {
				if(input[x] == ' ' || input[x] == '\t' ||  input[x] == '{')
					break;
				else
					input[y++] = input[x];
				}
			input[y] = '\x0';

			/* make sure an object type is specified... */
			if(input[0] == '\x0') {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: No object type specified in file '%s' on line %d.\n", filename, current_line);
				result = ERROR;
				break;
				}

			/* check validity of object type */
			if(strcmp(input, "timeperiod") && strcmp(input, "command") && strcmp(input, "contact") && strcmp(input, "contactgroup") && strcmp(input, "host") && strcmp(input, "hostgroup") && strcmp(input, "servicegroup") && strcmp(input, "service") && strcmp(input, "servicedependency") && strcmp(input, "serviceescalation") && strcmp(input, "hostgroupescalation") && strcmp(input, "hostdependency") && strcmp(input, "hostescalation")) {
				if(strcmp(input, "hostextinfo") && strcmp(input, "serviceextinfo")) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid object definition type '%s' in file '%s' on line %d.\n", input, filename, current_line);
					result = ERROR;
					break;
					}
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: Extinfo objects are deprecated and will be removed in future versions\n");
				}

			/* we're already in an object definition... */
			if(in_definition == TRUE) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Unexpected start of object definition in file '%s' on line %d.  Make sure you close preceding objects before starting a new one.\n", filename, current_line);
				result = ERROR;
				break;
				}

			/* start a new definition */
			if(xodtemplate_begin_object_definition(input, options, xodtemplate_current_config_file, current_line) == ERROR) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add object definition in file '%s' on line %d.\n", filename, current_line);
				result = ERROR;
				break;
				}

			in_definition = TRUE;
			}

		/* we're currently inside an object definition */
		else if(in_definition == TRUE) {

			/* this is the close of an object definition */
			if(!strcmp(input, "}")) {

				in_definition = FALSE;

				/* close out current definition */
				if(xodtemplate_end_object_definition(options) == ERROR) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not complete object definition in file '%s' on line %d. Have you named all your objects?\n", filename, current_line);
					result = ERROR;
					break;
					}
				}

			/* this is a directive inside an object definition */
			else {

				/* add directive to object definition */
				if(xodtemplate_add_object_property(input, options) == ERROR) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add object property in file '%s' on line %d.\n", filename, current_line);
					result = ERROR;
					break;
					}
				}
			}

		/* include another file */
		else if(strstr(input, "include_file=") == input) {

			ptr = strtok(input, "=");
			ptr = strtok(NULL, "\n");

			if(ptr != NULL) {
				result = xodtemplate_process_config_file(ptr, options);
				if(result == ERROR)
					break;
				}
			}

		/* include a directory */
		else if(strstr(input, "include_dir") == input) {

			ptr = strtok(input, "=");
			ptr = strtok(NULL, "\n");

			if(ptr != NULL) {
				result = xodtemplate_process_config_dir(ptr, options);
				if(result == ERROR)
					break;
				}
			}

		/* unexpected token or statement */
		else {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Unexpected token or statement in file '%s' on line %d.\n", filename, current_line);
			result = ERROR;
			break;
			}
		}

	/* free memory and close file */
	my_free(input);
	mmap_fclose(thefile);

	/* whoops - EOF while we were in the middle of an object definition... */
	if(in_definition == TRUE && result == OK) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Unexpected EOF in file '%s' on line %d - check for a missing closing bracket.\n", filename, current_line);
		result = ERROR;
		}

	return result;
	}





/******************************************************************/
/***************** OBJECT DEFINITION FUNCTIONS ********************/
/******************************************************************/

/*
 * all objects start the same way, so we can get rid of quite
 * a lot of code with this struct-offset-insensitive macro
 */
#define xod_begin_def(type) \
	do { \
		new_##type = (xodtemplate_##type *)calloc(1, sizeof(*new_##type)); \
		if (new_##type == NULL) \
			return ERROR; \
		new_##type->register_object=TRUE; \
		new_##type->_config_file=cfgfile; \
		new_##type->_start_line=start_line; \
	\
		/* precached object files are already sorted, so add to tail */ \
		if(presorted_objects==TRUE){ \
			\
			if(xodtemplate_##type##_list==NULL){ \
				xodtemplate_##type##_list=new_##type; \
				xodtemplate_##type##_list_tail=xodtemplate_##type##_list; \
			} else { \
				xodtemplate_##type##_list_tail->next=new_##type; \
				xodtemplate_##type##_list_tail=new_##type; \
			} \
	\
			/* update current object pointer */ \
			xodtemplate_current_object=xodtemplate_##type##_list_tail; \
		} else { \
			/* add new object to head of list in memory */ \
			new_##type->next=xodtemplate_##type##_list; \
			xodtemplate_##type##_list=new_##type; \
	\
			/* update current object pointer */ \
			xodtemplate_current_object=xodtemplate_##type##_list; \
		} \
	} while (0)

/* starts a new object definition */
int xodtemplate_begin_object_definition(char *input, int options, int cfgfile, int start_line) {
	int result = OK;
	xodtemplate_timeperiod *new_timeperiod = NULL;
	xodtemplate_command *new_command = NULL;
	xodtemplate_contactgroup *new_contactgroup = NULL;
	xodtemplate_hostgroup *new_hostgroup = NULL;
	xodtemplate_servicegroup *new_servicegroup = NULL;
	xodtemplate_servicedependency *new_servicedependency = NULL;
	xodtemplate_serviceescalation *new_serviceescalation = NULL;
	xodtemplate_contact *new_contact = NULL;
	xodtemplate_host *new_host = NULL;
	xodtemplate_service *new_service = NULL;
	xodtemplate_hostdependency *new_hostdependency = NULL;
	xodtemplate_hostescalation *new_hostescalation = NULL;
	xodtemplate_hostextinfo *new_hostextinfo = NULL;
	xodtemplate_serviceextinfo *new_serviceextinfo = NULL;

	if(!strcmp(input, "service"))
		xodtemplate_current_object_type = XODTEMPLATE_SERVICE;
	else if(!strcmp(input, "host"))
		xodtemplate_current_object_type = XODTEMPLATE_HOST;
	else if(!strcmp(input, "command"))
		xodtemplate_current_object_type = XODTEMPLATE_COMMAND;
	else if(!strcmp(input, "contact"))
		xodtemplate_current_object_type = XODTEMPLATE_CONTACT;
	else if(!strcmp(input, "contactgroup"))
		xodtemplate_current_object_type = XODTEMPLATE_CONTACTGROUP;
	else if(!strcmp(input, "hostgroup"))
		xodtemplate_current_object_type = XODTEMPLATE_HOSTGROUP;
	else if(!strcmp(input, "servicegroup"))
		xodtemplate_current_object_type = XODTEMPLATE_SERVICEGROUP;
	else if(!strcmp(input, "timeperiod"))
		xodtemplate_current_object_type = XODTEMPLATE_TIMEPERIOD;
	else if(!strcmp(input, "servicedependency"))
		xodtemplate_current_object_type = XODTEMPLATE_SERVICEDEPENDENCY;
	else if(!strcmp(input, "serviceescalation"))
		xodtemplate_current_object_type = XODTEMPLATE_SERVICEESCALATION;
	else if(!strcmp(input, "hostdependency"))
		xodtemplate_current_object_type = XODTEMPLATE_HOSTDEPENDENCY;
	else if(!strcmp(input, "hostescalation"))
		xodtemplate_current_object_type = XODTEMPLATE_HOSTESCALATION;
	else if(!strcmp(input, "hostextinfo"))
		xodtemplate_current_object_type = XODTEMPLATE_HOSTEXTINFO;
	else if(!strcmp(input, "serviceextinfo"))
		xodtemplate_current_object_type = XODTEMPLATE_SERVICEEXTINFO;
	else
		return ERROR;


	/* check to see if we should process this type of object */
	switch(xodtemplate_current_object_type) {
		case XODTEMPLATE_TIMEPERIOD:
			if(!(options & READ_TIMEPERIODS))
				return OK;
			break;
		case XODTEMPLATE_COMMAND:
			if(!(options & READ_COMMANDS))
				return OK;
			break;
		case XODTEMPLATE_CONTACT:
			if(!(options & READ_CONTACTS))
				return OK;
			break;
		case XODTEMPLATE_CONTACTGROUP:
			if(!(options & READ_CONTACTGROUPS))
				return OK;
			break;
		case XODTEMPLATE_HOST:
			if(!(options & READ_HOSTS))
				return OK;
			break;
		case XODTEMPLATE_HOSTGROUP:
			if(!(options & READ_HOSTGROUPS))
				return OK;
			break;
		case XODTEMPLATE_SERVICEGROUP:
			if(!(options & READ_SERVICEGROUPS))
				return OK;
			break;
		case XODTEMPLATE_SERVICE:
			if(!(options & READ_SERVICES))
				return OK;
			break;
		case XODTEMPLATE_SERVICEDEPENDENCY:
			if(!(options & READ_SERVICEDEPENDENCIES))
				return OK;
			break;
		case XODTEMPLATE_SERVICEESCALATION:
			if(!(options & READ_SERVICEESCALATIONS))
				return OK;
			break;
		case XODTEMPLATE_HOSTDEPENDENCY:
			if(!(options & READ_HOSTDEPENDENCIES))
				return OK;
			break;
		case XODTEMPLATE_HOSTESCALATION:
			if(!(options & READ_HOSTESCALATIONS))
				return OK;
			break;
		case XODTEMPLATE_HOSTEXTINFO:
			if(!(options & READ_HOSTEXTINFO))
				return OK;
			break;
		case XODTEMPLATE_SERVICEEXTINFO:
			if(!(options & READ_SERVICEEXTINFO))
				return OK;
			break;
		default:
			return ERROR;
			break;
		}



	/* add a new (blank) object */
	switch(xodtemplate_current_object_type) {

		case XODTEMPLATE_TIMEPERIOD:
			xod_begin_def(timeperiod);
			break;

		case XODTEMPLATE_COMMAND:
			xod_begin_def(command);
			break;

		case XODTEMPLATE_CONTACTGROUP:
			xod_begin_def(contactgroup);
			break;

		case XODTEMPLATE_HOSTGROUP:
			xod_begin_def(hostgroup);
			break;

		case XODTEMPLATE_SERVICEGROUP:
			xod_begin_def(servicegroup);
			break;

		case XODTEMPLATE_SERVICEDEPENDENCY:
			xod_begin_def(servicedependency);
			break;

		case XODTEMPLATE_SERVICEESCALATION:
			xod_begin_def(serviceescalation);
			new_serviceescalation->first_notification = -2;
			new_serviceescalation->last_notification = -2;
			break;

		case XODTEMPLATE_CONTACT:
			xod_begin_def(contact);

			new_contact->minimum_value = 0;
			new_contact->host_notifications_enabled = TRUE;
			new_contact->service_notifications_enabled = TRUE;
			new_contact->can_submit_commands = TRUE;
			new_contact->retain_status_information = TRUE;
			new_contact->retain_nonstatus_information = TRUE;
			break;

		case XODTEMPLATE_HOST:
			xod_begin_def(host);

			new_host->hourly_value = 0;
			new_host->check_interval = 5.0;
			new_host->retry_interval = 1.0;
			new_host->active_checks_enabled = TRUE;
			new_host->passive_checks_enabled = TRUE;
			new_host->obsess = TRUE;
			new_host->max_check_attempts = -2;
			new_host->event_handler_enabled = TRUE;
			new_host->flap_detection_enabled = TRUE;
			new_host->flap_detection_options = OPT_ALL;
			new_host->notifications_enabled = TRUE;
			new_host->notification_interval = 30.0;
			new_host->process_perf_data = TRUE;
			new_host->x_2d = -1;
			new_host->y_2d = -1;
			new_host->retain_status_information = TRUE;
			new_host->retain_nonstatus_information = TRUE;
			break;

		case XODTEMPLATE_SERVICE:
			xod_begin_def(service);

			new_service->hourly_value = 0;
			new_service->initial_state = STATE_OK;
			new_service->max_check_attempts = -2;
			new_service->check_interval = 5.0;
			new_service->retry_interval = 1.0;
			new_service->active_checks_enabled = TRUE;
			new_service->passive_checks_enabled = TRUE;
			new_service->parallelize_check = TRUE;
			new_service->obsess = TRUE;
			new_service->event_handler_enabled = TRUE;
			new_service->flap_detection_enabled = TRUE;
			new_service->flap_detection_options = OPT_ALL;
			new_service->notifications_enabled = TRUE;
			new_service->notification_interval = 30.0;
			new_service->process_perf_data = TRUE;
			new_service->retain_status_information = TRUE;
			new_service->retain_nonstatus_information = TRUE;

			/* true service, so is not from host group */
			new_service->is_from_hostgroup = 0;
			break;

		case XODTEMPLATE_HOSTDEPENDENCY:
			xod_begin_def(hostdependency);
			break;

		case XODTEMPLATE_HOSTESCALATION:
			xod_begin_def(hostescalation);
			new_hostescalation->first_notification = -2;
			new_hostescalation->last_notification = -2;
			break;

		case XODTEMPLATE_HOSTEXTINFO:
			xod_begin_def(hostextinfo);

			new_hostextinfo->x_2d = -1;
			new_hostextinfo->y_2d = -1;
			break;

		case XODTEMPLATE_SERVICEEXTINFO:
			xod_begin_def(serviceextinfo);
			break;

		default:
			return ERROR;
			break;
		}

	return result;
	}
#undef xod_begin_def /* we don't need this anymore */

static const char *xodtemplate_type_name(unsigned int id) {
	static const char *otype_name[] = {
		"NONE", "timeperiod", "command", "contact", "contactgroup",
		"host", "hostgroup", "service", "servicedependency",
		"serviceescalation", "hostescalation", "hostdependency",
		"hostextinfo", "serviceextinfo", "servicegroup"
	};
	if (id > ARRAY_SIZE(otype_name))
		return otype_name[0];
	return otype_name[id];

	}

static void xodtemplate_obsoleted(const char *var, int start_line) {
	logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: %s is obsoleted and no longer has any effect in %s type objects (config file '%s', starting at line %d)\n",
		 var, xodtemplate_type_name(xodtemplate_current_object_type),
		 xodtemplate_config_file_name(xodtemplate_current_config_file), start_line);
	}

/* adds a property to an object definition */
int xodtemplate_add_object_property(char *input, int options) {
	int result = OK;
	char *variable = NULL;
	char *value = NULL;
	char *temp_ptr = NULL;
	char *customvarname = NULL;
	char *customvarvalue = NULL;
	xodtemplate_timeperiod *temp_timeperiod = NULL;
	xodtemplate_command *temp_command = NULL;
	xodtemplate_contactgroup *temp_contactgroup = NULL;
	xodtemplate_hostgroup *temp_hostgroup = NULL;
	xodtemplate_servicegroup *temp_servicegroup = NULL;
	xodtemplate_servicedependency *temp_servicedependency = NULL;
	xodtemplate_serviceescalation *temp_serviceescalation = NULL;
	xodtemplate_contact *temp_contact = NULL;
	xodtemplate_host *temp_host = NULL;
	xodtemplate_service *temp_service = NULL;
	xodtemplate_hostdependency *temp_hostdependency = NULL;
	xodtemplate_hostescalation *temp_hostescalation = NULL;
	xodtemplate_hostextinfo *temp_hostextinfo = NULL;
	xodtemplate_serviceextinfo *temp_serviceextinfo = NULL;
	int x, lth, force_skiplists = FALSE;


	/* should some object definitions be added to skiplists immediately? */
#ifdef NSCORE
	if(use_precached_objects == TRUE)
		force_skiplists = TRUE;
#else
	force_skiplists = TRUE;
#endif

	/* check to see if we should process this type of object */
	switch(xodtemplate_current_object_type) {
		case XODTEMPLATE_TIMEPERIOD:
			if(!(options & READ_TIMEPERIODS))
				return OK;
			break;
		case XODTEMPLATE_COMMAND:
			if(!(options & READ_COMMANDS))
				return OK;
			break;
		case XODTEMPLATE_CONTACT:
			if(!(options & READ_CONTACTS))
				return OK;
			break;
		case XODTEMPLATE_CONTACTGROUP:
			if(!(options & READ_CONTACTGROUPS))
				return OK;
			break;
		case XODTEMPLATE_HOST:
			if(!(options & READ_HOSTS))
				return OK;
			break;
		case XODTEMPLATE_HOSTGROUP:
			if(!(options & READ_HOSTGROUPS))
				return OK;
			break;
		case XODTEMPLATE_SERVICEGROUP:
			if(!(options & READ_SERVICEGROUPS))
				return OK;
			break;
		case XODTEMPLATE_SERVICE:
			if(!(options & READ_SERVICES))
				return OK;
			break;
		case XODTEMPLATE_SERVICEDEPENDENCY:
			if(!(options & READ_SERVICEDEPENDENCIES))
				return OK;
			break;
		case XODTEMPLATE_SERVICEESCALATION:
			if(!(options & READ_SERVICEESCALATIONS))
				return OK;
			break;
		case XODTEMPLATE_HOSTDEPENDENCY:
			if(!(options & READ_HOSTDEPENDENCIES))
				return OK;
			break;
		case XODTEMPLATE_HOSTESCALATION:
			if(!(options & READ_HOSTESCALATIONS))
				return OK;
			break;
		case XODTEMPLATE_HOSTEXTINFO:
			if(!(options & READ_HOSTEXTINFO))
				return OK;
			break;
		case XODTEMPLATE_SERVICEEXTINFO:
			if(!(options & READ_SERVICEEXTINFO))
				return OK;
			break;
		default:
			return ERROR;
			break;
		}

	/* get variable name */
	variable = input;
	lth = strlen(variable);
	/* trim at first whitespace occurrence */
	for(x = 0; variable[x] != '\x0'; x++) {
		if(variable[x] == ' ' || variable[x] == '\t') {
			variable[x] = 0;
			break;
			}
		}

	/* get variable value */
	if (x == lth)
		value = "";
	else
		value = input + x + 1;
	while (*value == ' ' || *value == '\t')
		value++;
	/* now strip trailing spaces */
	strip(value);

	switch(xodtemplate_current_object_type) {

		case XODTEMPLATE_TIMEPERIOD:

			temp_timeperiod = (xodtemplate_timeperiod *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_timeperiod->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_timeperiod->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add timeperiod to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[TIMEPERIOD_SKIPLIST], (void *)temp_timeperiod);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for timeperiod '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_timeperiod->_config_file), temp_timeperiod->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "timeperiod_name")) {
				if((temp_timeperiod->timeperiod_name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add timeperiod to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[TIMEPERIOD_SKIPLIST], (void *)temp_timeperiod);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for timeperiod '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_timeperiod->_config_file), temp_timeperiod->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "alias")) {
				if((temp_timeperiod->alias = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "exclude")) {
				if((temp_timeperiod->exclusions = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "register"))
				temp_timeperiod->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else if(xodtemplate_parse_timeperiod_directive(temp_timeperiod, variable, value) == OK)
				result = OK;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid timeperiod object directive '%s'.\n", variable);
				return ERROR;
				}
			break;



		case XODTEMPLATE_COMMAND:

			temp_command = (xodtemplate_command *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_command->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {
				if((temp_command->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add command to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[COMMAND_SKIPLIST], (void *)temp_command);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for command '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_command->_config_file), temp_command->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "command_name")) {
				if((temp_command->command_name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add command to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[COMMAND_SKIPLIST], (void *)temp_command);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for command '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_command->_config_file), temp_command->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "command_line")) {
				if((temp_command->command_line = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "register"))
				temp_command->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid command object directive '%s'.\n", variable);
				return ERROR;
				}

			break;

		case XODTEMPLATE_CONTACTGROUP:

			temp_contactgroup = (xodtemplate_contactgroup *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_contactgroup->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_contactgroup->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add contactgroup to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[CONTACTGROUP_SKIPLIST], (void *)temp_contactgroup);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for contactgroup '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_contactgroup->_config_file), temp_contactgroup->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "contactgroup_name")) {
				if((temp_contactgroup->contactgroup_name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add contactgroup to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[CONTACTGROUP_SKIPLIST], (void *)temp_contactgroup);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for contactgroup '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_contactgroup->_config_file), temp_contactgroup->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "alias")) {
				if((temp_contactgroup->alias = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "members")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if(temp_contactgroup->members == NULL)
						temp_contactgroup->members = (char *)strdup(value);
					else {
						temp_contactgroup->members = (char *)realloc(temp_contactgroup->members, strlen(temp_contactgroup->members) + strlen(value) + 2);
						if(temp_contactgroup->members != NULL) {
							strcat(temp_contactgroup->members, ",");
							strcat(temp_contactgroup->members, value);
							}
						}
					if(temp_contactgroup->members == NULL)
						result = ERROR;
					}
				temp_contactgroup->have_members = TRUE;
				}
			else if(!strcmp(variable, "contactgroup_members")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if(temp_contactgroup->contactgroup_members == NULL)
						temp_contactgroup->contactgroup_members = (char *)strdup(value);
					else {
						temp_contactgroup->contactgroup_members = (char *)realloc(temp_contactgroup->contactgroup_members, strlen(temp_contactgroup->contactgroup_members) + strlen(value) + 2);
						if(temp_contactgroup->contactgroup_members != NULL) {
							strcat(temp_contactgroup->contactgroup_members, ",");
							strcat(temp_contactgroup->contactgroup_members, value);
							}
						}
					if(temp_contactgroup->contactgroup_members == NULL)
						result = ERROR;
					}
				temp_contactgroup->have_contactgroup_members = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_contactgroup->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid contactgroup object directive '%s'.\n", variable);
				return ERROR;
				}

			break;

		case XODTEMPLATE_HOSTGROUP:

			temp_hostgroup = (xodtemplate_hostgroup *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_hostgroup->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_hostgroup->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add hostgroup to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[HOSTGROUP_SKIPLIST], (void *)temp_hostgroup);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for hostgroup '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_hostgroup->_config_file), temp_hostgroup->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "hostgroup_name")) {
				if((temp_hostgroup->hostgroup_name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add hostgroup to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[HOSTGROUP_SKIPLIST], (void *)temp_hostgroup);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for hostgroup '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_hostgroup->_config_file), temp_hostgroup->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "alias")) {
				if((temp_hostgroup->alias = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "members")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if(temp_hostgroup->members == NULL)
						temp_hostgroup->members = (char *)strdup(value);
					else {
						temp_hostgroup->members = (char *)realloc(temp_hostgroup->members, strlen(temp_hostgroup->members) + strlen(value) + 2);
						if(temp_hostgroup->members != NULL) {
							strcat(temp_hostgroup->members, ",");
							strcat(temp_hostgroup->members, value);
							}
						}
					if(temp_hostgroup->members == NULL)
						result = ERROR;
					}
				temp_hostgroup->have_members = TRUE;
				}
			else if(!strcmp(variable, "hostgroup_members")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if(temp_hostgroup->hostgroup_members == NULL)
						temp_hostgroup->hostgroup_members = (char *)strdup(value);
					else {
						temp_hostgroup->hostgroup_members = (char *)realloc(temp_hostgroup->hostgroup_members, strlen(temp_hostgroup->hostgroup_members) + strlen(value) + 2);
						if(temp_hostgroup->hostgroup_members != NULL) {
							strcat(temp_hostgroup->hostgroup_members, ",");
							strcat(temp_hostgroup->hostgroup_members, value);
							}
						}
					if(temp_hostgroup->hostgroup_members == NULL)
						result = ERROR;
					}
				temp_hostgroup->have_hostgroup_members = TRUE;
				}
			else if(!strcmp(variable, "notes")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostgroup->notes = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostgroup->have_notes = TRUE;
				}
			else if(!strcmp(variable, "notes_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostgroup->notes_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostgroup->have_notes_url = TRUE;
				}
			else if(!strcmp(variable, "action_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostgroup->action_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostgroup->have_action_url = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_hostgroup->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid hostgroup object directive '%s'.\n", variable);
				return ERROR;
				}

			break;


		case XODTEMPLATE_SERVICEGROUP:

			temp_servicegroup = (xodtemplate_servicegroup *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_servicegroup->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_servicegroup->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add servicegroup to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[SERVICEGROUP_SKIPLIST], (void *)temp_servicegroup);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for servicegroup '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_servicegroup->_config_file), temp_servicegroup->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "servicegroup_name")) {
				if((temp_servicegroup->servicegroup_name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add servicegroup to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[SERVICEGROUP_SKIPLIST], (void *)temp_servicegroup);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for servicegroup '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_servicegroup->_config_file), temp_servicegroup->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "alias")) {
				if((temp_servicegroup->alias = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "members")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if(temp_servicegroup->members == NULL)
						temp_servicegroup->members = (char *)strdup(value);
					else {
						temp_servicegroup->members = (char *)realloc(temp_servicegroup->members, strlen(temp_servicegroup->members) + strlen(value) + 2);
						if(temp_servicegroup->members != NULL) {
							strcat(temp_servicegroup->members, ",");
							strcat(temp_servicegroup->members, value);
							}
						}
					if(temp_servicegroup->members == NULL)
						result = ERROR;
					}
				temp_servicegroup->have_members = TRUE;
				}
			else if(!strcmp(variable, "servicegroup_members")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if(temp_servicegroup->servicegroup_members == NULL)
						temp_servicegroup->servicegroup_members = (char *)strdup(value);
					else {
						temp_servicegroup->servicegroup_members = (char *)realloc(temp_servicegroup->servicegroup_members, strlen(temp_servicegroup->servicegroup_members) + strlen(value) + 2);
						if(temp_servicegroup->servicegroup_members != NULL) {
							strcat(temp_servicegroup->servicegroup_members, ",");
							strcat(temp_servicegroup->servicegroup_members, value);
							}
						}
					if(temp_servicegroup->servicegroup_members == NULL)
						result = ERROR;
					}
				temp_servicegroup->have_servicegroup_members = TRUE;
				}
			else if(!strcmp(variable, "notes")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicegroup->notes = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicegroup->have_notes = TRUE;
				}
			else if(!strcmp(variable, "notes_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicegroup->notes_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicegroup->have_notes_url = TRUE;
				}
			else if(!strcmp(variable, "action_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicegroup->action_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicegroup->have_action_url = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_servicegroup->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid servicegroup object directive '%s'.\n", variable);
				return ERROR;
				}

			break;


		case XODTEMPLATE_SERVICEDEPENDENCY:

			temp_servicedependency = (xodtemplate_servicedependency *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_servicedependency->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_servicedependency->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add dependency to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[SERVICEDEPENDENCY_SKIPLIST], (void *)temp_servicedependency);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for service dependency '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_servicedependency->_config_file), temp_servicedependency->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "servicegroup") || !strcmp(variable, "servicegroups") || !strcmp(variable, "servicegroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->servicegroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_servicegroup_name = TRUE;
				}
			else if(!strcmp(variable, "hostgroup") || !strcmp(variable, "hostgroups") || !strcmp(variable, "hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "host") || !strcmp(variable, "host_name") || !strcmp(variable, "master_host") || !strcmp(variable, "master_host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_host_name = TRUE;
				}
			else if(!strcmp(variable, "description") || !strcmp(variable, "service_description") || !strcmp(variable, "master_description") || !strcmp(variable, "master_service_description")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->service_description = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_service_description = TRUE;
				}
			else if(!strcmp(variable, "dependent_servicegroup") || !strcmp(variable, "dependent_servicegroups") || !strcmp(variable, "dependent_servicegroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->dependent_servicegroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_dependent_servicegroup_name = TRUE;
				}
			else if(!strcmp(variable, "dependent_hostgroup") || !strcmp(variable, "dependent_hostgroups") || !strcmp(variable, "dependent_hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->dependent_hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_dependent_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "dependent_host") || !strcmp(variable, "dependent_host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->dependent_host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_dependent_host_name = TRUE;
				}
			else if(!strcmp(variable, "dependent_description") || !strcmp(variable, "dependent_service_description")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->dependent_service_description = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_dependent_service_description = TRUE;
				}
			else if(!strcmp(variable, "dependency_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_servicedependency->dependency_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_servicedependency->have_dependency_period = TRUE;
				}
			else if(!strcmp(variable, "inherits_parent")) {
				temp_servicedependency->inherits_parent = (atoi(value) > 0) ? TRUE : FALSE;
				temp_servicedependency->have_inherits_parent = TRUE;
				}
			else if(!strcmp(variable, "execution_failure_options") || !strcmp(variable, "execution_failure_criteria")) {
				temp_servicedependency->have_execution_failure_options = TRUE;
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "ok"))
						flag_set(temp_servicedependency->execution_failure_options, OPT_OK);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unknown"))
						flag_set(temp_servicedependency->execution_failure_options, OPT_UNKNOWN);
					else if(!strcmp(temp_ptr, "w") || !strcmp(temp_ptr, "warning"))
						flag_set(temp_servicedependency->execution_failure_options, OPT_WARNING);
					else if(!strcmp(temp_ptr, "c") || !strcmp(temp_ptr, "critical"))
						flag_set(temp_servicedependency->execution_failure_options, OPT_CRITICAL);
					else if(!strcmp(temp_ptr, "p") || !strcmp(temp_ptr, "pending"))
						flag_set(temp_servicedependency->execution_failure_options, OPT_PENDING);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_servicedependency->execution_failure_options = OPT_NOTHING;
						temp_servicedependency->have_execution_failure_options = FALSE;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_servicedependency->execution_failure_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid execution dependency option '%s' in servicedependency definition.\n", temp_ptr);
						return ERROR;
						}
					}
				}
			else if(!strcmp(variable, "notification_failure_options") || !strcmp(variable, "notification_failure_criteria")) {
				temp_servicedependency->have_notification_failure_options = TRUE;
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "ok"))
						flag_set(temp_servicedependency->notification_failure_options, OPT_OK);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unknown"))
						flag_set(temp_servicedependency->notification_failure_options, OPT_UNKNOWN);
					else if(!strcmp(temp_ptr, "w") || !strcmp(temp_ptr, "warning"))
						flag_set(temp_servicedependency->notification_failure_options, OPT_WARNING);
					else if(!strcmp(temp_ptr, "c") || !strcmp(temp_ptr, "critical"))
						flag_set(temp_servicedependency->notification_failure_options, OPT_CRITICAL);
					else if(!strcmp(temp_ptr, "p") || !strcmp(temp_ptr, "pending"))
						flag_set(temp_servicedependency->notification_failure_options, OPT_PENDING);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_servicedependency->notification_failure_options = OPT_NOTHING;
						temp_servicedependency->have_notification_failure_options = FALSE;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_servicedependency->notification_failure_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid notification dependency option '%s' in servicedependency definition.\n", temp_ptr);
						return ERROR;
						}
					}
				}
			else if(!strcmp(variable, "register"))
				temp_servicedependency->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid servicedependency object directive '%s'.\n", variable);
				return ERROR;
				}

			break;


		case XODTEMPLATE_SERVICEESCALATION:

			temp_serviceescalation = (xodtemplate_serviceescalation *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_serviceescalation->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_serviceescalation->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add escalation to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[SERVICEESCALATION_SKIPLIST], (void *)temp_serviceescalation);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for service escalation '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_serviceescalation->_config_file), temp_serviceescalation->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "host") || !strcmp(variable, "host_name")) {

				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceescalation->host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceescalation->have_host_name = TRUE;
				}
			else if(!strcmp(variable, "description") || !strcmp(variable, "service_description")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceescalation->service_description = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceescalation->have_service_description = TRUE;
				}
			else if(!strcmp(variable, "servicegroup") || !strcmp(variable, "servicegroups") || !strcmp(variable, "servicegroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceescalation->servicegroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceescalation->have_servicegroup_name = TRUE;
				}
			else if(!strcmp(variable, "hostgroup") || !strcmp(variable, "hostgroups") || !strcmp(variable, "hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceescalation->hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceescalation->have_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "contact_groups")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contact_groups (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_serviceescalation->_config_file), temp_serviceescalation->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceescalation->contact_groups = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceescalation->have_contact_groups = TRUE;
				}
			else if(!strcmp(variable, "contacts")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contacts (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_serviceescalation->_config_file), temp_serviceescalation->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceescalation->contacts = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceescalation->have_contacts = TRUE;
				}
			else if(!strcmp(variable, "escalation_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceescalation->escalation_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceescalation->have_escalation_period = TRUE;
				}
			else if(!strcmp(variable, "first_notification")) {
				temp_serviceescalation->first_notification = atoi(value);
				temp_serviceescalation->have_first_notification = TRUE;
				}
			else if(!strcmp(variable, "last_notification")) {
				temp_serviceescalation->last_notification = atoi(value);
				temp_serviceescalation->have_last_notification = TRUE;
				}
			else if(!strcmp(variable, "notification_interval")) {
				temp_serviceescalation->notification_interval = strtod(value, NULL);
				temp_serviceescalation->have_notification_interval = TRUE;
				}
			else if(!strcmp(variable, "escalation_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "w") || !strcmp(temp_ptr, "warning"))
						flag_set(temp_serviceescalation->escalation_options, OPT_WARNING);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unknown"))
						flag_set(temp_serviceescalation->escalation_options, OPT_UNKNOWN);
					else if(!strcmp(temp_ptr, "c") || !strcmp(temp_ptr, "critical"))
						flag_set(temp_serviceescalation->escalation_options, OPT_CRITICAL);
					else if(!strcmp(temp_ptr, "r") || !strcmp(temp_ptr, "recovery"))
						flag_set(temp_serviceescalation->escalation_options, OPT_RECOVERY);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_serviceescalation->escalation_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_serviceescalation->escalation_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid escalation option '%s' in serviceescalation definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_serviceescalation->have_escalation_options = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_serviceescalation->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid serviceescalation object directive '%s'.\n", variable);
				return ERROR;
				}

			break;


		case XODTEMPLATE_CONTACT:

			temp_contact = (xodtemplate_contact *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_contact->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_contact->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add contact to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[CONTACT_SKIPLIST], (void *)temp_contact);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for contact '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_contact->_config_file), temp_contact->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "contact_name")) {
				if((temp_contact->contact_name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add contact to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[CONTACT_SKIPLIST], (void *)temp_contact);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for contact '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_contact->_config_file), temp_contact->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				temp_contact->id = xodcount.contacts++;
				}
			else if(!strcmp(variable, "alias")) {
				if((temp_contact->alias = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "contact_groups") || !strcmp(variable, "contactgroups")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->contact_groups = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_contact->have_contact_groups = TRUE;
				}
			else if(!strcmp(variable, "email")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->email = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_contact->have_email = TRUE;
				}
			else if(!strcmp(variable, "pager")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->pager = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_contact->have_pager = TRUE;
				}
			else if(strstr(variable, "address") == variable) {
				x = atoi(variable + 7);
				if(x < 1 || x > MAX_XODTEMPLATE_CONTACT_ADDRESSES)
					result = ERROR;
				else if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->address[x - 1] = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				if(result == OK)
					temp_contact->have_address[x - 1] = TRUE;
				}
			else if(!strcmp(variable, "host_notification_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->host_notification_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_contact->have_host_notification_period = TRUE;
				}
			else if(!strcmp(variable, "host_notification_commands")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->host_notification_commands = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_contact->have_host_notification_commands = TRUE;
				}
			else if(!strcmp(variable, "service_notification_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->service_notification_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_contact->have_service_notification_period = TRUE;
				}
			else if(!strcmp(variable, "service_notification_commands")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_contact->service_notification_commands = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_contact->have_service_notification_commands = TRUE;
				}
			else if(!strcmp(variable, "host_notification_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "d") || !strcmp(temp_ptr, "down"))
						flag_set(temp_contact->host_notification_options, OPT_DOWN);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unreachable"))
						flag_set(temp_contact->host_notification_options, OPT_UNREACHABLE);
					else if(!strcmp(temp_ptr, "r") || !strcmp(temp_ptr, "recovery"))
						flag_set(temp_contact->host_notification_options, OPT_RECOVERY);
					else if(!strcmp(temp_ptr, "f") || !strcmp(temp_ptr, "flapping"))
						flag_set(temp_contact->host_notification_options, OPT_FLAPPING);
					else if(!strcmp(temp_ptr, "s") || !strcmp(temp_ptr, "downtime"))
						flag_set(temp_contact->host_notification_options, OPT_DOWNTIME);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_contact->host_notification_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_contact->host_notification_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid host notification option '%s' in contact definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_contact->have_host_notification_options = TRUE;
				}
			else if(!strcmp(variable, "service_notification_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unknown"))
						flag_set(temp_contact->service_notification_options, OPT_UNKNOWN);
					else if(!strcmp(temp_ptr, "w") || !strcmp(temp_ptr, "warning"))
						flag_set(temp_contact->service_notification_options, OPT_WARNING);
					else if(!strcmp(temp_ptr, "c") || !strcmp(temp_ptr, "critical"))
						flag_set(temp_contact->service_notification_options, OPT_CRITICAL);
					else if(!strcmp(temp_ptr, "r") || !strcmp(temp_ptr, "recovery"))
						flag_set(temp_contact->service_notification_options, OPT_RECOVERY);
					else if(!strcmp(temp_ptr, "f") || !strcmp(temp_ptr, "flapping"))
						flag_set(temp_contact->service_notification_options, OPT_FLAPPING);
					else if(!strcmp(temp_ptr, "s") || !strcmp(temp_ptr, "downtime"))
						flag_set(temp_contact->service_notification_options, OPT_DOWNTIME);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_contact->service_notification_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_contact->service_notification_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid service notification option '%s' in contact definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_contact->have_service_notification_options = TRUE;
				}
			else if(!strcmp(variable, "host_notifications_enabled")) {
				temp_contact->host_notifications_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_contact->have_host_notifications_enabled = TRUE;
				}
			else if(!strcmp(variable, "service_notifications_enabled")) {
				temp_contact->service_notifications_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_contact->have_service_notifications_enabled = TRUE;
				}
			else if(!strcmp(variable, "can_submit_commands")) {
				temp_contact->can_submit_commands = (atoi(value) > 0) ? TRUE : FALSE;
				temp_contact->have_can_submit_commands = TRUE;
				}
			else if(!strcmp(variable, "retain_status_information")) {
				temp_contact->retain_status_information = (atoi(value) > 0) ? TRUE : FALSE;
				temp_contact->have_retain_status_information = TRUE;
				}
			else if(!strcmp(variable, "retain_nonstatus_information")) {
				temp_contact->retain_nonstatus_information = (atoi(value) > 0) ? TRUE : FALSE;
				temp_contact->have_retain_nonstatus_information = TRUE;
				}
			else if(!strcmp(variable, "minimum_importance") ||
					!strcmp(variable, "minimum_value")) {
				if(!strcmp(variable, "minimum_value")) {
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: The minimum_value attribute is deprecated and will be removed in future versions. Please use minimum_importance instead.\n");
					}
				temp_contact->minimum_value = strtoul(value, NULL, 10);
				temp_contact->have_minimum_value = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_contact->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else if(variable[0] == '_') {

				/* get the variable name */
				customvarname = (char *)strdup(variable + 1);

				/* make sure we have a variable name */
				if(customvarname == NULL || !strcmp(customvarname, "")) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Null custom variable name.\n");
					my_free(customvarname);
					return ERROR;
					}

				/* get the variable value */
				if(*value && strcmp(value, XODTEMPLATE_NULL))
					customvarvalue = (char *)strdup(value);
				else
					customvarvalue = NULL;

				/* add the custom variable */
				if(xodtemplate_add_custom_variable_to_contact(temp_contact, customvarname, customvarvalue) == NULL) {
					my_free(customvarname);
					my_free(customvarvalue);
					return ERROR;
					}

				/* free memory */
				my_free(customvarname);
				my_free(customvarvalue);
				}
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid contact object directive '%s'.\n", variable);
				return ERROR;
				}

			break;


		case XODTEMPLATE_HOST:

			temp_host = (xodtemplate_host *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_host->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_host->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add host to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[HOST_SKIPLIST], (void *)temp_host);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for host '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_host->_config_file), temp_host->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "host_name")) {
				if((temp_host->host_name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add host to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[HOST_SKIPLIST], (void *)temp_host);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for host '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_host->_config_file), temp_host->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				temp_host->id = xodcount.hosts++;
				}
			else if(!strcmp(variable, "display_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->display_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_display_name = TRUE;
				}
			else if(!strcmp(variable, "alias")) {
				if((temp_host->alias = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "address")) {
				if((temp_host->address = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "parents")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->parents = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_parents = TRUE;
				}
			else if(!strcmp(variable, "host_groups") || !strcmp(variable, "hostgroups")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->host_groups = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_host_groups = TRUE;
				}
			else if(!strcmp(variable, "contact_groups")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contact_groups (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_host->_config_file), temp_host->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->contact_groups = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_contact_groups = TRUE;
				}
			else if(!strcmp(variable, "contacts")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contacts (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_host->_config_file), temp_host->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->contacts = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_contacts = TRUE;
				}
			else if(!strcmp(variable, "notification_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->notification_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_notification_period = TRUE;
				}
			else if(!strcmp(variable, "check_command")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->check_command = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_check_command = TRUE;
				}
			else if(!strcmp(variable, "check_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->check_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_check_period = TRUE;
				}
			else if(!strcmp(variable, "event_handler")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->event_handler = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_event_handler = TRUE;
				}
			else if(!strcmp(variable, "failure_prediction_options")) {
				xodtemplate_obsoleted(variable, temp_host->_start_line);
				}
			else if(!strcmp(variable, "notes")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->notes = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_notes = TRUE;
				}
			else if(!strcmp(variable, "notes_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->notes_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_notes_url = TRUE;
				}
			else if(!strcmp(variable, "action_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->action_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_action_url = TRUE;
				}
			else if(!strcmp(variable, "icon_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->icon_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_icon_image = TRUE;
				}
			else if(!strcmp(variable, "icon_image_alt")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->icon_image_alt = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_icon_image_alt = TRUE;
				}
			else if(!strcmp(variable, "vrml_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->vrml_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_vrml_image = TRUE;
				}
			else if(!strcmp(variable, "gd2_image") || !strcmp(variable, "statusmap_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_host->statusmap_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_host->have_statusmap_image = TRUE;
				}
			else if(!strcmp(variable, "initial_state")) {
				if(!strcmp(value, "o") || !strcmp(value, "up"))
					temp_host->initial_state = 0; /* HOST_UP */
				else if(!strcmp(value, "d") || !strcmp(value, "down"))
					temp_host->initial_state = 1; /* HOST_DOWN */
				else if(!strcmp(value, "u") || !strcmp(value, "unreachable"))
					temp_host->initial_state = 2; /* HOST_UNREACHABLE */
				else {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid initial state '%s' in host definition.\n", value);
					result = ERROR;
					}
				temp_host->have_initial_state = TRUE;
				}
			else if(!strcmp(variable, "check_interval") || !strcmp(variable, "normal_check_interval")) {
				if(!strcmp(variable, "normal_check_interval"))
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: The normal_check_interval attribute is deprecated and will be removed in future versions. Please use check_interval instead.\n");
				temp_host->check_interval = strtod(value, NULL);
				temp_host->have_check_interval = TRUE;
				}
			else if(!strcmp(variable, "retry_interval") || !strcmp(variable, "retry_check_interval")) {
				if(!strcmp(variable, "retry_check_interval"))
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: The normal_retry_interval attribute is deprecated and will be removed in future versions. Please use retry_interval instead.\n");
				temp_host->retry_interval = strtod(value, NULL);
				temp_host->have_retry_interval = TRUE;
				}
			else if(!strcmp(variable, "importance") ||
					!strcmp(variable, "hourly_value")) {
				if(!strcmp(variable, "hourly_value")) {
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: The hourly_value attribute is deprecated and will be removed in future versions. Please use importance instead.\n");
					}
				temp_host->hourly_value = (unsigned int)strtoul(value, NULL, 10);
				temp_host->have_hourly_value = 1;
				}
			else if(!strcmp(variable, "max_check_attempts")) {
				temp_host->max_check_attempts = atoi(value);
				temp_host->have_max_check_attempts = TRUE;
				}
			else if(!strcmp(variable, "checks_enabled") || !strcmp(variable, "active_checks_enabled")) {
				temp_host->active_checks_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_active_checks_enabled = TRUE;
				}
			else if(!strcmp(variable, "passive_checks_enabled")) {
				temp_host->passive_checks_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_passive_checks_enabled = TRUE;
				}
			else if(!strcmp(variable, "event_handler_enabled")) {
				temp_host->event_handler_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_event_handler_enabled = TRUE;
				}
			else if(!strcmp(variable, "check_freshness")) {
				temp_host->check_freshness = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_check_freshness = TRUE;
				}
			else if(!strcmp(variable, "freshness_threshold")) {
				temp_host->freshness_threshold = atoi(value);
				temp_host->have_freshness_threshold = TRUE;
				}
			else if(!strcmp(variable, "low_flap_threshold")) {
				temp_host->low_flap_threshold = strtod(value, NULL);
				temp_host->have_low_flap_threshold = TRUE;
				}
			else if(!strcmp(variable, "high_flap_threshold")) {
				temp_host->high_flap_threshold = strtod(value, NULL);
				temp_host->have_high_flap_threshold = TRUE;
				}
			else if(!strcmp(variable, "flap_detection_enabled")) {
				temp_host->flap_detection_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_flap_detection_enabled = TRUE;
				}
			else if(!strcmp(variable, "flap_detection_options")) {

				/* user is specifying something, so discard defaults... */
				temp_host->flap_detection_options = OPT_NOTHING;

				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "up"))
						flag_set(temp_host->flap_detection_options, OPT_UP);
					else if(!strcmp(temp_ptr, "d") || !strcmp(temp_ptr, "down"))
						flag_set(temp_host->flap_detection_options, OPT_DOWN);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unreachable"))
						flag_set(temp_host->flap_detection_options, OPT_UNREACHABLE);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_host->flap_detection_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_host->flap_detection_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid flap detection option '%s' in host definition.\n", temp_ptr);
						result = ERROR;
						}
					}
				temp_host->have_flap_detection_options = TRUE;
				}
			else if(!strcmp(variable, "notification_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "d") || !strcmp(temp_ptr, "down"))
						flag_set(temp_host->notification_options, OPT_DOWN);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unreachable"))
						flag_set(temp_host->notification_options, OPT_UNREACHABLE);
					else if(!strcmp(temp_ptr, "r") || !strcmp(temp_ptr, "recovery"))
						flag_set(temp_host->notification_options, OPT_RECOVERY);
					else if(!strcmp(temp_ptr, "f") || !strcmp(temp_ptr, "flapping"))
						flag_set(temp_host->notification_options, OPT_FLAPPING);
					else if(!strcmp(temp_ptr, "s") || !strcmp(temp_ptr, "downtime"))
						flag_set(temp_host->notification_options, OPT_DOWNTIME);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_host->notification_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_host->notification_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid notification option '%s' in host definition.\n", temp_ptr);
						result = ERROR;
						}
					}
				temp_host->have_notification_options = TRUE;
				}
			else if(!strcmp(variable, "notifications_enabled")) {
				temp_host->notifications_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_notifications_enabled = TRUE;
				}
			else if(!strcmp(variable, "notification_interval")) {
				temp_host->notification_interval = strtod(value, NULL);
				temp_host->have_notification_interval = TRUE;
				}
			else if(!strcmp(variable, "first_notification_delay")) {
				temp_host->first_notification_delay = strtod(value, NULL);
				temp_host->have_first_notification_delay = TRUE;
				}
			else if(!strcmp(variable, "stalking_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "up"))
						flag_set(temp_host->stalking_options, OPT_UP);
					else if(!strcmp(temp_ptr, "d") || !strcmp(temp_ptr, "down"))
						flag_set(temp_host->stalking_options, OPT_DOWN);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unreachable"))
						flag_set(temp_host->stalking_options, OPT_UNREACHABLE);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_host->stalking_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_host->stalking_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid stalking option '%s' in host definition.\n", temp_ptr);
						result = ERROR;
						}
					}
				temp_host->have_stalking_options = TRUE;
				}
			else if(!strcmp(variable, "process_perf_data")) {
				temp_host->process_perf_data = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_process_perf_data = TRUE;
				}
			else if(!strcmp(variable, "failure_prediction_enabled")) {
				xodtemplate_obsoleted(variable, temp_host->_start_line);
				}
			else if(!strcmp(variable, "2d_coords")) {
				if((temp_ptr = strtok(value, ", ")) == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 2d_coords value '%s' in host definition.\n", temp_ptr);
					return ERROR;
					}
				temp_host->x_2d = atoi(temp_ptr);
				if((temp_ptr = strtok(NULL, ", ")) == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 2d_coords value '%s' in host definition.\n", temp_ptr);
					return ERROR;
					}
				temp_host->y_2d = atoi(temp_ptr);
				temp_host->have_2d_coords = TRUE;
				}
			else if(!strcmp(variable, "3d_coords")) {
				if((temp_ptr = strtok(value, ", ")) == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 3d_coords value '%s' in host definition.\n", temp_ptr);
					return ERROR;
					}
				temp_host->x_3d = strtod(temp_ptr, NULL);
				if((temp_ptr = strtok(NULL, ", ")) == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 3d_coords value '%s' in host definition.\n", temp_ptr);
					return ERROR;
					}
				temp_host->y_3d = strtod(temp_ptr, NULL);
				if((temp_ptr = strtok(NULL, ", ")) == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 3d_coords value '%s' in host definition.\n", temp_ptr);
					return ERROR;
					}
				temp_host->z_3d = strtod(temp_ptr, NULL);
				temp_host->have_3d_coords = TRUE;
				}
			else if(!strcmp(variable, "obsess_over_host") || !strcmp(variable, "obsess")) {
				temp_host->obsess = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_obsess = TRUE;
				}
			else if(!strcmp(variable, "retain_status_information")) {
				temp_host->retain_status_information = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_retain_status_information = TRUE;
				}
			else if(!strcmp(variable, "retain_nonstatus_information")) {
				temp_host->retain_nonstatus_information = (atoi(value) > 0) ? TRUE : FALSE;
				temp_host->have_retain_nonstatus_information = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_host->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else if(variable[0] == '_') {

				/* get the variable name */
				customvarname = (char *)strdup(variable + 1);

				/* make sure we have a variable name */
				if(customvarname == NULL || !strcmp(customvarname, "")) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Null custom variable name.\n");
					my_free(customvarname);
					return ERROR;
					}

				/* get the variable value */
				customvarvalue = NULL;
				if(*value && strcmp(value, XODTEMPLATE_NULL))
					customvarvalue = (char *)strdup(value);

				/* add the custom variable */
				if(xodtemplate_add_custom_variable_to_host(temp_host, customvarname, customvarvalue) == NULL) {
					my_free(customvarname);
					my_free(customvarvalue);
					return ERROR;
					}

				/* free memory */
				my_free(customvarname);
				my_free(customvarvalue);
				}
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid host object directive '%s'.\n", variable);
				return ERROR;
				}

			break;

		case XODTEMPLATE_SERVICE:

			temp_service = (xodtemplate_service *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_service->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_service->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add service to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[SERVICE_SKIPLIST], (void *)temp_service);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for service '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "host") || !strcmp(variable, "hosts") || !strcmp(variable, "host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_host_name = TRUE;

				/* NOTE: services are added to the skiplist in xodtemplate_duplicate_services(), except if daemon is using precached config */
				if(result == OK && force_skiplists == TRUE  && temp_service->host_name != NULL && temp_service->service_description != NULL) {
					/* add service to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[SERVICE_SKIPLIST], (void *)temp_service);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for service '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							temp_service->id = xodcount.services++;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "service_description") || !strcmp(variable, "description")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->service_description = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_service_description = TRUE;

				/* NOTE: services are added to the skiplist in xodtemplate_duplicate_services(), except if daemon is using precached config */
				if(result == OK && force_skiplists == TRUE  && temp_service->host_name != NULL && temp_service->service_description != NULL) {
					/* add service to template skiplist for fast searches */
					result = skiplist_insert(xobject_skiplists[SERVICE_SKIPLIST], (void *)temp_service);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for service '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							temp_service->id = xodcount.services++;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "display_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->display_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_display_name = TRUE;
				}
			else if(!strcmp(variable, "parents")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->parents = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_parents = TRUE;
				}
			else if(!strcmp(variable, "hostgroup") || !strcmp(variable, "hostgroups") || !strcmp(variable, "hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "service_groups") || !strcmp(variable, "servicegroups")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->service_groups = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_service_groups = TRUE;
				}
			else if(!strcmp(variable, "check_command")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if(value[0] == '!') {
						temp_service->have_important_check_command = TRUE;
						temp_ptr = value + 1;
						}
					else
						temp_ptr = value;
					if((temp_service->check_command = (char *)strdup(temp_ptr)) == NULL)
						result = ERROR;
					}
				temp_service->have_check_command = TRUE;
				}
			else if(!strcmp(variable, "check_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->check_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_check_period = TRUE;
				}
			else if(!strcmp(variable, "event_handler")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->event_handler = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_event_handler = TRUE;
				}
			else if(!strcmp(variable, "notification_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->notification_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_notification_period = TRUE;
				}
			else if(!strcmp(variable, "contact_groups")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contact_groups (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->contact_groups = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_contact_groups = TRUE;
				}
			else if(!strcmp(variable, "contacts")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contacts (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->contacts = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_contacts = TRUE;
				}
			else if(!strcmp(variable, "failure_prediction_options")) {
				xodtemplate_obsoleted(variable, temp_service->_start_line);
				}
			else if(!strcmp(variable, "notes")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->notes = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_notes = TRUE;
				}
			else if(!strcmp(variable, "notes_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->notes_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_notes_url = TRUE;
				}
			else if(!strcmp(variable, "action_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->action_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_action_url = TRUE;
				}
			else if(!strcmp(variable, "icon_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->icon_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_icon_image = TRUE;
				}
			else if(!strcmp(variable, "icon_image_alt")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_service->icon_image_alt = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_service->have_icon_image_alt = TRUE;
				}
			else if(!strcmp(variable, "initial_state")) {
				if(!strcmp(value, "o") || !strcmp(value, "ok"))
					temp_service->initial_state = STATE_OK;
				else if(!strcmp(value, "w") || !strcmp(value, "warning"))
					temp_service->initial_state = STATE_WARNING;
				else if(!strcmp(value, "u") || !strcmp(value, "unknown"))
					temp_service->initial_state = STATE_UNKNOWN;
				else if(!strcmp(value, "c") || !strcmp(value, "critical"))
					temp_service->initial_state = STATE_CRITICAL;
				else {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid initial state '%s' in service definition.\n", value);
					result = ERROR;
					}
				temp_service->have_initial_state = TRUE;
				}
			else if(!strcmp(variable, "importance") ||
					!strcmp(variable, "hourly_value")) {
				if(!strcmp(variable, "hourly_value")) {
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: The hourly_value attribute is deprecated and will be removed in future versions. Please use importance instead.\n");
					}
				temp_service->hourly_value = (unsigned int)strtoul(value, NULL, 10);
				temp_service->have_hourly_value = 1;
				}
			else if(!strcmp(variable, "max_check_attempts")) {
				temp_service->max_check_attempts = atoi(value);
				temp_service->have_max_check_attempts = TRUE;
				}
			else if(!strcmp(variable, "check_interval") || !strcmp(variable, "normal_check_interval")) {
				if(!strcmp(variable, "normal_check_interval"))
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: The normal_check_interval attribute is deprecated and will be removed in future versions. Please use check_interval instead.\n");
				temp_service->check_interval = strtod(value, NULL);
				temp_service->have_check_interval = TRUE;
				}
			else if(!strcmp(variable, "retry_interval") || !strcmp(variable, "retry_check_interval")) {
				if(!strcmp(variable, "retry_check_interval"))
					logit(NSLOG_CONFIG_WARNING, TRUE, "WARNING: The normal_retry_interval attribute is deprecated and will be removed in future versions. Please use retry_interval instead.\n");
				temp_service->retry_interval = strtod(value, NULL);
				temp_service->have_retry_interval = TRUE;
				}
			else if(!strcmp(variable, "active_checks_enabled")) {
				temp_service->active_checks_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_active_checks_enabled = TRUE;
				}
			else if(!strcmp(variable, "passive_checks_enabled")) {
				temp_service->passive_checks_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_passive_checks_enabled = TRUE;
				}
			else if(!strcmp(variable, "parallelize_check")) {
				temp_service->parallelize_check = atoi(value);
				temp_service->have_parallelize_check = TRUE;
				}
			else if(!strcmp(variable, "is_volatile")) {
				temp_service->is_volatile = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_is_volatile = TRUE;
				}
			else if(!strcmp(variable, "obsess_over_service") || !strcmp(variable, "obsess")) {
				temp_service->obsess = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_obsess = TRUE;
				}
			else if(!strcmp(variable, "event_handler_enabled")) {
				temp_service->event_handler_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_event_handler_enabled = TRUE;
				}
			else if(!strcmp(variable, "check_freshness")) {
				temp_service->check_freshness = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_check_freshness = TRUE;
				}
			else if(!strcmp(variable, "freshness_threshold")) {
				temp_service->freshness_threshold = atoi(value);
				temp_service->have_freshness_threshold = TRUE;
				}
			else if(!strcmp(variable, "low_flap_threshold")) {
				temp_service->low_flap_threshold = strtod(value, NULL);
				temp_service->have_low_flap_threshold = TRUE;
				}
			else if(!strcmp(variable, "high_flap_threshold")) {
				temp_service->high_flap_threshold = strtod(value, NULL);
				temp_service->have_high_flap_threshold = TRUE;
				}
			else if(!strcmp(variable, "flap_detection_enabled")) {
				temp_service->flap_detection_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_flap_detection_enabled = TRUE;
				}
			else if(!strcmp(variable, "flap_detection_options")) {

				/* user is specifying something, so discard defaults... */
				temp_service->flap_detection_options = OPT_NOTHING;

				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "ok"))
						flag_set(temp_service->flap_detection_options, OPT_OK);
					else if(!strcmp(temp_ptr, "w") || !strcmp(temp_ptr, "warning"))
						flag_set(temp_service->flap_detection_options, OPT_WARNING);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unknown"))
						flag_set(temp_service->flap_detection_options, OPT_UNKNOWN);
					else if(!strcmp(temp_ptr, "c") || !strcmp(temp_ptr, "critical"))
						flag_set(temp_service->flap_detection_options, OPT_CRITICAL);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_service->flap_detection_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_service->flap_detection_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid flap detection option '%s' in service definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_service->have_flap_detection_options = TRUE;
				}
			else if(!strcmp(variable, "notification_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unknown"))
						flag_set(temp_service->notification_options, OPT_UNKNOWN);
					else if(!strcmp(temp_ptr, "w") || !strcmp(temp_ptr, "warning"))
						flag_set(temp_service->notification_options, OPT_WARNING);
					else if(!strcmp(temp_ptr, "c") || !strcmp(temp_ptr, "critical"))
						flag_set(temp_service->notification_options, OPT_CRITICAL);
					else if(!strcmp(temp_ptr, "r") || !strcmp(temp_ptr, "recovery"))
						flag_set(temp_service->notification_options, OPT_RECOVERY);
					else if(!strcmp(temp_ptr, "f") || !strcmp(temp_ptr, "flapping"))
						flag_set(temp_service->notification_options, OPT_FLAPPING);
					else if(!strcmp(temp_ptr, "s") || !strcmp(temp_ptr, "downtime"))
						flag_set(temp_service->notification_options, OPT_DOWNTIME);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_service->notification_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_service->notification_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid notification option '%s' in service definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_service->have_notification_options = TRUE;
				}
			else if(!strcmp(variable, "notifications_enabled")) {
				temp_service->notifications_enabled = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_notifications_enabled = TRUE;
				}
			else if(!strcmp(variable, "notification_interval")) {
				temp_service->notification_interval = strtod(value, NULL);
				temp_service->have_notification_interval = TRUE;
				}
			else if(!strcmp(variable, "first_notification_delay")) {
				temp_service->first_notification_delay = strtod(value, NULL);
				temp_service->have_first_notification_delay = TRUE;
				}
			else if(!strcmp(variable, "stalking_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "ok"))
						flag_set(temp_service->stalking_options, OPT_OK);
					else if(!strcmp(temp_ptr, "w") || !strcmp(temp_ptr, "warning"))
						flag_set(temp_service->stalking_options, OPT_WARNING);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unknown"))
						flag_set(temp_service->stalking_options, OPT_UNKNOWN);
					else if(!strcmp(temp_ptr, "c") || !strcmp(temp_ptr, "critical"))
						flag_set(temp_service->stalking_options, OPT_CRITICAL);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_service->stalking_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_service->stalking_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid stalking option '%s' in service definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_service->have_stalking_options = TRUE;
				}
			else if(!strcmp(variable, "process_perf_data")) {
				temp_service->process_perf_data = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_process_perf_data = TRUE;
				}
			else if(!strcmp(variable, "failure_prediction_enabled")) {
				xodtemplate_obsoleted(variable, temp_service->_start_line);
				}
			else if(!strcmp(variable, "retain_status_information")) {
				temp_service->retain_status_information = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_retain_status_information = TRUE;
				}
			else if(!strcmp(variable, "retain_nonstatus_information")) {
				temp_service->retain_nonstatus_information = (atoi(value) > 0) ? TRUE : FALSE;
				temp_service->have_retain_nonstatus_information = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_service->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else if(variable[0] == '_') {

				/* get the variable name */
				customvarname = (char *)strdup(variable + 1);

				/* make sure we have a variable name */
				if(customvarname == NULL || !strcmp(customvarname, "")) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Null custom variable name.\n");
					my_free(customvarname);
					return ERROR;
					}

				/* get the variable value */
				if(*value && strcmp(value, XODTEMPLATE_NULL))
					customvarvalue = (char *)strdup(value);
				else
					customvarvalue = NULL;

				/* add the custom variable */
				if(xodtemplate_add_custom_variable_to_service(temp_service, customvarname, customvarvalue) == NULL) {
					my_free(customvarname);
					my_free(customvarvalue);
					return ERROR;
					}

				/* free memory */
				my_free(customvarname);
				my_free(customvarvalue);
				}
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid service object directive '%s'.\n", variable);
				return ERROR;
				}

			break;

		case XODTEMPLATE_HOSTDEPENDENCY:

			temp_hostdependency = (xodtemplate_hostdependency *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_hostdependency->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_hostdependency->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add dependency to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[HOSTDEPENDENCY_SKIPLIST], (void *)temp_hostdependency);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for host dependency '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_hostdependency->_config_file), temp_hostdependency->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "hostgroup") || !strcmp(variable, "hostgroups") || !strcmp(variable, "hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostdependency->hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostdependency->have_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "host") || !strcmp(variable, "host_name") || !strcmp(variable, "master_host") || !strcmp(variable, "master_host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostdependency->host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostdependency->have_host_name = TRUE;
				}
			else if(!strcmp(variable, "dependent_hostgroup") || !strcmp(variable, "dependent_hostgroups") || !strcmp(variable, "dependent_hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostdependency->dependent_hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostdependency->have_dependent_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "dependent_host") || !strcmp(variable, "dependent_host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostdependency->dependent_host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostdependency->have_dependent_host_name = TRUE;
				}
			else if(!strcmp(variable, "dependency_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostdependency->dependency_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostdependency->have_dependency_period = TRUE;
				}
			else if(!strcmp(variable, "inherits_parent")) {
				temp_hostdependency->inherits_parent = (atoi(value) > 0) ? TRUE : FALSE;
				temp_hostdependency->have_inherits_parent = TRUE;
				}
			else if(!strcmp(variable, "notification_failure_options") || !strcmp(variable, "notification_failure_criteria")) {
				temp_hostdependency->have_notification_failure_options = TRUE;
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "up"))
						flag_set(temp_hostdependency->notification_failure_options, OPT_UP);
					else if(!strcmp(temp_ptr, "d") || !strcmp(temp_ptr, "down"))
						flag_set(temp_hostdependency->notification_failure_options, OPT_DOWN);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unreachable"))
						flag_set(temp_hostdependency->notification_failure_options, OPT_UNREACHABLE);
					else if(!strcmp(temp_ptr, "p") || !strcmp(temp_ptr, "pending"))
						flag_set(temp_hostdependency->notification_failure_options, OPT_PENDING);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_hostdependency->notification_failure_options = OPT_NOTHING;
						temp_hostdependency->have_notification_failure_options = FALSE;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_hostdependency->notification_failure_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid notification dependency option '%s' in hostdependency definition.\n", temp_ptr);
						return ERROR;
						}
					}
				}
			else if(!strcmp(variable, "execution_failure_options") || !strcmp(variable, "execution_failure_criteria")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "o") || !strcmp(temp_ptr, "up"))
						flag_set(temp_hostdependency->execution_failure_options, OPT_UP);
					else if(!strcmp(temp_ptr, "d") || !strcmp(temp_ptr, "down"))
						flag_set(temp_hostdependency->execution_failure_options, OPT_DOWN);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unreachable"))
						flag_set(temp_hostdependency->execution_failure_options, OPT_UNREACHABLE);
					else if(!strcmp(temp_ptr, "p") || !strcmp(temp_ptr, "pending"))
						flag_set(temp_hostdependency->execution_failure_options, OPT_PENDING);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_hostdependency->execution_failure_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_hostdependency->execution_failure_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid execution dependency option '%s' in hostdependency definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_hostdependency->have_execution_failure_options = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_hostdependency->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid hostdependency object directive '%s'.\n", variable);
				return ERROR;
				}

			break;


		case XODTEMPLATE_HOSTESCALATION:

			temp_hostescalation = (xodtemplate_hostescalation *)xodtemplate_current_object;

			if(!strcmp(variable, "use")) {
				if((temp_hostescalation->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_hostescalation->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add escalation to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[HOSTESCALATION_SKIPLIST], (void *)temp_hostescalation);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for host escalation '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_hostescalation->_config_file), temp_hostescalation->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "hostgroup") || !strcmp(variable, "hostgroups") || !strcmp(variable, "hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostescalation->hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostescalation->have_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "host") || !strcmp(variable, "host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostescalation->host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostescalation->have_host_name = TRUE;
				}
			else if(!strcmp(variable, "contact_groups")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contact_groups (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_hostescalation->_config_file), temp_hostescalation->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostescalation->contact_groups = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostescalation->have_contact_groups = TRUE;
				}
			else if(!strcmp(variable, "contacts")) {
				if (*value == '\0') {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Error: Empty value for contacts (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_hostescalation->_config_file), temp_hostescalation->_start_line);
					result = ERROR;
					break;
				}
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostescalation->contacts = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostescalation->have_contacts = TRUE;
				}
			else if(!strcmp(variable, "escalation_period")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostescalation->escalation_period = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostescalation->have_escalation_period = TRUE;
				}
			else if(!strcmp(variable, "first_notification")) {
				temp_hostescalation->first_notification = atoi(value);
				temp_hostescalation->have_first_notification = TRUE;
				}
			else if(!strcmp(variable, "last_notification")) {
				temp_hostescalation->last_notification = atoi(value);
				temp_hostescalation->have_last_notification = TRUE;
				}
			else if(!strcmp(variable, "notification_interval")) {
				temp_hostescalation->notification_interval = strtod(value, NULL);
				temp_hostescalation->have_notification_interval = TRUE;
				}
			else if(!strcmp(variable, "escalation_options")) {
				for(temp_ptr = strtok(value, ", "); temp_ptr; temp_ptr = strtok(NULL, ", ")) {
					if(!strcmp(temp_ptr, "d") || !strcmp(temp_ptr, "down"))
						flag_set(temp_hostescalation->escalation_options, OPT_DOWN);
					else if(!strcmp(temp_ptr, "u") || !strcmp(temp_ptr, "unreachable"))
						flag_set(temp_hostescalation->escalation_options, OPT_UNREACHABLE);
					else if(!strcmp(temp_ptr, "r") || !strcmp(temp_ptr, "recovery"))
						flag_set(temp_hostescalation->escalation_options, OPT_RECOVERY);
					else if(!strcmp(temp_ptr, "n") || !strcmp(temp_ptr, "none")) {
						temp_hostescalation->escalation_options = OPT_NOTHING;
						}
					else if(!strcmp(temp_ptr, "a") || !strcmp(temp_ptr, "all")) {
						temp_hostescalation->escalation_options = OPT_ALL;
						}
					else {
						logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid escalation option '%s' in hostescalation definition.\n", temp_ptr);
						return ERROR;
						}
					}
				temp_hostescalation->have_escalation_options = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_hostescalation->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid hostescalation object directive '%s'.\n", variable);
				return ERROR;
				}

			break;

		case XODTEMPLATE_HOSTEXTINFO:

			temp_hostextinfo = xodtemplate_hostextinfo_list;

			if(!strcmp(variable, "use")) {
				if((temp_hostextinfo->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_hostextinfo->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[HOSTEXTINFO_SKIPLIST], (void *)temp_hostextinfo);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for extended host info '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_hostextinfo->_config_file), temp_hostextinfo->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_host_name = TRUE;
				}
			else if(!strcmp(variable, "hostgroup") || !strcmp(variable, "hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "notes")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->notes = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_notes = TRUE;
				}
			else if(!strcmp(variable, "notes_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->notes_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_notes_url = TRUE;
				}
			else if(!strcmp(variable, "action_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->action_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_action_url = TRUE;
				}
			else if(!strcmp(variable, "icon_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->icon_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_icon_image = TRUE;
				}
			else if(!strcmp(variable, "icon_image_alt")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->icon_image_alt = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_icon_image_alt = TRUE;
				}
			else if(!strcmp(variable, "vrml_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->vrml_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_vrml_image = TRUE;
				}
			else if(!strcmp(variable, "gd2_image") || !strcmp(variable, "statusmap_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_hostextinfo->statusmap_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_hostextinfo->have_statusmap_image = TRUE;
				}
			else if(!strcmp(variable, "2d_coords")) {
				temp_ptr = strtok(value, ", ");
				if(temp_ptr == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 2d_coords value '%s' in extended host info definition.\n", temp_ptr);
					return ERROR;
					}
				temp_hostextinfo->x_2d = atoi(temp_ptr);
				temp_ptr = strtok(NULL, ", ");
				if(temp_ptr == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 2d_coords value '%s' in extended host info definition.\n", temp_ptr);
					return ERROR;
					}
				temp_hostextinfo->y_2d = atoi(temp_ptr);
				temp_hostextinfo->have_2d_coords = TRUE;
				}
			else if(!strcmp(variable, "3d_coords")) {
				temp_ptr = strtok(value, ", ");
				if(temp_ptr == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 3d_coords value '%s' in extended host info definition.\n", temp_ptr);
					return ERROR;
					}
				temp_hostextinfo->x_3d = strtod(temp_ptr, NULL);
				temp_ptr = strtok(NULL, ", ");
				if(temp_ptr == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 3d_coords value '%s' in extended host info definition.\n", temp_ptr);
					return ERROR;
					}
				temp_hostextinfo->y_3d = strtod(temp_ptr, NULL);
				temp_ptr = strtok(NULL, ", ");
				if(temp_ptr == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid 3d_coords value '%s' in extended host info definition.\n", temp_ptr);
					return ERROR;
					}
				temp_hostextinfo->z_3d = strtod(temp_ptr, NULL);
				temp_hostextinfo->have_3d_coords = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_hostextinfo->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid hostextinfo object directive '%s'.\n", variable);
				return ERROR;
				}

			break;

		case XODTEMPLATE_SERVICEEXTINFO:

			temp_serviceextinfo = xodtemplate_serviceextinfo_list;

			if(!strcmp(variable, "use")) {
				if((temp_serviceextinfo->template = (char *)strdup(value)) == NULL)
					result = ERROR;
				}
			else if(!strcmp(variable, "name")) {

				if((temp_serviceextinfo->name = (char *)strdup(value)) == NULL)
					result = ERROR;

				if(result == OK) {
					/* add to template skiplist for fast searches */
					result = skiplist_insert(xobject_template_skiplists[SERVICEEXTINFO_SKIPLIST], (void *)temp_serviceextinfo);
					switch(result) {
						case SKIPLIST_ERROR_DUPLICATE:
							logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for extended service info '%s' (config file '%s', starting on line %d)\n", value, xodtemplate_config_file_name(temp_serviceextinfo->_config_file), temp_serviceextinfo->_start_line);
							result = ERROR;
							break;
						case SKIPLIST_OK:
							result = OK;
							break;
						default:
							result = ERROR;
							break;
						}
					}
				}
			else if(!strcmp(variable, "host_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->host_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_host_name = TRUE;
				}
			else if(!strcmp(variable, "hostgroup") || !strcmp(variable, "hostgroup_name")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->hostgroup_name = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_hostgroup_name = TRUE;
				}
			else if(!strcmp(variable, "service_description")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->service_description = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_service_description = TRUE;
				}
			else if(!strcmp(variable, "notes")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->notes = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_notes = TRUE;
				}
			else if(!strcmp(variable, "notes_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->notes_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_notes_url = TRUE;
				}
			else if(!strcmp(variable, "action_url")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->action_url = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_action_url = TRUE;
				}
			else if(!strcmp(variable, "icon_image")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->icon_image = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_icon_image = TRUE;
				}
			else if(!strcmp(variable, "icon_image_alt")) {
				if(strcmp(value, XODTEMPLATE_NULL)) {
					if((temp_serviceextinfo->icon_image_alt = (char *)strdup(value)) == NULL)
						result = ERROR;
					}
				temp_serviceextinfo->have_icon_image_alt = TRUE;
				}
			else if(!strcmp(variable, "register"))
				temp_serviceextinfo->register_object = (atoi(value) > 0) ? TRUE : FALSE;
			else {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid serviceextinfo object directive '%s'.\n", variable);
				return ERROR;
				}

			break;

		default:
			return ERROR;
			break;
		}

	return result;
	}



#define xod_check_complete(otype) \
	do { \
		xodtemplate_##otype *o = (xodtemplate_##otype *)xodtemplate_current_object; \
		if (o->register_object && !o->otype##_name && !o->name) { \
			return ERROR; \
		} \
	} while(0)
/* completes an object definition */
int xodtemplate_end_object_definition(int options) {
	int result = OK;

	switch(xodtemplate_current_object_type) {
	case XODTEMPLATE_HOSTESCALATION:
		xodcount.hostescalations += !!use_precached_objects;
		break;
	case XODTEMPLATE_SERVICEESCALATION:
		xodcount.serviceescalations += !!use_precached_objects;
		break;
	case XODTEMPLATE_TIMEPERIOD:
		xod_check_complete(timeperiod);
		break;
	case XODTEMPLATE_COMMAND:
		xod_check_complete(command);
		break;
	case XODTEMPLATE_CONTACT:
		xod_check_complete(contact);
		break;
	case XODTEMPLATE_CONTACTGROUP:
		xod_check_complete(contactgroup);
		break;
	case XODTEMPLATE_HOST:
		xod_check_complete(host);
		break;
	case XODTEMPLATE_HOSTGROUP:
		xod_check_complete(hostgroup);
		break;
	case XODTEMPLATE_SERVICEGROUP:
		xod_check_complete(servicegroup);
		break;
	}


	xodtemplate_current_object = NULL;
	xodtemplate_current_object_type = XODTEMPLATE_NONE;

	return result;
	}



/* adds a custom variable to a host */
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_host(xodtemplate_host *hst, char *varname, char *varvalue) {

	return xodtemplate_add_custom_variable_to_object(&hst->custom_variables, varname, varvalue);
	}



/* adds a custom variable to a service */
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_service(xodtemplate_service *svc, char *varname, char *varvalue) {

	return xodtemplate_add_custom_variable_to_object(&svc->custom_variables, varname, varvalue);
	}



/* adds a custom variable to a contact */
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_contact(xodtemplate_contact *cntct, char *varname, char *varvalue) {

	return xodtemplate_add_custom_variable_to_object(&cntct->custom_variables, varname, varvalue);
	}



/* adds a custom variable to an object */
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_object(xodtemplate_customvariablesmember **object_ptr, char *varname, char *varvalue) {
	xodtemplate_customvariablesmember *new_customvariablesmember = NULL;
	register int x = 0;

	/* make sure we have the data we need */
	if(object_ptr == NULL)
		return NULL;

	if(varname == NULL || !strcmp(varname, ""))
		return NULL;

	/* allocate memory for a new member */
	if((new_customvariablesmember = malloc(sizeof(xodtemplate_customvariablesmember))) == NULL)
		return NULL;
	if((new_customvariablesmember->variable_name = (char *)strdup(varname)) == NULL) {
		my_free(new_customvariablesmember);
		return NULL;
		}
	if(varvalue && *varvalue) {
		if((new_customvariablesmember->variable_value = (char *)strdup(varvalue)) == NULL) {
			my_free(new_customvariablesmember->variable_name);
			my_free(new_customvariablesmember);
			return NULL;
			}
		}
	else
		new_customvariablesmember->variable_value = NULL;

	/* convert varname to all uppercase (saves CPU time during macro functions) */
	for(x = 0; new_customvariablesmember->variable_name[x] != '\x0'; x++)
		new_customvariablesmember->variable_name[x] = toupper(new_customvariablesmember->variable_name[x]);

	/* add the new member to the head of the member list */
	new_customvariablesmember->next = *object_ptr;
	*object_ptr = new_customvariablesmember;

	return new_customvariablesmember;
	}



/* parses a timeperiod directive... :-) */
int xodtemplate_parse_timeperiod_directive(xodtemplate_timeperiod *tperiod, char *var, char *val) {
	char *input = NULL;
	char temp_buffer[5][MAX_INPUT_BUFFER] = {"", "", "", "", ""};
	int items = 0;
	int result = OK;

	int syear = 0;
	int smon = 0;
	int smday = 0;
	int swday = 0;
	int swday_offset = 0;
	int eyear = 0;
	int emon = 0;
	int emday = 0;
	int ewday = 0;
	int ewday_offset = 0;
	int skip_interval = 0;

	/* make sure we've got the reqs */
	if(tperiod == NULL || var == NULL || val == NULL)
		return ERROR;

	/* we'll need the full (unsplit) input later */
	if((input = (char *)malloc(strlen(var) + strlen(val) + 2)) == NULL)
		return ERROR;
	strcpy(input, var);
	strcat(input, " ");
	strcat(input, val);

	if(0)
		return OK;

	/* calendar dates */
	else if((items = sscanf(input, "%4d-%2d-%2d - %4d-%2d-%2d / %d %[0-9:, -]", &syear, &smon, &smday, &eyear, &emon, &emday, &skip_interval, temp_buffer[0])) == 8) {
		/* add timerange exception */
		if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_CALENDAR_DATE, syear, smon - 1, smday, 0, 0, eyear, emon - 1, emday, 0, 0, skip_interval, temp_buffer[0]) == NULL)
			result = ERROR;
		}

	else if((items = sscanf(input, "%4d-%2d-%2d / %d %[0-9:, -]", &syear, &smon, &smday, &skip_interval, temp_buffer[0])) == 5) {
		eyear = syear;
		emon = smon;
		emday = smday;
		/* add timerange exception */
		if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_CALENDAR_DATE, syear, smon - 1, smday, 0, 0, eyear, emon - 1, emday, 0, 0, skip_interval, temp_buffer[0]) == NULL)
			result = ERROR;
		}

	else if((items = sscanf(input, "%4d-%2d-%2d - %4d-%2d-%2d %[0-9:, -]", &syear, &smon, &smday, &eyear, &emon, &emday, temp_buffer[0])) == 7) {
		/* add timerange exception */
		if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_CALENDAR_DATE, syear, smon - 1, smday, 0, 0, eyear, emon - 1, emday, 0, 0, 0, temp_buffer[0]) == NULL)
			result = ERROR;
		}

	else if((items = sscanf(input, "%4d-%2d-%2d %[0-9:, -]", &syear, &smon, &smday, temp_buffer[0])) == 4) {
		eyear = syear;
		emon = smon;
		emday = smday;
		/* add timerange exception */
		if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_CALENDAR_DATE, syear, smon - 1, smday, 0, 0, eyear, emon - 1, emday, 0, 0, 0, temp_buffer[0]) == NULL)
			result = ERROR;
		}

	/* other types... */
	else if((items = sscanf(input, "%[a-z] %d %[a-z] - %[a-z] %d %[a-z] / %d %[0-9:, -]", temp_buffer[0], &swday_offset, temp_buffer[1], temp_buffer[2], &ewday_offset, temp_buffer[3], &skip_interval, temp_buffer[4])) == 8) {
		/* wednesday 1 january - thursday 2 july / 3 */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK && (result = xodtemplate_get_month_from_string(temp_buffer[1], &smon)) == OK && (result = xodtemplate_get_weekday_from_string(temp_buffer[2], &ewday)) == OK && (result = xodtemplate_get_month_from_string(temp_buffer[3], &emon)) == OK) {
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_WEEK_DAY, 0, smon, 0, swday, swday_offset, 0, emon, 0, ewday, ewday_offset, skip_interval, temp_buffer[4]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %d - %[a-z] %d / %d %[0-9:, -]", temp_buffer[0], &smday, temp_buffer[1], &emday, &skip_interval, temp_buffer[2])) == 6) {
		/* february 1 - march 15 / 3 */
		/* monday 2 - thursday 3 / 2 */
		/* day 4 - day 6 / 2 */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK && (result = xodtemplate_get_weekday_from_string(temp_buffer[1], &ewday)) == OK) {
			/* monday 2 - thursday 3 / 2 */
			swday_offset = smday;
			ewday_offset = emday;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_WEEK_DAY, 0, 0, 0, swday, swday_offset, 0, 0, 0, ewday, ewday_offset, skip_interval, temp_buffer[2]) == NULL)
				result = ERROR;
			}
		else if((result = xodtemplate_get_month_from_string(temp_buffer[0], &smon)) == OK && (result = xodtemplate_get_month_from_string(temp_buffer[1], &emon)) == OK) {
			/* february 1 - march 15 / 3 */
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DATE, 0, smon, smday, 0, 0, 0, emon, emday, 0, 0, skip_interval, temp_buffer[2]) == NULL)
				result = ERROR;
			}
		else if(!strcmp(temp_buffer[0], "day")  && !strcmp(temp_buffer[1], "day")) {
			/* day 4 - 6 / 2 */
			/* add timeperiod exception */
			result = OK;
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DAY, 0, 0, smday, 0, 0, 0, 0, emday, 0, 0, skip_interval, temp_buffer[2]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %d - %d / %d %[0-9:, -]", temp_buffer[0], &smday, &emday, &skip_interval, temp_buffer[1])) == 5) {
		/* february 1 - 15 / 3 */
		/* monday 2 - 3 / 2 */
		/* day 1 - 25 / 4 */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK) {
			/* thursday 2 - 4 */
			swday_offset = smday;
			ewday = swday;
			ewday_offset = emday;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_WEEK_DAY, 0, 0, 0, swday, swday_offset, 0, 0, 0, ewday, ewday_offset, skip_interval, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		else if((result = xodtemplate_get_month_from_string(temp_buffer[0], &smon)) == OK) {
			/* february 3 - 5 */
			emon = smon;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DATE, 0, smon, smday, 0, 0, 0, emon, emday, 0, 0, skip_interval, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		else if(!strcmp(temp_buffer[0], "day")) {
			/* day 1 - 4 */
			/* add timeperiod exception */
			result = OK;
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DAY, 0, 0, smday, 0, 0, 0, 0, emday, 0, 0, skip_interval, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %d %[a-z] - %[a-z] %d %[a-z] %[0-9:, -]", temp_buffer[0], &swday_offset, temp_buffer[1], temp_buffer[2], &ewday_offset, temp_buffer[3], temp_buffer[4])) == 7) {
		/* wednesday 1 january - thursday 2 july */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK && (result = xodtemplate_get_month_from_string(temp_buffer[1], &smon)) == OK && (result = xodtemplate_get_weekday_from_string(temp_buffer[2], &ewday)) == OK && (result = xodtemplate_get_month_from_string(temp_buffer[3], &emon)) == OK) {
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_WEEK_DAY, 0, smon, 0, swday, swday_offset, 0, emon, 0, ewday, ewday_offset, 0, temp_buffer[4]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %d - %d %[0-9:, -]", temp_buffer[0], &smday, &emday, temp_buffer[1])) == 4) {
		/* february 3 - 5 */
		/* thursday 2 - 4 */
		/* day 1 - 4 */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK) {
			/* thursday 2 - 4 */
			swday_offset = smday;
			ewday = swday;
			ewday_offset = emday;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_WEEK_DAY, 0, 0, 0, swday, swday_offset, 0, 0, 0, ewday, ewday_offset, 0, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		else if((result = xodtemplate_get_month_from_string(temp_buffer[0], &smon)) == OK) {
			/* february 3 - 5 */
			emon = smon;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DATE, 0, smon, smday, 0, 0, 0, emon, emday, 0, 0, 0, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		else if(!strcmp(temp_buffer[0], "day")) {
			/* day 1 - 4 */
			/* add timeperiod exception */
			result = OK;
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DAY, 0, 0, smday, 0, 0, 0, 0, emday, 0, 0, 0, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %d - %[a-z] %d %[0-9:, -]", temp_buffer[0], &smday, temp_buffer[1], &emday, temp_buffer[2])) == 5) {
		/* february 1 - march 15 */
		/* monday 2 - thursday 3 */
		/* day 1 - day 5 */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK && (result = xodtemplate_get_weekday_from_string(temp_buffer[1], &ewday)) == OK) {
			/* monday 2 - thursday 3 */
			swday_offset = smday;
			ewday_offset = emday;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_WEEK_DAY, 0, 0, 0, swday, swday_offset, 0, 0, 0, ewday, ewday_offset, 0, temp_buffer[2]) == NULL)
				result = ERROR;
			}
		else if((result = xodtemplate_get_month_from_string(temp_buffer[0], &smon)) == OK && (result = xodtemplate_get_month_from_string(temp_buffer[1], &emon)) == OK) {
			/* february 1 - march 15 */
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DATE, 0, smon, smday, 0, 0, 0, emon, emday, 0, 0, 0, temp_buffer[2]) == NULL)
				result = ERROR;
			}
		else if(!strcmp(temp_buffer[0], "day")  && !strcmp(temp_buffer[1], "day")) {
			/* day 1 - day 5 */
			/* add timeperiod exception */
			result = OK;
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DAY, 0, 0, smday, 0, 0, 0, 0, emday, 0, 0, 0, temp_buffer[2]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %d%*[ \t]%[0-9:, -]", temp_buffer[0], &smday, temp_buffer[1])) == 3) {
		/* february 3 */
		/* thursday 2 */
		/* day 1 */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK) {
			/* thursday 2 */
			swday_offset = smday;
			ewday = swday;
			ewday_offset = swday_offset;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_WEEK_DAY, 0, 0, 0, swday, swday_offset, 0, 0, 0, ewday, ewday_offset, 0, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		else if((result = xodtemplate_get_month_from_string(temp_buffer[0], &smon)) == OK) {
			/* february 3 */
			emon = smon;
			emday = smday;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DATE, 0, smon, smday, 0, 0, 0, emon, emday, 0, 0, 0, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		else if(!strcmp(temp_buffer[0], "day")) {
			/* day 1 */
			emday = smday;
			/* add timeperiod exception */
			result = OK;
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_DAY, 0, 0, smday, 0, 0, 0, 0, emday, 0, 0, 0, temp_buffer[1]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %d %[a-z] %[0-9:, -]", temp_buffer[0], &swday_offset, temp_buffer[1], temp_buffer[2])) == 4) {
		/* thursday 3 february */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK && (result = xodtemplate_get_month_from_string(temp_buffer[1], &smon)) == OK) {
			emon = smon;
			ewday = swday;
			ewday_offset = swday_offset;
			/* add timeperiod exception */
			if(xodtemplate_add_exception_to_timeperiod(tperiod, DATERANGE_MONTH_WEEK_DAY, 0, smon, 0, swday, swday_offset, 0, emon, 0, ewday, ewday_offset, 0, temp_buffer[2]) == NULL)
				result = ERROR;
			}
		}

	else if((items = sscanf(input, "%[a-z] %[0-9:, -]", temp_buffer[0], temp_buffer[1])) == 2) {
		/* monday */
		if((result = xodtemplate_get_weekday_from_string(temp_buffer[0], &swday)) == OK) {
			/* add normal weekday timerange */
			if((tperiod->timeranges[swday] = (char *)strdup(temp_buffer[1])) == NULL)
				result = ERROR;
			}
		}

	else
		result = ERROR;

#ifdef NSCORE
	if(result == ERROR) {
		printf("Error: Could not parse timeperiod directive '%s'!\n", input);
		}
#endif

	/* free memory */
	my_free(input);

	return result;
	}



/* add a new exception to a timeperiod */
xodtemplate_daterange *xodtemplate_add_exception_to_timeperiod(xodtemplate_timeperiod *period, int type, int syear, int smon, int smday, int swday, int swday_offset, int eyear, int emon, int emday, int ewday, int ewday_offset, int skip_interval, char *timeranges) {
	xodtemplate_daterange *new_daterange = NULL;

	/* make sure we have the data we need */
	if(period == NULL || timeranges == NULL)
		return NULL;

	/* allocate memory for the date range range */
	if((new_daterange = malloc(sizeof(xodtemplate_daterange))) == NULL)
		return NULL;

	new_daterange->next = NULL;

	new_daterange->type = type;
	new_daterange->syear = syear;
	new_daterange->smon = smon;
	new_daterange->smday = smday;
	new_daterange->swday = swday;
	new_daterange->swday_offset = swday_offset;
	new_daterange->eyear = eyear;
	new_daterange->emon = emon;
	new_daterange->emday = emday;
	new_daterange->ewday = ewday;
	new_daterange->ewday_offset = ewday_offset;
	new_daterange->skip_interval = skip_interval;
	new_daterange->timeranges = (char *)strdup(timeranges);

	/* add the new date range to the head of the range list for this exception type */
	new_daterange->next = period->exceptions[type];
	period->exceptions[type] = new_daterange;

	return new_daterange;
	}



int xodtemplate_get_month_from_string(char *str, int *month) {
	const char *months[12] = {"january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december"};
	int x = 0;

	if(str == NULL || month == NULL)
		return ERROR;

	for(x = 0; x < 12; x++) {
		if(!strcmp(str, months[x])) {
			*month = x;
			return OK;
			}
		}

	return ERROR;
	}




int xodtemplate_get_weekday_from_string(char *str, int *weekday) {
	const char *days[7] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
	int x = 0;

	if(str == NULL || weekday == NULL)
		return ERROR;

	for(x = 0; x < 7; x++) {
		if(!strcmp(str, days[x])) {
			*weekday = x;
			return OK;
			}
		}

	return ERROR;
	}



/******************************************************************/
/***************** OBJECT DUPLICATION FUNCTIONS *******************/
/******************************************************************/

#ifdef NSCORE

/* duplicates service definitions */
int xodtemplate_duplicate_services(void) {
	int result = OK;
	xodtemplate_service *temp_service = NULL;

	xodcount.services = 0;
	/****** DUPLICATE SERVICE DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {
		objectlist *hlist = NULL, *list = NULL, *glist = NULL, *next;
		xodtemplate_hostgroup fake_hg;

		/* clear for each round */
		bitmap_clear(host_map);

		/* skip services that shouldn't be registered */
		if(temp_service->register_object == FALSE)
			continue;

		/* bail out on service definitions without enough data */
		if((temp_service->hostgroup_name == NULL && temp_service->host_name == NULL) || temp_service->service_description == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service has no hosts and/or service_description (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
			return ERROR;
			}

		if(temp_service->hostgroup_name != NULL) {
			if(xodtemplate_expand_hostgroups(&glist, host_map, temp_service->hostgroup_name, temp_service->_config_file, temp_service->_start_line) == ERROR) {
				return ERROR;
				}
			/* no longer needed */
			my_free(temp_service->hostgroup_name);

			/* empty result is only bad if allow_empty_hostgroup_assignment is off */
			if(!glist && !bitmap_count_set_bits(host_map)) {
				if(!allow_empty_hostgroup_assignment) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand hostgroups and/or hosts specified in service (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
					return ERROR;
					}
				else if (allow_empty_hostgroup_assignment == 2) {
					logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Could not expand hostgroups and/or hosts specified in service (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
					}
				}
			}

		/* now find direct hosts */
		if(temp_service->host_name) {
			if (xodtemplate_expand_hosts(&hlist, host_map, temp_service->host_name, temp_service->_config_file, temp_service->_start_line) != OK) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to expand host list '%s' for service '%s' (%s:%d)\n",
					  temp_service->host_name, temp_service->service_description,
					  xodtemplate_config_file_name(temp_service->_config_file),
					  temp_service->_start_line);
				return ERROR;
				}
			/* we don't need this anymore now that we have the hlist */
			my_free(temp_service->host_name);
		}

		/*
		 * host_map now contains all rejected hosts
		 * group_map contains all rejected hostgroups
		 * hlist contains all hosts we're directly assigned to.
		 * glist contains all hostgroups we're assigned to.
		 * We ignore hostgroups we're assigned to that are also rejected.
		 * We do a dirty trick here and prepend a fake hostgroup
		 * to the hostgroup list so we can use the same loop for
		 * the rest of the code.
		 */
		fake_hg.hostgroup_name = "!!FAKE HOSTGROUP";
		fake_hg.member_list = hlist;
		prepend_object_to_objectlist(&glist, &fake_hg);
		for(list = glist; list; list = next) {
			xodtemplate_hostgroup *hg = (xodtemplate_hostgroup *)list->object_ptr;
			next = list->next;
			free(list);

			/* we don't free this list */
			for(hlist = hg->member_list; hlist; hlist = hlist->next) {
				xodtemplate_host *h = (xodtemplate_host *)hlist->object_ptr;

				/* ignore this host if it's rejected */
				if(bitmap_isset(host_map, h->id))
					continue;

				/*
				 * reject more copies of this host. This happens
				 * if the service is assigned to multiple hostgroups
				 * where the same host is part of more than one of
				 * them
				 */
				bitmap_set(host_map, h->id);

				/* if this is the last duplication, use the existing entry */
				if(!next && !hlist->next) {
					temp_service->id = xodcount.services++;
					temp_service->host_name = h->host_name;
					}
				else {
					/* duplicate service definition */
					result = xodtemplate_duplicate_service(temp_service, h->host_name, hg != &fake_hg);
					}
				}
			free_objectlist(&fake_hg.member_list);
			}
		}

	/***************************************/
	/* SKIPLIST STUFF FOR FAST SORT/SEARCH */
	/***************************************/

	/* First loop for single host service definition*/
	for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {
		/* skip services that shouldn't be registered */
		if(temp_service->register_object == FALSE)
			continue;

		/* skip service definitions without enough data */
		if(temp_service->host_name == NULL || temp_service->service_description == NULL)
			continue;

		if(temp_service->is_from_hostgroup) {
			continue;
			}


		result = skiplist_insert(xobject_skiplists[SERVICE_SKIPLIST], (void *)temp_service);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for service '%s' on host '%s' (config file '%s', starting on line %d)\n", temp_service->service_description, temp_service->host_name, xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				result = ERROR;
				break;
			}
		}


	/* second loop for host group service definition*/
	/* add services to skiplist for fast searches */
	for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* skip services that shouldn't be registered */
		if(temp_service->register_object == FALSE)
			continue;

		/* skip service definitions without enough data */
		if(temp_service->host_name == NULL || temp_service->service_description == NULL)
			continue;

		if(!temp_service->is_from_hostgroup) {
			continue;
			}

		temp_service->is_from_hostgroup = 0;

		result = skiplist_insert(xobject_skiplists[SERVICE_SKIPLIST], (void *)temp_service);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Duplicate definition found for service '%s' on host '%s' (config file '%s', starting on line %d)\n", temp_service->service_description, temp_service->host_name, xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				result = ERROR;
				break;
			}
		}

	return OK;
	}


/**
 * Create an objectlist of services from whatever someone put into a
 * servicedescription. Rules go like this:
 * NOT servicegroup:
 * If we have host_name and service_description, we do a simple
 * lookup.
 * If we have host_name and/or hostgroup_name and service_description,
 * we do multiple lookups and concatenate the results.
 * If we have host_name/hostgroup_name and service_description, we do multiple
 * simple lookups and concatenate the results.
 */
static int xodtemplate_create_service_list(objectlist **ret, bitmap *reject_map, char *host_name, char *hostgroup_name, char *servicegroup_name, char *service_description, int _config_file, int _start_line)
{
	objectlist *hlist = NULL, *hglist = NULL, *slist = NULL, *sglist = NULL;
	objectlist *glist, *gnext, *list, *next; /* iterators */
	xodtemplate_hostgroup fake_hg;
	bitmap *in;

	/*
	 * if we have a service_description, we need host_name
	 * or host_group name
	 */
	if(service_description && !host_name && !hostgroup_name)
		return ERROR;
	/*
	 * if we have host_name or a hostgroup_name we also need
	 * service_description
	 */
	if((host_name || hostgroup_name) && !service_description)
		return ERROR;

	/* we'll need these */
	bitmap_clear(host_map);
	if (!(in = bitmap_create(xodcount.services)))
		return ERROR;

	/*
	 * all services in the accepted servicegroups can be added, except
	 * if they're also in service_map, in which case they're also
	 * in rejected servicegroups and must NOT be added
	 */
	if(servicegroup_name && xodtemplate_expand_servicegroups(&sglist, reject_map, servicegroup_name, _config_file, _start_line) != OK)
		return ERROR;
	for(glist = sglist; glist; glist = gnext) {
		xodtemplate_servicegroup *sg = (xodtemplate_servicegroup *)glist->object_ptr;
		gnext = glist->next;
		free(glist); /* free it as we go along */
		for(list = sg->member_list; list; list = list->next) {
			xodtemplate_service *s = (xodtemplate_service *)list->object_ptr;

			/* rejected or already added */
			if(bitmap_isset(in, s->id) || bitmap_isset(reject_map, s->id))
				continue;
			bitmap_set(in, s->id);
			if(prepend_object_to_objectlist(ret, s) != OK) {
				free_objectlist(&gnext);
				return ERROR;
			}
		}
	}

	/*
	 * get the lists we'll need, with reject markers included.
	 * We have to get both hostlist and hostgroup list at once
	 * to get the full reject markers.
	 */
	if(host_name && xodtemplate_expand_hosts(&hlist, host_map, host_name, _config_file, _start_line) != OK)
		return ERROR;
	if(hostgroup_name && xodtemplate_expand_hostgroups(&hglist, host_map, hostgroup_name, _config_file, _start_line) != OK) {
		free_objectlist(&hlist);
		return ERROR;
	}

	/*
	 * if hlist isn't NULL, we add a fake hostgroup to the mix so we
	 * can get away with a single loop, but we must always set its
	 * memberlist so we can safely call free_objectlist() on it.
	 */
	fake_hg.member_list = hlist;
	if(hlist) {
		if(prepend_object_to_objectlist(&hglist, &fake_hg) != OK) {
			free_objectlist(&hlist);
			free_objectlist(&hglist);
			bitmap_destroy(in);
			return ERROR;
		}
	}

	for(glist = hglist; glist; glist = gnext) {
		xodtemplate_hostgroup *hg = (xodtemplate_hostgroup *)glist->object_ptr;
		gnext = glist->next;
		free(glist);

		for(hlist = hg->member_list; hlist; hlist = hlist->next) {
			xodtemplate_host *h = (xodtemplate_host *)hlist->object_ptr;
			if(bitmap_isset(host_map, h->id))
				continue;

			/* expand services and add them all, unless they're rejected */
			slist = NULL;
			if(xodtemplate_expand_services(&slist, reject_map, h->host_name, service_description, _config_file, _start_line) != OK) {
				free_objectlist(&gnext);
				bitmap_destroy(in);
				return ERROR;
			}
			for(list = slist; list; list = next) {
				xodtemplate_service *s = (xodtemplate_service *)list->object_ptr;
				next = list->next;
				free(list);
				if(bitmap_isset(in, s->id) || bitmap_isset(reject_map, s->id))
					continue;
				bitmap_set(in, s->id);
				if(prepend_object_to_objectlist(ret, s) != OK) {
					free_objectlist(&next);
					free_objectlist(&gnext);
					free_objectlist(&fake_hg.member_list);
					bitmap_destroy(in);
					return ERROR;
				}
			}
		}
	}

	bitmap_destroy(in);
	free_objectlist(&fake_hg.member_list);
	return OK;
}

/* duplicates object definitions */
int xodtemplate_duplicate_objects(void) {
	int result = OK;
	xodtemplate_hostescalation *temp_hostescalation = NULL;
	xodtemplate_serviceescalation *temp_serviceescalation = NULL;
	xodtemplate_hostextinfo *next_he = NULL, *temp_hostextinfo = NULL;
	xodtemplate_serviceextinfo *next_se = NULL, *temp_serviceextinfo = NULL;
	objectlist *master_hostlist, *master_servicelist;
	objectlist *list, *next;


	/*************************************/
	/* SERVICES ARE DUPLICATED ELSEWHERE */
	/*************************************/


	/* duplicate host escalations */
	for(temp_hostescalation = xodtemplate_hostescalation_list; temp_hostescalation != NULL; temp_hostescalation = temp_hostescalation->next) {

		/* skip host escalation definitions without enough data */
		if(temp_hostescalation->hostgroup_name == NULL && temp_hostescalation->host_name == NULL)
			continue;

		/* get list of hosts */
		master_hostlist = xodtemplate_expand_hostgroups_and_hosts(temp_hostescalation->hostgroup_name, temp_hostescalation->host_name, temp_hostescalation->_config_file, temp_hostescalation->_start_line);
		if(master_hostlist == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand hostgroups and/or hosts specified in host escalation (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_hostescalation->_config_file), temp_hostescalation->_start_line);
			return ERROR;
			}

		/* add a copy of the hostescalation for every host in the hostgroup/host name list */
		for(list = master_hostlist; list; list = next) {
			xodtemplate_host *h = (xodtemplate_host *)list->object_ptr;
			next = list->next;
			free(list);

			xodcount.hostescalations++;

			/* if this is the last duplication, use the existing entry */
			if(!next) {
				my_free(temp_hostescalation->name);
				my_free(temp_hostescalation->template);
				my_free(temp_hostescalation->host_name);
				my_free(temp_hostescalation->hostgroup_name);
				temp_hostescalation->host_name = h->host_name;
				continue;
				}

			/* duplicate hostescalation definition */
			result = xodtemplate_duplicate_hostescalation(temp_hostescalation, h->host_name);

			/* exit on error */
			if(result == ERROR) {
				free_objectlist(&next);
				return ERROR;
				}
			}
		}
	timing_point("Created %u hostescalations (dupes possible)\n", xodcount.hostescalations);


	/* duplicate service escalations */
	for(temp_serviceescalation = xodtemplate_serviceescalation_list; temp_serviceescalation != NULL; temp_serviceescalation = temp_serviceescalation->next) {

		/* skip serviceescalations without enough data */
		if(temp_serviceescalation->servicegroup_name == NULL && temp_serviceescalation->service_description == NULL && (temp_serviceescalation->host_name == NULL || temp_serviceescalation->hostgroup_name == NULL))
			continue;
		if(temp_serviceescalation->register_object == FALSE)
			continue;

		bitmap_clear(service_map);

		master_servicelist = NULL;

		/* get list of services */
		if(xodtemplate_create_service_list(&master_servicelist, service_map, temp_serviceescalation->host_name, temp_serviceescalation->hostgroup_name, temp_serviceescalation->servicegroup_name, temp_serviceescalation->service_description, temp_serviceescalation->_config_file, temp_serviceescalation->_start_line) != OK)
			return ERROR;

		/* we won't need these anymore */
		my_free(temp_serviceescalation->host_name);
		my_free(temp_serviceescalation->hostgroup_name);
		my_free(temp_serviceescalation->service_description);
		my_free(temp_serviceescalation->servicegroup_name);

		/* duplicate service escalation entries */
		for(list = master_servicelist; list; list = next) {
			xodtemplate_service *s = (xodtemplate_service *)list->object_ptr;
			next = list->next;
			free(list);

			if(bitmap_isset(service_map, s->id))
				continue;

			xodcount.serviceescalations++;

			/* if this is the last duplication, use the existing entry */
			if(!next) {
				temp_serviceescalation->host_name = s->host_name;
				temp_serviceescalation->service_description = s->service_description;
				continue;
				}

			/* duplicate service escalation definition */
			result = xodtemplate_duplicate_serviceescalation(temp_serviceescalation, s->host_name, s->service_description);

			/* exit on error */
			if(result == ERROR) {
				free_objectlist(&next);
				return ERROR;
				}
			}
		}
	timing_point("Created %u serviceescalations (dupes possible)\n", xodcount.serviceescalations);


	/****** DUPLICATE HOSTEXTINFO DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_hostextinfo = xodtemplate_hostextinfo_list; temp_hostextinfo != NULL; temp_hostextinfo = next_he) {
		next_he = temp_hostextinfo->next;

		/* skip definitions without enough data */
		if(temp_hostextinfo->hostgroup_name == NULL && temp_hostextinfo->host_name == NULL)
			continue;

		/* get list of hosts */
		master_hostlist = xodtemplate_expand_hostgroups_and_hosts(temp_hostextinfo->hostgroup_name, temp_hostextinfo->host_name, temp_hostextinfo->_config_file, temp_hostextinfo->_start_line);
		if(master_hostlist == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand hostgroups and/or hosts specified in extended host info (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_hostextinfo->_config_file), temp_hostextinfo->_start_line);
			return ERROR;
			}

		/* merge this extinfo with every host in the hostgroup/host name list */
		for(list = master_hostlist; list; list = next) {
			xodtemplate_host *h = (xodtemplate_host *)list->object_ptr;
			next = list->next;
			free(list);

			/* merge it. we ignore errors here */
			xodtemplate_merge_host_extinfo_object(h, temp_hostextinfo);
			}
		/* might as well kill it off early */
		my_free(temp_hostextinfo->template);
		my_free(temp_hostextinfo->name);
		my_free(temp_hostextinfo->notes);
		my_free(temp_hostextinfo->host_name);
		my_free(temp_hostextinfo->hostgroup_name);
		my_free(temp_hostextinfo->notes_url);
		my_free(temp_hostextinfo->action_url);
		my_free(temp_hostextinfo->icon_image);
		my_free(temp_hostextinfo->vrml_image);
		my_free(temp_hostextinfo->statusmap_image);
		my_free(temp_hostextinfo);
		}
	timing_point("Done merging hostextinfo\n");


	/****** DUPLICATE SERVICEEXTINFO DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_serviceextinfo = xodtemplate_serviceextinfo_list; temp_serviceextinfo != NULL; temp_serviceextinfo = next_se) {
		next_se = temp_serviceextinfo->next;

		/* skip definitions without enough data */
		if(temp_serviceextinfo->service_description == NULL || (temp_serviceextinfo->hostgroup_name == NULL && temp_serviceextinfo->host_name == NULL))
			continue;

		bitmap_clear(service_map);
		master_servicelist = NULL;

		/* get list of services */
		if(xodtemplate_create_service_list(&master_servicelist, service_map, temp_serviceextinfo->host_name, temp_serviceextinfo->hostgroup_name, NULL, temp_serviceextinfo->service_description, temp_serviceextinfo->_config_file, temp_serviceextinfo->_start_line) != OK) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand services specified in extended service info (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_serviceextinfo->_config_file), temp_serviceextinfo->_start_line);
			return ERROR;
			}

		/* merge this serviceextinfo with every service in the list */
		for(list = master_servicelist; list; list = next) {
			xodtemplate_service *s = (xodtemplate_service *)list->object_ptr;
			next = list->next;
			free(list);
			if(bitmap_isset(service_map, s->id))
				continue;
			xodtemplate_merge_service_extinfo_object(s, temp_serviceextinfo);
			}
		/* now we're done, so we might as well kill it off */
		my_free(temp_serviceextinfo->template);
		my_free(temp_serviceextinfo->name);
		my_free(temp_serviceextinfo->host_name);
		my_free(temp_serviceextinfo->hostgroup_name);
		my_free(temp_serviceextinfo->service_description);
		my_free(temp_serviceextinfo->notes);
		my_free(temp_serviceextinfo->notes_url);
		my_free(temp_serviceextinfo->action_url);
		my_free(temp_serviceextinfo->icon_image);
		my_free(temp_serviceextinfo->icon_image_alt);
		my_free(temp_serviceextinfo);
		}
	timing_point("Done merging serviceextinfo\n");

	return OK;
	}



/* duplicates a service definition (with a new host name) */
int xodtemplate_duplicate_service(xodtemplate_service *temp_service, char *host_name, int from_hg) {
	xodtemplate_service *new_service = NULL;
	int error = FALSE;

	/* allocate zero'd out memory for a new service definition */
	new_service = (xodtemplate_service *)calloc(1, sizeof(xodtemplate_service));
	if(new_service == NULL)
		return ERROR;

	/* copy the entire thing and override what we have to */
	memcpy(new_service, temp_service, sizeof(*new_service));
	new_service->is_copy = TRUE;
	new_service->id = xodcount.services++;
	new_service->host_name = host_name;

	/* tag service apply on host group */
	new_service->is_from_hostgroup = from_hg;

	/* allocate memory for and copy string members of service definition (host name provided, DO NOT duplicate hostgroup member!)*/
	if(temp_service->service_groups != NULL && (new_service->service_groups = (char *)strdup(temp_service->service_groups)) == NULL)
		error = TRUE;
	if(temp_service->contact_groups != NULL && (new_service->contact_groups = (char *)strdup(temp_service->contact_groups)) == NULL)
		error = TRUE;
	if(temp_service->contacts != NULL && (new_service->contacts = (char *)strdup(temp_service->contacts)) == NULL)
		error = TRUE;

	if(error == TRUE) {
		my_free(new_service->service_groups);
		my_free(new_service->contact_groups);
		my_free(new_service->contacts);
		my_free(new_service);
		return ERROR;
		}

	/* add new service to head of list in memory */
	new_service->next = xodtemplate_service_list;
	xodtemplate_service_list = new_service;

	return OK;
	}




/* duplicates a host escalation definition (with a new host name) */
int xodtemplate_duplicate_hostescalation(xodtemplate_hostescalation *temp_hostescalation, char *host_name) {
	xodtemplate_hostescalation *new_hostescalation = NULL;
	int error = FALSE;


	/* allocate zero'd out memory for a new host escalation definition */
	new_hostescalation = (xodtemplate_hostescalation *)calloc(1, sizeof(xodtemplate_hostescalation));
	if(new_hostescalation == NULL)
		return ERROR;

	memcpy(new_hostescalation, temp_hostescalation, sizeof(*new_hostescalation));
	new_hostescalation->is_copy = TRUE;

	new_hostescalation->host_name = host_name;

	if(temp_hostescalation->contact_groups != NULL && (new_hostescalation->contact_groups = (char *)strdup(temp_hostescalation->contact_groups)) == NULL)
		error = TRUE;
	if(temp_hostescalation->contacts != NULL && (new_hostescalation->contacts = (char *)strdup(temp_hostescalation->contacts)) == NULL)
		error = TRUE;

	if(error == TRUE) {
		my_free(new_hostescalation->contact_groups);
		my_free(new_hostescalation->contacts);
		my_free(new_hostescalation);
		return ERROR;
		}

	/* add new hostescalation to head of list in memory */
	new_hostescalation->next = xodtemplate_hostescalation_list;
	xodtemplate_hostescalation_list = new_hostescalation;

	return OK;
	}



/* duplicates a service escalation definition (with a new host name and/or service description) */
int xodtemplate_duplicate_serviceescalation(xodtemplate_serviceescalation *temp_serviceescalation, char *host_name, char *svc_description) {
	xodtemplate_serviceescalation *new_serviceescalation = NULL;
	int error = FALSE;

	/* allocate zero'd out memory for a new service escalation definition */
	new_serviceescalation = (xodtemplate_serviceescalation *)calloc(1, sizeof(xodtemplate_serviceescalation));
	if(new_serviceescalation == NULL)
		return ERROR;

	memcpy(new_serviceescalation, temp_serviceescalation, sizeof(*new_serviceescalation));
	new_serviceescalation->is_copy = TRUE;
	new_serviceescalation->host_name = host_name;
	new_serviceescalation->service_description = svc_description;

	if(temp_serviceescalation->contact_groups != NULL && (new_serviceescalation->contact_groups = (char *)strdup(temp_serviceescalation->contact_groups)) == NULL)
		error = TRUE;
	if(temp_serviceescalation->contacts != NULL && (new_serviceescalation->contacts = (char *)strdup(temp_serviceescalation->contacts)) == NULL)
		error = TRUE;

	if(error == TRUE) {
		my_free(new_serviceescalation->contact_groups);
		my_free(new_serviceescalation->contacts);
		my_free(new_serviceescalation);
		return ERROR;
		}

	/* add new serviceescalation to head of list in memory */
	new_serviceescalation->next = xodtemplate_serviceescalation_list;
	xodtemplate_serviceescalation_list = new_serviceescalation;

	return OK;
	}

/******************************************************************/
/***************** OBJECT RESOLUTION FUNCTIONS ********************/
/******************************************************************/


/* inherit object properties */
/* some missing defaults (notification options, etc.) are also applied here */
int xodtemplate_inherit_object_properties(void) {
	xodtemplate_host *temp_host = NULL;
	xodtemplate_service *temp_service = NULL;
	xodtemplate_serviceescalation *temp_serviceescalation = NULL;
	xodtemplate_hostescalation *temp_hostescalation = NULL;


	/* fill in missing defaults for hosts... */
	for(temp_host = xodtemplate_host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* if notification options are missing, assume all */
		if(temp_host->have_notification_options == FALSE) {
			temp_host->notification_options = OPT_ALL;
			temp_host->have_notification_options = TRUE;
			}
		}

	/* services inherit some properties from their associated host... */
	for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* find the host */
		if((temp_host = xodtemplate_find_real_host(temp_service->host_name)) == NULL)
			continue;

		/*
		 * if the service has no contacts specified, it will inherit
		 * them from the host
		 */
		if(temp_service->have_contact_groups == FALSE && temp_service->have_contacts == FALSE) {
			xod_inherit_str(temp_service, temp_host, contact_groups);
			xod_inherit_str(temp_service, temp_host, contacts);
			}

		/* services inherit notification interval from host if not already specified */
		xod_inherit(temp_service, temp_host, notification_interval);

		/* services inherit notification period from host if not already specified */
		xod_inherit_str(temp_service, temp_host, notification_period);

		/* if notification options are missing, assume all */
		if(temp_service->have_notification_options == FALSE) {
			temp_service->notification_options = OPT_ALL;
			temp_service->have_notification_options = TRUE;
			}
		}

	/* service escalations inherit some properties from their associated service... */
	for(temp_serviceescalation = xodtemplate_serviceescalation_list; temp_serviceescalation != NULL; temp_serviceescalation = temp_serviceescalation->next) {

		/* find the service */
		if((temp_service = xodtemplate_find_real_service(temp_serviceescalation->host_name, temp_serviceescalation->service_description)) == NULL)
			continue;

		/* SPECIAL RULE 10/04/07 - additive inheritance from service's contactgroup(s) */
		if(temp_serviceescalation->contact_groups != NULL && temp_serviceescalation->contact_groups[0] == '+')
			xodtemplate_get_inherited_string(&temp_service->have_contact_groups, &temp_service->contact_groups, &temp_serviceescalation->have_contact_groups, &temp_serviceescalation->contact_groups);

		/* SPECIAL RULE 10/04/07 - additive inheritance from service's contact(s) */
		if(temp_serviceescalation->contacts != NULL && temp_serviceescalation->contacts[0] == '+')
			xodtemplate_get_inherited_string(&temp_service->have_contacts, &temp_service->contacts, &temp_serviceescalation->have_contacts, &temp_serviceescalation->contacts);

		/* service escalations inherit contacts from service if none are specified */
		if(temp_serviceescalation->have_contact_groups == FALSE && temp_serviceescalation->have_contacts == FALSE) {
			xod_inherit_str(temp_serviceescalation, temp_service, contact_groups);
			xod_inherit_str(temp_serviceescalation, temp_service, contacts);
			}

		/* service escalations inherit notification interval from service if not already defined */
		xod_inherit(temp_serviceescalation, temp_service, notification_interval);

		/* service escalations inherit escalation period from service if not already defined */
		if(temp_serviceescalation->have_escalation_period == FALSE && temp_service->have_notification_period == TRUE && temp_service->notification_period != NULL) {
			temp_serviceescalation->escalation_period = (char *)strdup(temp_service->notification_period);
			temp_serviceescalation->have_escalation_period = TRUE;
			}

		/* if escalation options are missing, assume all */
		if(temp_serviceescalation->have_escalation_options == FALSE) {
			temp_serviceescalation->escalation_options = OPT_ALL;
			temp_serviceescalation->have_escalation_options = TRUE;
			}

		/* 03/05/08 clear additive string chars - not done in xodtemplate_clean_additive_strings() anymore */
		xodtemplate_clean_additive_string(&temp_serviceescalation->contact_groups);
		xodtemplate_clean_additive_string(&temp_serviceescalation->contacts);
		}

	/* host escalations inherit some properties from their associated host... */
	for(temp_hostescalation = xodtemplate_hostescalation_list; temp_hostescalation != NULL; temp_hostescalation = temp_hostescalation->next) {

		/* find the host */
		if((temp_host = xodtemplate_find_real_host(temp_hostescalation->host_name)) == NULL)
			continue;

		/* SPECIAL RULE 10/04/07 - additive inheritance from host's contactgroup(s) */
		if(temp_hostescalation->contact_groups != NULL && temp_hostescalation->contact_groups[0] == '+')
			xodtemplate_get_inherited_string(&temp_host->have_contact_groups, &temp_host->contact_groups, &temp_hostescalation->have_contact_groups, &temp_hostescalation->contact_groups);

		/* SPECIAL RULE 10/04/07 - additive inheritance from host's contact(s) */
		if(temp_hostescalation->contacts != NULL && temp_hostescalation->contacts[0] == '+')
			xodtemplate_get_inherited_string(&temp_host->have_contacts, &temp_host->contacts, &temp_hostescalation->have_contacts, &temp_hostescalation->contacts);

		/* host escalations inherit contacts from host if none are specified */
		if(temp_hostescalation->have_contact_groups == FALSE && temp_hostescalation->have_contacts == FALSE) {
			xod_inherit_str(temp_hostescalation, temp_host, contact_groups);
			xod_inherit_str(temp_hostescalation, temp_host, contacts);
			}

		/* host escalations inherit notification interval from host if not already defined */
		xod_inherit(temp_hostescalation, temp_host, notification_interval);

		/* host escalations inherit escalation period from host if not already defined */
		if(temp_hostescalation->have_escalation_period == FALSE && temp_host->have_notification_period == TRUE && temp_host->notification_period != NULL) {
			temp_hostescalation->escalation_period = (char *)strdup(temp_host->notification_period);
			temp_hostescalation->have_escalation_period = TRUE;
			}

		/* if escalation options are missing, assume all */
		if(temp_hostescalation->have_escalation_options == FALSE) {
			temp_hostescalation->escalation_options = OPT_ALL;
			temp_hostescalation->have_escalation_options = TRUE;
			}

		/* 03/05/08 clear additive string chars - not done in xodtemplate_clean_additive_strings() anymore */
		xodtemplate_clean_additive_string(&temp_hostescalation->contact_groups);
		xodtemplate_clean_additive_string(&temp_hostescalation->contacts);
		}

	return OK;
	}

#endif


/******************************************************************/
/***************** OBJECT RESOLUTION FUNCTIONS ********************/
/******************************************************************/

static int xodtemplate_register_and_destroy_hostescalation(void *he_)
{
	xodtemplate_hostescalation *he = (xodtemplate_hostescalation *)he_;
	int result;

	result = xodtemplate_register_hostescalation(he);
	if(he->is_copy == FALSE) {
		my_free(he->escalation_period);
		}
	my_free(he->contact_groups);
	my_free(he->contacts);
	my_free(he);

	return result;
}

static int xodtemplate_register_and_destroy_serviceescalation(void *se_)
{
	xodtemplate_serviceescalation *se = (xodtemplate_serviceescalation *)se_;
	int result;
	result = xodtemplate_register_serviceescalation(se);

	if(se->is_copy == FALSE)
		my_free(se->escalation_period);

	my_free(se->contact_groups);
	my_free(se->contacts);
	my_free(se);

	return result;
}

#ifndef NSCGI
static int xodtemplate_register_and_destroy_hostdependency(void *hd_)
{
	xodtemplate_hostdependency *temp_hostdependency = (xodtemplate_hostdependency *)hd_;
	objectlist *master_hostlist = NULL, *dependent_hostlist = NULL;
	objectlist *list, *next, *l2;

	/* skip host dependencies without enough data */
	if(temp_hostdependency->hostgroup_name == NULL && temp_hostdependency->dependent_hostgroup_name == NULL && temp_hostdependency->host_name == NULL && temp_hostdependency->dependent_host_name == NULL)
		return OK;

	/* get list of master host names */
	master_hostlist = xodtemplate_expand_hostgroups_and_hosts(temp_hostdependency->hostgroup_name, temp_hostdependency->host_name, temp_hostdependency->_config_file, temp_hostdependency->_start_line);
	if(master_hostlist == NULL && allow_empty_hostgroup_assignment==0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand master hostgroups and/or hosts specified in host dependency (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_hostdependency->_config_file), temp_hostdependency->_start_line);
		return ERROR;
		}

	/* get list of dependent host names */
	dependent_hostlist = xodtemplate_expand_hostgroups_and_hosts(temp_hostdependency->dependent_hostgroup_name, temp_hostdependency->dependent_host_name, temp_hostdependency->_config_file, temp_hostdependency->_start_line);
	if(dependent_hostlist == NULL && allow_empty_hostgroup_assignment==0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand dependent hostgroups (%s) and/or hosts (%s) specified in host dependency (config file '%s', starting on line %d)\n", temp_hostdependency->dependent_hostgroup_name, temp_hostdependency->dependent_host_name, xodtemplate_config_file_name(temp_hostdependency->_config_file), temp_hostdependency->_start_line);
		free_objectlist(&master_hostlist);
		return ERROR;
		}

	my_free(temp_hostdependency->name);
	my_free(temp_hostdependency->template);
	my_free(temp_hostdependency->host_name);
	my_free(temp_hostdependency->hostgroup_name);
	my_free(temp_hostdependency->dependent_host_name);
	my_free(temp_hostdependency->dependent_hostgroup_name);

	/* duplicate the dependency definitions */
	for(list = master_hostlist; list; list = next) {
		xodtemplate_host *master = (xodtemplate_host *)list->object_ptr;
		next = list->next;
		free(list);

		for(l2 = dependent_hostlist; l2; l2 = l2->next) {
			xodtemplate_host *child = (xodtemplate_host *)l2->object_ptr;

			temp_hostdependency->host_name = master->host_name;
			temp_hostdependency->dependent_host_name = child->host_name;
			if(xodtemplate_register_hostdependency(temp_hostdependency) != OK) {
				/* exit on error */
				free_objectlist(&dependent_hostlist);
				free_objectlist(&next);
				return ERROR;
				}
			}
		}

	free_objectlist(&dependent_hostlist);
	my_free(temp_hostdependency->dependency_period);
	my_free(temp_hostdependency);

	return OK;
}

static int xodtemplate_register_and_destroy_servicedependency(void *sd_)
{
	objectlist *parents = NULL, *plist, *pnext;
	objectlist *children = NULL, *clist;
	xodtemplate_servicedependency *temp_servicedependency = (xodtemplate_servicedependency *)sd_;
	int same_host = FALSE, children_first = FALSE, pret = OK, cret = OK;
	char *hname, *sdesc, *dhname, *dsdesc;

	hname = temp_servicedependency->host_name;
	sdesc = temp_servicedependency->service_description;
	dhname = temp_servicedependency->dependent_host_name;
	dsdesc = temp_servicedependency->dependent_service_description;

	my_free(temp_servicedependency->name);
	my_free(temp_servicedependency->template);

	/* skip templates, but free them first */
	if(temp_servicedependency->register_object == 0) {
		my_free(temp_servicedependency->host_name);
		my_free(temp_servicedependency->service_description);
		my_free(temp_servicedependency->hostgroup_name);
		my_free(temp_servicedependency->dependent_host_name);
		my_free(temp_servicedependency->dependent_service_description);
		my_free(temp_servicedependency->dependent_hostgroup_name);
		return OK;
		}

	if(!temp_servicedependency->host_name && !temp_servicedependency->hostgroup_name
	   && !temp_servicedependency->servicegroup_name)
	{
		/*
		 * parent service is in exact. We must take children first
		 * and build parent chain from same-host deps there
		 */
		children_first = TRUE;
		same_host = TRUE;
	}

	/* take care of same-host dependencies */
	if(!temp_servicedependency->dependent_host_name && !temp_servicedependency->dependent_hostgroup_name) {
		if (!temp_servicedependency->dependent_servicegroup_name) {
			if (children_first || !temp_servicedependency->dependent_service_description) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Impossible service dependency definition\n (config file '%s', starting at line '%d')",
					  xodtemplate_config_file_name(temp_servicedependency->_config_file),
					  temp_servicedependency->_start_line);
				return ERROR;
			}
			same_host = TRUE;
		}
	}

	parents = children = NULL;
	bitmap_clear(parent_map);
	bitmap_clear(service_map);

	/* create the two object lists */
	if(!children_first) {
		pret = xodtemplate_create_service_list(&parents, parent_map, temp_servicedependency->host_name, temp_servicedependency->hostgroup_name, temp_servicedependency->servicegroup_name, temp_servicedependency->service_description, temp_servicedependency->_config_file, temp_servicedependency->_start_line);
		if (!same_host)
			cret = xodtemplate_create_service_list(&children, service_map, temp_servicedependency->dependent_host_name, temp_servicedependency->dependent_hostgroup_name, temp_servicedependency->dependent_servicegroup_name, temp_servicedependency->dependent_service_description, temp_servicedependency->_config_file, temp_servicedependency->_start_line);
	} else {
		/*
		 * NOTE: we flip the variables here to avoid duplicating
		 * the entire loop down below
		 */
		cret = xodtemplate_create_service_list(&parents, parent_map, temp_servicedependency->dependent_host_name, temp_servicedependency->dependent_hostgroup_name, temp_servicedependency->dependent_servicegroup_name, temp_servicedependency->dependent_service_description, temp_servicedependency->_config_file, temp_servicedependency->_start_line);
		dsdesc = temp_servicedependency->service_description;
		sdesc = temp_servicedependency->dependent_service_description;
	}

	/* now log errors and bail out if we failed to get members */
	if(pret != OK) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand master service(s) (config file '%s', starting at line %d)\n",
			  xodtemplate_config_file_name(temp_servicedependency->_config_file),
			  temp_servicedependency->_start_line);
		}
	if(cret != OK) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand dependent service(s) (at config file '%s', starting on line %d)\n",
			  xodtemplate_config_file_name(temp_servicedependency->_config_file),
			  temp_servicedependency->_start_line);
		}

	if(cret != OK || pret != OK)
		return ERROR;

	/*
	 * every service in "children" depends on every service in
	 * "parents", so just loop twice and create them all.
	 */
	for(plist = parents; plist; plist = pnext) {
		xodtemplate_service *p = (xodtemplate_service *)plist->object_ptr;
		pnext = plist->next;
		free(plist); /* free it as we go along */

		if(bitmap_isset(parent_map, p->id))
			continue;
		bitmap_set(parent_map, p->id);

        bitmap_clear(service_map);


		/*
		 * if this is a same-host dependency, we must expand
		 * dependent_service_description for the host we're
		 * currently looking at
		 */
		if(same_host) {
			children = NULL;
			if(xodtemplate_expand_services(&children, service_map, p->host_name, dsdesc, temp_servicedependency->_config_file, temp_servicedependency->_start_line) != OK) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to expand same-host servicedependency services (config file '%s', starting at line %d)\n",
					  xodtemplate_config_file_name(temp_servicedependency->_config_file),
					  temp_servicedependency->_start_line);
				return ERROR;
			}
		}

		for(clist = children; clist; clist = clist->next) {
			xodtemplate_service *c = (xodtemplate_service *)clist->object_ptr;
			if(bitmap_isset(service_map, c->id))
				continue;
			bitmap_set(service_map, c->id);

			/* now register, but flip the states again if necessary */
			if(children_first) {
				xodtemplate_service *tmp = p;
				p = c;
				c = tmp;
			}
			temp_servicedependency->host_name = p->host_name;
			temp_servicedependency->service_description = p->service_description;
			temp_servicedependency->dependent_host_name = c->host_name;
			temp_servicedependency->dependent_service_description = c->service_description;
			if(xodtemplate_register_servicedependency(temp_servicedependency) != OK) {
				logit(NSLOG_VERIFICATION_WARNING, TRUE, "Error: Failed to register servicedependency from '%s;%s' to '%s;%s' (config file '%s', starting at line %d)\n",
					  p->host_name, p->service_description,
					  c->host_name, c->service_description,
					  xodtemplate_config_file_name(temp_servicedependency->_config_file), temp_servicedependency->_start_line);
				return ERROR;
			}
		}
		if(same_host == TRUE)
			free_objectlist(&children);
	}
	if(same_host == FALSE)
		free_objectlist(&children);

	my_free(hname);
	my_free(sdesc);
	my_free(dhname);
	my_free(dsdesc);
	my_free(temp_servicedependency->hostgroup_name);
	my_free(temp_servicedependency->servicegroup_name);
	my_free(temp_servicedependency->dependent_hostgroup_name);
	my_free(temp_servicedependency->dependent_servicegroup_name);
	my_free(temp_servicedependency->dependency_period);
	my_free(temp_servicedependency);
	return OK;
}


/* resolves object definitions */
int xodtemplate_resolve_objects(void) {
	xodtemplate_timeperiod *temp_timeperiod = NULL;
	xodtemplate_command *temp_command = NULL;
	xodtemplate_contactgroup *temp_contactgroup = NULL;
	xodtemplate_hostgroup *temp_hostgroup = NULL;
	xodtemplate_servicegroup *temp_servicegroup = NULL;
	xodtemplate_servicedependency *temp_servicedependency = NULL;
	xodtemplate_serviceescalation *temp_serviceescalation = NULL;
	xodtemplate_contact *temp_contact = NULL;
	xodtemplate_host *temp_host = NULL;
	xodtemplate_service *temp_service = NULL;
	xodtemplate_hostdependency *temp_hostdependency = NULL;
	xodtemplate_hostescalation *temp_hostescalation = NULL;
	xodtemplate_hostextinfo *temp_hostextinfo = NULL;
	xodtemplate_serviceextinfo *temp_serviceextinfo = NULL;

	/* resolve all timeperiod objects */
	for(temp_timeperiod = xodtemplate_timeperiod_list; temp_timeperiod != NULL; temp_timeperiod = temp_timeperiod->next) {
		if(xodtemplate_resolve_timeperiod(temp_timeperiod) == ERROR)
			return ERROR;
		}

	/* resolve all command objects */
	for(temp_command = xodtemplate_command_list; temp_command != NULL; temp_command = temp_command->next) {
		if(xodtemplate_resolve_command(temp_command) == ERROR)
			return ERROR;
		}

	/* resolve all contactgroup objects */
	for(temp_contactgroup = xodtemplate_contactgroup_list; temp_contactgroup != NULL; temp_contactgroup = temp_contactgroup->next) {
		if(xodtemplate_resolve_contactgroup(temp_contactgroup) == ERROR)
			return ERROR;
		}

	/* resolve all hostgroup objects */
	for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {
		if(xodtemplate_resolve_hostgroup(temp_hostgroup) == ERROR)
			return ERROR;
		}

	/* resolve all servicegroup objects */
	for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {
		if(xodtemplate_resolve_servicegroup(temp_servicegroup) == ERROR)
			return ERROR;
		}

	/* resolve all servicedependency objects */
	for(temp_servicedependency = xodtemplate_servicedependency_list; temp_servicedependency != NULL; temp_servicedependency = temp_servicedependency->next) {
		if(xodtemplate_resolve_servicedependency(temp_servicedependency) == ERROR)
			return ERROR;
		}

	/* resolve all serviceescalation objects */
	for(temp_serviceescalation = xodtemplate_serviceescalation_list; temp_serviceescalation != NULL; temp_serviceescalation = temp_serviceescalation->next) {
		if(xodtemplate_resolve_serviceescalation(temp_serviceescalation) == ERROR)
			return ERROR;
		}

	/* resolve all contact objects */
	for(temp_contact = xodtemplate_contact_list; temp_contact != NULL; temp_contact = temp_contact->next) {
		if(xodtemplate_resolve_contact(temp_contact) == ERROR)
			return ERROR;
		}

	/* resolve all host objects */
	for(temp_host = xodtemplate_host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(xodtemplate_resolve_host(temp_host) == ERROR)
			return ERROR;
		}

	/* resolve all service objects */
	for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {
		if(xodtemplate_resolve_service(temp_service) == ERROR)
			return ERROR;
		}

	/* resolve all hostdependency objects */
	for(temp_hostdependency = xodtemplate_hostdependency_list; temp_hostdependency != NULL; temp_hostdependency = temp_hostdependency->next) {
		if(xodtemplate_resolve_hostdependency(temp_hostdependency) == ERROR)
			return ERROR;
		}

	/* resolve all hostescalation objects */
	for(temp_hostescalation = xodtemplate_hostescalation_list; temp_hostescalation != NULL; temp_hostescalation = temp_hostescalation->next) {
		if(xodtemplate_resolve_hostescalation(temp_hostescalation) == ERROR)
			return ERROR;
		}

	/* resolve all hostextinfo objects */
	for(temp_hostextinfo = xodtemplate_hostextinfo_list; temp_hostextinfo != NULL; temp_hostextinfo = temp_hostextinfo->next) {
		if(xodtemplate_resolve_hostextinfo(temp_hostextinfo) == ERROR)
			return ERROR;
		}

	/* resolve all serviceextinfo objects */
	for(temp_serviceextinfo = xodtemplate_serviceextinfo_list; temp_serviceextinfo != NULL; temp_serviceextinfo = temp_serviceextinfo->next) {
		if(xodtemplate_resolve_serviceextinfo(temp_serviceextinfo) == ERROR)
			return ERROR;
		}

	return OK;
	}

/* resolves a timeperiod object */
int xodtemplate_resolve_timeperiod(xodtemplate_timeperiod *this_timeperiod) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_daterange *template_daterange = NULL;
	xodtemplate_daterange *this_daterange = NULL;
	xodtemplate_daterange *new_daterange = NULL;
	xodtemplate_timeperiod *template_timeperiod = NULL;
	int x;

	/* return if this timeperiod has already been resolved */
	if(this_timeperiod->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_timeperiod->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_timeperiod->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_timeperiod->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_timeperiod = xodtemplate_find_timeperiod(temp_ptr);
		if(template_timeperiod == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in timeperiod definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template timeperiod... */
		xodtemplate_resolve_timeperiod(template_timeperiod);

		/* apply missing properties from template timeperiod... */
		xod_inherit_str_nohave(this_timeperiod, template_timeperiod, timeperiod_name);
		xod_inherit_str_nohave(this_timeperiod, template_timeperiod, alias);
		xod_inherit_str_nohave(this_timeperiod, template_timeperiod, exclusions);
		for(x = 0; x < 7; x++)
			xod_inherit_str_nohave(this_timeperiod, template_timeperiod, timeranges[x]);

		/* daterange exceptions require more work to apply missing ranges... */
		for(x = 0; x < DATERANGE_TYPES; x++) {
			for(template_daterange = template_timeperiod->exceptions[x]; template_daterange != NULL; template_daterange = template_daterange->next) {

				/* see if this same daterange already exists in the timeperiod */
				for(this_daterange = this_timeperiod->exceptions[x]; this_daterange != NULL; this_daterange = this_daterange->next) {
					if((this_daterange->type == template_daterange->type) && (this_daterange->syear == template_daterange->syear) && (this_daterange->smon == template_daterange->smon) && (this_daterange->smday == template_daterange->smday) && (this_daterange->swday == template_daterange->swday) && (this_daterange->swday_offset == template_daterange->swday_offset) && (this_daterange->eyear == template_daterange->eyear) && (this_daterange->emon == template_daterange->emon) && (this_daterange->emday == template_daterange->emday) && (this_daterange->ewday == template_daterange->ewday) && (this_daterange->ewday_offset == template_daterange->ewday_offset) && (this_daterange->skip_interval == template_daterange->skip_interval))
						break;
					}

				/* this daterange already exists in the timeperiod, so don't inherit it */
				if(this_daterange != NULL)
					continue;

				/* inherit the daterange from the template */
				if((new_daterange = (xodtemplate_daterange *)malloc(sizeof(xodtemplate_daterange))) == NULL)
					continue;
				new_daterange->type = template_daterange->type;
				new_daterange->syear = template_daterange->syear;
				new_daterange->smon = template_daterange->smon;
				new_daterange->smday = template_daterange->smday;
				new_daterange->swday = template_daterange->swday;
				new_daterange->swday_offset = template_daterange->swday_offset;
				new_daterange->eyear = template_daterange->eyear;
				new_daterange->emon = template_daterange->emon;
				new_daterange->emday = template_daterange->emday;
				new_daterange->ewday = template_daterange->ewday;
				new_daterange->ewday_offset = template_daterange->ewday_offset;
				new_daterange->skip_interval = template_daterange->skip_interval;
				new_daterange->timeranges = NULL;
				if(template_daterange->timeranges != NULL)
					new_daterange->timeranges = (char *)strdup(template_daterange->timeranges);

				/* add new daterange to head of list (should it be added to the end instead?) */
				new_daterange->next = this_timeperiod->exceptions[x];
				this_timeperiod->exceptions[x] = new_daterange;
				}
			}
		}

	my_free(template_names);

	return OK;
	}




/* resolves a command object */
int xodtemplate_resolve_command(xodtemplate_command *this_command) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_command *template_command = NULL;

	/* return if this command has already been resolved */
	if(this_command->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_command->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_command->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_command->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_command = xodtemplate_find_command(temp_ptr);
		if(template_command == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in command definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_command->_config_file), this_command->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template command... */
		xodtemplate_resolve_command(template_command);

		/* apply missing properties from template command... */
		xod_inherit_str_nohave(this_command, template_command, command_name);
		xod_inherit_str_nohave(this_command, template_command, command_line);
		}

	my_free(template_names);

	return OK;
	}




/* resolves a contactgroup object */
int xodtemplate_resolve_contactgroup(xodtemplate_contactgroup *this_contactgroup) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_contactgroup *template_contactgroup = NULL;

	/* return if this contactgroup has already been resolved */
	if(this_contactgroup->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_contactgroup->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_contactgroup->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_contactgroup->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_contactgroup = xodtemplate_find_contactgroup(temp_ptr);
		if(template_contactgroup == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in contactgroup definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_contactgroup->_config_file), this_contactgroup->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template contactgroup... */
		xodtemplate_resolve_contactgroup(template_contactgroup);

		/* apply missing properties from template contactgroup... */
		xod_inherit_str_nohave(this_contactgroup, template_contactgroup, contactgroup_name);
		xod_inherit_str_nohave(this_contactgroup, template_contactgroup, alias);

		xodtemplate_get_inherited_string(&template_contactgroup->have_members, &template_contactgroup->members, &this_contactgroup->have_members, &this_contactgroup->members);
		xodtemplate_get_inherited_string(&template_contactgroup->have_contactgroup_members, &template_contactgroup->contactgroup_members, &this_contactgroup->have_contactgroup_members, &this_contactgroup->contactgroup_members);

		}

	my_free(template_names);

	return OK;
	}




/* resolves a hostgroup object */
int xodtemplate_resolve_hostgroup(xodtemplate_hostgroup *this_hostgroup) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_hostgroup *template_hostgroup = NULL;

	/* return if this hostgroup has already been resolved */
	if(this_hostgroup->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_hostgroup->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_hostgroup->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_hostgroup->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_hostgroup = xodtemplate_find_hostgroup(temp_ptr);
		if(template_hostgroup == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in hostgroup definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_hostgroup->_config_file), this_hostgroup->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template hostgroup... */
		xodtemplate_resolve_hostgroup(template_hostgroup);

		/* apply missing properties from template hostgroup... */
		xod_inherit_str_nohave(this_hostgroup, template_hostgroup, hostgroup_name);
		xod_inherit_str_nohave(this_hostgroup, template_hostgroup, alias);

		xodtemplate_get_inherited_string(&template_hostgroup->have_members, &template_hostgroup->members, &this_hostgroup->have_members, &this_hostgroup->members);
		xodtemplate_get_inherited_string(&template_hostgroup->have_hostgroup_members, &template_hostgroup->hostgroup_members, &this_hostgroup->have_hostgroup_members, &this_hostgroup->hostgroup_members);

		xod_inherit_str(this_hostgroup, template_hostgroup, notes);
		xod_inherit_str(this_hostgroup, template_hostgroup, notes_url);
		xod_inherit_str(this_hostgroup, template_hostgroup, action_url);
		}

	my_free(template_names);

	return OK;
	}




/* resolves a servicegroup object */
int xodtemplate_resolve_servicegroup(xodtemplate_servicegroup *this_servicegroup) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_servicegroup *template_servicegroup = NULL;

	/* return if this servicegroup has already been resolved */
	if(this_servicegroup->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_servicegroup->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_servicegroup->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_servicegroup->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_servicegroup = xodtemplate_find_servicegroup(temp_ptr);
		if(template_servicegroup == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in servicegroup definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_servicegroup->_config_file), this_servicegroup->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template servicegroup... */
		xodtemplate_resolve_servicegroup(template_servicegroup);

		/* apply missing properties from template servicegroup... */
		xod_inherit_str_nohave(this_servicegroup, template_servicegroup, servicegroup_name);
		xod_inherit_str_nohave(this_servicegroup, template_servicegroup, alias);

		xodtemplate_get_inherited_string(&template_servicegroup->have_members, &template_servicegroup->members, &this_servicegroup->have_members, &this_servicegroup->members);
		xodtemplate_get_inherited_string(&template_servicegroup->have_servicegroup_members, &template_servicegroup->servicegroup_members, &this_servicegroup->have_servicegroup_members, &this_servicegroup->servicegroup_members);

		xod_inherit_str(this_servicegroup, template_servicegroup, notes);
		xod_inherit_str(this_servicegroup, template_servicegroup, notes_url);
		xod_inherit_str(this_servicegroup, template_servicegroup, action_url);
		}

	my_free(template_names);

	return OK;
	}


/* resolves a servicedependency object */
int xodtemplate_resolve_servicedependency(xodtemplate_servicedependency *this_servicedependency) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_servicedependency *template_servicedependency = NULL;

	/* return if this servicedependency has already been resolved */
	if(this_servicedependency->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_servicedependency->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_servicedependency->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_servicedependency->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_servicedependency = xodtemplate_find_servicedependency(temp_ptr);
		if(template_servicedependency == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in service dependency definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_servicedependency->_config_file), this_servicedependency->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template servicedependency... */
		xodtemplate_resolve_servicedependency(template_servicedependency);

		/* apply missing properties from template servicedependency... */
		xodtemplate_get_inherited_string(&template_servicedependency->have_servicegroup_name, &template_servicedependency->servicegroup_name, &this_servicedependency->have_servicegroup_name, &this_servicedependency->servicegroup_name);
		xodtemplate_get_inherited_string(&template_servicedependency->have_hostgroup_name, &template_servicedependency->hostgroup_name, &this_servicedependency->have_hostgroup_name, &this_servicedependency->hostgroup_name);
		xodtemplate_get_inherited_string(&template_servicedependency->have_host_name, &template_servicedependency->host_name, &this_servicedependency->have_host_name, &this_servicedependency->host_name);
		xodtemplate_get_inherited_string(&template_servicedependency->have_service_description, &template_servicedependency->service_description, &this_servicedependency->have_service_description, &this_servicedependency->service_description);
		xodtemplate_get_inherited_string(&template_servicedependency->have_dependent_servicegroup_name, &template_servicedependency->dependent_servicegroup_name, &this_servicedependency->have_dependent_servicegroup_name, &this_servicedependency->dependent_servicegroup_name);
		xodtemplate_get_inherited_string(&template_servicedependency->have_dependent_hostgroup_name, &template_servicedependency->dependent_hostgroup_name, &this_servicedependency->have_dependent_hostgroup_name, &this_servicedependency->dependent_hostgroup_name);
		xodtemplate_get_inherited_string(&template_servicedependency->have_dependent_host_name, &template_servicedependency->dependent_host_name, &this_servicedependency->have_dependent_host_name, &this_servicedependency->dependent_host_name);
		xodtemplate_get_inherited_string(&template_servicedependency->have_dependent_service_description, &template_servicedependency->dependent_service_description, &this_servicedependency->have_dependent_service_description, &this_servicedependency->dependent_service_description);

		if(this_servicedependency->have_dependency_period == FALSE && template_servicedependency->have_dependency_period == TRUE) {
			if(this_servicedependency->dependency_period == NULL && template_servicedependency->dependency_period != NULL)
				this_servicedependency->dependency_period = (char *)strdup(template_servicedependency->dependency_period);
			this_servicedependency->have_dependency_period = TRUE;
			}
		if(this_servicedependency->have_inherits_parent == FALSE && template_servicedependency->have_inherits_parent == TRUE) {
			this_servicedependency->inherits_parent = template_servicedependency->inherits_parent;
			this_servicedependency->have_inherits_parent = TRUE;
			}
		if(this_servicedependency->have_execution_failure_options == FALSE && template_servicedependency->have_execution_failure_options == TRUE) {
			this_servicedependency->execution_failure_options = template_servicedependency->execution_failure_options;
			this_servicedependency->have_execution_failure_options = TRUE;
			}
		if(this_servicedependency->have_notification_failure_options == FALSE && template_servicedependency->have_notification_failure_options == TRUE) {
			this_servicedependency->notification_failure_options = template_servicedependency->notification_failure_options;
			this_servicedependency->have_notification_failure_options = TRUE;
			}
		}

	my_free(template_names);

	return OK;
	}


/* resolves a serviceescalation object */
int xodtemplate_resolve_serviceescalation(xodtemplate_serviceescalation *this_serviceescalation) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_serviceescalation *template_serviceescalation = NULL;

	/* return if this serviceescalation has already been resolved */
	if(this_serviceescalation->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_serviceescalation->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_serviceescalation->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_serviceescalation->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_serviceescalation = xodtemplate_find_serviceescalation(temp_ptr);
		if(template_serviceescalation == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in service escalation definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_serviceescalation->_config_file), this_serviceescalation->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template serviceescalation... */
		xodtemplate_resolve_serviceescalation(template_serviceescalation);

		/* apply missing properties from template serviceescalation... */
		xodtemplate_get_inherited_string(&template_serviceescalation->have_servicegroup_name, &template_serviceescalation->servicegroup_name, &this_serviceescalation->have_servicegroup_name, &this_serviceescalation->servicegroup_name);
		xodtemplate_get_inherited_string(&template_serviceescalation->have_hostgroup_name, &template_serviceescalation->hostgroup_name, &this_serviceescalation->have_hostgroup_name, &this_serviceescalation->hostgroup_name);
		xodtemplate_get_inherited_string(&template_serviceescalation->have_host_name, &template_serviceescalation->host_name, &this_serviceescalation->have_host_name, &this_serviceescalation->host_name);
		xodtemplate_get_inherited_string(&template_serviceescalation->have_service_description, &template_serviceescalation->service_description, &this_serviceescalation->have_service_description, &this_serviceescalation->service_description);
		xodtemplate_get_inherited_string(&template_serviceescalation->have_contact_groups, &template_serviceescalation->contact_groups, &this_serviceescalation->have_contact_groups, &this_serviceescalation->contact_groups);
		xodtemplate_get_inherited_string(&template_serviceescalation->have_contacts, &template_serviceescalation->contacts, &this_serviceescalation->have_contacts, &this_serviceescalation->contacts);

		if(this_serviceescalation->have_escalation_period == FALSE && template_serviceescalation->have_escalation_period == TRUE) {
			if(this_serviceescalation->escalation_period == NULL && template_serviceescalation->escalation_period != NULL)
				this_serviceescalation->escalation_period = (char *)strdup(template_serviceescalation->escalation_period);
			this_serviceescalation->have_escalation_period = TRUE;
			}
		if(this_serviceescalation->have_first_notification == FALSE && template_serviceescalation->have_first_notification == TRUE) {
			this_serviceescalation->first_notification = template_serviceescalation->first_notification;
			this_serviceescalation->have_first_notification = TRUE;
			}
		if(this_serviceescalation->have_last_notification == FALSE && template_serviceescalation->have_last_notification == TRUE) {
			this_serviceescalation->last_notification = template_serviceescalation->last_notification;
			this_serviceescalation->have_last_notification = TRUE;
			}
		if(this_serviceescalation->have_notification_interval == FALSE && template_serviceescalation->have_notification_interval == TRUE) {
			this_serviceescalation->notification_interval = template_serviceescalation->notification_interval;
			this_serviceescalation->have_notification_interval = TRUE;
			}
		if(this_serviceescalation->have_escalation_options == FALSE && template_serviceescalation->have_escalation_options == TRUE) {
			this_serviceescalation->escalation_options = template_serviceescalation->escalation_options;
			this_serviceescalation->have_escalation_options = TRUE;
			}
		}

	my_free(template_names);

	return OK;
	}



/* resolves a contact object */
int xodtemplate_resolve_contact(xodtemplate_contact *this_contact) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_contact *template_contact = NULL;
	xodtemplate_customvariablesmember *this_customvariablesmember = NULL;
	xodtemplate_customvariablesmember *temp_customvariablesmember = NULL;
	int x;

	/* return if this contact has already been resolved */
	if(this_contact->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_contact->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_contact->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_contact->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_contact = xodtemplate_find_contact(temp_ptr);
		if(template_contact == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in contact definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_contact->_config_file), this_contact->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template contact... */
		xodtemplate_resolve_contact(template_contact);

		/* apply missing properties from template contact... */
		xod_inherit_str_nohave(this_contact, template_contact, contact_name);
		xod_inherit_str_nohave(this_contact, template_contact, alias);

		xod_inherit_str(this_contact, template_contact, email);
		xod_inherit_str(this_contact, template_contact, pager);
		for(x = 0; x < MAX_XODTEMPLATE_CONTACT_ADDRESSES; x++)
			xod_inherit_str(this_contact, template_contact, address[x]);

		xodtemplate_get_inherited_string(&template_contact->have_contact_groups, &template_contact->contact_groups, &this_contact->have_contact_groups, &this_contact->contact_groups);
		xodtemplate_get_inherited_string(&template_contact->have_host_notification_commands, &template_contact->host_notification_commands, &this_contact->have_host_notification_commands, &this_contact->host_notification_commands);
		xodtemplate_get_inherited_string(&template_contact->have_service_notification_commands, &template_contact->service_notification_commands, &this_contact->have_service_notification_commands, &this_contact->service_notification_commands);

		xod_inherit_str(this_contact, template_contact, host_notification_period);
		xod_inherit_str(this_contact, template_contact, service_notification_period);
		xod_inherit(this_contact, template_contact, host_notification_options);
		xod_inherit(this_contact, template_contact, service_notification_options);
		xod_inherit(this_contact, template_contact, host_notifications_enabled);
		xod_inherit(this_contact, template_contact, service_notifications_enabled);
		xod_inherit(this_contact, template_contact, can_submit_commands);
		xod_inherit(this_contact, template_contact, retain_status_information);
		xod_inherit(this_contact, template_contact, retain_nonstatus_information);
		xod_inherit(this_contact, template_contact, minimum_value);

		/* apply missing custom variables from template contact... */
		for(temp_customvariablesmember = template_contact->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {

			/* see if this host has a variable by the same name */
			for(this_customvariablesmember = this_contact->custom_variables; this_customvariablesmember != NULL; this_customvariablesmember = this_customvariablesmember->next) {
				if(!strcmp(temp_customvariablesmember->variable_name, this_customvariablesmember->variable_name))
					break;
				}

			/* we didn't find the same variable name, so add a new custom variable */
			if(this_customvariablesmember == NULL)
				xodtemplate_add_custom_variable_to_contact(this_contact, temp_customvariablesmember->variable_name, temp_customvariablesmember->variable_value);
			}
		}

	my_free(template_names);

	return OK;
	}



/* resolves a host object */
int xodtemplate_resolve_host(xodtemplate_host *this_host) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_host *template_host = NULL;
	xodtemplate_customvariablesmember *this_customvariablesmember = NULL;
	xodtemplate_customvariablesmember *temp_customvariablesmember = NULL;

	/* return if this host has already been resolved */
	if(this_host->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_host->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_host->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_host->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_host = xodtemplate_find_host(temp_ptr);
		if(template_host == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in host definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_host->_config_file), this_host->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template host... */
		xodtemplate_resolve_host(template_host);

		/* apply missing properties from template host... */
		xod_inherit_str_nohave(this_host, template_host, host_name);
		xod_inherit_str(this_host, template_host, display_name);
		xod_inherit_str_nohave(this_host, template_host, alias);
		xod_inherit_str_nohave(this_host, template_host, address);

		xodtemplate_get_inherited_string(&template_host->have_parents, &template_host->parents, &this_host->have_parents, &this_host->parents);
		xodtemplate_get_inherited_string(&template_host->have_host_groups, &template_host->host_groups, &this_host->have_host_groups, &this_host->host_groups);
		xodtemplate_get_inherited_string(&template_host->have_contact_groups, &template_host->contact_groups, &this_host->have_contact_groups, &this_host->contact_groups);
		xodtemplate_get_inherited_string(&template_host->have_contacts, &template_host->contacts, &this_host->have_contacts, &this_host->contacts);

		xod_inherit_str(this_host, template_host, check_command);
		xod_inherit_str(this_host, template_host, check_period);
		xod_inherit_str(this_host, template_host, event_handler);
		xod_inherit_str(this_host, template_host, notification_period);
		xod_inherit_str(this_host, template_host, notes);
		xod_inherit_str(this_host, template_host, notes_url);
		xod_inherit_str(this_host, template_host, action_url);
		xod_inherit_str(this_host, template_host, icon_image);
		xod_inherit_str(this_host, template_host, icon_image_alt);
		xod_inherit_str(this_host, template_host, vrml_image);
		xod_inherit_str(this_host, template_host, statusmap_image);

		xod_inherit(this_host, template_host, initial_state);
		xod_inherit(this_host, template_host, check_interval);
		xod_inherit(this_host, template_host, retry_interval);
		xod_inherit(this_host, template_host, max_check_attempts);
		xod_inherit(this_host, template_host, active_checks_enabled);
		xod_inherit(this_host, template_host, passive_checks_enabled);
		xod_inherit(this_host, template_host, obsess);
		xod_inherit(this_host, template_host, event_handler_enabled);
		xod_inherit(this_host, template_host, check_freshness);
		xod_inherit(this_host, template_host, freshness_threshold);
		xod_inherit(this_host, template_host, low_flap_threshold);
		xod_inherit(this_host, template_host, high_flap_threshold);
		xod_inherit(this_host, template_host, flap_detection_enabled);
		xod_inherit(this_host, template_host, flap_detection_options);
		xod_inherit(this_host, template_host, notification_options);
		xod_inherit(this_host, template_host, notifications_enabled);
		xod_inherit(this_host, template_host, notification_interval);
		xod_inherit(this_host, template_host, first_notification_delay);
		xod_inherit(this_host, template_host, stalking_options);
		xod_inherit(this_host, template_host, process_perf_data);
		xod_inherit(this_host, template_host, hourly_value);

		if(this_host->have_2d_coords == FALSE && template_host->have_2d_coords == TRUE) {
			this_host->x_2d = template_host->x_2d;
			this_host->y_2d = template_host->y_2d;
			this_host->have_2d_coords = TRUE;
			}
		if(this_host->have_3d_coords == FALSE && template_host->have_3d_coords == TRUE) {
			this_host->x_3d = template_host->x_3d;
			this_host->y_3d = template_host->y_3d;
			this_host->z_3d = template_host->z_3d;
			this_host->have_3d_coords = TRUE;
			}

		xod_inherit(this_host, template_host, retain_status_information);
		xod_inherit(this_host, template_host, retain_nonstatus_information);

		/* apply missing custom variables from template host... */
		for(temp_customvariablesmember = template_host->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {

			/* see if this host has a variable by the same name */
			for(this_customvariablesmember = this_host->custom_variables; this_customvariablesmember != NULL; this_customvariablesmember = this_customvariablesmember->next) {
				if(!strcmp(temp_customvariablesmember->variable_name, this_customvariablesmember->variable_name))
					break;
				}

			/* we didn't find the same variable name, so add a new custom variable */
			if(this_customvariablesmember == NULL)
				xodtemplate_add_custom_variable_to_host(this_host, temp_customvariablesmember->variable_name, temp_customvariablesmember->variable_value);
			}
		}

	my_free(template_names);

	return OK;
	}



/* resolves a service object */
int xodtemplate_resolve_service(xodtemplate_service *this_service) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_service *template_service = NULL;
	xodtemplate_customvariablesmember *this_customvariablesmember = NULL;
	xodtemplate_customvariablesmember *temp_customvariablesmember = NULL;

	/* return if this service has already been resolved */
	if(this_service->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_service->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_service->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_service->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_service = xodtemplate_find_service(temp_ptr);
		if(template_service == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in service definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_service->_config_file), this_service->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template service... */
		xodtemplate_resolve_service(template_service);

		/* apply missing properties from template service... */
		xod_inherit_str(this_service, template_service, service_description);
		xod_inherit_str(this_service, template_service, display_name);

		xodtemplate_get_inherited_string(&template_service->have_parents, &template_service->parents, &this_service->have_parents, &this_service->parents);
		xodtemplate_get_inherited_string(&template_service->have_host_name, &template_service->host_name, &this_service->have_host_name, &this_service->host_name);
		xodtemplate_get_inherited_string(&template_service->have_hostgroup_name, &template_service->hostgroup_name, &this_service->have_hostgroup_name, &this_service->hostgroup_name);
		xodtemplate_get_inherited_string(&template_service->have_service_groups, &template_service->service_groups, &this_service->have_service_groups, &this_service->service_groups);
		xodtemplate_get_inherited_string(&template_service->have_contact_groups, &template_service->contact_groups, &this_service->have_contact_groups, &this_service->contact_groups);
		xodtemplate_get_inherited_string(&template_service->have_contacts, &template_service->contacts, &this_service->have_contacts, &this_service->contacts);

		if(template_service->have_check_command == TRUE) {
			if(template_service->have_important_check_command == TRUE) {
				my_free(this_service->check_command);
				this_service->have_check_command = FALSE;
				}
			}
		xod_inherit_str(this_service, template_service, check_command);

		xod_inherit_str(this_service, template_service, check_period);
		xod_inherit_str(this_service, template_service, event_handler);
		xod_inherit_str(this_service, template_service, notification_period);
		xod_inherit_str(this_service, template_service, notes);
		xod_inherit_str(this_service, template_service, notes_url);
		xod_inherit_str(this_service, template_service, action_url);
		xod_inherit_str(this_service, template_service, icon_image);
		xod_inherit_str(this_service, template_service, icon_image_alt);

		xod_inherit(this_service, template_service, initial_state);
		xod_inherit(this_service, template_service, max_check_attempts);
		xod_inherit(this_service, template_service, check_interval);
		xod_inherit(this_service, template_service, retry_interval);
		xod_inherit(this_service, template_service, active_checks_enabled);
		xod_inherit(this_service, template_service, passive_checks_enabled);
		xod_inherit(this_service, template_service, parallelize_check);
		xod_inherit(this_service, template_service, is_volatile);
		xod_inherit(this_service, template_service, obsess);
		xod_inherit(this_service, template_service, event_handler_enabled);
		xod_inherit(this_service, template_service, check_freshness);
		xod_inherit(this_service, template_service, freshness_threshold);
		xod_inherit(this_service, template_service, low_flap_threshold);
		xod_inherit(this_service, template_service, high_flap_threshold);
		xod_inherit(this_service, template_service, flap_detection_enabled);
		xod_inherit(this_service, template_service, flap_detection_options);
		xod_inherit(this_service, template_service, notification_options);
		xod_inherit(this_service, template_service, notifications_enabled);
		xod_inherit(this_service, template_service, notification_interval);
		xod_inherit(this_service, template_service, first_notification_delay);
		xod_inherit(this_service, template_service, stalking_options);
		xod_inherit(this_service, template_service, process_perf_data);
		xod_inherit(this_service, template_service, retain_status_information);
		xod_inherit(this_service, template_service, retain_nonstatus_information);
		xod_inherit(this_service, template_service, hourly_value);

		/* apply missing custom variables from template service... */
		for(temp_customvariablesmember = template_service->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {

			/* see if this host has a variable by the same name */
			for(this_customvariablesmember = this_service->custom_variables; this_customvariablesmember != NULL; this_customvariablesmember = this_customvariablesmember->next) {
				if(!strcmp(temp_customvariablesmember->variable_name, this_customvariablesmember->variable_name))
					break;
				}

			/* we didn't find the same variable name, so add a new custom variable */
			if(this_customvariablesmember == NULL)
				xodtemplate_add_custom_variable_to_service(this_service, temp_customvariablesmember->variable_name, temp_customvariablesmember->variable_value);
			}
		}

	my_free(template_names);

	return OK;
	}


/* resolves a hostdependency object */
int xodtemplate_resolve_hostdependency(xodtemplate_hostdependency *this_hostdependency) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_hostdependency *template_hostdependency = NULL;

	/* return if this hostdependency has already been resolved */
	if(this_hostdependency->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_hostdependency->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_hostdependency->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_hostdependency->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_hostdependency = xodtemplate_find_hostdependency(temp_ptr);
		if(template_hostdependency == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in host dependency definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_hostdependency->_config_file), this_hostdependency->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template hostdependency... */
		xodtemplate_resolve_hostdependency(template_hostdependency);

		/* apply missing properties from template hostdependency... */

		xodtemplate_get_inherited_string(&template_hostdependency->have_host_name, &template_hostdependency->host_name, &this_hostdependency->have_host_name, &this_hostdependency->host_name);
		xodtemplate_get_inherited_string(&template_hostdependency->have_dependent_host_name, &template_hostdependency->dependent_host_name, &this_hostdependency->have_dependent_host_name, &this_hostdependency->dependent_host_name);
		xodtemplate_get_inherited_string(&template_hostdependency->have_hostgroup_name, &template_hostdependency->hostgroup_name, &this_hostdependency->have_hostgroup_name, &this_hostdependency->hostgroup_name);
		xodtemplate_get_inherited_string(&template_hostdependency->have_dependent_hostgroup_name, &template_hostdependency->dependent_hostgroup_name, &this_hostdependency->have_dependent_hostgroup_name, &this_hostdependency->dependent_hostgroup_name);

		xod_inherit_str(this_hostdependency, template_hostdependency, dependency_period);
		xod_inherit(this_hostdependency, template_hostdependency, inherits_parent);
		xod_inherit(this_hostdependency, template_hostdependency, execution_failure_options);
		xod_inherit(this_hostdependency, template_hostdependency, notification_failure_options);
		}

	my_free(template_names);

	return OK;
	}


/* resolves a hostescalation object */
int xodtemplate_resolve_hostescalation(xodtemplate_hostescalation *this_hostescalation) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_hostescalation *template_hostescalation = NULL;

	/* return if this hostescalation has already been resolved */
	if(this_hostescalation->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_hostescalation->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_hostescalation->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_hostescalation->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_hostescalation = xodtemplate_find_hostescalation(temp_ptr);
		if(template_hostescalation == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in host escalation definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_hostescalation->_config_file), this_hostescalation->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template hostescalation... */
		xodtemplate_resolve_hostescalation(template_hostescalation);

		/* apply missing properties from template hostescalation... */
		xodtemplate_get_inherited_string(&template_hostescalation->have_host_name, &template_hostescalation->host_name, &this_hostescalation->have_host_name, &this_hostescalation->host_name);
		xodtemplate_get_inherited_string(&template_hostescalation->have_hostgroup_name, &template_hostescalation->hostgroup_name, &this_hostescalation->have_hostgroup_name, &this_hostescalation->hostgroup_name);
		xodtemplate_get_inherited_string(&template_hostescalation->have_contact_groups, &template_hostescalation->contact_groups, &this_hostescalation->have_contact_groups, &this_hostescalation->contact_groups);
		xodtemplate_get_inherited_string(&template_hostescalation->have_contacts, &template_hostescalation->contacts, &this_hostescalation->have_contacts, &this_hostescalation->contacts);

		xod_inherit_str(this_hostescalation, template_hostescalation, escalation_period);
		xod_inherit(this_hostescalation, template_hostescalation, first_notification);
		xod_inherit(this_hostescalation, template_hostescalation, last_notification);
		xod_inherit(this_hostescalation, template_hostescalation, notification_interval);
		xod_inherit(this_hostescalation, template_hostescalation, escalation_options);
		}

	my_free(template_names);

	return OK;
	}



/* resolves a hostextinfo object */
int xodtemplate_resolve_hostextinfo(xodtemplate_hostextinfo *this_hostextinfo) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_hostextinfo *template_hostextinfo = NULL;

	/* return if this object has already been resolved */
	if(this_hostextinfo->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_hostextinfo->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_hostextinfo->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_hostextinfo->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_hostextinfo = xodtemplate_find_hostextinfo(temp_ptr);
		if(template_hostextinfo == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in extended host info definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_hostextinfo->_config_file), this_hostextinfo->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template hostextinfo... */
		xodtemplate_resolve_hostextinfo(template_hostextinfo);

		/* apply missing properties from template hostextinfo... */
		xod_inherit_str(this_hostextinfo, template_hostextinfo, host_name);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, hostgroup_name);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, notes);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, notes_url);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, action_url);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, icon_image);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, icon_image_alt);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, vrml_image);
		xod_inherit_str(this_hostextinfo, template_hostextinfo, statusmap_image);

		if(this_hostextinfo->have_2d_coords == FALSE && template_hostextinfo->have_2d_coords == TRUE) {
			this_hostextinfo->x_2d = template_hostextinfo->x_2d;
			this_hostextinfo->y_2d = template_hostextinfo->y_2d;
			this_hostextinfo->have_2d_coords = TRUE;
			}
		if(this_hostextinfo->have_3d_coords == FALSE && template_hostextinfo->have_3d_coords == TRUE) {
			this_hostextinfo->x_3d = template_hostextinfo->x_3d;
			this_hostextinfo->y_3d = template_hostextinfo->y_3d;
			this_hostextinfo->z_3d = template_hostextinfo->z_3d;
			this_hostextinfo->have_3d_coords = TRUE;
			}
		}

	my_free(template_names);

	return OK;
	}



/* resolves a serviceextinfo object */
int xodtemplate_resolve_serviceextinfo(xodtemplate_serviceextinfo *this_serviceextinfo) {
	char *temp_ptr = NULL;
	char *template_names = NULL;
	char *template_name_ptr = NULL;
	xodtemplate_serviceextinfo *template_serviceextinfo = NULL;

	/* return if this object has already been resolved */
	if(this_serviceextinfo->has_been_resolved == TRUE)
		return OK;

	/* set the resolved flag */
	this_serviceextinfo->has_been_resolved = TRUE;

	/* return if we have no template */
	if(this_serviceextinfo->template == NULL)
		return OK;

	if((template_names = (char *)strdup(this_serviceextinfo->template)) == NULL)
		return ERROR;

	/* apply all templates */
	template_name_ptr = template_names;
	for(temp_ptr = my_strsep(&template_name_ptr, ","); temp_ptr != NULL; temp_ptr = my_strsep(&template_name_ptr, ",")) {

		template_serviceextinfo = xodtemplate_find_serviceextinfo(temp_ptr);
		if(template_serviceextinfo == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Template '%s' specified in extended service info definition could not be not found (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_serviceextinfo->_config_file), this_serviceextinfo->_start_line);
			my_free(template_names);
			return ERROR;
			}

		/* resolve the template serviceextinfo... */
		xodtemplate_resolve_serviceextinfo(template_serviceextinfo);

		/* apply missing properties from template serviceextinfo... */
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, host_name);
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, hostgroup_name);
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, service_description);
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, notes);
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, notes_url);
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, action_url);
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, icon_image);
		xod_inherit_str(this_serviceextinfo, template_serviceextinfo, icon_image_alt);
		}

	my_free(template_names);

	return OK;
	}



#endif

/******************************************************************/
/*************** OBJECT RECOMBOBULATION FUNCTIONS *****************/
/******************************************************************/

/*
 * note: The cast to xodtemplate_host works for all objects because 'id'
 * is the first item of host, service and contact structs, and C
 * guarantees that the first member is always the same as the one listed
 * in the struct definition.
 */
static int _xodtemplate_add_group_member(objectlist **list, bitmap *in, bitmap *reject, void *obj) {
	xodtemplate_host *h = (xodtemplate_host *)obj;

	if(!list || !obj)
		return ERROR;

	if(bitmap_isset(in, h->id) || bitmap_isset(reject, h->id))
		return OK;
	bitmap_set(in, h->id);
	return prepend_object_to_objectlist(list, obj);
	}
#define xodtemplate_add_group_member(g, m) \
	_xodtemplate_add_group_member(&g->member_list, g->member_map, g->reject_map, m)
#define xodtemplate_add_servicegroup_member xodtemplate_add_group_member
#define xodtemplate_add_contactgroup_member xodtemplate_add_group_member
#define xodtemplate_add_hostgroup_member xodtemplate_add_group_member


/* recombobulates contactgroup definitions */
static int xodtemplate_recombobulate_contactgroup_subgroups(xodtemplate_contactgroup *temp_contactgroup) {
	objectlist *mlist, *glist;

	if(temp_contactgroup == NULL)
		return ERROR;

	/* if this one's already handled somehow, we return early */
	if(temp_contactgroup->loop_status != XOD_NEW)
		return temp_contactgroup->loop_status;

	/* mark it as seen */
	temp_contactgroup->loop_status = XOD_SEEN;

	/* resolve included groups' members and add them to ours */
	for(glist = temp_contactgroup->group_list; glist; glist = glist->next) {
		int result;
		xodtemplate_contactgroup *inc = (xodtemplate_contactgroup *)glist->object_ptr;
		result = xodtemplate_recombobulate_contactgroup_subgroups(inc);
		if(result != XOD_OK) {
			if(result == ERROR)
				return ERROR;
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contactgroups '%s' and '%s' are part of a contactgroup_members include loop\n", temp_contactgroup->contactgroup_name, inc->contactgroup_name);
			inc->loop_status = XOD_LOOPY;
			temp_contactgroup->loop_status = XOD_LOOPY;
			break;
			}

		for(mlist = inc->member_list; mlist; mlist = mlist->next) {
			xodtemplate_contact *c = (xodtemplate_contact *)mlist->object_ptr;
			if(xodtemplate_add_contactgroup_member(temp_contactgroup, c) != OK) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to add '%s' as a subgroup member contact of contactgroup '%s' from contactgroup '%s'\n",
					  c->contact_name, temp_contactgroup->contactgroup_name, inc->contactgroup_name);
				return ERROR;
				}
			}
		}

	if(temp_contactgroup->loop_status == XOD_SEEN)
		temp_contactgroup->loop_status = XOD_OK;

	return temp_contactgroup->loop_status;
	}



int xodtemplate_recombobulate_contactgroups(void) {
	xodtemplate_contact *temp_contact = NULL;
	xodtemplate_contactgroup *temp_contactgroup = NULL;
	char *contactgroup_names = NULL;
	char *temp_ptr = NULL;

	/* expand members of all contactgroups - this could be done in xodtemplate_register_contactgroup(), but we can save the CGIs some work if we do it here */
	for(temp_contactgroup = xodtemplate_contactgroup_list; temp_contactgroup; temp_contactgroup = temp_contactgroup->next) {
		objectlist *next, *list, *accept = NULL;

		/*
		 * If the contactgroup has no accept or reject list and no group
		 * members we don't need the bitmaps for it. bitmap_isset()
		 * will return 0 when passed a NULL map, so we can safely use
		 * that to add any items from the object list later
		 */
		if(temp_contactgroup->members == NULL && temp_contactgroup->contactgroup_members == NULL)
			continue;

		if(!(temp_contactgroup->member_map = bitmap_create(xodcount.contacts))) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not create contactgroup bitmap\n");
			return ERROR;
			}

		if(temp_contactgroup->contactgroup_members) {
			xodtemplate_contactgroup *cg;
			char *ptr, *next_ptr;

			for(next_ptr = ptr = temp_contactgroup->contactgroup_members; next_ptr; ptr = next_ptr + 1) {
				next_ptr = strchr(ptr, ',');
				if(next_ptr)
					*next_ptr = 0;
				while(*ptr == ' ' || *ptr == '\t')
					ptr++;
				strip(ptr);
				if(!(cg = xodtemplate_find_real_contactgroup(ptr))) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find member group '%s' specified in contactgroup '%s' (config file '%s', starting on line %d)\n", ptr, temp_contactgroup->contactgroup_name, xodtemplate_config_file_name(temp_contactgroup->_config_file), temp_contactgroup->_start_line);
					return ERROR;
					}
				add_object_to_objectlist(&temp_contactgroup->group_list, cg);
				}
			my_free(temp_contactgroup->contactgroup_members);
			}

		/* move on if we have no members */
		if(temp_contactgroup->members == NULL)
			continue;

		/* we might need this */
		if(!use_precached_objects && !(temp_contactgroup->reject_map = bitmap_create(xodcount.contacts))) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not create reject map for contactgroup '%s'", temp_contactgroup->contactgroup_name);
			return ERROR;
			}

		/* get list of contacts in the contactgroup */
		if(xodtemplate_expand_contacts(&accept, temp_contactgroup->reject_map, temp_contactgroup->members, temp_contactgroup->_config_file, temp_contactgroup->_start_line) != OK) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to expand contacts for contactgroup '%s' (config file '%s', starting at line %d)\n",
				  temp_contactgroup->contactgroup_name,
				  xodtemplate_config_file_name(temp_contactgroup->_config_file),
				  temp_contactgroup->_start_line);
			return ERROR;
			}

		my_free(temp_contactgroup->members);
		for(list = accept; list; list = next) {
			temp_contact = (xodtemplate_contact *)list->object_ptr;
			next = list->next;
			free(list);
			xodtemplate_add_contactgroup_member(temp_contactgroup, temp_contact);
			}
		}

	/* if we're using precached objects we can bail out now */
	if(use_precached_objects)
		return OK;

	/* process all contacts with contactgroups directives */
	for(temp_contact = xodtemplate_contact_list; temp_contact != NULL; temp_contact = temp_contact->next) {

		/* skip contacts without contactgroup directives or contact names */
		if(temp_contact->contact_groups == NULL || temp_contact->contact_name == NULL)
			continue;

		/* preprocess the contactgroup list, to change "grp1,grp2,grp3,!grp2" into "grp1,grp3" */
		if((contactgroup_names = xodtemplate_process_contactgroup_names(temp_contact->contact_groups, temp_contact->_config_file, temp_contact->_start_line)) == NULL) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to process contactgroups for contact '%s' (config file '%s', starting at line %d)\n",
				  temp_contact->contact_name, xodtemplate_config_file_name(temp_contact->_config_file),	temp_contact->_start_line);
			return ERROR;
			}

		/* process the list of contactgroups */
		for(temp_ptr = strtok(contactgroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

			/* strip trailing spaces */
			strip(temp_ptr);

			/* find the contactgroup */
			temp_contactgroup = xodtemplate_find_real_contactgroup(temp_ptr);
			if(temp_contactgroup == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find contactgroup '%s' specified in contact '%s' definition (config file '%s', starting on line %d)\n", temp_ptr, temp_contact->contact_name, xodtemplate_config_file_name(temp_contact->_config_file), temp_contact->_start_line);
				my_free(contactgroup_names);
				return ERROR;
				}

			if(!temp_contactgroup->member_map && !(temp_contactgroup->member_map = bitmap_create(xodcount.contacts))) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to create member map for contactgroup '%s'\n",
					  temp_contactgroup->contactgroup_name);
				return ERROR;
				}
			/* add this contact to the contactgroup members directive */
			xodtemplate_add_contactgroup_member(temp_contactgroup, temp_contact);
			}

		/* free memory */
		my_free(contactgroup_names);
		}

	/* expand subgroup membership recursively */
	for(temp_contactgroup = xodtemplate_contactgroup_list; temp_contactgroup; temp_contactgroup = temp_contactgroup->next) {
		if(xodtemplate_recombobulate_contactgroup_subgroups(temp_contactgroup) != XOD_OK)
			return ERROR;
		/* rejects are no longer necessary */
		bitmap_destroy(temp_contactgroup->reject_map);
		/* make sure we don't recursively add subgroup members again */
		free_objectlist(&temp_contactgroup->group_list);
	}

	return OK;
	}



static int xodtemplate_recombobulate_hostgroup_subgroups(xodtemplate_hostgroup *temp_hostgroup) {
	objectlist *mlist, *glist;

	if(temp_hostgroup == NULL)
		return ERROR;

	/* if this one's already handled somehow, we return early */
	if(temp_hostgroup->loop_status != XOD_NEW)
		return temp_hostgroup->loop_status;

	/* mark this one as seen */
	temp_hostgroup->loop_status = XOD_SEEN;

	/* resolve included groups' members and add them to ours */
	for(glist = temp_hostgroup->group_list; glist; glist = glist->next) {
		int result;
		xodtemplate_hostgroup *inc = (xodtemplate_hostgroup *)glist->object_ptr;
		result = xodtemplate_recombobulate_hostgroup_subgroups(inc);
		if(result != XOD_OK) {
			if(result == ERROR)
				return ERROR;
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Hostgroups '%s' and '%s' part of a hostgroup_members include loop\n", temp_hostgroup->hostgroup_name, inc->hostgroup_name);
			inc->loop_status = XOD_LOOPY;
			temp_hostgroup->loop_status = XOD_LOOPY;
			break;
			}
		for(mlist = inc->member_list; mlist; mlist = mlist->next) {
			xodtemplate_host *h = (xodtemplate_host *)mlist->object_ptr;
			if (xodtemplate_add_hostgroup_member(temp_hostgroup, mlist->object_ptr) != OK) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to add '%s' as a subgroup member host of hostgroup '%s' from hostgroup '%s'\n",
					  h->host_name, temp_hostgroup->hostgroup_name, inc->hostgroup_name);
				return ERROR;
				}
			}
		}

	if(temp_hostgroup->loop_status == XOD_SEEN)
		temp_hostgroup->loop_status = XOD_OK;

	return temp_hostgroup->loop_status;
	}



/* recombobulates hostgroup definitions */
int xodtemplate_recombobulate_hostgroups(void) {
	xodtemplate_host *temp_host = NULL;
	xodtemplate_hostgroup *temp_hostgroup = NULL;
	char *hostgroup_names = NULL;
	char *ptr, *next_ptr, *temp_ptr = NULL;

	/* expand members of all hostgroups - this could be done in xodtemplate_register_hostgroup(), but we can save the CGIs some work if we do it here */
	for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup; temp_hostgroup = temp_hostgroup->next) {
		objectlist *next, *list, *accept = NULL;

		/*
		 * if the hostgroup has no accept or reject list and no group
		 * members we don't need the bitmaps for it. bitmap_isset()
		 * will return 0 when passed a NULL map, so we can safely use
		 * that to add any items from the object list later.
		 */
		if(temp_hostgroup->members == NULL && temp_hostgroup->hostgroup_members == NULL)
			continue;

		/* we'll need the member_map */
		if (!(temp_hostgroup->member_map = bitmap_create(xodcount.hosts))) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not create member map for hostgroup '%s'\n", temp_hostgroup->hostgroup_name);
			return ERROR;
			}

		/* resolve groups into a group-list */
		for(next_ptr = ptr = temp_hostgroup->hostgroup_members; next_ptr; ptr = next_ptr + 1) {
			xodtemplate_hostgroup *hg;
			next_ptr = strchr(ptr, ',');
			if(next_ptr)
				*next_ptr = 0;
			while(*ptr == ' ' || *ptr == '\t')
				ptr++;

			strip(ptr);

			if (!(hg = xodtemplate_find_real_hostgroup(ptr))) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find member group '%s' specified in hostgroup '%s' (config file '%s', starting on line %d)\n", ptr, temp_hostgroup->hostgroup_name, xodtemplate_config_file_name(temp_hostgroup->_config_file), temp_hostgroup->_start_line);
				return ERROR;
				}
			add_object_to_objectlist(&temp_hostgroup->group_list, hg);
			}

		/* move on if we have no members */
		if(temp_hostgroup->members == NULL)
			continue;

		/* we might need this */
		if(!use_precached_objects && !(temp_hostgroup->reject_map = bitmap_create(xodcount.hosts))) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not create reject map for hostgroup '%s'\n", temp_hostgroup->hostgroup_name);
			return ERROR;
			}

		/* get list of hosts in the hostgroup */
		if (xodtemplate_expand_hosts(&accept, temp_hostgroup->reject_map, temp_hostgroup->members, temp_hostgroup->_config_file, temp_hostgroup->_start_line) == ERROR) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand members specified in hostgroup (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_hostgroup->_config_file), temp_hostgroup->_start_line);
			return ERROR;
		}

		if (!accept && !bitmap_count_set_bits(temp_hostgroup->reject_map)) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand members specified in hostgroup (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(temp_hostgroup->_config_file), temp_hostgroup->_start_line);
			return ERROR;
			}

		my_free(temp_hostgroup->members);

		for (list = accept; list; list = next) {
			temp_host = (xodtemplate_host *)list->object_ptr;
			next = list->next;
			free(list);
			xodtemplate_add_hostgroup_member(temp_hostgroup, temp_host);
			}
		}

	/* if we're using precached objects we can bail out now */
	if(use_precached_objects)
		return OK;

	/* process all hosts that have hostgroup directives */
	for(temp_host = xodtemplate_host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* skip hosts without hostgroup directives or host names */
		if(temp_host->host_groups == NULL || temp_host->host_name == NULL)
			continue;

		/* skip hosts that shouldn't be registered */
		if(temp_host->register_object == FALSE)
			continue;

		/* preprocess the hostgroup list, to change "grp1,grp2,grp3,!grp2" into "grp1,grp3" */
		/* 10/18/07 EG an empty return value means an error occured */
		if((hostgroup_names = xodtemplate_process_hostgroup_names(temp_host->host_groups, temp_host->_config_file, temp_host->_start_line)) == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to process hostgroup names for host '%s' (config file '%s', starting at line %d)\n",
				  temp_host->host_name, xodtemplate_config_file_name(temp_host->_config_file), temp_host->_start_line);
			return ERROR;
			}

		/* process the list of hostgroups */
		for(temp_ptr = strtok(hostgroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

			/* strip trailing spaces */
			strip(temp_ptr);

			/* find the hostgroup */
			temp_hostgroup = xodtemplate_find_real_hostgroup(temp_ptr);
			if(temp_hostgroup == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find hostgroup '%s' specified in host '%s' definition (config file '%s', starting on line %d)\n", temp_ptr, temp_host->host_name, xodtemplate_config_file_name(temp_host->_config_file), temp_host->_start_line);
				my_free(hostgroup_names);
				return ERROR;
				}
			if(!temp_hostgroup->member_map && !(temp_hostgroup->member_map = bitmap_create(xodcount.hosts))) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Failed to create bitmap to join host '%s' to group '%s'\n",
					  temp_host->host_name, temp_hostgroup->hostgroup_name);
				return ERROR;
				}

			/* add ourselves to the hostgroup member list */
			xodtemplate_add_hostgroup_member(temp_hostgroup, temp_host);
			}

		/* free memory */
		my_free(hostgroup_names);
		}

	/* expand subgroup membership recursively */
	for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup; temp_hostgroup = temp_hostgroup->next) {
		if(xodtemplate_recombobulate_hostgroup_subgroups(temp_hostgroup) != XOD_OK) {
			return ERROR;
			}
		/* rejects are no longer necessary */
		bitmap_destroy(temp_hostgroup->reject_map);
		/* make sure we don't recursively add subgroup members again */
		free_objectlist(&temp_hostgroup->group_list);
		}

	return OK;
	}



static int xodtemplate_recombobulate_servicegroup_subgroups(xodtemplate_servicegroup *temp_servicegroup) {
	objectlist *mlist, *glist;

	if(temp_servicegroup == NULL)
		return ERROR;

	if(temp_servicegroup->loop_status != XOD_NEW)
		return temp_servicegroup->loop_status;

	/* mark this as seen */
	temp_servicegroup->loop_status = XOD_SEEN;

	for(glist = temp_servicegroup->group_list; glist; glist = glist->next) {
		int result;
		xodtemplate_servicegroup *inc = (xodtemplate_servicegroup *)glist->object_ptr;

		result = xodtemplate_recombobulate_servicegroup_subgroups(inc);
		if(result != XOD_OK) {
			if(result == ERROR)
				return ERROR;
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Servicegroups '%s' and '%s' are part of a servicegroup_members include loop\n",
				  temp_servicegroup->servicegroup_name, inc->servicegroup_name);
			inc->loop_status = XOD_LOOPY;
			temp_servicegroup->loop_status = XOD_LOOPY;
			break;
			}
		for(mlist = inc->member_list; mlist; mlist = mlist->next) {
			xodtemplate_service *s = (xodtemplate_service *)mlist->object_ptr;
			if(xodtemplate_add_servicegroup_member(temp_servicegroup, s) != OK) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to add service '%s' on host '%s' as a subgroup member of servicegroup '%s' from servicegroup '%s'\n",
					  s->host_name, s->service_description, temp_servicegroup->servicegroup_name, inc->servicegroup_name);
				return ERROR;
				}
			}
		}

	if(temp_servicegroup->loop_status == XOD_SEEN)
		temp_servicegroup->loop_status = XOD_OK;

	return temp_servicegroup->loop_status;
	}

/* recombobulates servicegroup definitions */
/***** THIS NEEDS TO BE CALLED AFTER OBJECTS (SERVICES) ARE RESOLVED AND DUPLICATED *****/
int xodtemplate_recombobulate_servicegroups(void) {
	xodtemplate_service *temp_service = NULL;
	xodtemplate_servicegroup *temp_servicegroup = NULL;
	char *servicegroup_names = NULL;
	char *temp_ptr;

	/*
	 * expand servicegroup members. We need this to get the rejected ones
	 * before we add members from the servicelist.
	 */
	for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup; temp_servicegroup = temp_servicegroup->next) {
		objectlist *list, *next, *accept = NULL;

		if(temp_servicegroup->members == NULL && temp_servicegroup->servicegroup_members == NULL)
			continue;

		/* we'll need the member map */
		if(!(temp_servicegroup->member_map = bitmap_create(xodcount.services))) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not create member map for servicegroup '%s'\n", temp_servicegroup->servicegroup_name);
			return ERROR;
			}

		/* resolve groups into a group-list */
		if(temp_servicegroup->servicegroup_members) {
			xodtemplate_servicegroup *sg;
			char *ptr, *next_ptr = NULL;
			for(ptr = temp_servicegroup->servicegroup_members; ptr; ptr = next_ptr + 1) {
				next_ptr = strchr(ptr, ',');
				if(next_ptr)
					*next_ptr = 0;
				strip(ptr);
				if(!(sg = xodtemplate_find_real_servicegroup(ptr))) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find member group '%s' specified in servicegroup '%s' (config file '%s', starting on line %d)\n", ptr, temp_servicegroup->servicegroup_name, xodtemplate_config_file_name(temp_servicegroup->_config_file), temp_servicegroup->_start_line);
					return ERROR;

				}
				add_object_to_objectlist(&temp_servicegroup->group_list, sg);
				if(!next_ptr)
					break;
				}
			my_free(temp_servicegroup->servicegroup_members);
			}

		/* move on if we have no members */
		if(temp_servicegroup->members == NULL)
			continue;

		/* we might need this */
		if(!use_precached_objects && !(temp_servicegroup->reject_map = bitmap_create(xodcount.services))) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not create reject map for hostgroup '%s'\n", temp_servicegroup->servicegroup_name);
			return ERROR;
			}

		/* get list of service members in the servicegroup */
		xodtemplate_expand_services(&accept, temp_servicegroup->reject_map, NULL, temp_servicegroup->members, temp_servicegroup->_config_file, temp_servicegroup->_start_line);
		if(!accept && !bitmap_count_set_bits(temp_servicegroup->reject_map)) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not expand members specified in servicegroup '%s' (config file '%s', starting at line %d)\n", temp_servicegroup->servicegroup_name, xodtemplate_config_file_name(temp_servicegroup->_config_file), temp_servicegroup->_start_line);
			return ERROR;
			}

		/* we don't need this anymore */
		my_free(temp_servicegroup->members);

		for(list = accept; list; list = next) {
			xodtemplate_service *s = (xodtemplate_service *)list->object_ptr;
			next = list->next;
			free(list);
			xodtemplate_add_servicegroup_member(temp_servicegroup, s);
			}
		}

	/* if we're using precached objects we can bail out now */
	if(use_precached_objects == TRUE)
		return OK;

	/* Add services from 'servicegroups' directive */
	for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* skip services without servicegroup directives or service names */
		if(temp_service->service_groups == NULL || temp_service->host_name == NULL || temp_service->service_description == NULL)
			continue;

		/* skip services that shouldn't be registered */
		if(temp_service->register_object == FALSE)
			continue;

		/* preprocess the servicegroup list, to change "grp1,grp2,grp3,!grp2" into "grp1,grp3" */
		/* 10/19/07 EG an empty return value means an error occured */
		if((servicegroup_names = xodtemplate_process_servicegroup_names(temp_service->service_groups, temp_service->_config_file, temp_service->_start_line)) == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to process servicegroup names for service '%s' on host '%s' (config file '%s', starting at line %d)\n",
				  temp_service->service_description, temp_service->host_name, xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
			return ERROR;
			}

		/* process the list of servicegroups */
		for(temp_ptr = strtok(servicegroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

			/* strip trailing spaces */
			strip(temp_ptr);

			/* find the servicegroup */
			temp_servicegroup = xodtemplate_find_real_servicegroup(temp_ptr);
			if(temp_servicegroup == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find servicegroup '%s' specified in service '%s' on host '%s' definition (config file '%s', starting on line %d)\n", temp_ptr, temp_service->service_description, temp_service->host_name, xodtemplate_config_file_name(temp_service->_config_file), temp_service->_start_line);
				my_free(servicegroup_names);
				return ERROR;
				}

			if(!temp_servicegroup->member_map && !(temp_servicegroup->member_map = bitmap_create(xodcount.services))) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to create member map for service group %s\n", temp_servicegroup->servicegroup_name);
				return ERROR;
				}
			/* add ourselves as members to the group */
			xodtemplate_add_servicegroup_member(temp_servicegroup, temp_service);
		}

		/* free servicegroup names */
		my_free(servicegroup_names);
		}

	/* expand subgroup membership recursively */
	for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup; temp_servicegroup = temp_servicegroup->next) {
		if(xodtemplate_recombobulate_servicegroup_subgroups(temp_servicegroup) != XOD_OK) {
			return ERROR;
			}
		/* rejects are no longer necessary */
		bitmap_destroy(temp_servicegroup->reject_map);
		/* make sure we don't recursively add subgroup members again */
		free_objectlist(&temp_servicegroup->group_list);
		}

	return OK;
	}




/******************************************************************/
/******************* OBJECT SEARCH FUNCTIONS **********************/
/******************************************************************/

#ifdef NSCORE

/* finds a specific timeperiod object */
xodtemplate_timeperiod *xodtemplate_find_timeperiod(char *name) {
	xodtemplate_timeperiod temp_timeperiod;

	if(name == NULL)
		return NULL;

	temp_timeperiod.name = name;

	return skiplist_find_first(xobject_template_skiplists[TIMEPERIOD_SKIPLIST], &temp_timeperiod, NULL);
	}


/* finds a specific command object */
xodtemplate_command *xodtemplate_find_command(char *name) {
	xodtemplate_command temp_command;

	if(name == NULL)
		return NULL;

	temp_command.name = name;

	return skiplist_find_first(xobject_template_skiplists[COMMAND_SKIPLIST], &temp_command, NULL);
	}

#endif

/* finds a specific contactgroup object */
xodtemplate_contactgroup *xodtemplate_find_contactgroup(char *name) {
	xodtemplate_contactgroup temp_contactgroup;

	if(name == NULL)
		return NULL;

	temp_contactgroup.name = name;

	return skiplist_find_first(xobject_template_skiplists[CONTACTGROUP_SKIPLIST], &temp_contactgroup, NULL);
	}


/* finds a specific contactgroup object by its REAL name, not its TEMPLATE name */
xodtemplate_contactgroup *xodtemplate_find_real_contactgroup(char *name) {
	xodtemplate_contactgroup temp_contactgroup;

	if(name == NULL)
		return NULL;

	temp_contactgroup.contactgroup_name = name;

	return skiplist_find_first(xobject_skiplists[CONTACTGROUP_SKIPLIST], &temp_contactgroup, NULL);
	}


/* finds a specific hostgroup object */
xodtemplate_hostgroup *xodtemplate_find_hostgroup(char *name) {
	xodtemplate_hostgroup temp_hostgroup;

	if(name == NULL)
		return NULL;

	temp_hostgroup.name = name;

	return skiplist_find_first(xobject_template_skiplists[HOSTGROUP_SKIPLIST], &temp_hostgroup, NULL);
	}


/* finds a specific hostgroup object by its REAL name, not its TEMPLATE name */
xodtemplate_hostgroup *xodtemplate_find_real_hostgroup(char *name) {
	xodtemplate_hostgroup temp_hostgroup;

	if(name == NULL)
		return NULL;

	temp_hostgroup.hostgroup_name = name;

	return skiplist_find_first(xobject_skiplists[HOSTGROUP_SKIPLIST], &temp_hostgroup, NULL);
	}


/* finds a specific servicegroup object */
xodtemplate_servicegroup *xodtemplate_find_servicegroup(char *name) {
	xodtemplate_servicegroup temp_servicegroup;

	if(name == NULL)
		return NULL;

	temp_servicegroup.name = name;

	return skiplist_find_first(xobject_template_skiplists[SERVICEGROUP_SKIPLIST], &temp_servicegroup, NULL);
	}


/* finds a specific servicegroup object by its REAL name, not its TEMPLATE name */
xodtemplate_servicegroup *xodtemplate_find_real_servicegroup(char *name) {
	xodtemplate_servicegroup temp_servicegroup;

	if(name == NULL)
		return NULL;

	temp_servicegroup.servicegroup_name = name;

	return skiplist_find_first(xobject_skiplists[SERVICEGROUP_SKIPLIST], &temp_servicegroup, NULL);
	}


/* finds a specific servicedependency object */
xodtemplate_servicedependency *xodtemplate_find_servicedependency(char *name) {
	xodtemplate_servicedependency temp_servicedependency;

	if(name == NULL)
		return NULL;

	temp_servicedependency.name = name;

	return skiplist_find_first(xobject_template_skiplists[SERVICEDEPENDENCY_SKIPLIST], &temp_servicedependency, NULL);
	}


/* finds a specific serviceescalation object */
xodtemplate_serviceescalation *xodtemplate_find_serviceescalation(char *name) {
	xodtemplate_serviceescalation temp_serviceescalation;

	if(name == NULL)
		return NULL;

	temp_serviceescalation.name = name;

	return skiplist_find_first(xobject_template_skiplists[SERVICEESCALATION_SKIPLIST], &temp_serviceescalation, NULL);
	}


/* finds a specific contact object */
xodtemplate_contact *xodtemplate_find_contact(char *name) {
	xodtemplate_contact temp_contact;

	if(name == NULL)
		return NULL;

	temp_contact.name = name;

	return skiplist_find_first(xobject_template_skiplists[CONTACT_SKIPLIST], &temp_contact, NULL);
	}


/* finds a specific contact object by its REAL name, not its TEMPLATE name */
xodtemplate_contact *xodtemplate_find_real_contact(char *name) {
	xodtemplate_contact temp_contact;

	if(name == NULL)
		return NULL;

	temp_contact.contact_name = name;

	return skiplist_find_first(xobject_skiplists[CONTACT_SKIPLIST], &temp_contact, NULL);
	}


/* finds a specific host object */
xodtemplate_host *xodtemplate_find_host(char *name) {
	xodtemplate_host temp_host;

	if(name == NULL)
		return NULL;

	temp_host.name = name;

	return skiplist_find_first(xobject_template_skiplists[HOST_SKIPLIST], &temp_host, NULL);
	}


/* finds a specific host object by its REAL name, not its TEMPLATE name */
xodtemplate_host *xodtemplate_find_real_host(char *name) {
	xodtemplate_host temp_host;

	if(name == NULL)
		return NULL;

	temp_host.host_name = name;

	return skiplist_find_first(xobject_skiplists[HOST_SKIPLIST], &temp_host, NULL);
	}


/* finds a specific hostdependency object */
xodtemplate_hostdependency *xodtemplate_find_hostdependency(char *name) {
	xodtemplate_hostdependency temp_hostdependency;

	if(name == NULL)
		return NULL;

	temp_hostdependency.name = name;

	return skiplist_find_first(xobject_template_skiplists[HOSTDEPENDENCY_SKIPLIST], &temp_hostdependency, NULL);
	}


/* finds a specific hostescalation object */
xodtemplate_hostescalation *xodtemplate_find_hostescalation(char *name) {
	xodtemplate_hostescalation temp_hostescalation;

	if(name == NULL)
		return NULL;

	temp_hostescalation.name = name;

	return skiplist_find_first(xobject_template_skiplists[HOSTESCALATION_SKIPLIST], &temp_hostescalation, NULL);
	}


/* finds a specific hostextinfo object */
xodtemplate_hostextinfo *xodtemplate_find_hostextinfo(char *name) {
	xodtemplate_hostextinfo temp_hostextinfo;

	if(name == NULL)
		return NULL;

	temp_hostextinfo.name = name;

	return skiplist_find_first(xobject_template_skiplists[HOSTEXTINFO_SKIPLIST], &temp_hostextinfo, NULL);
	}


/* finds a specific serviceextinfo object */
xodtemplate_serviceextinfo *xodtemplate_find_serviceextinfo(char *name) {
	xodtemplate_serviceextinfo temp_serviceextinfo;

	if(name == NULL)
		return NULL;

	temp_serviceextinfo.name = name;

	return skiplist_find_first(xobject_template_skiplists[SERVICEEXTINFO_SKIPLIST], &temp_serviceextinfo, NULL);
	}


/* finds a specific service object */
xodtemplate_service *xodtemplate_find_service(char *name) {
	xodtemplate_service temp_service;

	if(name == NULL)
		return NULL;

	temp_service.name = name;

	return skiplist_find_first(xobject_template_skiplists[SERVICE_SKIPLIST], &temp_service, NULL);
	}

/* finds a specific service object by its REAL name, not its TEMPLATE name */
xodtemplate_service *xodtemplate_find_real_service(char *host_name, char *service_description) {
	xodtemplate_service temp_service;

	if(host_name == NULL || service_description == NULL)
		return NULL;

	temp_service.host_name = host_name;
	temp_service.service_description = service_description;

	return skiplist_find_first(xobject_skiplists[SERVICE_SKIPLIST], &temp_service, NULL);
	}




/******************************************************************/
/**************** OBJECT REGISTRATION FUNCTIONS *******************/
/******************************************************************/

static int xodtemplate_register_contactgroup_members(xodtemplate_contactgroup *this_contactgroup)
{
	objectlist *list;
	struct contactgroup *cg;
	int num_regs = 0;

	if (!this_contactgroup->register_object)
		return 0;

	cg = find_contactgroup(this_contactgroup->contactgroup_name);
	for(list = this_contactgroup->member_list; list; list = list->next) {
		xodtemplate_contact *c = (xodtemplate_contact *)list->object_ptr;
		if (!add_contact_to_contactgroup(cg, c->contact_name)) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Bad member of contactgroup '%s' (config file '%s', starting on line %d)\n", cg->group_name, xodtemplate_config_file_name(this_contactgroup->_config_file), this_contactgroup->_start_line);
			return -1;
			}
		num_regs++;
		}
	return num_regs;
}

static int xodtemplate_register_hostgroup_members(xodtemplate_hostgroup *this_hostgroup)
{
	objectlist *list;
	struct hostgroup *hg;
	int num_regs = 0;

	if (!this_hostgroup->register_object)
		return 0;

	hg = find_hostgroup(this_hostgroup->hostgroup_name);
	for(list = this_hostgroup->member_list; list; list = list->next) {
		xodtemplate_host *h = (xodtemplate_host *)list->object_ptr;
		if (!add_host_to_hostgroup(hg, h->host_name)) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Bad member of hostgroup '%s' (config file '%s', starting on line %d)\n", hg->group_name, xodtemplate_config_file_name(this_hostgroup->_config_file), this_hostgroup->_start_line);
			return -1;
			}
		num_regs++;
		}
	return num_regs;
	}

static int xodtemplate_register_servicegroup_members(xodtemplate_servicegroup *this_servicegroup)
{
	objectlist *list, *next;
	struct servicegroup *sg;
	int num_regs = 0;

	if (!this_servicegroup->register_object)
		return 0;

	sg = find_servicegroup(this_servicegroup->servicegroup_name);
	for(list = this_servicegroup->member_list; list; list = next) {
		xodtemplate_service *s = (xodtemplate_service *)list->object_ptr;
		next = list->next;
		if (!add_service_to_servicegroup(sg, s->host_name, s->service_description)) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Bad member of servicegroup '%s' (config file '%s', starting on line %d)\n", sg->group_name, xodtemplate_config_file_name(this_servicegroup->_config_file), this_servicegroup->_start_line);
			return -1;
			}
		num_regs++;
		}

	return num_regs;
	}

/*
 * registers object definitions
 * The order goes like this:
 *   Timeperiods
 *   Commands
 *   Contactgroups
 *   Hostgroups
 *   Servicegroups
 *   Contacts
 *   Hosts
 *   Services
 *   Servicedependencies
 *   Serviceescalations
 *   Hostdependencies
 *   Hostescalations
 *
 * Why are contactgroups done before contacts? A reasonable assumption
 * would be that contacts should be flattened and added to the checked
 * objects directly rather than forcing us to fiddle with that crap
 * during runtime.
 */
int xodtemplate_register_objects(void) {
	unsigned int i;
	int mcount;
	xodtemplate_timeperiod *temp_timeperiod = NULL;
	xodtemplate_command *temp_command = NULL;
	xodtemplate_contactgroup *temp_contactgroup = NULL;
	xodtemplate_hostgroup *temp_hostgroup = NULL;
	xodtemplate_servicegroup *temp_servicegroup = NULL;
	xodtemplate_contact *temp_contact = NULL;
	xodtemplate_host *temp_host = NULL;
	xodtemplate_service *temp_service = NULL;
	void *ptr = NULL;
	xodtemplate_hostdependency *hd, *next_hd;
	xodtemplate_hostescalation *he, *next_he;
	xodtemplate_servicedependency *sd, *next_sd;
	xodtemplate_serviceescalation *se, *next_se;
	unsigned int ocount[NUM_OBJECT_SKIPLISTS];
	unsigned int tot_members = 0;


	for (i = 0; i < ARRAY_SIZE(ocount); i++) {
		ocount[i] = (unsigned int)skiplist_num_items(xobject_skiplists[i]);
	}

	/* dependencies are handled specially */
	ocount[SERVICEDEPENDENCY_SKIPLIST] = 0;
	ocount[HOSTDEPENDENCY_SKIPLIST] = 0;
	ocount[HOSTESCALATION_SKIPLIST] = xodcount.hostescalations;
	ocount[SERVICEESCALATION_SKIPLIST] = xodcount.serviceescalations;

	if (create_object_tables(ocount) != OK) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Failed to create object tables\n");
		return ERROR;
	}

	/* register timeperiods */
	ptr = NULL;
	for(temp_timeperiod = (xodtemplate_timeperiod *)skiplist_get_first(xobject_skiplists[TIMEPERIOD_SKIPLIST], &ptr); temp_timeperiod != NULL; temp_timeperiod = (xodtemplate_timeperiod *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_timeperiod(temp_timeperiod) == ERROR)
			return ERROR;
		}
	timing_point("%u timeperiods registered\n", num_objects.timeperiods);

	/* register commands */
	ptr = NULL;
	for(temp_command = (xodtemplate_command *)skiplist_get_first(xobject_skiplists[COMMAND_SKIPLIST], &ptr); temp_command != NULL; temp_command = (xodtemplate_command *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_command(temp_command) == ERROR)
			return ERROR;
		}
	timing_point("%u commands registered\n", num_objects.commands);

	/* register contactgroups */
	ptr = NULL;
	for(temp_contactgroup = (xodtemplate_contactgroup *)skiplist_get_first(xobject_skiplists[CONTACTGROUP_SKIPLIST], &ptr); temp_contactgroup != NULL; temp_contactgroup = (xodtemplate_contactgroup *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_contactgroup(temp_contactgroup) == ERROR)
			return ERROR;
		}
	timing_point("%u contactgroups registered\n", num_objects.contactgroups);

	/* register hostgroups */
	ptr = NULL;
	for(temp_hostgroup = (xodtemplate_hostgroup *)skiplist_get_first(xobject_skiplists[HOSTGROUP_SKIPLIST], &ptr); temp_hostgroup != NULL; temp_hostgroup = (xodtemplate_hostgroup *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_hostgroup(temp_hostgroup) == ERROR)
			return ERROR;
		}
	timing_point("%u hostgroups registered\n", num_objects.hostgroups);

	/* register servicegroups */
	ptr = NULL;
	for(temp_servicegroup = (xodtemplate_servicegroup *)skiplist_get_first(xobject_skiplists[SERVICEGROUP_SKIPLIST], &ptr); temp_servicegroup != NULL; temp_servicegroup = (xodtemplate_servicegroup *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_servicegroup(temp_servicegroup) == ERROR)
			return ERROR;
		}
	timing_point("%u servicegroups registered\n", num_objects.servicegroups);

	/* register contacts */
	ptr = NULL;
	for(temp_contact = (xodtemplate_contact *)skiplist_get_first(xobject_skiplists[CONTACT_SKIPLIST], &ptr); temp_contact != NULL; temp_contact = (xodtemplate_contact *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_contact(temp_contact) == ERROR)
			return ERROR;
		}
	timing_point("%u contacts registered\n", num_objects.contacts);

	/* register hosts */
	ptr = NULL;
	for(temp_host = (xodtemplate_host *)skiplist_get_first(xobject_skiplists[HOST_SKIPLIST], &ptr); temp_host != NULL; temp_host = (xodtemplate_host *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_host(temp_host) == ERROR)
			return ERROR;
		}
	timing_point("%u hosts registered\n", num_objects.hosts);

	/* register services */
	ptr = NULL;
	for(temp_service = (xodtemplate_service *)skiplist_get_first(xobject_skiplists[SERVICE_SKIPLIST], &ptr); temp_service != NULL; temp_service = (xodtemplate_service *)skiplist_get_next(&ptr)) {
		if(xodtemplate_register_service(temp_service) == ERROR)
			return ERROR;
		}
	timing_point("%u services registered\n", num_objects.services);

	/* groups and objects are registered, so join them up */
	/* register contactgroup members */
	ptr = NULL; tot_members = 0;
	for(temp_contactgroup = (xodtemplate_contactgroup *)skiplist_get_first(xobject_skiplists[CONTACTGROUP_SKIPLIST], &ptr); temp_contactgroup != NULL; temp_contactgroup = (xodtemplate_contactgroup *)skiplist_get_next(&ptr)) {
		if((mcount = xodtemplate_register_contactgroup_members(temp_contactgroup)) < 0)
			return ERROR;
		tot_members += mcount;
		}
	timing_point("%u contactgroup memberships registered\n", tot_members);

	/* register hostgroup members */
	ptr = NULL; tot_members = 0;
	for(temp_hostgroup = (xodtemplate_hostgroup *)skiplist_get_first(xobject_skiplists[HOSTGROUP_SKIPLIST], &ptr); temp_hostgroup != NULL; temp_hostgroup = (xodtemplate_hostgroup *)skiplist_get_next(&ptr)) {
		if((mcount = xodtemplate_register_hostgroup_members(temp_hostgroup)) < 0)
			return ERROR;
		tot_members += mcount;
		}
	timing_point("%u hostgroup memberships registered\n", tot_members);

	/* register servicegroup members */
	ptr = NULL; tot_members = 0;
	for(temp_servicegroup = (xodtemplate_servicegroup *)skiplist_get_first(xobject_skiplists[SERVICEGROUP_SKIPLIST], &ptr); temp_servicegroup != NULL; temp_servicegroup = (xodtemplate_servicegroup *)skiplist_get_next(&ptr)) {
		if((mcount = xodtemplate_register_servicegroup_members(temp_servicegroup)) < 0)
			return ERROR;
		tot_members += mcount;
		}
	timing_point("%u servicegroup memberships registered\n", tot_members);

	/*
	 * These aren't in skiplists at all, but it's safe to destroy
	 * them as we go along, since all dupes are at the head of the list
	 */
	if(xodtemplate_servicedependency_list) {
		parent_map = bitmap_create(xodcount.services);
		if(!parent_map) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to create parent bitmap for service dependencies\n");
			return ERROR;
			}
		for(sd = xodtemplate_servicedependency_list; sd; sd = next_sd) {
			next_sd = sd->next;
#ifdef NSCGI
			if(xodtemplate_register_servicedependency(sd) == ERROR)
				return ERROR;
#else
			if(xodtemplate_register_and_destroy_servicedependency(sd) == ERROR)
				return ERROR;
#endif
			}
		bitmap_destroy(parent_map);
		parent_map = NULL;
		}
	timing_point("%u unique / %u total servicedependencies registered\n",
				 num_objects.servicedependencies, xodcount.servicedependencies);

	for(se = xodtemplate_serviceescalation_list; se; se = next_se) {
		next_se = se->next;
		if(xodtemplate_register_and_destroy_serviceescalation(se) == ERROR)
			return ERROR;
		}
	timing_point("%u serviceescalations registered\n", num_objects.serviceescalations);

	for(hd = xodtemplate_hostdependency_list; hd; hd = next_hd) {
		next_hd = hd->next;
#ifdef NSCGI
		if(xodtemplate_register_hostdependency(hd) == ERROR)
			return ERROR;
#else
		if(xodtemplate_register_and_destroy_hostdependency(hd) == ERROR)
			return ERROR;
#endif
		}
	timing_point("%u unique / %u total hostdependencies registered\n",
				 num_objects.hostdependencies, xodcount.hostdependencies);

	for(he = xodtemplate_hostescalation_list; he; he = next_he) {
		next_he = he->next;
		if(xodtemplate_register_and_destroy_hostescalation(he) == ERROR)
			return ERROR;
		}
	timing_point("%u hostescalations registered\n", num_objects.hostescalations);

	return OK;
	}



/* registers a timeperiod definition */
int xodtemplate_register_timeperiod(xodtemplate_timeperiod *this_timeperiod) {
	xodtemplate_daterange *temp_daterange = NULL;
	timeperiod *new_timeperiod = NULL;
	daterange *new_daterange = NULL;
	timerange *new_timerange = NULL;
	timeperiodexclusion *new_timeperiodexclusion = NULL;
	int day = 0;
	int range = 0;
	int x = 0;
	char *day_range_ptr = NULL;
	char *day_range_start_buffer = NULL;
	char *temp_ptr = NULL;
	unsigned long range_start_time = 0L;
	unsigned long range_end_time = 0L;


	/* bail out if we shouldn't register this object */
	if(this_timeperiod->register_object == FALSE)
		return OK;

	/* add the timeperiod */
	new_timeperiod = add_timeperiod(this_timeperiod->timeperiod_name, this_timeperiod->alias);

	/* return with an error if we couldn't add the timeperiod */
	if(new_timeperiod == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register timeperiod (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
		return ERROR;
		}

	/* add all exceptions to timeperiod */
	for(x = 0; x < DATERANGE_TYPES; x++) {
		for(temp_daterange = this_timeperiod->exceptions[x]; temp_daterange != NULL; temp_daterange = temp_daterange->next) {

			/* skip null entries */
			if(temp_daterange->timeranges == NULL || !strcmp(temp_daterange->timeranges, XODTEMPLATE_NULL))
				continue;

			/* add new exception to timeperiod */
			new_daterange = add_exception_to_timeperiod(new_timeperiod, temp_daterange->type, temp_daterange->syear, temp_daterange->smon, temp_daterange->smday, temp_daterange->swday, temp_daterange->swday_offset, temp_daterange->eyear, temp_daterange->emon, temp_daterange->emday, temp_daterange->ewday, temp_daterange->ewday_offset, temp_daterange->skip_interval);
			if(new_daterange == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add date exception to timeperiod (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
				return ERROR;
				}

			/* add timeranges to exception */
			day_range_ptr = temp_daterange->timeranges;
			range = 0;
			for(day_range_start_buffer = my_strsep(&day_range_ptr, ", "); day_range_start_buffer != NULL; day_range_start_buffer = my_strsep(&day_range_ptr, ", ")) {

				range++;

				/* get time ranges */
				if(xodtemplate_get_time_ranges(day_range_start_buffer, &range_start_time, &range_end_time) == ERROR) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not parse timerange #%d of timeperiod (config file '%s', starting on line %d)\n", range, xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
					return ERROR;
					}

				/* add the new time range to the date range */
				new_timerange = add_timerange_to_daterange(new_daterange, range_start_time, range_end_time);
				if(new_timerange == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add timerange #%d to timeperiod (config file '%s', starting on line %d)\n", range, xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
					return ERROR;
					}
				}
			}
		}

	/* add all necessary timeranges to timeperiod */
	for(day = 0; day < 7; day++) {

		/* skip null entries */
		if(this_timeperiod->timeranges[day] == NULL || !strcmp(this_timeperiod->timeranges[day], XODTEMPLATE_NULL))
			continue;

		day_range_ptr = this_timeperiod->timeranges[day];
		range = 0;
		for(day_range_start_buffer = my_strsep(&day_range_ptr, ", "); day_range_start_buffer != NULL; day_range_start_buffer = my_strsep(&day_range_ptr, ", ")) {

			range++;

			/* get time ranges */
			if(xodtemplate_get_time_ranges(day_range_start_buffer, &range_start_time, &range_end_time) == ERROR) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not parse timerange #%d for day %d of timeperiod (config file '%s', starting on line %d)\n", range, day, xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
				return ERROR;
				}

			/* add the new time range to the time period */
			new_timerange = add_timerange_to_timeperiod(new_timeperiod, day, range_start_time, range_end_time);
			if(new_timerange == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add timerange #%d for day %d to timeperiod (config file '%s', starting on line %d)\n", range, day, xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
				return ERROR;
				}
			}

		}

	/* add timeperiod exclusions */
	if(this_timeperiod->exclusions) {
		for(temp_ptr = strtok(this_timeperiod->exclusions, ","); temp_ptr != NULL; temp_ptr = strtok(NULL, ",")) {
			strip(temp_ptr);
			new_timeperiodexclusion = add_exclusion_to_timeperiod(new_timeperiod, temp_ptr);
			if(new_timeperiodexclusion == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add excluded timeperiod '%s' to timeperiod (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(this_timeperiod->_config_file), this_timeperiod->_start_line);
				return ERROR;
				}
			}
		}

	return OK;
	}



/* parses timerange string into start and end minutes */
int xodtemplate_get_time_ranges(char *buf, unsigned long *range_start, unsigned long *range_end) {
	char *range_ptr = NULL;
	char *range_buffer = NULL;
	char *time_ptr = NULL;
	char *time_buffer = NULL;
	int hours = 0;
	int minutes = 0;

	if(buf == NULL || range_start == NULL || range_end == NULL)
		return ERROR;

	range_ptr = buf;
	range_buffer = my_strsep(&range_ptr, "-");
	if(range_buffer == NULL)
		return ERROR;

	time_ptr = range_buffer;
	time_buffer = my_strsep(&time_ptr, ":");
	if(time_buffer == NULL)
		return ERROR;
	hours = atoi(time_buffer);

	time_buffer = my_strsep(&time_ptr, ":");
	if(time_buffer == NULL)
		return ERROR;
	minutes = atoi(time_buffer);

	/* calculate the range start time in seconds */
	*range_start = (unsigned long)((minutes * 60) + (hours * 60 * 60));

	range_buffer = my_strsep(&range_ptr, "-");
	if(range_buffer == NULL)
		return ERROR;

	time_ptr = range_buffer;
	time_buffer = my_strsep(&time_ptr, ":");
	if(time_buffer == NULL)
		return ERROR;
	hours = atoi(time_buffer);

	time_buffer = my_strsep(&time_ptr, ":");
	if(time_buffer == NULL)
		return ERROR;
	minutes = atoi(time_buffer);

	/* calculate the range end time in seconds */
	*range_end = (unsigned long)((minutes * 60) + (hours * 3600));

	return OK;
	}



/* registers a command definition */
int xodtemplate_register_command(xodtemplate_command *this_command) {
	command *new_command = NULL;

	/* bail out if we shouldn't register this object */
	if(this_command->register_object == FALSE)
		return OK;

	/* add the command */
	new_command = add_command(this_command->command_name, this_command->command_line);

	/* return with an error if we couldn't add the command */
	if(new_command == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register command (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_command->_config_file), this_command->_start_line);
		return ERROR;
		}

	return OK;
	}


/* registers a contactgroup definition */
int xodtemplate_register_contactgroup(xodtemplate_contactgroup *this_contactgroup) {
	contactgroup *new_contactgroup = NULL;

	/* bail out if we shouldn't register this object */
	if(this_contactgroup->register_object == FALSE)
		return OK;

	/* add the contact group */
	new_contactgroup = add_contactgroup(this_contactgroup->contactgroup_name, this_contactgroup->alias);

	/* return with an error if we couldn't add the contactgroup */
	if(new_contactgroup == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register contactgroup (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_contactgroup->_config_file), this_contactgroup->_start_line);
		return ERROR;
		}

	return OK;
	}



/* registers a hostgroup definition */
int xodtemplate_register_hostgroup(xodtemplate_hostgroup *this_hostgroup) {
	hostgroup *new_hostgroup = NULL;

	/* bail out if we shouldn't register this object */
	if(this_hostgroup->register_object == FALSE)
		return OK;

	/* add the  host group */
	new_hostgroup = add_hostgroup(this_hostgroup->hostgroup_name, this_hostgroup->alias, this_hostgroup->notes, this_hostgroup->notes_url, this_hostgroup->action_url);

	/* return with an error if we couldn't add the hostgroup */
	if(new_hostgroup == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register hostgroup (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_hostgroup->_config_file), this_hostgroup->_start_line);
		return ERROR;
		}

	return OK;
	}

/* registers a servicegroup definition */
int xodtemplate_register_servicegroup(xodtemplate_servicegroup *this_servicegroup) {
	servicegroup *new_servicegroup = NULL;

	/* bail out if we shouldn't register this object */
	if(this_servicegroup->register_object == FALSE)
		return OK;

	/* add the  service group */
	new_servicegroup = add_servicegroup(this_servicegroup->servicegroup_name, this_servicegroup->alias, this_servicegroup->notes, this_servicegroup->notes_url, this_servicegroup->action_url);

	/* return with an error if we couldn't add the servicegroup */
	if(new_servicegroup == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register servicegroup (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_servicegroup->_config_file), this_servicegroup->_start_line);
		return ERROR;
		}

	return OK;
	}


/* registers a servicedependency definition */
int xodtemplate_register_servicedependency(xodtemplate_servicedependency *this_servicedependency) {
	servicedependency *new_servicedependency = NULL;

	/* bail out if we shouldn't register this object */
	if(this_servicedependency->register_object == FALSE)
		return OK;

	/* throw a warning on servicedeps that have no options */
	if(this_servicedependency->have_notification_failure_options == FALSE && this_servicedependency->have_execution_failure_options == FALSE) {
		logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Ignoring lame service dependency (config file '%s', line %d)\n", xodtemplate_config_file_name(this_servicedependency->_config_file), this_servicedependency->_start_line);
		return OK;
		}

	/* add the servicedependency */
	if(this_servicedependency->have_execution_failure_options == TRUE) {
		xodcount.servicedependencies++;

		new_servicedependency = add_service_dependency(this_servicedependency->dependent_host_name, this_servicedependency->dependent_service_description, this_servicedependency->host_name, this_servicedependency->service_description, EXECUTION_DEPENDENCY, this_servicedependency->inherits_parent, this_servicedependency->execution_failure_options, this_servicedependency->dependency_period);

		/* return with an error if we couldn't add the servicedependency */
		if(new_servicedependency == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register service execution dependency (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_servicedependency->_config_file), this_servicedependency->_start_line);
			return ERROR;
			}
		}
	if(this_servicedependency->have_notification_failure_options == TRUE) {
		xodcount.servicedependencies++;

		new_servicedependency = add_service_dependency(this_servicedependency->dependent_host_name, this_servicedependency->dependent_service_description, this_servicedependency->host_name, this_servicedependency->service_description, NOTIFICATION_DEPENDENCY, this_servicedependency->inherits_parent, this_servicedependency->notification_failure_options, this_servicedependency->dependency_period);

		/* return with an error if we couldn't add the servicedependency */
		if(new_servicedependency == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register service notification dependency (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_servicedependency->_config_file), this_servicedependency->_start_line);
			return ERROR;
			}
		}

	return OK;
	}



/* registers a serviceescalation definition */
int xodtemplate_register_serviceescalation(xodtemplate_serviceescalation *this_serviceescalation) {
	serviceescalation *new_serviceescalation = NULL;
	contactsmember *new_contactsmember = NULL;
	contactgroupsmember *new_contactgroupsmember = NULL;
	char *contact_name = NULL;
	char *contact_group = NULL;

	/* bail out if we shouldn't register this object */
	if(this_serviceescalation->register_object == FALSE)
		return OK;

	/* default options if none specified */
	if(this_serviceescalation->have_escalation_options == FALSE) {
		this_serviceescalation->escalation_options = OPT_ALL;
		}

	/* add the serviceescalation */
	new_serviceescalation = add_serviceescalation(this_serviceescalation->host_name, this_serviceescalation->service_description, this_serviceescalation->first_notification, this_serviceescalation->last_notification, this_serviceescalation->notification_interval, this_serviceescalation->escalation_period, this_serviceescalation->escalation_options);

	/* return with an error if we couldn't add the serviceescalation */
	if(new_serviceescalation == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register service escalation (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_serviceescalation->_config_file), this_serviceescalation->_start_line);
		return ERROR;
		}

	/* add the contact groups */
	if(this_serviceescalation->contact_groups != NULL) {

		for(contact_group = strtok(this_serviceescalation->contact_groups, ","); contact_group != NULL; contact_group = strtok(NULL, ",")) {

			strip(contact_group);
			new_contactgroupsmember = add_contactgroup_to_serviceescalation(new_serviceescalation, contact_group);
			if(new_contactgroupsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contactgroup '%s' to service escalation (config file '%s', starting on line %d)\n", contact_group, xodtemplate_config_file_name(this_serviceescalation->_config_file), this_serviceescalation->_start_line);
				return ERROR;
				}
			}
		}

	/* add the contacts */
	if(this_serviceescalation->contacts != NULL) {

		for(contact_name = strtok(this_serviceescalation->contacts, ","); contact_name != NULL; contact_name = strtok(NULL, ",")) {

			strip(contact_name);
			new_contactsmember = add_contact_to_serviceescalation(new_serviceescalation, contact_name);
			if(new_contactsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contact '%s' to service escalation (config file '%s', starting on line %d)\n", contact_name, xodtemplate_config_file_name(this_serviceescalation->_config_file), this_serviceescalation->_start_line);
				return ERROR;
				}
			}
		}

	return OK;
	}



/* registers a contact definition */
int xodtemplate_register_contact(xodtemplate_contact *this_contact) {
	contact *new_contact = NULL;
	char *command_name = NULL;
	commandsmember *new_commandsmember = NULL;
	xodtemplate_customvariablesmember *temp_customvariablesmember = NULL;

	/* bail out if we shouldn't register this object */
	if(this_contact->register_object == FALSE)
		return OK;

	/* add the contact */
	new_contact = add_contact(this_contact->contact_name, this_contact->alias, this_contact->email, this_contact->pager, this_contact->address, this_contact->service_notification_period, this_contact->host_notification_period, this_contact->service_notification_options, this_contact->host_notification_options, this_contact->host_notifications_enabled, this_contact->service_notifications_enabled, this_contact->can_submit_commands, this_contact->retain_status_information, this_contact->retain_nonstatus_information, this_contact->minimum_value);

	/* return with an error if we couldn't add the contact */
	if(new_contact == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register contact (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_contact->_config_file), this_contact->_start_line);
		return ERROR;
		}

	/* add all the host notification commands */
	if(this_contact->host_notification_commands != NULL) {

		for(command_name = strtok(this_contact->host_notification_commands, ", "); command_name != NULL; command_name = strtok(NULL, ", ")) {
			new_commandsmember = add_host_notification_command_to_contact(new_contact, command_name);
			if(new_commandsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add host notification command '%s' to contact (config file '%s', starting on line %d)\n", command_name, xodtemplate_config_file_name(this_contact->_config_file), this_contact->_start_line);
				return ERROR;
				}
			}
		}

	/* add all the service notification commands */
	if(this_contact->service_notification_commands != NULL) {

		for(command_name = strtok(this_contact->service_notification_commands, ", "); command_name != NULL; command_name = strtok(NULL, ", ")) {
			new_commandsmember = add_service_notification_command_to_contact(new_contact, command_name);
			if(new_commandsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add service notification command '%s' to contact (config file '%s', starting on line %d)\n", command_name, xodtemplate_config_file_name(this_contact->_config_file), this_contact->_start_line);
				return ERROR;
				}
			}
		}

	/* add all custom variables */
	for(temp_customvariablesmember = this_contact->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
		if((add_custom_variable_to_contact(new_contact, temp_customvariablesmember->variable_name, temp_customvariablesmember->variable_value)) == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not custom variable to contact (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_contact->_config_file), this_contact->_start_line);
			return ERROR;
			}
		}

	return OK;
	}



/* registers a host definition */
int xodtemplate_register_host(xodtemplate_host *this_host) {
	host *new_host = NULL;
	char *parent_host = NULL;
	hostsmember *new_hostsmember = NULL;
	contactsmember *new_contactsmember = NULL;
	contactgroupsmember *new_contactgroupsmember = NULL;
	char *contact_name = NULL;
	char *contact_group = NULL;
	xodtemplate_customvariablesmember *temp_customvariablesmember = NULL;

	/* bail out if we shouldn't register this object */
	if(this_host->register_object == FALSE)
		return OK;

	/* add the host definition */
	new_host = add_host(this_host->host_name, this_host->display_name, this_host->alias, this_host->address, this_host->check_period, this_host->initial_state, this_host->check_interval, this_host->retry_interval, this_host->max_check_attempts, this_host->notification_options, this_host->notification_interval, this_host->first_notification_delay, this_host->notification_period, this_host->notifications_enabled, this_host->check_command, this_host->active_checks_enabled, this_host->passive_checks_enabled, this_host->event_handler, this_host->event_handler_enabled, this_host->flap_detection_enabled, this_host->low_flap_threshold, this_host->high_flap_threshold, this_host->flap_detection_options, this_host->stalking_options, this_host->process_perf_data, this_host->check_freshness, this_host->freshness_threshold, this_host->notes, this_host->notes_url, this_host->action_url, this_host->icon_image, this_host->icon_image_alt, this_host->vrml_image, this_host->statusmap_image, this_host->x_2d, this_host->y_2d, this_host->have_2d_coords, this_host->x_3d, this_host->y_3d, this_host->z_3d, this_host->have_3d_coords, TRUE, this_host->retain_status_information, this_host->retain_nonstatus_information, this_host->obsess, this_host->hourly_value);


	/* return with an error if we couldn't add the host */
	if(new_host == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register host (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_host->_config_file), this_host->_start_line);
		return ERROR;
		}

	/* add the parent hosts */
	if(this_host->parents != NULL) {

		for(parent_host = strtok(this_host->parents, ","); parent_host != NULL; parent_host = strtok(NULL, ",")) {
			strip(parent_host);
			new_hostsmember = add_parent_host_to_host(new_host, parent_host);
			if(new_hostsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add parent host '%s' to host (config file '%s', starting on line %d)\n", parent_host, xodtemplate_config_file_name(this_host->_config_file), this_host->_start_line);
				return ERROR;
				}
			}
		}

	/* add all contact groups to the host */
	if(this_host->contact_groups != NULL) {

		for(contact_group = strtok(this_host->contact_groups, ","); contact_group != NULL; contact_group = strtok(NULL, ",")) {

			strip(contact_group);
			new_contactgroupsmember = add_contactgroup_to_host(new_host, contact_group);
			if(new_contactgroupsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contactgroup '%s' to host (config file '%s', starting on line %d)\n", contact_group, xodtemplate_config_file_name(this_host->_config_file), this_host->_start_line);
				return ERROR;
				}
			}
		}

	/* add all contacts to the host */
	if(this_host->contacts != NULL) {

		for(contact_name = strtok(this_host->contacts, ","); contact_name != NULL; contact_name = strtok(NULL, ",")) {

			strip(contact_name);
			new_contactsmember = add_contact_to_host(new_host, contact_name);
			if(new_contactsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contact '%s' to host (config file '%s', starting on line %d)\n", contact_name, xodtemplate_config_file_name(this_host->_config_file), this_host->_start_line);
				return ERROR;
				}
			}
		}

	/* add all custom variables */
	for(temp_customvariablesmember = this_host->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
		if((add_custom_variable_to_host(new_host, temp_customvariablesmember->variable_name, temp_customvariablesmember->variable_value)) == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not custom variable to host (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_host->_config_file), this_host->_start_line);
			return ERROR;
			}
		}

	return OK;
	}



/* registers a service definition */
int xodtemplate_register_service(xodtemplate_service *this_service) {
	service *new_service = NULL;
	contactsmember *new_contactsmember = NULL;
	contactgroupsmember *new_contactgroupsmember = NULL;
	char *contact_name = NULL;
	char *contact_group = NULL;
	xodtemplate_customvariablesmember *temp_customvariablesmember = NULL;

	/* bail out if we shouldn't register this object */
	if(this_service->register_object == FALSE)
		return OK;

	/* add the service */
	new_service = add_service(this_service->host_name, this_service->service_description, this_service->display_name, this_service->check_period, this_service->initial_state, this_service->max_check_attempts, this_service->parallelize_check, this_service->passive_checks_enabled, this_service->check_interval, this_service->retry_interval, this_service->notification_interval, this_service->first_notification_delay, this_service->notification_period, this_service->notification_options, this_service->notifications_enabled, this_service->is_volatile, this_service->event_handler, this_service->event_handler_enabled, this_service->check_command, this_service->active_checks_enabled, this_service->flap_detection_enabled, this_service->low_flap_threshold, this_service->high_flap_threshold, this_service->flap_detection_options, this_service->stalking_options, this_service->process_perf_data, this_service->check_freshness, this_service->freshness_threshold, this_service->notes, this_service->notes_url, this_service->action_url, this_service->icon_image, this_service->icon_image_alt, this_service->retain_status_information, this_service->retain_nonstatus_information, this_service->obsess, this_service->hourly_value);

	/* return with an error if we couldn't add the service */
	if(new_service == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register service (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_service->_config_file), this_service->_start_line);
		return ERROR;
		}

	/* add all service parents */
	if(this_service->parents != NULL) {
		servicesmember *new_servicesmember;
		char *comma = strchr(this_service->parents, ',');

		if(!comma) { /* same-host single-service parent */
			new_servicesmember = add_parent_service_to_service(new_service, new_service->host_name, this_service->parents);
			if(new_servicesmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add same-host service parent '%s' to service (config file '%s', starting on line %d)\n",
					  this_service->parents,
					  xodtemplate_config_file_name(this_service->_config_file),
					  this_service->_start_line);
				return ERROR;
				}
			}
		else {
			/* Multiple parents, so let's do this the hard way */
			objectlist *list = NULL, *next;
			bitmap_clear(service_map);
			if(xodtemplate_expand_services(&list, service_map, NULL, this_service->parents, this_service->_config_file, this_service->_start_line) != OK) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to expand service parents (config file '%s', starting at line %d)\n",
					  xodtemplate_config_file_name(this_service->_config_file),
					  this_service->_start_line);
				return ERROR;
				}
			for(; list; list = next) {
				xodtemplate_service *parent = (xodtemplate_service *)list->object_ptr;
				next = list->next;
				free(list);
				new_servicesmember = add_parent_service_to_service(new_service, parent->host_name, parent->service_description);
				if(new_servicesmember == NULL) {
					logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add service '%s' on host '%s' as parent to service '%s' on host '%s' (config file '%s', starting on line %d)\n",
						  parent->host_name, parent->service_description,
						  new_service->host_name, new_service->description,
						  xodtemplate_config_file_name(this_service->_config_file),
						  this_service->_start_line);
					free_objectlist(&next);
					return ERROR;
					}
				}
			}
		}

	/* add all contact groups to the service */
	if(this_service->contact_groups != NULL) {

		for(contact_group = strtok(this_service->contact_groups, ","); contact_group != NULL; contact_group = strtok(NULL, ",")) {

			strip(contact_group);
			new_contactgroupsmember = add_contactgroup_to_service(new_service, contact_group);
			if(new_contactgroupsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contactgroup '%s' to service (config file '%s', starting on line %d)\n", contact_group, xodtemplate_config_file_name(this_service->_config_file), this_service->_start_line);
				return ERROR;
				}
			}
		}

	/* add all the contacts to the service */
	if(this_service->contacts != NULL) {

		for(contact_name = strtok(this_service->contacts, ","); contact_name != NULL; contact_name = strtok(NULL, ",")) {

			/* add this contact to the service definition */
			strip(contact_name);
			new_contactsmember = add_contact_to_service(new_service, contact_name);

			/* stop adding contacts if we ran into an error */
			if(new_contactsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contact '%s' to service (config file '%s', starting on line %d)\n", contact_name, xodtemplate_config_file_name(this_service->_config_file), this_service->_start_line);
				return ERROR;
				}
			}
		}

	/* add all custom variables */
	for(temp_customvariablesmember = this_service->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
		if((add_custom_variable_to_service(new_service, temp_customvariablesmember->variable_name, temp_customvariablesmember->variable_value)) == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not custom variable to service (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_service->_config_file), this_service->_start_line);
			return ERROR;
			}
		}

	return OK;
	}



/* registers a hostdependency definition */
int xodtemplate_register_hostdependency(xodtemplate_hostdependency *this_hostdependency) {
	hostdependency *new_hostdependency = NULL;

	/* bail out if we shouldn't register this object */
	if(this_hostdependency->register_object == FALSE)
		return OK;

	/* add the host execution dependency */
	if(this_hostdependency->have_execution_failure_options == TRUE) {
		xodcount.hostdependencies++;

		new_hostdependency = add_host_dependency(this_hostdependency->dependent_host_name, this_hostdependency->host_name, EXECUTION_DEPENDENCY, this_hostdependency->inherits_parent, this_hostdependency->execution_failure_options, this_hostdependency->dependency_period);

		/* return with an error if we couldn't add the hostdependency */
		if(new_hostdependency == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register host execution dependency (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_hostdependency->_config_file), this_hostdependency->_start_line);
			return ERROR;
			}
		}

	/* add the host notification dependency */
	if(this_hostdependency->have_notification_failure_options == TRUE) {
		xodcount.hostdependencies++;

		new_hostdependency = add_host_dependency(this_hostdependency->dependent_host_name, this_hostdependency->host_name, NOTIFICATION_DEPENDENCY, this_hostdependency->inherits_parent, this_hostdependency->notification_failure_options, this_hostdependency->dependency_period);

		/* return with an error if we couldn't add the hostdependency */
		if(new_hostdependency == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register host notification dependency (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_hostdependency->_config_file), this_hostdependency->_start_line);
			return ERROR;
			}
		}

	return OK;
	}



/* registers a hostescalation definition */
int xodtemplate_register_hostescalation(xodtemplate_hostescalation *this_hostescalation) {
	hostescalation *new_hostescalation = NULL;
	contactsmember *new_contactsmember = NULL;
	contactgroupsmember *new_contactgroupsmember = NULL;
	char *contact_name = NULL;
	char *contact_group = NULL;

	/* bail out if we shouldn't register this object */
	if(this_hostescalation->register_object == FALSE)
		return OK;

	/* default options if none specified */
	if(this_hostescalation->have_escalation_options == FALSE) {
		this_hostescalation->escalation_options = OPT_ALL;
		}

	/* add the hostescalation */
	new_hostescalation = add_hostescalation(this_hostescalation->host_name, this_hostescalation->first_notification, this_hostescalation->last_notification, this_hostescalation->notification_interval, this_hostescalation->escalation_period, this_hostescalation->escalation_options);

	/* return with an error if we couldn't add the hostescalation */
	if(new_hostescalation == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not register host escalation (config file '%s', starting on line %d)\n", xodtemplate_config_file_name(this_hostescalation->_config_file), this_hostescalation->_start_line);
		return ERROR;
		}

	/* add all contact groups */
	if(this_hostescalation->contact_groups != NULL) {

		for(contact_group = strtok(this_hostescalation->contact_groups, ","); contact_group != NULL; contact_group = strtok(NULL, ",")) {

			strip(contact_group);
			new_contactgroupsmember = add_contactgroup_to_hostescalation(new_hostescalation, contact_group);
			if(new_contactgroupsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contactgroup '%s' to host escalation (config file '%s', starting on line %d)\n", contact_group, xodtemplate_config_file_name(this_hostescalation->_config_file), this_hostescalation->_start_line);
				return ERROR;
				}
			}
		}

	/* add the contacts */
	if(this_hostescalation->contacts != NULL) {

		for(contact_name = strtok(this_hostescalation->contacts, ","); contact_name != NULL; contact_name = strtok(NULL, ",")) {

			strip(contact_name);
			new_contactsmember = add_contact_to_hostescalation(new_hostescalation, contact_name);
			if(new_contactsmember == NULL) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contact '%s' to host escalation (config file '%s', starting on line %d)\n", contact_name, xodtemplate_config_file_name(this_hostescalation->_config_file), this_hostescalation->_start_line);
				return ERROR;
				}
			}
		}

	return OK;
	}




/******************************************************************/
/*********************** MERGE FUNCTIONS **************************/
/******************************************************************/

#ifdef NSCORE

/* merges a service extinfo definition */
int xodtemplate_merge_service_extinfo_object(xodtemplate_service *this_service, xodtemplate_serviceextinfo *this_serviceextinfo) {

	if(this_service == NULL || this_serviceextinfo == NULL)
		return ERROR;

	if(this_service->notes == NULL && this_serviceextinfo->notes != NULL)
		this_service->notes = strdup(this_serviceextinfo->notes);
	if(this_service->notes_url == NULL && this_serviceextinfo->notes_url != NULL)
		this_service->notes_url = strdup(this_serviceextinfo->notes_url);
	if(this_service->action_url == NULL && this_serviceextinfo->action_url != NULL)
		this_service->action_url = strdup(this_serviceextinfo->action_url);
	if(this_service->icon_image == NULL && this_serviceextinfo->icon_image != NULL)
		this_service->icon_image = strdup(this_serviceextinfo->icon_image);
	if(this_service->icon_image_alt == NULL && this_serviceextinfo->icon_image_alt != NULL)
		this_service->icon_image_alt = strdup(this_serviceextinfo->icon_image_alt);

	return OK;
	}


/* merges a host extinfo definition */
int xodtemplate_merge_host_extinfo_object(xodtemplate_host *this_host, xodtemplate_hostextinfo *this_hostextinfo) {

	if(this_host == NULL || this_hostextinfo == NULL)
		return ERROR;

	if(this_host->notes == NULL && this_hostextinfo->notes != NULL)
		this_host->notes = strdup(this_hostextinfo->notes);
	if(this_host->notes_url == NULL && this_hostextinfo->notes_url != NULL)
		this_host->notes_url = strdup(this_hostextinfo->notes_url);
	if(this_host->action_url == NULL && this_hostextinfo->action_url != NULL)
		this_host->action_url = strdup(this_hostextinfo->action_url);
	if(this_host->icon_image == NULL && this_hostextinfo->icon_image != NULL)
		this_host->icon_image = strdup(this_hostextinfo->icon_image);
	if(this_host->icon_image_alt == NULL && this_hostextinfo->icon_image_alt != NULL)
		this_host->icon_image_alt = strdup(this_hostextinfo->icon_image_alt);
	if(this_host->vrml_image == NULL && this_hostextinfo->vrml_image != NULL)
		this_host->vrml_image = strdup(this_hostextinfo->vrml_image);
	if(this_host->statusmap_image == NULL && this_hostextinfo->statusmap_image != NULL)
		this_host->statusmap_image = strdup(this_hostextinfo->statusmap_image);

	if(this_host->have_2d_coords == FALSE && this_hostextinfo->have_2d_coords == TRUE) {
		this_host->x_2d = this_hostextinfo->x_2d;
		this_host->y_2d = this_hostextinfo->y_2d;
		this_host->have_2d_coords = TRUE;
		}
	if(this_host->have_3d_coords == FALSE && this_hostextinfo->have_3d_coords == TRUE) {
		this_host->x_3d = this_hostextinfo->x_3d;
		this_host->y_3d = this_hostextinfo->y_3d;
		this_host->z_3d = this_hostextinfo->z_3d;
		this_host->have_3d_coords = TRUE;
		}

	return OK;
	}

#endif


/******************************************************************/
/******************** SKIPLIST FUNCTIONS **************************/
/******************************************************************/

int xodtemplate_init_xobject_skiplists(void) {
	int x = 0;

	for(x = 0; x < NUM_XOBJECT_SKIPLISTS; x++) {
		xobject_template_skiplists[x] = NULL;
		xobject_skiplists[x] = NULL;
		}

	xobject_template_skiplists[HOST_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_host_template);
	xobject_template_skiplists[SERVICE_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_service_template);
	xobject_template_skiplists[COMMAND_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_command_template);
	xobject_template_skiplists[TIMEPERIOD_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_timeperiod_template);
	xobject_template_skiplists[CONTACT_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_contact_template);
	xobject_template_skiplists[CONTACTGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_contactgroup_template);
	xobject_template_skiplists[HOSTGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_hostgroup_template);
	xobject_template_skiplists[SERVICEGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_servicegroup_template);
	xobject_template_skiplists[HOSTDEPENDENCY_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_hostdependency_template);
	xobject_template_skiplists[SERVICEDEPENDENCY_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_servicedependency_template);
	xobject_template_skiplists[HOSTESCALATION_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_hostescalation_template);
	xobject_template_skiplists[SERVICEESCALATION_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_serviceescalation_template);
	xobject_template_skiplists[HOSTEXTINFO_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_hostextinfo_template);
	xobject_template_skiplists[SERVICEEXTINFO_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_serviceextinfo_template);

	xobject_skiplists[HOST_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_host);
	xobject_skiplists[SERVICE_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_service);
	xobject_skiplists[COMMAND_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_command);
	xobject_skiplists[TIMEPERIOD_SKIPLIST] = skiplist_new(16, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_timeperiod);
	xobject_skiplists[CONTACT_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_contact);
	xobject_skiplists[CONTACTGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_contactgroup);
	xobject_skiplists[HOSTGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_hostgroup);
	xobject_skiplists[SERVICEGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, xodtemplate_skiplist_compare_servicegroup);
	/*
	 * host and service extinfo, dependencies, and escalations don't
	 * need to be sorted, so we avoid creating skiplists for them.
	 */
	return OK;
	}



int xodtemplate_free_xobject_skiplists(void) {
	int x = 0;

	for(x = 0; x < NUM_XOBJECT_SKIPLISTS; x++) {
		skiplist_free(&xobject_template_skiplists[x]);
		skiplist_free(&xobject_skiplists[x]);
		}

	return OK;
	}


int xodtemplate_skiplist_compare_host_template(void *a, void *b) {
	xodtemplate_host *oa = NULL;
	xodtemplate_host *ob = NULL;

	oa = (xodtemplate_host *)a;
	ob = (xodtemplate_host *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_host(void *a, void *b) {
	xodtemplate_host *oa = NULL;
	xodtemplate_host *ob = NULL;

	oa = (xodtemplate_host *)a;
	ob = (xodtemplate_host *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->host_name, NULL, ob->host_name, NULL);
	}



int xodtemplate_skiplist_compare_service_template(void *a, void *b) {
	xodtemplate_service *oa = NULL;
	xodtemplate_service *ob = NULL;

	oa = (xodtemplate_service *)a;
	ob = (xodtemplate_service *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_service(void *a, void *b) {
	xodtemplate_service *oa = NULL;
	xodtemplate_service *ob = NULL;

	oa = (xodtemplate_service *)a;
	ob = (xodtemplate_service *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->host_name, oa->service_description, ob->host_name, ob->service_description);
	}



int xodtemplate_skiplist_compare_timeperiod_template(void *a, void *b) {
	xodtemplate_timeperiod *oa = NULL;
	xodtemplate_timeperiod *ob = NULL;

	oa = (xodtemplate_timeperiod *)a;
	ob = (xodtemplate_timeperiod *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_timeperiod(void *a, void *b) {
	xodtemplate_timeperiod *oa = NULL;
	xodtemplate_timeperiod *ob = NULL;

	oa = (xodtemplate_timeperiod *)a;
	ob = (xodtemplate_timeperiod *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->timeperiod_name, NULL, ob->timeperiod_name, NULL);
	}



int xodtemplate_skiplist_compare_command_template(void *a, void *b) {
	xodtemplate_command *oa = NULL;
	xodtemplate_command *ob = NULL;

	oa = (xodtemplate_command *)a;
	ob = (xodtemplate_command *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_command(void *a, void *b) {
	xodtemplate_command *oa = NULL;
	xodtemplate_command *ob = NULL;

	oa = (xodtemplate_command *)a;
	ob = (xodtemplate_command *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->command_name, NULL, ob->command_name, NULL);
	}



int xodtemplate_skiplist_compare_contact_template(void *a, void *b) {
	xodtemplate_contact *oa = NULL;
	xodtemplate_contact *ob = NULL;

	oa = (xodtemplate_contact *)a;
	ob = (xodtemplate_contact *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_contact(void *a, void *b) {
	xodtemplate_contact *oa = NULL;
	xodtemplate_contact *ob = NULL;

	oa = (xodtemplate_contact *)a;
	ob = (xodtemplate_contact *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->contact_name, NULL, ob->contact_name, NULL);
	}



int xodtemplate_skiplist_compare_contactgroup_template(void *a, void *b) {
	xodtemplate_contactgroup *oa = NULL;
	xodtemplate_contactgroup *ob = NULL;

	oa = (xodtemplate_contactgroup *)a;
	ob = (xodtemplate_contactgroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_contactgroup(void *a, void *b) {
	xodtemplate_contactgroup *oa = NULL;
	xodtemplate_contactgroup *ob = NULL;

	oa = (xodtemplate_contactgroup *)a;
	ob = (xodtemplate_contactgroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->contactgroup_name, NULL, ob->contactgroup_name, NULL);
	}



int xodtemplate_skiplist_compare_hostgroup_template(void *a, void *b) {
	xodtemplate_hostgroup *oa = NULL;
	xodtemplate_hostgroup *ob = NULL;

	oa = (xodtemplate_hostgroup *)a;
	ob = (xodtemplate_hostgroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_hostgroup(void *a, void *b) {
	xodtemplate_hostgroup *oa = NULL;
	xodtemplate_hostgroup *ob = NULL;

	oa = (xodtemplate_hostgroup *)a;
	ob = (xodtemplate_hostgroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->hostgroup_name, NULL, ob->hostgroup_name, NULL);
	}



int xodtemplate_skiplist_compare_servicegroup_template(void *a, void *b) {
	xodtemplate_servicegroup *oa = NULL;
	xodtemplate_servicegroup *ob = NULL;

	oa = (xodtemplate_servicegroup *)a;
	ob = (xodtemplate_servicegroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_servicegroup(void *a, void *b) {
	xodtemplate_servicegroup *oa = NULL;
	xodtemplate_servicegroup *ob = NULL;

	oa = (xodtemplate_servicegroup *)a;
	ob = (xodtemplate_servicegroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->servicegroup_name, NULL, ob->servicegroup_name, NULL);
	}



int xodtemplate_skiplist_compare_hostdependency_template(void *a, void *b) {
	xodtemplate_hostdependency *oa = NULL;
	xodtemplate_hostdependency *ob = NULL;

	oa = (xodtemplate_hostdependency *)a;
	ob = (xodtemplate_hostdependency *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_hostdependency(void *a, void *b) {
	xodtemplate_hostdependency *oa = NULL;
	xodtemplate_hostdependency *ob = NULL;

	oa = (xodtemplate_hostdependency *)a;
	ob = (xodtemplate_hostdependency *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->dependent_host_name, NULL, ob->dependent_host_name, NULL);
	}



int xodtemplate_skiplist_compare_servicedependency_template(void *a, void *b) {
	xodtemplate_servicedependency *oa = NULL;
	xodtemplate_servicedependency *ob = NULL;

	oa = (xodtemplate_servicedependency *)a;
	ob = (xodtemplate_servicedependency *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_servicedependency(void *a, void *b) {
	xodtemplate_servicedependency *oa = NULL;
	xodtemplate_servicedependency *ob = NULL;

	oa = (xodtemplate_servicedependency *)a;
	ob = (xodtemplate_servicedependency *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->dependent_host_name, oa->dependent_service_description, ob->dependent_host_name, ob->dependent_service_description);
	}



int xodtemplate_skiplist_compare_hostescalation_template(void *a, void *b) {
	xodtemplate_hostescalation *oa = NULL;
	xodtemplate_hostescalation *ob = NULL;

	oa = (xodtemplate_hostescalation *)a;
	ob = (xodtemplate_hostescalation *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_hostescalation(void *a, void *b) {
	xodtemplate_hostescalation *oa = NULL;
	xodtemplate_hostescalation *ob = NULL;

	oa = (xodtemplate_hostescalation *)a;
	ob = (xodtemplate_hostescalation *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->host_name, NULL, ob->host_name, NULL);
	}



int xodtemplate_skiplist_compare_serviceescalation_template(void *a, void *b) {
	xodtemplate_serviceescalation *oa = NULL;
	xodtemplate_serviceescalation *ob = NULL;

	oa = (xodtemplate_serviceescalation *)a;
	ob = (xodtemplate_serviceescalation *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_serviceescalation(void *a, void *b) {
	xodtemplate_serviceescalation *oa = NULL;
	xodtemplate_serviceescalation *ob = NULL;

	oa = (xodtemplate_serviceescalation *)a;
	ob = (xodtemplate_serviceescalation *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->host_name, oa->service_description, ob->host_name, ob->service_description);
	}



int xodtemplate_skiplist_compare_hostextinfo_template(void *a, void *b) {
	xodtemplate_hostextinfo *oa = NULL;
	xodtemplate_hostextinfo *ob = NULL;

	oa = (xodtemplate_hostextinfo *)a;
	ob = (xodtemplate_hostextinfo *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}



int xodtemplate_skiplist_compare_serviceextinfo_template(void *a, void *b) {
	xodtemplate_serviceextinfo *oa = NULL;
	xodtemplate_serviceextinfo *ob = NULL;

	oa = (xodtemplate_serviceextinfo *)a;
	ob = (xodtemplate_serviceextinfo *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}




/******************************************************************/
/********************** CLEANUP FUNCTIONS *************************/
/******************************************************************/

/* frees memory */
int xodtemplate_free_memory(void) {
	xodtemplate_timeperiod *this_timeperiod = NULL;
	xodtemplate_timeperiod *next_timeperiod = NULL;
	xodtemplate_daterange *this_daterange = NULL;
	xodtemplate_daterange *next_daterange = NULL;
	xodtemplate_command *this_command = NULL;
	xodtemplate_command *next_command = NULL;
	xodtemplate_contactgroup *this_contactgroup = NULL;
	xodtemplate_contactgroup *next_contactgroup = NULL;
	xodtemplate_hostgroup *this_hostgroup = NULL;
	xodtemplate_hostgroup *next_hostgroup = NULL;
	xodtemplate_servicegroup *this_servicegroup = NULL;
	xodtemplate_servicegroup *next_servicegroup = NULL;
	xodtemplate_contact *this_contact = NULL;
	xodtemplate_contact *next_contact = NULL;
	xodtemplate_host *this_host = NULL;
	xodtemplate_host *next_host = NULL;
	xodtemplate_service *this_service = NULL;
	xodtemplate_service *next_service = NULL;
	xodtemplate_customvariablesmember *this_customvariablesmember = NULL;
	xodtemplate_customvariablesmember *next_customvariablesmember = NULL;
	register int x = 0;


	/* free memory allocated to timeperiod list */
	for(this_timeperiod = xodtemplate_timeperiod_list; this_timeperiod != NULL; this_timeperiod = next_timeperiod) {
		next_timeperiod = this_timeperiod->next;
		my_free(this_timeperiod->template);
		my_free(this_timeperiod->name);
		if (!this_timeperiod->register_object) {
			my_free(this_timeperiod->timeperiod_name);
			my_free(this_timeperiod->alias);
		}
		for(x = 0; x < 7; x++)
			my_free(this_timeperiod->timeranges[x]);
		for(x = 0; x < DATERANGE_TYPES; x++) {
			for(this_daterange = this_timeperiod->exceptions[x]; this_daterange != NULL; this_daterange = next_daterange) {
				next_daterange = this_daterange->next;
				my_free(this_daterange->timeranges);
				my_free(this_daterange);
				}
			}
		my_free(this_timeperiod->exclusions);
		my_free(this_timeperiod);
		}
	xodtemplate_timeperiod_list = NULL;
	xodtemplate_timeperiod_list_tail = NULL;

	/* free memory allocated to command list */
	for(this_command = xodtemplate_command_list; this_command != NULL; this_command = next_command) {
		next_command = this_command->next;
		my_free(this_command->template);
		my_free(this_command->name);
		if (!this_command->register_object) {
			my_free(this_command->command_name);
			my_free(this_command->command_line);
		}
		my_free(this_command);
		}
	xodtemplate_command_list = NULL;
	xodtemplate_command_list_tail = NULL;

	/* free memory allocated to contactgroup list */
	for(this_contactgroup = xodtemplate_contactgroup_list; this_contactgroup != NULL; this_contactgroup = next_contactgroup) {
		next_contactgroup = this_contactgroup->next;
		my_free(this_contactgroup->template);
		my_free(this_contactgroup->name);
		my_free(this_contactgroup->members);
		my_free(this_contactgroup->contactgroup_members);
		bitmap_destroy(this_contactgroup->member_map);
		free_objectlist(&this_contactgroup->member_list);
		free_objectlist(&this_contactgroup->group_list);
		if (!this_contactgroup->register_object) {
			my_free(this_contactgroup->contactgroup_name);
			my_free(this_contactgroup->alias);
		}
		my_free(this_contactgroup);
		}
	xodtemplate_contactgroup_list = NULL;
	xodtemplate_contactgroup_list_tail = NULL;

	/* free memory allocated to hostgroup list */
	for(this_hostgroup = xodtemplate_hostgroup_list; this_hostgroup != NULL; this_hostgroup = next_hostgroup) {
		next_hostgroup = this_hostgroup->next;
		my_free(this_hostgroup->template);
		my_free(this_hostgroup->name);
		my_free(this_hostgroup->members);
		my_free(this_hostgroup->hostgroup_members);
		bitmap_destroy(this_hostgroup->member_map);
		free_objectlist(&this_hostgroup->member_list);
		free_objectlist(&this_hostgroup->group_list);
		if (!this_hostgroup->register_object) {
			my_free(this_hostgroup->hostgroup_name);
			my_free(this_hostgroup->alias);
			my_free(this_hostgroup->notes);
			my_free(this_hostgroup->notes_url);
			my_free(this_hostgroup->action_url);
		}
		my_free(this_hostgroup);
		}
	xodtemplate_hostgroup_list = NULL;
	xodtemplate_hostgroup_list_tail = NULL;

	/* free memory allocated to servicegroup list */
	for(this_servicegroup = xodtemplate_servicegroup_list; this_servicegroup != NULL; this_servicegroup = next_servicegroup) {
		next_servicegroup = this_servicegroup->next;
		my_free(this_servicegroup->members);
		my_free(this_servicegroup->servicegroup_members);
		bitmap_destroy(this_servicegroup->member_map);
		free_objectlist(&this_servicegroup->member_list);
		free_objectlist(&this_servicegroup->group_list);
		my_free(this_servicegroup->template);
		my_free(this_servicegroup->name);
		if (!this_servicegroup->register_object) {
			my_free(this_servicegroup->servicegroup_name);
			my_free(this_servicegroup->alias);
			my_free(this_servicegroup->notes);
			my_free(this_servicegroup->notes_url);
			my_free(this_servicegroup->action_url);
		}
		my_free(this_servicegroup);
		}
	xodtemplate_servicegroup_list = NULL;
	xodtemplate_servicegroup_list_tail = NULL;

	/* free memory allocated to contact list */
	for(this_contact = xodtemplate_contact_list; this_contact != NULL; this_contact = next_contact) {
		next_contact = this_contact->next;
		/* free custom variables */
		this_customvariablesmember = this_contact->custom_variables;
		while(this_customvariablesmember != NULL) {
			next_customvariablesmember = this_customvariablesmember->next;
			my_free(this_customvariablesmember->variable_name);
			my_free(this_customvariablesmember->variable_value);
			my_free(this_customvariablesmember);
			this_customvariablesmember = next_customvariablesmember;
			}
		my_free(this_contact->template);
		my_free(this_contact->name);
		my_free(this_contact->contact_groups);
		my_free(this_contact->service_notification_period);
		my_free(this_contact->service_notification_commands);
		my_free(this_contact->host_notification_period);
		my_free(this_contact->host_notification_commands);
		if (!this_contact->register_object) {
			my_free(this_contact->contact_name);
			my_free(this_contact->alias);
			my_free(this_contact->email);
			my_free(this_contact->pager);
			for (x = 0; x < MAX_XODTEMPLATE_CONTACT_ADDRESSES; x++)
				my_free(this_contact->address[x]);
			}
		my_free(this_contact);
		}
	xodtemplate_contact_list = NULL;
	xodtemplate_contact_list_tail = NULL;

	/* free memory allocated to host list */
	for(this_host = xodtemplate_host_list; this_host != NULL; this_host = next_host) {
		next_host = this_host->next;
		/* free custom variables */
		this_customvariablesmember = this_host->custom_variables;
		while(this_customvariablesmember != NULL) {
			next_customvariablesmember = this_customvariablesmember->next;
			my_free(this_customvariablesmember->variable_name);
			my_free(this_customvariablesmember->variable_value);
			my_free(this_customvariablesmember);
			this_customvariablesmember = next_customvariablesmember;
			}

		my_free(this_host->template);
		my_free(this_host->name);
		my_free(this_host->parents);
		my_free(this_host->host_groups);
		my_free(this_host->check_period);
		my_free(this_host->contact_groups);
		my_free(this_host->contacts);
		my_free(this_host->notification_period);
		if (!this_host->register_object) {
			my_free(this_host->host_name);
			my_free(this_host->alias);
			my_free(this_host->display_name);
			my_free(this_host->address);
			my_free(this_host->check_command);
			my_free(this_host->event_handler);
			my_free(this_host->notes);
			my_free(this_host->notes_url);
			my_free(this_host->action_url);
			my_free(this_host->icon_image);
			my_free(this_host->icon_image_alt);
			my_free(this_host->statusmap_image);
			my_free(this_host->vrml_image);
			}
		my_free(this_host);
		}
	xodtemplate_host_list = NULL;
	xodtemplate_host_list_tail = NULL;

	/* free memory allocated to service list */
	for(this_service = xodtemplate_service_list; this_service != NULL; this_service = next_service) {
		next_service = this_service->next;
		my_free(this_service->contact_groups);
		my_free(this_service->contacts);
		my_free(this_service->service_groups);

		if(this_service->is_copy == FALSE || !this_service->register_object) {
			/* free custom variables */
			this_customvariablesmember = this_service->custom_variables;
			while(this_customvariablesmember != NULL) {
				next_customvariablesmember = this_customvariablesmember->next;
				my_free(this_customvariablesmember->variable_name);
				my_free(this_customvariablesmember->variable_value);
				my_free(this_customvariablesmember);
				this_customvariablesmember = next_customvariablesmember;
				}

			my_free(this_service->template);
			my_free(this_service->name);
			my_free(this_service->parents);
			my_free(this_service->display_name);
			my_free(this_service->check_command);
			my_free(this_service->check_period);
			my_free(this_service->event_handler);
			my_free(this_service->notification_period);
			my_free(this_service->notes);
			my_free(this_service->notes_url);
			my_free(this_service->action_url);
			my_free(this_service->icon_image);
			my_free(this_service->icon_image_alt);
			my_free(this_service->hostgroup_name);
			my_free(this_service->service_description);
			}
		my_free(this_service);
		}
	xodtemplate_service_list = NULL;
	xodtemplate_service_list_tail = NULL;

	/*
	 * extinfo objects are free()'d while they're parsed, as are
	 * dependencies and escalations
	 */
	xodtemplate_hostextinfo_list = NULL;
	xodtemplate_hostextinfo_list_tail = NULL;
	xodtemplate_serviceextinfo_list = NULL;
	xodtemplate_serviceextinfo_list_tail = NULL;

	/* free memory for the config file names */
	for(x = 0; x < xodtemplate_current_config_file; x++)
		my_free(xodtemplate_config_files[x]);
	my_free(xodtemplate_config_files);
	xodtemplate_current_config_file = 0;

	/* free skiplists */
	xodtemplate_free_xobject_skiplists();

	return OK;
	}




/* adds a member to a list */
int xodtemplate_add_member_to_memberlist(xodtemplate_memberlist **list, char *name1, char *name2) {
	xodtemplate_memberlist *temp_item = NULL;
	xodtemplate_memberlist *new_item = NULL;
	int error = FALSE;

	if(list == NULL)
		return ERROR;
	if(name1 == NULL)
		return ERROR;

	/* skip this member if its already in the list */
	for(temp_item = *list; temp_item; temp_item = temp_item->next) {
		if(!strcmp(temp_item->name1, name1)) {
			if(temp_item->name2 == NULL) {
				if(name2 == NULL)
					break;
				}
			else if(name2 != NULL && !strcmp(temp_item->name2, name2))
				break;
			}
		}
	if(temp_item)
		return OK;

	/* allocate zero'd out memory for a new list item */
	if((new_item = (xodtemplate_memberlist *)calloc(1, sizeof(xodtemplate_memberlist))) == NULL)
		return ERROR;

	/* save the member name(s) */
	if(name1) {
		if((new_item->name1 = (char *)strdup(name1)) == NULL)
			error = TRUE;
		}
	if(name2) {
		if((new_item->name2 = (char *)strdup(name2)) == NULL)
			error = TRUE;
		}

	if(error == TRUE) {
		my_free(new_item->name1);
		my_free(new_item->name2);
		my_free(new_item);
		return ERROR;
		}

	/* add new item to head of list */
	new_item->next = *list;
	*list = new_item;

	return OK;
	}


/* frees memory allocated to a temporary member list */
int xodtemplate_free_memberlist(xodtemplate_memberlist **temp_list) {
	xodtemplate_memberlist *this_memberlist = NULL;
	xodtemplate_memberlist *next_memberlist = NULL;

	/* free memory allocated to member name list */
	for(this_memberlist = *temp_list; this_memberlist != NULL; this_memberlist = next_memberlist) {
		next_memberlist = this_memberlist->next;
		my_free(this_memberlist->name1);
		my_free(this_memberlist->name2);
		my_free(this_memberlist);
		}

	*temp_list = NULL;

	return OK;
	}


/* remove an entry from the member list */
void xodtemplate_remove_memberlist_item(xodtemplate_memberlist *item, xodtemplate_memberlist **list) {
	xodtemplate_memberlist *temp_item = NULL;

	if(item == NULL || list == NULL)
		return;

	if(*list == NULL)
		return;

	if(*list == item)
		*list = item->next;

	else {

		for(temp_item = *list; temp_item != NULL; temp_item = temp_item->next) {
			if(temp_item->next == item) {
				temp_item->next = item->next;
				break;
				}
			}
		}

	my_free(item->name1);
	my_free(item->name2);
	my_free(item);

	return;
	}


/******************************************************************/
/********************** UTILITY FUNCTIONS *************************/
/******************************************************************/


/* expands contacts */
int xodtemplate_expand_contacts(objectlist **ret, bitmap *reject_map, char *contacts, int _config_file, int _start_line) {
	char *contact_names = NULL;
	char *temp_ptr = NULL;
	xodtemplate_contact *temp_contact = NULL;
	regex_t preg;
	int found_match = TRUE;
	int reject_item = FALSE;
	int use_regexp = FALSE;

	if(ret == NULL || contacts == NULL)
		return ERROR;

	*ret = NULL;

	if((contact_names = (char *)strdup(contacts)) == NULL)
		return ERROR;

	/* expand each contact name */
	for(temp_ptr = strtok(contact_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

		/* should we use regular expression matching? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp = TRUE;

		/* use regular expression matching */
		if(use_regexp == TRUE) {

			/* compile regular expression */
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				my_free(contact_names);
				return ERROR;
				}

			/* test match against all contacts */
			for(temp_contact = xodtemplate_contact_list; temp_contact != NULL; temp_contact = temp_contact->next) {

				if(temp_contact->contact_name == NULL)
					continue;

				/* skip this contact if it did not match the expression */
				if(regexec(&preg, temp_contact->contact_name, 0, NULL, 0))
					continue;

				found_match = TRUE;

				/* dont' add contacts that shouldn't be registered */
				if(temp_contact->register_object == FALSE)
					continue;

				/* add contact to list */
				add_object_to_objectlist(ret, temp_contact);
				}

			/* free memory allocated to compiled regexp */
			regfree(&preg);
			}

		/* use standard matching... */
		else {

			/* return a list of all contacts */
			if(!strcmp(temp_ptr, "*")) {

				found_match = TRUE;

				for(temp_contact = xodtemplate_contact_list; temp_contact != NULL; temp_contact = temp_contact->next) {

					if(temp_contact->contact_name == NULL)
						continue;

					/* dont' add contacts that shouldn't be registered */
					if(temp_contact->register_object == FALSE)
						continue;

					/* add contact to list */
					add_object_to_objectlist(ret, temp_contact);
					}
				}

			/* else this is just a single contact... */
			else {

				/* this contact should be excluded (rejected) */
				if(temp_ptr[0] == '!') {
					reject_item = TRUE;
					temp_ptr++;
					}

				/* find the contact */
				temp_contact = xodtemplate_find_real_contact(temp_ptr);
				if(temp_contact != NULL) {

					found_match = TRUE;

					/* add contact to list */
					if(reject_item) {
						bitmap_set(reject_map, temp_contact->id);
						}
					else {
						add_object_to_objectlist(ret, temp_contact);
						}
					}
				}
			}

		if(found_match == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find any contact matching '%s' (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}

	/* free memory */
	my_free(contact_names);

	if(found_match == FALSE)
		return ERROR;

	return OK;
	}


#ifdef NSCORE

/*
 * expands a comma-delimited list of hostgroups and/or hosts to
 * an objectlist of hosts. This cannot be called until hostgroups
 * have been recombobulated.
 */
objectlist *xodtemplate_expand_hostgroups_and_hosts(char *hostgroups, char *hosts, int _config_file, int _start_line) {
	objectlist *ret = NULL, *glist = NULL, *hlist, *list = NULL, *next;
	bitmap *reject;
	int result;

	reject = bitmap_create(xodcount.hosts);
	if(!reject) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Unable to create reject map for expanding hosts and hostgroups\n");
		return NULL;
		}

	/*
	 * process host names first. If they're explicitly added we must obey
	 */
	if(hosts != NULL) {
		/* expand hosts */
		result = xodtemplate_expand_hosts(&ret, reject, hosts, _config_file, _start_line);
		if(result != OK) {
			free_objectlist(&glist);
			free_objectlist(&ret);
			bitmap_destroy(reject);
			return NULL;
			}
		}

	/* process list of hostgroups... */
	if(hostgroups != NULL) {
		/* expand host */
		result = xodtemplate_expand_hostgroups(&glist, reject, hostgroups, _config_file, _start_line);
		if(result != OK) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Failed to expand hostgroups '%s' to something sensible\n", hostgroups);
			free_objectlist(&glist);
			bitmap_destroy(reject);
			return NULL;
			}
		}

	/*
	 * add hostgroup hosts to ret, taking care not to add any that are
	 * in the rejected list
	 */
	for(list = glist; list; list = next) {
		xodtemplate_hostgroup *hg = (xodtemplate_hostgroup *)list->object_ptr;
		next = list->next;
		free(list); /* free it as we go along */
		for(hlist = hg->member_list; hlist; hlist = hlist->next) {
			xodtemplate_host *h = (xodtemplate_host *)hlist->object_ptr;
			if(bitmap_isset(reject, h->id))
				continue;
			add_object_to_objectlist(&ret, h);
		}
	}
	bitmap_destroy(reject);

	return ret;
	}

#endif

/*
 * expands hostgroups.
 * list will be populated with all selected hostgroups on success
 * and set to NULL on errors.
 * reject_map marks rejected *hosts* from rejected hostgroups
 * This can only be called after hostgroups are recombobulated.
 * returns ERROR on error and OK on success.
 */
int xodtemplate_expand_hostgroups(objectlist **list, bitmap *reject_map, char *hostgroups, int _config_file, int _start_line) {
	char *hostgroup_names = NULL;
	char *temp_ptr = NULL;
	xodtemplate_hostgroup *temp_hostgroup = NULL;
	regex_t preg;
	int found_match = TRUE;
	int reject_item = FALSE;
	int use_regexp = FALSE;

	if(list == NULL || hostgroups == NULL)
		return ERROR;

	*list = NULL;

	/* allocate memory for hostgroup name list */
	if((hostgroup_names = (char *)strdup(hostgroups)) == NULL)
		return ERROR;

	for(temp_ptr = strtok(hostgroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

		/* should we use regular expression matching? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp = TRUE;
		else
			use_regexp = FALSE;

		/* use regular expression matching */
		if(use_regexp == TRUE) {

			/* compile regular expression */
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				my_free(hostgroup_names);
				return ERROR;
				}

			/* test match against all hostgroup names */
			for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {

				if(temp_hostgroup->hostgroup_name == NULL)
					continue;

				/* skip this hostgroup if it did not match the expression */
				if(regexec(&preg, temp_hostgroup->hostgroup_name, 0, NULL, 0))
					continue;

				found_match = TRUE;

				/* dont' add hostgroups that shouldn't be registered */
				if(temp_hostgroup->register_object == FALSE)
					continue;

				add_object_to_objectlist(list, temp_hostgroup);
				}

			/* free memory allocated to compiled regexp */
			regfree(&preg);
			}

		/* use standard matching... */
		else {

			/* return a list of all hostgroups */
			if(!strcmp(temp_ptr, "*")) {

				found_match = TRUE;

				for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {

					/* dont' add hostgroups that shouldn't be registered */
					if(temp_hostgroup->register_object == FALSE)
						continue;

					/* add hostgroup to list */
					add_object_to_objectlist(list, temp_hostgroup);
					}
				}

			/* else this is just a single hostgroup... */
			else {

				/* this hostgroup should be excluded (rejected) */
				if(temp_ptr[0] == '!') {
					reject_item = TRUE;
					temp_ptr++;
					}

				/* find the hostgroup */
				temp_hostgroup = xodtemplate_find_real_hostgroup(temp_ptr);
				if(temp_hostgroup != NULL) {
					found_match = TRUE;

					if(reject_item) {
						bitmap_unite(reject_map, temp_hostgroup->member_map);
						}
					else {
						/* add hostgroup members to proper list */
						add_object_to_objectlist(list, temp_hostgroup);
						}
					}
				}
			}

		if(found_match == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find any hostgroup matching '%s' (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}

	/* free memory */
	my_free(hostgroup_names);

	if(found_match == FALSE)
		return ERROR;

	return OK;
	}



/* expands hosts */
int xodtemplate_expand_hosts(objectlist **list, bitmap *reject_map, char *hosts, int _config_file, int _start_line) {
	char *temp_ptr = NULL;
	xodtemplate_host *temp_host = NULL;
	regex_t preg;
	int found_match = TRUE;
	int reject_item = FALSE;
	int use_regexp = FALSE;

	if(list == NULL || hosts == NULL)
		return ERROR;

	/* expand each host name */
	for(temp_ptr = strtok(hosts, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

		/* should we use regular expression matching? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp = TRUE;

		/* use regular expression matching */
		if(use_regexp == TRUE) {

			/* compile regular expression */
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				return ERROR;
				}

			/* test match against all hosts */
			for(temp_host = xodtemplate_host_list; temp_host != NULL; temp_host = temp_host->next) {

				if(temp_host->host_name == NULL)
					continue;

				/* skip this host if it did not match the expression */
				if(regexec(&preg, temp_host->host_name, 0, NULL, 0))
					continue;

				found_match = TRUE;

				/* dont' add hosts that shouldn't be registered */
				if(temp_host->register_object == FALSE)
					continue;

				/* add host to list */
				add_object_to_objectlist(list, temp_host);
				}

			/* free memory allocated to compiled regexp */
			regfree(&preg);
			}

		/* use standard matching... */
		else {

			/* return a list of all hosts */
			if(!strcmp(temp_ptr, "*")) {

				found_match = TRUE;

				for(temp_host = xodtemplate_host_list; temp_host != NULL; temp_host = temp_host->next) {

					if(temp_host->host_name == NULL)
						continue;

					/* dont' add hosts that shouldn't be registered */
					if(temp_host->register_object == FALSE)
						continue;

					/* add host to list */
					add_object_to_objectlist(list, temp_host);
					}
				}

			/* else this is just a single host... */
			else {

				/* this host should be excluded (rejected) */
				if(temp_ptr[0] == '!') {
					reject_item = TRUE;
					temp_ptr++;
					}

				/* find the host */
				temp_host = xodtemplate_find_real_host(temp_ptr);
				if(temp_host != NULL) {

					found_match = TRUE;

					/* add host to list */
					if(!reject_item) {
						add_object_to_objectlist(list, temp_host);
						}
					else {
						bitmap_set(reject_map, temp_host->id);
						}
					}
				}
			}

		if(found_match == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find any host matching '%s' (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}

	if(found_match == FALSE)
		return ERROR;

	return OK;
	}


/*
 * expands servicegroups.
 * list will hold all selected servicegroups.
 * reject will map services from all rejected servicegroups
 * This can only be called after servicegroups are recombobulated.
 */
int xodtemplate_expand_servicegroups(objectlist **list, bitmap *reject, char *servicegroups, int _config_file, int _start_line) {
	xodtemplate_servicegroup  *temp_servicegroup = NULL;
	regex_t preg;
	char *servicegroup_names = NULL;
	char *temp_ptr = NULL;
	int found_match = TRUE;
	int reject_item = FALSE;
	int use_regexp = FALSE;

	if(list == NULL)
		return ERROR;
	if(servicegroups == NULL)
		return OK;

	/* allocate memory for servicegroup name list */
	if((servicegroup_names = (char *)strdup(servicegroups)) == NULL)
		return ERROR;

	/* expand each servicegroup */
	for(temp_ptr = strtok(servicegroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

		/* should we use regular expression matching? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp = TRUE;
		else
			use_regexp = FALSE;

		/* use regular expression matching */
		if(use_regexp == TRUE) {

			/* compile regular expression */
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				my_free(servicegroup_names);
				return ERROR;
				}

			/* test match against all servicegroup names */
			for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {

				if(temp_servicegroup->servicegroup_name == NULL)
					continue;

				/* skip this servicegroup if it did not match the expression */
				if(regexec(&preg, temp_servicegroup->servicegroup_name, 0, NULL, 0))
					continue;

				found_match = TRUE;

				/* dont' add servicegroups that shouldn't be registered */
				if(temp_servicegroup->register_object == FALSE)
					continue;

				/* add servicegroup to list */
				add_object_to_objectlist(list, temp_servicegroup);
				}

			/* free memory allocated to compiled regexp */
			regfree(&preg);
			}

		/* use standard matching... */
		else {

			/* return a list of all servicegroups */
			if(!strcmp(temp_ptr, "*")) {

				found_match = TRUE;

				for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {

					/* dont' add servicegroups that shouldn't be registered */
					if(temp_servicegroup->register_object == FALSE)
						continue;

					/* add servicegroup to list */
					prepend_object_to_objectlist(list, temp_servicegroup);
					}
				}

			/* else this is just a single servicegroup... */
			else {

				/* this servicegroup should be excluded (rejected) */
				if(temp_ptr[0] == '!') {
					reject_item = TRUE;
					temp_ptr++;
					}

				/* find the servicegroup */
				if((temp_servicegroup = xodtemplate_find_real_servicegroup(temp_ptr)) != NULL) {

					found_match = TRUE;

					/* add servicegroup members to list */
					if(reject_item)
						bitmap_unite(reject, temp_servicegroup->member_map);
					else
						add_object_to_objectlist(list, temp_servicegroup);
					}
				}
			}

		/* we didn't find a matching servicegroup */
		if(found_match == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find any servicegroup matching '%s' (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}

	/* free memory */
	my_free(servicegroup_names);

	if(found_match == FALSE)
		return ERROR;

	return OK;
	}

/* expands services (host name is not expanded) */
int xodtemplate_expand_services(objectlist **list, bitmap *reject_map, char *host_name, char *services, int _config_file, int _start_line) {
	char *service_names = NULL;
	char *temp_ptr = NULL;
	xodtemplate_service *temp_service = NULL;
#ifndef NSCGI
	regex_t preg;
	regex_t preg2;
	int use_regexp_host = FALSE;
	int use_regexp_service = FALSE;
#endif
	int found_match = TRUE;
	int reject_item = FALSE;

	if(list == NULL)
		return ERROR;

	/*
	 * One-step recursion for convenience.
	 * Useful for servicegroups' "members" directive
	 */
	if(host_name == NULL && services != NULL) {
		char *scopy, *next_p, *p1, *p2;

		if (!(scopy = strdup(services)))
			return ERROR;
		for(next_p = p1 = scopy; next_p; p1 = next_p + 1) {
			p2 = strchr(p1, ',');
			if (!p2) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service description missing from list '%s' (config file '%s', starting at line %d)\n",
					  services, xodtemplate_config_file_name(_config_file), _start_line);
				free(scopy);
				return ERROR;
			}
			*p2 = 0;
			while(!*p2 || *p2 == ' ' || *p2 == '\t')
				p2++;
			while(*p1 == ',' || *p1 == ' ' || *p1 == '\t')
				p1++;
			next_p = strchr(p2 + 1, ',');
			if(next_p)
				*next_p = 0;
			strip(p1);
			strip(p2);

			/* now we have arguments we can handle safely, so do that */
			if(xodtemplate_expand_services(list, reject_map, p1, p2, _config_file, _start_line) != OK) {
				free(scopy);
				return ERROR;
				}
			}
		free(scopy);
		}
	if(host_name == NULL || services == NULL)
		return OK;

#ifndef NSCGI
	/* should we use regular expression matching for the host name? */
	if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(host_name, "*") || strstr(host_name, "?") || strstr(host_name, "+") || strstr(host_name, "\\.")))
		use_regexp_host = TRUE;

	/* compile regular expression for host name */
	if(use_regexp_host == TRUE) {
		if(regcomp(&preg2, host_name, REG_EXTENDED))
			return ERROR;
		}
#endif

	if((service_names = (char *)strdup(services)) == NULL) {
#ifndef NSCGI
		if(use_regexp_host == TRUE)
			regfree(&preg2);
#endif
		return ERROR;
		}

	/* expand each service description */
	for(temp_ptr = strtok(service_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

#ifndef NSCGI
		/* should we use regular expression matching for the service description? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp_service = TRUE;
		else
			use_regexp_service = FALSE;

		/* compile regular expression for service description */
		if(use_regexp_service == TRUE) {
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				if(use_regexp_host == TRUE)
					regfree(&preg2);
				my_free(service_names);
				return ERROR;
				}
			}

		/* use regular expression matching */
		if(use_regexp_host == TRUE || use_regexp_service == TRUE) {

			/* test match against all services */
			for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {

				if(temp_service->host_name == NULL || temp_service->service_description == NULL)
					continue;

				/* skip this service if it doesn't match the host name expression */
				if(use_regexp_host == TRUE) {
					if(regexec(&preg2, temp_service->host_name, 0, NULL, 0))
						continue;
					}
				else {
					if(strcmp(temp_service->host_name, host_name))
						continue;
					}

				/* skip this service if it doesn't match the service description expression */
				if(use_regexp_service == TRUE) {
					if(regexec(&preg, temp_service->service_description, 0, NULL, 0))
						continue;
					}
				else {
					if(strcmp(temp_service->service_description, temp_ptr))
						continue;
					}

				found_match = TRUE;

				/* dont' add services that shouldn't be registered */
				if(temp_service->register_object == FALSE)
					continue;

				/* add service to the list */
				add_object_to_objectlist(list, temp_service);
				}

			/* free memory allocated to compiled regexp */
			if(use_regexp_service == TRUE)
				regfree(&preg);
			}

		/* use standard matching... */
		else if(!strcmp(temp_ptr, "*")) {
			/* return a list of all services on the host */

			found_match = TRUE;

			for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {

				if(temp_service->host_name == NULL || temp_service->service_description == NULL)
					continue;

				if(strcmp(temp_service->host_name, host_name))
					continue;

				/* dont' add services that shouldn't be registered */
				if(temp_service->register_object == FALSE)
					continue;

				/* add service to the list */
				add_object_to_objectlist(list, temp_service);
				}
			}

		/* else this is just a single service... */
		else {

			/* this service should be excluded (rejected) */
			if(temp_ptr[0] == '!') {
				reject_item = TRUE;
				temp_ptr++;
				}

#endif
			/* find the service */
			if((temp_service = xodtemplate_find_real_service(host_name, temp_ptr)) != NULL) {

				found_match = TRUE;

				if(reject_item == TRUE)
					bitmap_set(reject_map, temp_service->id);
				else
					add_object_to_objectlist(list, temp_service);
			}
#ifndef NSCGI
		}
#endif
		/* we didn't find a match */
		if(found_match == FALSE && reject_item == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find a service matching host name '%s' and description '%s' (config file '%s', starting on line %d)\n", host_name, temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}
#ifndef NSCGI
	if(use_regexp_host == TRUE)
		regfree(&preg2);
	my_free(service_names);
#endif
	if(found_match == FALSE && reject_item == FALSE)
		return ERROR;

	return OK;
	}

/* returns a comma-delimited list of hostgroup names */
char * xodtemplate_process_hostgroup_names(char *hostgroups, int _config_file, int _start_line) {
	xodtemplate_memberlist *temp_list = NULL;
	xodtemplate_memberlist *reject_list = NULL;
	xodtemplate_memberlist *list_ptr = NULL;
	xodtemplate_memberlist *reject_ptr = NULL;
	xodtemplate_memberlist *this_list = NULL;
	char *buf = NULL;
	int result = OK;

	/* process list of hostgroups... */
	if(hostgroups != NULL) {

		/* split group names into two lists */
		result = xodtemplate_get_hostgroup_names(&temp_list, &reject_list, hostgroups, _config_file, _start_line);
		if(result != OK) {
			xodtemplate_free_memberlist(&temp_list);
			xodtemplate_free_memberlist(&reject_list);
			return NULL;
			}

		/* remove rejects (if any) from the list (no duplicate entries exist in either list) */
		for(reject_ptr = reject_list; reject_ptr != NULL; reject_ptr = reject_ptr->next) {
			for(list_ptr = temp_list; list_ptr != NULL; list_ptr = list_ptr->next) {
				if(!strcmp(reject_ptr->name1, list_ptr->name1)) {
					xodtemplate_remove_memberlist_item(list_ptr, &temp_list);
					break;
					}
				}
			}

		xodtemplate_free_memberlist(&reject_list);
		reject_list = NULL;
		}

	/* generate the list of group members */
	for(this_list = temp_list; this_list != NULL; this_list = this_list->next) {
		if(buf == NULL) {
			buf = (char *)malloc(strlen(this_list->name1) + 1);
			strcpy(buf, this_list->name1);
			}
		else {
			buf = (char *)realloc(buf, strlen(buf) + strlen(this_list->name1) + 2);
			strcat(buf, ",");
			strcat(buf, this_list->name1);
			}
		}

	xodtemplate_free_memberlist(&temp_list);

	return buf;
	}



/* return a list of hostgroup names */
int xodtemplate_get_hostgroup_names(xodtemplate_memberlist **list, xodtemplate_memberlist **reject_list, char *hostgroups, int _config_file, int _start_line) {
	char *hostgroup_names = NULL;
	char *temp_ptr = NULL;
	xodtemplate_hostgroup *temp_hostgroup = NULL;
	regex_t preg;
	int found_match = TRUE;
	int reject_item = FALSE;
	int use_regexp = FALSE;

	if(list == NULL || hostgroups == NULL)
		return ERROR;

	/* allocate memory for hostgroup name list */
	if((hostgroup_names = (char *)strdup(hostgroups)) == NULL)
		return ERROR;

	for(temp_ptr = strtok(hostgroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

		/* should we use regular expression matching? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp = TRUE;
		else
			use_regexp = FALSE;

		/* use regular expression matching */
		if(use_regexp == TRUE) {

			/* compile regular expression */
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				my_free(hostgroup_names);
				return ERROR;
				}

			/* test match against all hostgroup names */
			for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {

				if(temp_hostgroup->hostgroup_name == NULL)
					continue;

				/* skip this hostgroup if it did not match the expression */
				if(regexec(&preg, temp_hostgroup->hostgroup_name, 0, NULL, 0))
					continue;

				found_match = TRUE;

				/* dont' add hostgroups that shouldn't be registered */
				if(temp_hostgroup->register_object == FALSE)
					continue;

				/* add hostgroup to list */
				xodtemplate_add_member_to_memberlist(list, temp_hostgroup->hostgroup_name, NULL);
				}

			/* free memory allocated to compiled regexp */
			regfree(&preg);
			}

		/* use standard matching... */
		else {

			/* return a list of all hostgroups */
			if(!strcmp(temp_ptr, "*")) {

				found_match = TRUE;

				for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {

					/* dont' add hostgroups that shouldn't be registered */
					if(temp_hostgroup->register_object == FALSE)
						continue;

					/* add hostgroup to list */
					xodtemplate_add_member_to_memberlist(list, temp_hostgroup->hostgroup_name, NULL);
					}
				}

			/* else this is just a single hostgroup... */
			else {

				/* this hostgroup should be excluded (rejected) */
				if(temp_ptr[0] == '!') {
					reject_item = TRUE;
					temp_ptr++;
					}

				/* find the hostgroup */
				temp_hostgroup = xodtemplate_find_real_hostgroup(temp_ptr);
				if(temp_hostgroup != NULL) {

					found_match = TRUE;

					/* add hostgroup to proper list */
					xodtemplate_add_member_to_memberlist((reject_item == TRUE) ? reject_list : list, temp_hostgroup->hostgroup_name, NULL);
					}
				}
			}

		if(found_match == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find any hostgroup matching '%s' (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}

	/* free memory */
	my_free(hostgroup_names);

	if(found_match == FALSE)
		return ERROR;

	return OK;
	}



/* returns a comma-delimited list of contactgroup names */
char * xodtemplate_process_contactgroup_names(char *contactgroups, int _config_file, int _start_line) {
	xodtemplate_memberlist *temp_list = NULL;
	xodtemplate_memberlist *reject_list = NULL;
	xodtemplate_memberlist *list_ptr = NULL;
	xodtemplate_memberlist *reject_ptr = NULL;
	xodtemplate_memberlist *this_list = NULL;
	char *buf = NULL;
	int result = OK;

	/* process list of contactgroups... */
	if(contactgroups != NULL) {

		/* split group names into two lists */
		result = xodtemplate_get_contactgroup_names(&temp_list, &reject_list, contactgroups, _config_file, _start_line);
		if(result != OK) {
			xodtemplate_free_memberlist(&temp_list);
			xodtemplate_free_memberlist(&reject_list);
			return NULL;
			}

		/* remove rejects (if any) from the list (no duplicate entries exist in either list) */
		for(reject_ptr = reject_list; reject_ptr != NULL; reject_ptr = reject_ptr->next) {
			for(list_ptr = temp_list; list_ptr != NULL; list_ptr = list_ptr->next) {
				if(!strcmp(reject_ptr->name1, list_ptr->name1)) {
					xodtemplate_remove_memberlist_item(list_ptr, &temp_list);
					break;
					}
				}
			}

		xodtemplate_free_memberlist(&reject_list);
		reject_list = NULL;
		}

	/* generate the list of group members */
	for(this_list = temp_list; this_list != NULL; this_list = this_list->next) {
		if(buf == NULL) {
			buf = (char *)malloc(strlen(this_list->name1) + 1);
			strcpy(buf, this_list->name1);
			}
		else {
			buf = (char *)realloc(buf, strlen(buf) + strlen(this_list->name1) + 2);
			strcat(buf, ",");
			strcat(buf, this_list->name1);
			}
		}

	xodtemplate_free_memberlist(&temp_list);

	return buf;
	}



/* return a list of contactgroup names */
int xodtemplate_get_contactgroup_names(xodtemplate_memberlist **list, xodtemplate_memberlist **reject_list, char *contactgroups, int _config_file, int _start_line) {
	char *contactgroup_names = NULL;
	char *temp_ptr = NULL;
	xodtemplate_contactgroup *temp_contactgroup = NULL;
	regex_t preg;
	int found_match = TRUE;
	int reject_item = FALSE;
	int use_regexp = FALSE;

	if(list == NULL || contactgroups == NULL)
		return ERROR;

	/* allocate memory for contactgroup name list */
	if((contactgroup_names = (char *)strdup(contactgroups)) == NULL)
		return ERROR;

	for(temp_ptr = strtok(contactgroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

		/* should we use regular expression matching? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp = TRUE;
		else
			use_regexp = FALSE;

		/* use regular expression matching */
		if(use_regexp == TRUE) {

			/* compile regular expression */
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				my_free(contactgroup_names);
				return ERROR;
				}

			/* test match against all contactgroup names */
			for(temp_contactgroup = xodtemplate_contactgroup_list; temp_contactgroup != NULL; temp_contactgroup = temp_contactgroup->next) {

				if(temp_contactgroup->contactgroup_name == NULL)
					continue;

				/* skip this contactgroup if it did not match the expression */
				if(regexec(&preg, temp_contactgroup->contactgroup_name, 0, NULL, 0))
					continue;

				found_match = TRUE;

				/* dont' add contactgroups that shouldn't be registered */
				if(temp_contactgroup->register_object == FALSE)
					continue;

				/* add contactgroup to list */
				xodtemplate_add_member_to_memberlist(list, temp_contactgroup->contactgroup_name, NULL);
				}

			/* free memory allocated to compiled regexp */
			regfree(&preg);
			}

		/* use standard matching... */
		else {

			/* return a list of all contactgroups */
			if(!strcmp(temp_ptr, "*")) {

				found_match = TRUE;

				for(temp_contactgroup = xodtemplate_contactgroup_list; temp_contactgroup != NULL; temp_contactgroup = temp_contactgroup->next) {

					/* dont' add contactgroups that shouldn't be registered */
					if(temp_contactgroup->register_object == FALSE)
						continue;

					/* add contactgroup to list */
					xodtemplate_add_member_to_memberlist(list, temp_contactgroup->contactgroup_name, NULL);
					}
				}

			/* else this is just a single contactgroup... */
			else {

				/* this contactgroup should be excluded (rejected) */
				if(temp_ptr[0] == '!') {
					reject_item = TRUE;
					temp_ptr++;
					}

				/* find the contactgroup */
				temp_contactgroup = xodtemplate_find_real_contactgroup(temp_ptr);
				if(temp_contactgroup != NULL) {

					found_match = TRUE;

					/* add contactgroup members to proper list */
					xodtemplate_add_member_to_memberlist((reject_item == TRUE) ? reject_list : list, temp_contactgroup->contactgroup_name, NULL);
					}
				}
			}

		if(found_match == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find any contactgroup matching '%s' (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}

	/* free memory */
	my_free(contactgroup_names);

	if(found_match == FALSE)
		return ERROR;

	return OK;
	}



/* returns a comma-delimited list of servicegroup names */
char * xodtemplate_process_servicegroup_names(char *servicegroups, int _config_file, int _start_line) {
	xodtemplate_memberlist *temp_list = NULL;
	xodtemplate_memberlist *reject_list = NULL;
	xodtemplate_memberlist *list_ptr = NULL;
	xodtemplate_memberlist *reject_ptr = NULL;
	xodtemplate_memberlist *this_list = NULL;
	char *buf = NULL;
	int result = OK;

	/* process list of servicegroups... */
	if(servicegroups != NULL) {

		/* split group names into two lists */
		result = xodtemplate_get_servicegroup_names(&temp_list, &reject_list, servicegroups, _config_file, _start_line);
		if(result != OK) {
			xodtemplate_free_memberlist(&temp_list);
			xodtemplate_free_memberlist(&reject_list);
			return NULL;
			}

		/* remove rejects (if any) from the list (no duplicate entries exist in either list) */
		for(reject_ptr = reject_list; reject_ptr != NULL; reject_ptr = reject_ptr->next) {
			for(list_ptr = temp_list; list_ptr != NULL; list_ptr = list_ptr->next) {
				if(!strcmp(reject_ptr->name1, list_ptr->name1)) {
					xodtemplate_remove_memberlist_item(list_ptr, &temp_list);
					break;
					}
				}
			}

		xodtemplate_free_memberlist(&reject_list);
		reject_list = NULL;
		}

	/* generate the list of group members */
	for(this_list = temp_list; this_list != NULL; this_list = this_list->next) {
		if(buf == NULL) {
			buf = (char *)malloc(strlen(this_list->name1) + 1);
			strcpy(buf, this_list->name1);
			}
		else {
			buf = (char *)realloc(buf, strlen(buf) + strlen(this_list->name1) + 2);
			strcat(buf, ",");
			strcat(buf, this_list->name1);
			}
		}

	xodtemplate_free_memberlist(&temp_list);

	return buf;
	}



/* return a list of servicegroup names */
int xodtemplate_get_servicegroup_names(xodtemplate_memberlist **list, xodtemplate_memberlist **reject_list, char *servicegroups, int _config_file, int _start_line) {
	char *servicegroup_names = NULL;
	char *temp_ptr = NULL;
	xodtemplate_servicegroup *temp_servicegroup = NULL;
	regex_t preg;
	int found_match = TRUE;
	int reject_item = FALSE;
	int use_regexp = FALSE;

	if(list == NULL || servicegroups == NULL)
		return ERROR;

	/* allocate memory for servicegroup name list */
	if((servicegroup_names = (char *)strdup(servicegroups)) == NULL)
		return ERROR;

	for(temp_ptr = strtok(servicegroup_names, ","); temp_ptr; temp_ptr = strtok(NULL, ",")) {

		found_match = FALSE;
		reject_item = FALSE;

		/* strip trailing spaces */
		strip(temp_ptr);

		/* should we use regular expression matching? */
		if(use_regexp_matches == TRUE && (use_true_regexp_matching == TRUE || strstr(temp_ptr, "*") || strstr(temp_ptr, "?") || strstr(temp_ptr, "+") || strstr(temp_ptr, "\\.")))
			use_regexp = TRUE;
		else
			use_regexp = FALSE;

		/* use regular expression matching */
		if(use_regexp == TRUE) {

			/* compile regular expression */
			if(regcomp(&preg, temp_ptr, REG_EXTENDED)) {
				my_free(servicegroup_names);
				return ERROR;
				}

			/* test match against all servicegroup names */
			for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {

				if(temp_servicegroup->servicegroup_name == NULL)
					continue;

				/* skip this servicegroup if it did not match the expression */
				if(regexec(&preg, temp_servicegroup->servicegroup_name, 0, NULL, 0))
					continue;

				found_match = TRUE;

				/* dont' add servicegroups that shouldn't be registered */
				if(temp_servicegroup->register_object == FALSE)
					continue;

				/* add servicegroup to list */
				xodtemplate_add_member_to_memberlist(list, temp_servicegroup->servicegroup_name, NULL);
				}

			/* free memory allocated to compiled regexp */
			regfree(&preg);
			}

		/* use standard matching... */
		else {

			/* return a list of all servicegroups */
			if(!strcmp(temp_ptr, "*")) {

				found_match = TRUE;

				for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {

					/* dont' add servicegroups that shouldn't be registered */
					if(temp_servicegroup->register_object == FALSE)
						continue;

					/* add servicegroup to list */
					xodtemplate_add_member_to_memberlist(list, temp_servicegroup->servicegroup_name, NULL);
					}
				}

			/* else this is just a single servicegroup... */
			else {

				/* this servicegroup should be excluded (rejected) */
				if(temp_ptr[0] == '!') {
					reject_item = TRUE;
					temp_ptr++;
					}

				/* find the servicegroup */
				temp_servicegroup = xodtemplate_find_real_servicegroup(temp_ptr);
				if(temp_servicegroup != NULL) {

					found_match = TRUE;

					/* add servicegroup members to proper list */
					xodtemplate_add_member_to_memberlist((reject_item == TRUE) ? reject_list : list, temp_servicegroup->servicegroup_name, NULL);
					}
				}
			}

		if(found_match == FALSE) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not find any servicegroup matching '%s' (config file '%s', starting on line %d)\n", temp_ptr, xodtemplate_config_file_name(_config_file), _start_line);
			break;
			}
		}

	/* free memory */
	my_free(servicegroup_names);

	if(found_match == FALSE)
		return ERROR;

	return OK;
	}

#ifndef NSCGI

/******************************************************************/
/****************** ADDITIVE INHERITANCE STUFF ********************/
/******************************************************************/

/* determines the value of an inherited string */
int xodtemplate_get_inherited_string(char *have_template_value, char **template_value, char *have_this_value, char **this_value) {
	char *buf = NULL;

	/* template has a value we should use */
	if(*have_template_value == TRUE) {

		/* template has a non-NULL value */
		if(*template_value != NULL) {

			/* we have no value... */
			if(*this_value == NULL) {

				/* use the template value only if we need a value - otherwise stay NULL */
				if(*have_this_value == FALSE) {
					/* NOTE: leave leading + sign if present, as it needed during object resolution and will get stripped later */
					*this_value = (char *)strdup(*template_value);
					}
				}

			/* we already have a value... */
			else {
				/* our value should be added to the template value */
				if(*this_value[0] == '+') {
					if((buf = (char *)malloc(strlen(*template_value) + strlen(*this_value) + 1))) {
						strcpy(buf, *template_value);
						strcat(buf, ",");
						strcat(buf, *this_value + 1);
						my_free(*this_value);
						*this_value = buf;
						}
					}

				/* otherwise our value overrides/replaces the template value */
				}
			}

		/* template has a NULL value.... */

		*have_this_value = TRUE;
		}

	return OK;
	}


/* removes leading + sign from various directives */
int xodtemplate_clean_additive_string(char **str) {
	char *buf = NULL;

	/* remove the additive symbol if present */
	if(*str != NULL && *str[0] == '+') {
		buf = (char *)strdup(*str + 1);
		my_free(*str);
		*str = buf;
		}

	return OK;
	}


/* cleans strings which may contain additive inheritance directives */
/* NOTE: this must be done after objects are resolved */
int xodtemplate_clean_additive_strings(void) {
	xodtemplate_contactgroup *temp_contactgroup = NULL;
	xodtemplate_hostgroup *temp_hostgroup = NULL;
	xodtemplate_servicegroup *temp_servicegroup = NULL;
	xodtemplate_servicedependency *temp_servicedependency = NULL;
	xodtemplate_serviceescalation *temp_serviceescalation = NULL;
	xodtemplate_contact *temp_contact = NULL;
	xodtemplate_host *temp_host = NULL;
	xodtemplate_service *temp_service = NULL;
	xodtemplate_hostdependency *temp_hostdependency = NULL;
	xodtemplate_hostescalation *temp_hostescalation = NULL;

	/* resolve all contactgroup objects */
	for(temp_contactgroup = xodtemplate_contactgroup_list; temp_contactgroup != NULL; temp_contactgroup = temp_contactgroup->next) {
		xodtemplate_clean_additive_string(&temp_contactgroup->members);
		xodtemplate_clean_additive_string(&temp_contactgroup->contactgroup_members);
		}

	/* resolve all hostgroup objects */
	for(temp_hostgroup = xodtemplate_hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {
		xodtemplate_clean_additive_string(&temp_hostgroup->members);
		xodtemplate_clean_additive_string(&temp_hostgroup->hostgroup_members);
		}

	/* resolve all servicegroup objects */
	for(temp_servicegroup = xodtemplate_servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {
		xodtemplate_clean_additive_string(&temp_servicegroup->members);
		xodtemplate_clean_additive_string(&temp_servicegroup->servicegroup_members);
		}

	/* resolve all servicedependency objects */
	for(temp_servicedependency = xodtemplate_servicedependency_list; temp_servicedependency != NULL; temp_servicedependency = temp_servicedependency->next) {
		xodtemplate_clean_additive_string(&temp_servicedependency->servicegroup_name);
		xodtemplate_clean_additive_string(&temp_servicedependency->hostgroup_name);
		xodtemplate_clean_additive_string(&temp_servicedependency->host_name);
		xodtemplate_clean_additive_string(&temp_servicedependency->service_description);
		xodtemplate_clean_additive_string(&temp_servicedependency->dependent_servicegroup_name);
		xodtemplate_clean_additive_string(&temp_servicedependency->dependent_hostgroup_name);
		xodtemplate_clean_additive_string(&temp_servicedependency->dependent_host_name);
		xodtemplate_clean_additive_string(&temp_servicedependency->dependent_service_description);
		}

	/* resolve all serviceescalation objects */
	for(temp_serviceescalation = xodtemplate_serviceescalation_list; temp_serviceescalation != NULL; temp_serviceescalation = temp_serviceescalation->next) {
		/* 03/05/08 some vars are now handled in xodtemplate_inherit_object_properties() */
		/*
		xodtemplate_clean_additive_string(&temp_serviceescalation->contact_groups);
		xodtemplate_clean_additive_string(&temp_serviceescalation->contacts);
		*/
		xodtemplate_clean_additive_string(&temp_serviceescalation->servicegroup_name);
		xodtemplate_clean_additive_string(&temp_serviceescalation->hostgroup_name);
		xodtemplate_clean_additive_string(&temp_serviceescalation->host_name);
		xodtemplate_clean_additive_string(&temp_serviceescalation->service_description);
		}

	/* resolve all contact objects */
	for(temp_contact = xodtemplate_contact_list; temp_contact != NULL; temp_contact = temp_contact->next) {
		xodtemplate_clean_additive_string(&temp_contact->contact_groups);
		xodtemplate_clean_additive_string(&temp_contact->host_notification_commands);
		xodtemplate_clean_additive_string(&temp_contact->service_notification_commands);
		}

	/* clean all host objects */
	for(temp_host = xodtemplate_host_list; temp_host != NULL; temp_host = temp_host->next) {
		xodtemplate_clean_additive_string(&temp_host->contact_groups);
		xodtemplate_clean_additive_string(&temp_host->contacts);
		xodtemplate_clean_additive_string(&temp_host->parents);
		xodtemplate_clean_additive_string(&temp_host->host_groups);
		}

	/* clean all service objects */
	for(temp_service = xodtemplate_service_list; temp_service != NULL; temp_service = temp_service->next) {
		xodtemplate_clean_additive_string(&temp_service->contact_groups);
		xodtemplate_clean_additive_string(&temp_service->contacts);
		xodtemplate_clean_additive_string(&temp_service->host_name);
		xodtemplate_clean_additive_string(&temp_service->hostgroup_name);
		xodtemplate_clean_additive_string(&temp_service->service_groups);
		}

	/* resolve all hostdependency objects */
	for(temp_hostdependency = xodtemplate_hostdependency_list; temp_hostdependency != NULL; temp_hostdependency = temp_hostdependency->next) {
		xodtemplate_clean_additive_string(&temp_hostdependency->host_name);
		xodtemplate_clean_additive_string(&temp_hostdependency->dependent_host_name);
		xodtemplate_clean_additive_string(&temp_hostdependency->hostgroup_name);
		xodtemplate_clean_additive_string(&temp_hostdependency->dependent_hostgroup_name);
		}

	/* resolve all hostescalation objects */
	for(temp_hostescalation = xodtemplate_hostescalation_list; temp_hostescalation != NULL; temp_hostescalation = temp_hostescalation->next) {
		/* 03/05/08 some vars are now handled in xodtemplate_inherit_object_properties() */
		/*
		xodtemplate_clean_additive_string(&temp_hostescalation->contact_groups);
		xodtemplate_clean_additive_string(&temp_hostescalation->contacts);
		*/
		xodtemplate_clean_additive_string(&temp_hostescalation->host_name);
		xodtemplate_clean_additive_string(&temp_hostescalation->hostgroup_name);
		}

	return OK;
	}

#endif
