/*****************************************************************************
 *
 * STATUSWRL.C - Nagios 3-D (VRML) Network Status View
 *
 *
 * Description:
 *
 * This CGI will dynamically create a 3-D VRML model of all hosts that are
 * being monitored on your network.
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
#include "../include/objects.h"
#include "../include/statusdata.h"

#include "../include/cgiutils.h"
#include "../include/getcgi.h"
#include "../include/cgiauth.h"

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_logo_images_path[MAX_FILENAME_LENGTH];
extern char *status_file;

extern char *statuswrl_include;

extern int default_statuswrl_layout_method;


#define NAGIOS_VRML_IMAGE               "nagiosvrml.png"

#define DEFAULT_NODE_WIDTH		0.5
#define DEFAULT_HORIZONTAL_SPACING	1.0
#define DEFAULT_VERTICAL_SPACING	1.0

/* needed for auto-layout modes */
#define DEFAULT_NODE_HEIGHT             0.5
#define DEFAULT_NODE_HSPACING           1.0
#define DEFAULT_NODE_VSPACING           1.0
#define CIRCULAR_DRAWING_RADIUS         5.0

#define LAYOUT_USER_SUPPLIED            0
#define LAYOUT_COLLAPSED_TREE           2
#define LAYOUT_BALANCED_TREE            3
#define LAYOUT_CIRCULAR                 4


void calculate_host_coords(void);
void calculate_world_bounds(void);
void display_world(void);
void write_global_vrml_data(void);
void draw_process_icon(void);
void draw_host(host *);
void draw_host_links(void);
void draw_host_link(host *, double, double, double, double, double, double);
void document_header(void);
int process_cgivars(void);

int number_of_host_layer_members(host *, int);
int max_child_host_layer_members(host *);
int host_child_depth_separation(host *, host *);
int max_child_host_drawing_width(host *);

void calculate_balanced_tree_coords(host *, int, int);
void calculate_circular_coords(void);
void calculate_circular_layer_coords(host *, double, double, int, int);


authdata current_authdata;

float link_radius = 0.016;

float floor_width = 0.0;
float floor_depth = 0.0;

double min_z_coord = 0.0;
double min_x_coord = 0.0;
double min_y_coord = 0.0;
double max_z_coord = 0.0;
double max_x_coord = 0.0;
double max_y_coord = 0.0;

double max_world_size = 0.0;

double nagios_icon_x = 0.0;
double nagios_icon_y = 0.0;
int draw_nagios_icon = FALSE;

double custom_viewpoint_x = 0.0;
double custom_viewpoint_y = 0.0;
double custom_viewpoint_z = 0.0;
int custom_viewpoint = FALSE;

float vertical_spacing = DEFAULT_VERTICAL_SPACING;
float horizontal_spacing = DEFAULT_HORIZONTAL_SPACING;
float node_width = DEFAULT_NODE_WIDTH;
float node_height = DEFAULT_NODE_WIDTH;	/* should be the same as the node width */

char *host_name = "all";
int show_all_hosts = TRUE;

int use_textures = TRUE;
int use_text = TRUE;
int use_links = TRUE;

int layout_method = LAYOUT_USER_SUPPLIED;

int coordinates_were_specified = FALSE; /* were drawing coordinates specified with extended host info entries? */





int main(int argc, char **argv) {
	int result;

	/* reset internal variables */
	reset_cgi_vars();

	/* Initialize shared configuration variables */                             
	init_shared_cfg_vars(1);

	/* read the CGI configuration file */
	result = read_cgi_config_file(get_cgi_config_location());
	if(result == ERROR) {
		document_header();
		return ERROR;
		}

	/* defaults from CGI config file */
	layout_method = default_statuswrl_layout_method;

	/* get the arguments passed in the URL */
	process_cgivars();

	document_header();

	/* read the main configuration file */
	result = read_main_config_file(main_config_file);
	if(result == ERROR)
		return ERROR;

	/* read all object configuration data */
	result = read_all_object_configuration_data(main_config_file, READ_ALL_OBJECT_DATA);
	if(result == ERROR)
		return ERROR;

	/* read all status data */
	result = read_all_status_data(status_file, READ_ALL_STATUS_DATA);
	if(result == ERROR) {
		free_memory();
		return ERROR;
		}

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* display the 3-D VRML world... */
	display_world();

	/* free all allocated memory */
	free_memory();

	return OK;
	}



void document_header(void) {
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;


	printf("Cache-Control: no-store\r\n");
	printf("Pragma: no-cache\r\n");

	time(&current_time);
	get_time_string(&current_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
	printf("Last-Modified: %s\r\n", date_time);

	expire_time = 0L;
	get_time_string(&expire_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
	printf("Expires: %s\r\n", date_time);

	printf("Content-Type: x-world/x-vrml\r\n\r\n");

	return;
	}



int process_cgivars(void) {
	char **variables;
	int error = FALSE;
	int x;

	variables = getcgivars();

	for(x = 0; variables[x] != NULL; x++) {

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x]) >= MAX_INPUT_BUFFER - 1) {
			x++;
			continue;
			}


		/* we found the host argument */
		else if(!strcmp(variables[x], "host")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((host_name = (char *)strdup(variables[x])) == NULL)
				host_name = "all";
			else
				strip_html_brackets(host_name);

			if(!strcmp(host_name, "all"))
				show_all_hosts = TRUE;
			else
				show_all_hosts = FALSE;
			}

		/* we found the no textures argument*/
		else if(!strcmp(variables[x], "notextures"))
			use_textures = FALSE;

		/* we found the no text argument*/
		else if(!strcmp(variables[x], "notext"))
			use_text = FALSE;

		/* we found the no links argument*/
		else if(!strcmp(variables[x], "nolinks"))
			use_links = FALSE;

		/* we found the layout method option */
		else if(!strcmp(variables[x], "layout")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			layout_method = atoi(variables[x]);
			}

		/* we found custom viewpoint coord */
		else if(!strcmp(variables[x], "viewx")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			custom_viewpoint_x = strtod(variables[x], NULL);
			custom_viewpoint = TRUE;
			}
		else if(!strcmp(variables[x], "viewy")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			custom_viewpoint_y = strtod(variables[x], NULL);
			custom_viewpoint = TRUE;
			}
		else if(!strcmp(variables[x], "viewz")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			custom_viewpoint_z = strtod(variables[x], NULL);
			custom_viewpoint = TRUE;
			}

		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



/* top-level VRML world generation... */
void display_world(void) {
	host *temp_host = NULL;

	/* get the url we will use to grab the logo images... */
	snprintf(url_logo_images_path, sizeof(url_logo_images_path), "%slogos/", url_images_path);
	url_logo_images_path[sizeof(url_logo_images_path) - 1] = '\x0';

	/* calculate host drawing coordinates */
	calculate_host_coords();

	/* calculate world bounds */
	calculate_world_bounds();

	/* get the floor dimensions */
	if(max_x_coord > 0)
		floor_width = (float)(max_x_coord - min_x_coord) + (node_width * 2);
	else
		floor_width = (float)(max_x_coord + min_x_coord) + (node_width * 2);
	if(max_z_coord > 0)
		floor_depth = (float)(max_z_coord - min_z_coord) + (node_height * 2);
	else
		floor_depth = (float)(max_z_coord + min_z_coord) + (node_height * 2);

	/* write global VRML data */
	write_global_vrml_data();

	/* no coordinates were specified, so display warning message */
	if(coordinates_were_specified == FALSE) {

		printf("\n");
		printf("Transform{\n");
		printf("translation 0.0 0.0 0.0\n");
		printf("children[\n");

		printf("Billboard{\n");
		printf("children[\n");
		printf("Shape{\n");
		printf("appearance Appearance {\n");
		printf("material Material {\n");
		printf("diffuseColor 1 0 0\n");
		printf("}\n");
		printf("}\n");
		printf("geometry Text {\n");
		printf("string [ \"Error: You have not supplied any 3-D drawing coordinates.\", \"Read the documentation for more information on supplying\", \"3-D drawing coordinates by defining\", \"extended host information entries in your config files.\" ]\n");
		printf("fontStyle FontStyle {\n");
		printf("family \"TYPEWRITER\"\n");
		printf("size 0.3\n");
		printf("justify \"MIDDLE\"\n");
		printf("}\n");
		printf("}\n");
		printf("}\n");
		printf("]\n");
		printf("}\n");

		printf("]\n");
		printf("}\n");
		}

	/* coordinates were specified... */
	else {

		/* draw Nagios icon */
		if(layout_method != LAYOUT_USER_SUPPLIED)
			draw_process_icon();

		/* draw all hosts */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next)
			draw_host(temp_host);

		/* draw host links */
		draw_host_links();
		}

	return;
	}




/******************************************************************/
/************************ UTILITY FUNCTIONS ***********************/
/******************************************************************/

/* calculates how many "layers" separate parent and child - used by collapsed tree layout method */
int host_child_depth_separation(host *parent, host *child) {
	int this_depth = 0;
	int min_depth = 0;
	int have_min_depth = FALSE;
	host *temp_host;

	if(child == NULL)
		return -1;

	if(parent == child)
		return 0;

	if(is_host_immediate_child_of_host(parent, child) == TRUE)
		return 1;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(is_host_immediate_child_of_host(parent, temp_host) == TRUE) {

			this_depth = host_child_depth_separation(temp_host, child);

			if(this_depth >= 0 && (have_min_depth == FALSE || (have_min_depth == TRUE && (this_depth < min_depth)))) {
				have_min_depth = TRUE;
				min_depth = this_depth;
				}
			}
		}

	if(have_min_depth == FALSE)
		return -1;
	else
		return min_depth + 1;
	}



/* calculates how many hosts reside on a specific "layer" - used by collapsed tree layout method */
int number_of_host_layer_members(host *parent, int layer) {
	int current_layer;
	int layer_members = 0;
	host *temp_host;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		current_layer = host_child_depth_separation(parent, temp_host);

		if(current_layer == layer)
			layer_members++;
		}

	return layer_members;
	}



/* calculate max number of members on all "layers" beneath and including parent host - used by collapsed tree layout method */
int max_child_host_layer_members(host *parent) {
	int current_layer;
	int max_members = 1;
	int current_members = 0;

	for(current_layer = 1;; current_layer++) {

		current_members = number_of_host_layer_members(parent, current_layer);

		if(current_members <= 0)
			break;

		if(current_members > max_members)
			max_members = current_members;
		}

	return max_members;
	}



/* calculate max drawing width for host and children - used by balanced tree layout method */
int max_child_host_drawing_width(host *parent) {
	host *temp_host;
	int child_width = 0;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(is_host_immediate_child_of_host(parent, temp_host) == TRUE)
			child_width += max_child_host_drawing_width(temp_host);
		}

	/* no children, so set width to 1 for this host */
	if(child_width == 0)
		return 1;

	else
		return child_width;
	}




/******************************************************************/
/********************* CALCULATION FUNCTIONS **********************/
/******************************************************************/

/* calculates host drawing coordinates */
void calculate_host_coords(void) {
	host *this_host;
	host *temp_host;
	int parent_hosts = 0;
	int max_layer_width = 1;
	int current_parent_host = 0;
	int center_x = 0;
	int offset_x = DEFAULT_NODE_WIDTH / 2;
	int offset_y = DEFAULT_NODE_WIDTH / 2;
	int current_layer = 0;
	int layer_members = 0;
	int current_layer_member = 0;
	int max_drawing_width = 0;


	/******************************/
	/***** MANUAL LAYOUT MODE *****/
	/******************************/

	/* user-supplied coords */
	if(layout_method == LAYOUT_USER_SUPPLIED) {

		/* see which hosts we should draw (only those with 3-D coords) */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

			if(temp_host->have_3d_coords == TRUE)
				temp_host->should_be_drawn = TRUE;
			else
				temp_host->should_be_drawn = FALSE;
			}

		return;
		}

	/*****************************/
	/***** AUTO-LAYOUT MODES *****/
	/*****************************/

	/* add empty extended host info entries for all hosts that don't have any */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* none was found, so add a blank one */
		/*
		if(temp_hostextinfo==NULL)
			add_hostextinfo(temp_host->name,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,0.0,0.0,0.0,0,0);
		*/

		/* default z coord should 0 for auto-layout modes unless overridden later */
		/*
		else
		*/
		temp_host->z_3d = 0.0;
		}


	/***** COLLAPSED TREE MODE *****/
	if(layout_method == LAYOUT_COLLAPSED_TREE) {

		/* always use NULL as the "main" host, screen coords/dimensions are adjusted automatically */
		this_host = NULL;

		/* find total number of immediate parents for this host */
		parent_hosts = number_of_immediate_parent_hosts(this_host);

		/* find the max layer width we have... */
		max_layer_width = max_child_host_layer_members(this_host);
		if(parent_hosts > max_layer_width)
			max_layer_width = parent_hosts;

		/* calculate center x coord */
		center_x = (((DEFAULT_NODE_WIDTH * max_layer_width) + (DEFAULT_NODE_HSPACING * (max_layer_width - 1))) / 2) + offset_x;

		/* coords for Nagios icon if necessary */
		if(this_host == NULL || this_host->parent_hosts == NULL) {
			nagios_icon_x = center_x;
			nagios_icon_y = offset_y;
			draw_nagios_icon = TRUE;
			}

		/* do we need to draw a link to parent(s)? */
		if(this_host != NULL && is_host_immediate_child_of_host(NULL, this_host) == FALSE)
			offset_y += DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;

		/* see which hosts we should draw and calculate drawing coords */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

			/* this is an immediate parent of the "main" host we're drawing */
			if(is_host_immediate_parent_of_host(this_host, temp_host) == TRUE) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_3d_coords = TRUE;
				temp_host->x_3d = center_x - (((parent_hosts * DEFAULT_NODE_WIDTH) + ((parent_hosts - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_parent_host * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
				temp_host->y_3d = offset_y;
				current_parent_host++;
				}

			/* this is the "main" host we're drawing */
			else if(this_host == temp_host) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_3d_coords = TRUE;
				temp_host->x_3d = center_x;
				temp_host->y_3d = DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING + offset_y;
				}

			/* else do not draw this host (we might if its a child - see below, but assume no for now) */
			else {
				temp_host->should_be_drawn = FALSE;
				temp_host->have_3d_coords = FALSE;
				}
			}


		/* TODO: REORDER CHILD LAYER MEMBERS SO THAT WE MINIMIZE LINK CROSSOVERS FROM PARENT HOSTS */

		/* draw hosts in child "layers" */
		for(current_layer = 1;; current_layer++) {

			/* how many members in this layer? */
			layer_members = number_of_host_layer_members(this_host, current_layer);

			if(layer_members == 0)
				break;

			current_layer_member = 0;

			/* see which hosts are members of this layer and calculate drawing coords */
			for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

				/* is this host a member of the current child layer? */
				if(host_child_depth_separation(this_host, temp_host) == current_layer) {
					temp_host->should_be_drawn = TRUE;
					temp_host->have_3d_coords = TRUE;
					temp_host->x_3d = center_x - (((layer_members * DEFAULT_NODE_WIDTH) + ((layer_members - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_layer_member * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
					if(this_host == NULL)
						temp_host->y_3d = ((DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING) * current_layer) + offset_y;
					else
						temp_host->y_3d = ((DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING) * (current_layer + 1)) + offset_y;
					current_layer_member++;
					}
				}
			}

		}


	/***** "BALANCED" TREE MODE *****/
	else if(layout_method == LAYOUT_BALANCED_TREE) {

		/* always use NULL as the "main" host, screen coords/dimensions are adjusted automatically */
		this_host = NULL;

		/* find total number of immediate parents for this host */
		parent_hosts = number_of_immediate_parent_hosts(this_host);

		/* find the max drawing width we have... */
		max_drawing_width = max_child_host_drawing_width(this_host);
		if(parent_hosts > max_drawing_width)
			max_drawing_width = parent_hosts;

		/* calculate center x coord */
		center_x = (((DEFAULT_NODE_WIDTH * max_drawing_width) + (DEFAULT_NODE_HSPACING * (max_drawing_width - 1))) / 2) + offset_x;

		/* coords for Nagios icon if necessary */
		if(this_host == NULL || this_host->parent_hosts == NULL) {
			nagios_icon_x = center_x;
			nagios_icon_y = offset_y;
			draw_nagios_icon = TRUE;
			}

		/* do we need to draw a link to parent(s)? */
		if(this_host != NULL && is_host_immediate_child_of_host(NULL, this_host) == FALSE)
			offset_y += DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;

		/* see which hosts we should draw and calculate drawing coords */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

			/* this is an immediate parent of the "main" host we're drawing */
			if(is_host_immediate_parent_of_host(this_host, temp_host) == TRUE) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_3d_coords = TRUE;
				temp_host->x_3d = center_x - (((parent_hosts * DEFAULT_NODE_WIDTH) + ((parent_hosts - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_parent_host * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
				temp_host->y_3d = offset_y;
				current_parent_host++;
				}

			/* this is the "main" host we're drawing */
			else if(this_host == temp_host) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_3d_coords = TRUE;
				temp_host->x_3d = center_x;
				temp_host->y_3d = DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING + offset_y;
				}

			/* else do not draw this host (we might if its a child - see below, but assume no for now) */
			else {
				temp_host->should_be_drawn = FALSE;
				temp_host->have_3d_coords = FALSE;
				}
			}

		/* draw all children hosts */
		calculate_balanced_tree_coords(this_host, center_x, DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING + offset_y);

		}


	/***** CIRCULAR LAYOUT MODE *****/
	else if(layout_method == LAYOUT_CIRCULAR) {

		/* draw process icon */
		nagios_icon_x = 0;
		nagios_icon_y = 0;
		draw_nagios_icon = TRUE;

		/* calculate coordinates for all hosts */
		calculate_circular_coords();
		}

	return;
	}



/* calculate world dimensions */
void calculate_world_bounds(void) {
	host *temp_host;

	min_x_coord = 0.0;
	min_y_coord = 0.0;
	min_z_coord = 0.0;
	max_x_coord = 0.0;
	max_y_coord = 0.0;
	max_z_coord = 0.0;

	/* check all extended host entries */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(temp_host->have_3d_coords == FALSE) {
			temp_host->should_be_drawn = FALSE;
			continue;
			}

		if(temp_host->should_be_drawn == FALSE)
			continue;

		if(temp_host->x_3d < min_x_coord)
			min_x_coord = temp_host->x_3d;
		else if(temp_host->x_3d > max_x_coord)
			max_x_coord = temp_host->x_3d;
		if(temp_host->y_3d < min_y_coord)
			min_y_coord = temp_host->y_3d;
		else if(temp_host->y_3d > max_y_coord)
			max_y_coord = temp_host->y_3d;
		if(temp_host->z_3d < min_z_coord)
			min_z_coord = temp_host->z_3d;
		else if(temp_host->z_3d > max_z_coord)
			max_z_coord = temp_host->z_3d;

		coordinates_were_specified = TRUE;
		}

	/* no drawing coordinates were specified */
	if(coordinates_were_specified == FALSE) {
		min_x_coord = 0.0;
		max_x_coord = 0.0;
		min_y_coord = 0.0;
		max_y_coord = 0.0;
		min_z_coord = 0.0;
		max_z_coord = 6.0;
		}

	max_world_size = max_x_coord - min_x_coord;
	if(max_world_size < (max_y_coord - min_y_coord))
		max_world_size = max_y_coord - min_y_coord;
	if(max_world_size < (max_z_coord - min_z_coord))
		max_world_size = max_z_coord - min_z_coord;

	return;
	}


/******************************************************************/
/*********************** DRAWING FUNCTIONS ************************/
/******************************************************************/


/* write global VRML data */
void write_global_vrml_data(void) {
	host *temp_host;
	float visibility_range = 0.0;
	float viewpoint_z = 0.0;

	/* write VRML code header */
	printf("#VRML V2.0 utf8\n");

	/* write world information */
	printf("\n");
	printf("WorldInfo{\n");
	printf("title \"Nagios 3-D Network Status View\"\n");
	printf("info [\"Copyright (c) 1999-2002 Ethan Galstad\"\n");
	printf("\"egalstad@nagios.org\"]\n");
	printf("}\n");

	/* background color */
	printf("\n");
	printf("Background{\n");
	printf("skyColor 0.1 0.1 0.15\n");
	printf("}\n");

	/* calculate visibility range - don't let it get too low */
	visibility_range = (max_world_size * 2.0);
	if(visibility_range < 25.0)
		visibility_range = 25.0;

	/* write fog information */
	printf("\n");
	printf("Fog{\n");
	printf("color 0.1 0.1 0.15\n");
	printf("fogType \"EXPONENTIAL\"\n");
	printf("visibilityRange %2.2f\n", visibility_range);
	printf("}\n");

	/* custom viewpoint */
	if(custom_viewpoint == TRUE) {
		printf("\n");
		printf("Viewpoint{\n");
		printf("position %2.2f %2.2f %2.2f\n", custom_viewpoint_x, custom_viewpoint_y, custom_viewpoint_z);
		printf("fieldOfView 0.78\n");
		printf("description \"Entry Viewpoint\"\n");
		printf("}\n");
		}

	/* host close-up viewpoint */
	if(show_all_hosts == FALSE) {

		temp_host = find_host(host_name);
		if(temp_host != NULL && temp_host->have_3d_coords == TRUE) {
			printf("\n");
			printf("Viewpoint{\n");
			printf("position %2.3f %2.3f %2.3f\n", temp_host->x_3d, temp_host->y_3d, temp_host->z_3d + 5.0);
			printf("fieldOfView 0.78\n");
			printf("description \"Host Close-Up Viewpoint\"\n");
			printf("}\n");
			}
		}

	/* calculate z coord for default viewpoint - don't get too close */
	viewpoint_z = max_world_size;
	if(viewpoint_z < 10.0)
		viewpoint_z = 10.0;

	/* default viewpoint */
	printf("\n");
	printf("Viewpoint{\n");
	printf("position %2.2f %2.2f %2.2f\n", min_x_coord + ((max_x_coord - min_x_coord) / 2.0), min_y_coord + ((max_y_coord - min_y_coord) / 2.0), viewpoint_z);
	printf("fieldOfView 0.78\n");
	printf("description \"Default Viewpoint\"\n");
	printf("}\n");

	/* problem timer */
	printf("DEF ProblemTimer TimeSensor{\n");
	printf("loop TRUE\n");
	printf("cycleInterval 5\n");
	printf("}\n");

	/* host text prototype */
	printf("PROTO HostText[\n");
	printf("field MFString the_text [\"\"]\n");
	printf("field SFColor font_color 0.6 0.6 0.6");
	printf("]\n");
	printf("{\n");
	printf("Billboard{\n");
	printf("children[\n");
	printf("Shape{\n");
	printf("appearance Appearance {\n");
	printf("material Material {\n");
	printf("diffuseColor IS font_color\n");
	printf("}\n");
	printf("}\n");
	printf("geometry Text {\n");
	printf("string IS the_text\n");
	printf("fontStyle FontStyle {\n");
	printf("family \"TYPEWRITER\"\n");
	printf("size 0.1\n");
	printf("justify \"MIDDLE\"\n");
	printf("}\n");
	printf("}\n");
	printf("}\n");
	printf("]\n");
	printf("}\n");
	printf("}\n");

	/* include user-defined world */
	if(statuswrl_include != NULL && coordinates_were_specified == TRUE && layout_method == LAYOUT_USER_SUPPLIED) {
		printf("\n");
		printf("Inline{\n");
		printf("url \"%s%s\"\n", url_html_path, statuswrl_include);
		printf("}\n");
		}

	return;
	}



/* draws a host */
void draw_host(host *temp_host) {
	hoststatus *temp_hoststatus = NULL;
	char state_string[16] = "";
	double x, y, z;
	char *vrml_safe_hostname = NULL;
	int a, ch;

	if(temp_host == NULL)
		return;

	/* make sure we have the coordinates */
	if(temp_host->have_3d_coords == FALSE)
		return;
	else {
		x = temp_host->x_3d;
		y = temp_host->y_3d;
		z = temp_host->z_3d;
		}

	/* make the host name safe for embedding in VRML */
	vrml_safe_hostname = (char *)strdup(temp_host->name);
	if(vrml_safe_hostname == NULL)
		return;
	for(a = 0; vrml_safe_hostname[a] != '\x0'; a++) {
		ch = vrml_safe_hostname[a];
		if((ch < 'a' || ch > 'z') && (ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9'))
			vrml_safe_hostname[a] = '_';
		}

	/* see if user is authorized to view this host  */
	if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
		return;

	/* get the status of the host */
	temp_hoststatus = find_hoststatus(temp_host->name);

	printf("\n");


	/* host object */
	printf("Anchor{\n");
	printf("children[\n");

	printf("Transform {\n");
	printf("translation %2.2f %2.2f %2.2f\n", x, y, z);
	printf("children [\n");

	printf("DEF Host%s Shape{\n", vrml_safe_hostname);
	printf("appearance Appearance{\n");
	printf("material DEF HostMat%s Material{\n", vrml_safe_hostname);
	if(temp_hoststatus == NULL)
		printf("emissiveColor 0.2 0.2 0.2\ndiffuseColor 0.2 0.2 0.2\n");
	else if(temp_hoststatus->status == SD_HOST_UP)
		printf("emissiveColor 0.2 1.0 0.2\ndiffuseColor 0.2 1.0 0.2\n");
	else
		printf("emissiveColor 1.0 0.2 0.2\ndiffuseColor 1.0 0.2 0.2\n");
	printf("transparency 0.4\n");
	printf("}\n");
	if(use_textures == TRUE && temp_host->vrml_image != NULL) {
		printf("texture ImageTexture{\n");
		printf("url \"%s%s\"\n", url_logo_images_path, temp_host->vrml_image);
		printf("}\n");
		}
	printf("}\n");
	printf("geometry Box{\n");
	printf("size %2.2f %2.2f %2.2f\n", node_width, node_width, node_width);
	printf("}\n");
	printf("}\n");

	printf("]\n");
	printf("}\n");

	printf("]\n");
	printf("description \"View status details for host '%s' (%s)\"\n", temp_host->name, temp_host->alias);
	printf("url \"%s?host=%s\"\n", STATUS_CGI, temp_host->name);
	printf("}\n");


	/* draw status text */
	if(use_text == TRUE) {

		printf("\n");
		printf("Transform{\n");
		printf("translation %2.3f %2.3f %2.3f\n", x, y + DEFAULT_NODE_WIDTH, z);
		printf("children[\n");
		printf("HostText{\n");

		if(temp_hoststatus != NULL) {
			if(temp_hoststatus->status == SD_HOST_UP)
				printf("font_color 0 1 0\n");
			else if(temp_hoststatus->status == SD_HOST_DOWN || temp_hoststatus->status == SD_HOST_UNREACHABLE)
				printf("font_color 1 0 0\n");
			}
		printf("the_text [\"%s\", \"%s\", ", temp_host->name, temp_host->alias);
		if(temp_hoststatus == NULL)
			strcpy(state_string, "UNKNOWN");
		else {
			if(temp_hoststatus->status == SD_HOST_DOWN)
				strcpy(state_string, "DOWN");
			else if(temp_hoststatus->status == SD_HOST_UNREACHABLE)
				strcpy(state_string, "UNREACHABLE");
			else if(temp_hoststatus->status == HOST_PENDING)
				strcpy(state_string, "PENDING");
			else
				strcpy(state_string, "UP");
			}
		printf("\"%s\"]\n", state_string);

		printf("}\n");
		printf("]\n");
		printf("}\n");
		}

	/* host is down or unreachable, so make it fade in and out */
	if(temp_hoststatus != NULL && (temp_hoststatus->status == SD_HOST_DOWN || temp_hoststatus->status == SD_HOST_UNREACHABLE))
		printf("ROUTE ProblemTimer.fraction_changed TO HostMat%s.set_transparency\n", vrml_safe_hostname);

	free(vrml_safe_hostname);

	return;
	}



/* draw links between hosts */
void draw_host_links(void) {
	host *parent_host;
	host *child_host;

	if(use_links == FALSE)
		return;

	for(child_host = host_list; child_host != NULL; child_host = child_host->next) {

		if(child_host->have_3d_coords == FALSE)
			continue;

		/* check authorization */
		if(is_authorized_for_host(child_host, &current_authdata) == FALSE)
			continue;

		/* draw a link from this host to all of its parent hosts */
		for(parent_host = host_list; parent_host != NULL; parent_host = parent_host->next) {

			if(is_host_immediate_child_of_host(child_host, parent_host) == TRUE) {

				if(parent_host->have_3d_coords == FALSE)
					continue;

				/* check authorization */
				if(is_authorized_for_host(parent_host, &current_authdata) == FALSE)
					continue;

				/* draw the link between the child and parent hosts */
				draw_host_link(parent_host, parent_host->x_3d, parent_host->y_3d, parent_host->z_3d, child_host->x_3d, child_host->y_3d, child_host->z_3d);
				}
			}
		}


	return;
	}




/* draws a link from a parent host to a child host */
void draw_host_link(host *hst, double x0, double y0, double z0, double x1, double y1, double z1) {

	printf("\n");

	if(hst != NULL)
		printf("# Host '%s' LINK\n", hst->name);

	printf("Shape{\n");

	printf("appearance DEF MATslategrey_0_ Appearance {\n");
	printf("material Material {\n");
	printf("diffuseColor 0.6 0.6 0.6\n");
	printf("ambientIntensity 0.5\n");
	printf("emissiveColor 0.6 0.6 0.6\n");
	printf("}\n");
	printf("}\n");

	printf("geometry IndexedLineSet{\n");
	printf("coord Coordinate{\n");
	printf("point [ %2.3f %2.3f %2.3f, %2.3f %2.3f %2.3f ]\n", x0, y0, z0, x1, y1, z1);
	printf("}\n");
	printf("coordIndex [ 0,1,-1 ]\n");
	printf("}\n");

	printf("}\n");

	return;
	}



/* draw process icon */
void draw_process_icon(void) {
	host *child_host;

	if(draw_nagios_icon == FALSE)
		return;

	/* draw process icon */
	printf("\n");


	printf("Anchor{\n");
	printf("children[\n");

	printf("Transform {\n");
	printf("translation %2.2f %2.2f %2.2f\n", nagios_icon_x, nagios_icon_y, 0.0);
	printf("children [\n");

	printf("DEF ProcessNode Shape{\n");
	printf("appearance Appearance{\n");
	printf("material Material{\n");
	printf("emissiveColor 0.5 0.5 0.5\n");
	printf("diffuseColor 0.5 0.5 0.5\n");
	printf("transparency 0.2\n");
	printf("}\n");
	if(use_textures == TRUE) {
		printf("texture ImageTexture{\n");
		printf("url \"%s%s\"\n", url_logo_images_path, NAGIOS_VRML_IMAGE);
		printf("}\n");
		}
	printf("}\n");
	printf("geometry Box{\n");
	printf("size %2.2f %2.2f %2.2f\n", node_width * 3.0, node_width * 3.0, node_width * 3.0);
	printf("}\n");
	printf("}\n");

	printf("]\n");
	printf("}\n");

	printf("]\n");
	printf("description \"View Nagios Process Information\"\n");
	printf("url \"%s?type=%d\"\n", EXTINFO_CGI, DISPLAY_PROCESS_INFO);
	printf("}\n");


	if(use_links == FALSE)
		return;

	/* draw links to immediate child hosts */
	for(child_host = host_list; child_host != NULL; child_host = child_host->next) {

		if(child_host->have_3d_coords == FALSE)
			continue;

		/* check authorization */
		if(is_authorized_for_host(child_host, &current_authdata) == FALSE)
			continue;

		/* draw a link to the host */
		if(is_host_immediate_child_of_host(NULL, child_host) == TRUE)
			draw_host_link(NULL, nagios_icon_x, nagios_icon_y, 0.0, child_host->x_3d, child_host->y_3d, child_host->z_3d);
		}

	return;
	}




/******************************************************************/
/***************** COORDINATE CALCULATION FUNCTIONS ***************/
/******************************************************************/

/* calculates coords of a host's children - used by balanced tree layout method */
void calculate_balanced_tree_coords(host *parent, int x, int y) {
	int parent_drawing_width;
	int start_drawing_x;
	int current_drawing_x;
	int this_drawing_width;
	host *temp_host;

	/* calculate total drawing width of parent host */
	parent_drawing_width = max_child_host_drawing_width(parent);

	/* calculate starting x coord */
	start_drawing_x = x - (((DEFAULT_NODE_WIDTH * parent_drawing_width) + (DEFAULT_NODE_HSPACING * (parent_drawing_width - 1))) / 2);
	current_drawing_x = start_drawing_x;


	/* calculate coords for children */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(is_host_immediate_child_of_host(parent, temp_host) == TRUE) {

			/* get drawing width of child host */
			this_drawing_width = max_child_host_drawing_width(temp_host);

			temp_host->x_3d = current_drawing_x + (((DEFAULT_NODE_WIDTH * this_drawing_width) + (DEFAULT_NODE_HSPACING * (this_drawing_width - 1))) / 2);
			temp_host->y_3d = y + DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;
			temp_host->have_3d_coords = TRUE;
			temp_host->should_be_drawn = TRUE;

			current_drawing_x += (this_drawing_width * DEFAULT_NODE_WIDTH) + ((this_drawing_width - 1) * DEFAULT_NODE_HSPACING) + DEFAULT_NODE_HSPACING;

			/* recurse into child host ... */
			calculate_balanced_tree_coords(temp_host, temp_host->x_3d, temp_host->y_3d);
			}

		}

	return;
	}


/* calculate coords of all hosts in circular layout method */
void calculate_circular_coords(void) {

	/* calculate all host coords, starting with first layer */
	calculate_circular_layer_coords(NULL, 0.0, 360.0, 1, CIRCULAR_DRAWING_RADIUS);

	return;
	}


/* calculates coords of all hosts in a particular "layer" in circular layout method */
void calculate_circular_layer_coords(host *parent, double start_angle, double useable_angle, int layer, int radius) {
	int parent_drawing_width = 0;
	int this_drawing_width = 0;
	int immediate_children = 0;
	double current_drawing_angle = 0.0;
	double this_drawing_angle = 0.0;
	double available_angle = 0.0;
	double clipped_available_angle = 0.0;
	double x_coord = 0.0;
	double y_coord = 0.0;
	host *temp_host;


	/* get the total number of immediate children to this host */
	immediate_children = number_of_immediate_child_hosts(parent);

	/* bail out if we're done */
	if(immediate_children == 0)
		return;

	/* calculate total drawing "width" of parent host */
	parent_drawing_width = max_child_host_drawing_width(parent);

	/* calculate initial drawing angle */
	current_drawing_angle = start_angle;


	/* calculate coords for children */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(is_host_immediate_child_of_host(parent, temp_host) == TRUE) {

			/* get drawing width of child host */
			this_drawing_width = max_child_host_drawing_width(temp_host);

			/* calculate angle this host gets for drawing */
			available_angle = useable_angle * ((double)this_drawing_width / (double)parent_drawing_width);

			/* clip available angle if necessary */
			/* this isn't really necessary, but helps keep things looking a bit more sane with less potential connection crossover */
			clipped_available_angle = 360.0 / layer;
			if(available_angle < clipped_available_angle)
				clipped_available_angle = available_angle;

			/* calculate the exact angle at which we should draw this child */
			this_drawing_angle = current_drawing_angle + (available_angle / 2.0);

			/* compensate for angle overflow */
			while(this_drawing_angle >= 360.0)
				this_drawing_angle -= 360.0;
			while(this_drawing_angle < 0.0)
				this_drawing_angle += 360.0;

			/* calculate drawing coords of this host using good ol' geometry... */
			x_coord = -(sin(-this_drawing_angle * (M_PI / 180.0)) * radius);
			y_coord = -(sin((90 + this_drawing_angle) * (M_PI / 180.0)) * radius);

			temp_host->x_3d = (int)x_coord;
			temp_host->y_3d = (int)y_coord;
			temp_host->have_3d_coords = TRUE;
			temp_host->should_be_drawn = TRUE;

			/* recurse into child host ... */
			calculate_circular_layer_coords(temp_host, current_drawing_angle + ((available_angle - clipped_available_angle) / 2), clipped_available_angle, layer + 1, radius + CIRCULAR_DRAWING_RADIUS);

			/* increment current drawing angle */
			current_drawing_angle += available_angle;
			}
		}

	return;
	}
