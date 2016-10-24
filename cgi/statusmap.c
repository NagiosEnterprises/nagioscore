/*****************************************************************************
 *
 * STATUSMAP.C - Nagios Network Status Map CGI
 *
 *
 * Description:
 *
 * This CGI will create a map of all hosts that are being monitored on your
 * network.
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
#include "../include/macros.h"
#include "../include/statusdata.h"
#include "../include/cgiutils.h"
#include "../include/getcgi.h"
#include "../include/cgiauth.h"

#include <gd.h>			/* Boutell's GD library function */
#include <gdfonts.h>		/* GD library small font definition */

static nagios_macros *mac;

extern int             refresh_rate;

/*#define DEBUG*/

#define UNKNOWN_GD2_ICON      "unknown.gd2"
#define UNKNOWN_ICON_IMAGE    "unknown.gif"
#define NAGIOS_GD2_ICON       "nagios.gd2"

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char physical_images_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_logo_images_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];
extern char *status_file;

extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;

extern char *statusmap_background_image;

extern int default_statusmap_layout_method;

#define DEFAULT_NODE_WIDTH		40
#define DEFAULT_NODE_HEIGHT		65

#define DEFAULT_NODE_VSPACING           15
#define DEFAULT_NODE_HSPACING           45

#define DEFAULT_PROXIMITY_WIDTH		1000
#define DEFAULT_PROXIMITY_HEIGHT	800

#define MINIMUM_PROXIMITY_WIDTH         250
#define MINIMUM_PROXIMITY_HEIGHT        200

#define COORDS_WARNING_WIDTH            650
#define COORDS_WARNING_HEIGHT           60

#define CIRCULAR_DRAWING_RADIUS         100

#define CREATE_HTML	0
#define CREATE_IMAGE	1

#define LAYOUT_USER_SUPPLIED            0
#define LAYOUT_SUBLAYERS                1
#define LAYOUT_COLLAPSED_TREE           2
#define LAYOUT_BALANCED_TREE            3
#define LAYOUT_CIRCULAR                 4
#define LAYOUT_CIRCULAR_MARKUP          5
#define LAYOUT_CIRCULAR_BALLOON         6


struct layer {
	char *layer_name;
	struct layer *next;
};


void document_header(int);
void document_footer(void);
int process_cgivars(void);

void display_page_header(void);
void display_map(void);
void calculate_host_coords(void);
void calculate_total_image_bounds(void);
void calculate_canvas_bounds(void);
void calculate_canvas_bounds_from_host(char *);
void calculate_scaling_factor(void);
void find_eligible_hosts(void);
void load_background_image(void);
void draw_background_image(void);
void draw_background_extras(void);
void draw_host_links(void);
void draw_hosts(void);
void draw_host_text(char *, int, int);
void draw_text(char *, int, int, int);
void write_popup_code(void);
void write_host_popup_text(host *);

int initialize_graphics(void);
gdImagePtr load_image_from_file(char *);
void write_graphics(void);
void cleanup_graphics(void);
void draw_line(int, int, int, int, int);
void draw_dotted_line(int, int, int, int, int);
void draw_dashed_line(int, int, int, int, int);

int is_host_in_layer_list(host *);
int add_layer(char *);
void free_layer_list(void);
void print_layer_url(int);

int number_of_host_layer_members(host *, int);
int max_child_host_layer_members(host *);
int host_child_depth_separation(host *, host *);
int max_child_host_drawing_width(host *);
int number_of_host_services(host *);

void calculate_balanced_tree_coords(host *, int, int);
void calculate_circular_coords(void);
void calculate_circular_layer_coords(host *, double, double, int, int);

void draw_circular_markup(void);
void draw_circular_layer_markup(host *, double, double, int, int);


char physical_logo_images_path[MAX_FILENAME_LENGTH];

authdata current_authdata;

int create_type = CREATE_HTML;

gdImagePtr unknown_logo_image = NULL;
gdImagePtr logo_image = NULL;
gdImagePtr map_image = NULL;
gdImagePtr background_image = NULL;
int color_white = 0;
int color_black = 0;
int color_red = 0;
int color_lightred = 0;
int color_green = 0;
int color_lightgreen = 0;
int color_blue = 0;
int color_yellow = 0;
int color_orange = 0;
int color_grey = 0;
int color_lightgrey = 0;
int color_transparency_index = 0;
extern int color_transparency_index_r;
extern int color_transparency_index_g;
extern int color_transparency_index_b;

int show_all_hosts = TRUE;
char *host_name = "all";

int embedded = FALSE;
int display_header = TRUE;
int display_popups = TRUE;
int use_links = TRUE;
int use_text = TRUE;
int use_highlights = TRUE;
int user_supplied_canvas = FALSE;
int user_supplied_scaling = FALSE;

int layout_method = LAYOUT_USER_SUPPLIED;

int proximity_width = DEFAULT_PROXIMITY_WIDTH;
int proximity_height = DEFAULT_PROXIMITY_HEIGHT;

int coordinates_were_specified = FALSE; /* were any coordinates specified in extended host information entries? */

int scaled_image_width = 0;      /* size of the image actually displayed on the screen (after scaling) */
int scaled_image_height = 0;
int canvas_width = 0;            /* actual size of the image (or portion thereof) that we are drawing */
int canvas_height = 0;
int total_image_width = 0;       /* actual size of the image that would be created if we drew all hosts */
int total_image_height = 0;
int max_image_width = 0;         /* max image size the user wants (scaled) */
int max_image_height = 0;
double scaling_factor = 1.0;     /* scaling factor to use */
double user_scaling_factor = 1.0; /* user-supplied scaling factor */
int background_image_width = 0;
int background_image_height = 0;

int canvas_x = 0;                   /* upper left coords of drawing canvas */
int canvas_y = 0;

int bottom_margin = 0;

int draw_child_links = FALSE;
int draw_parent_links = FALSE;

int draw_nagios_icon = FALSE;  /* should we drawn the Nagios process icon? */
int nagios_icon_x = 0;         /* coords of Nagios icon */
int nagios_icon_y = 0;

extern hoststatus *hoststatus_list;

struct layer *layer_list = NULL;
int exclude_layers = TRUE;
int all_layers = FALSE;





int main(int argc, char **argv) {
	int result;

	mac = get_global_macros();

	/* reset internal variables */
	reset_cgi_vars();

	/* Initialize shared configuration variables */                             
	init_shared_cfg_vars(1);

	/* read the CGI configuration file */
	result = read_cgi_config_file(get_cgi_config_location());
	if(result == ERROR) {
		document_header(FALSE);
		if(create_type == CREATE_HTML)
			cgi_config_file_error(get_cgi_config_location());
		document_footer();
		return ERROR;
		}

	/* defaults from CGI config file */
	layout_method = default_statusmap_layout_method;

	/* get the arguments passed in the URL */
	process_cgivars();

	/* read the main configuration file */
	result = read_main_config_file(main_config_file);
	if(result == ERROR) {
		document_header(FALSE);
		if(create_type == CREATE_HTML)
			main_config_file_error(main_config_file);
		document_footer();
		return ERROR;
		}

	/* read all object configuration data */
	result = read_all_object_configuration_data(main_config_file, READ_ALL_OBJECT_DATA);
	if(result == ERROR) {
		document_header(FALSE);
		if(create_type == CREATE_HTML)
			object_data_error();
		document_footer();
		return ERROR;
		}

	/* read all status data */
	result = read_all_status_data(status_file, READ_ALL_STATUS_DATA);
	if(result == ERROR) {
		document_header(FALSE);
		if(create_type == CREATE_HTML)
			status_data_error();
		document_footer();
		free_memory();
		return ERROR;
		}

	/* initialize macros */
	init_macros();


	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* display the network map... */
	display_map();

	document_footer();

	/* free all allocated memory */
	free_memory();
	free_layer_list();

	return OK;
	}



void document_header(int use_stylesheet) {
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;

	if(create_type == CREATE_HTML) {
		printf("Cache-Control: no-store\r\n");
		printf("Pragma: no-cache\r\n");
		printf("Refresh: %d\r\n", refresh_rate);

		time(&current_time);
		get_time_string(&current_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Last-Modified: %s\r\n", date_time);

		expire_time = 0L;
		get_time_string(&expire_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Expires: %s\r\n", date_time);

		printf("Content-Type: text/html; charset=utf-8\r\n\r\n");

		if(embedded == TRUE)
			return;

		printf("<html>\n");
		printf("<head>\n");
		printf("<link rel=\"shortcut icon\" href=\"%sfavicon.ico\" type=\"image/ico\">\n", url_images_path);
		printf("<title>\n");
		printf("Network Map\n");
		printf("</title>\n");

		if(use_stylesheet == TRUE) {
			printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMON_CSS);
			printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, STATUSMAP_CSS);
			}

		/* write JavaScript code for popup window */
		write_popup_code();

		printf("</head>\n");

		printf("<body CLASS='statusmap' name='mappage' id='mappage'>\n");

		/* include user SSI header */
#ifdef LEGACY_GRAPHICAL_CGIS
		include_ssi_files(STATUSMAP_CGI, SSI_HEADER);
#else
		include_ssi_files(LEGACY_STATUSMAP_CGI, SSI_HEADER);
#endif

		printf("<div id=\"popup\" style=\"position:absolute; z-index:1; visibility: hidden\"></div>\n");
		}

	else {
		printf("Cache-Control: no-store\n");
		printf("Pragma: no-cache\n");

		time(&current_time);
		get_time_string(&current_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Last-Modified: %s\n", date_time);

		expire_time = (time_t)0L;
		get_time_string(&expire_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Expires: %s\n", date_time);

		printf("Content-Type: image/png\n\n");
		}

	return;
	}


void document_footer(void) {

	if(embedded == TRUE)
		return;

	if(create_type == CREATE_HTML) {

		/* include user SSI footer */
#ifdef LEGACY_GRAPHICAL_CGIS
		include_ssi_files(STATUSMAP_CGI, SSI_FOOTER);
#else
		include_ssi_files(LEGACY_STATUSMAP_CGI, SSI_FOOTER);
#endif

		printf("</body>\n");
		printf("</html>\n");
		}

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

		/* we found the image creation option */
		else if(!strcmp(variables[x], "createimage")) {
			create_type = CREATE_IMAGE;
			}

		/* we found the embed option */
		else if(!strcmp(variables[x], "embedded"))
			embedded = TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x], "noheader"))
			display_header = FALSE;

		/* we found the canvas origin */
		else if(!strcmp(variables[x], "canvas_x")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			canvas_x = atoi(variables[x]);
			user_supplied_canvas = TRUE;
			}
		else if(!strcmp(variables[x], "canvas_y")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			canvas_y = atoi(variables[x]);
			user_supplied_canvas = TRUE;
			}

		/* we found the canvas size */
		else if(!strcmp(variables[x], "canvas_width")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			canvas_width = atoi(variables[x]);
			user_supplied_canvas = TRUE;
			}
		else if(!strcmp(variables[x], "canvas_height")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			canvas_height = atoi(variables[x]);
			user_supplied_canvas = TRUE;
			}
		else if(!strcmp(variables[x], "proximity_width")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			proximity_width = atoi(variables[x]);
			if(proximity_width < 0)
				proximity_width = DEFAULT_PROXIMITY_WIDTH;
			}
		else if(!strcmp(variables[x], "proximity_height")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			proximity_height = atoi(variables[x]);
			if(proximity_height < 0)
				proximity_height = DEFAULT_PROXIMITY_HEIGHT;
			}

		/* we found the scaling factor */
		else if(!strcmp(variables[x], "scaling_factor")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			user_scaling_factor = strtod(variables[x], NULL);
			if(user_scaling_factor > 0.0)
				user_supplied_scaling = TRUE;
			}

		/* we found the max image size */
		else if(!strcmp(variables[x], "max_width")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			max_image_width = atoi(variables[x]);
			}
		else if(!strcmp(variables[x], "max_height")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			max_image_height = atoi(variables[x]);
			}

		/* we found the layout method option */
		else if(!strcmp(variables[x], "layout")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			layout_method = atoi(variables[x]);
			}

		/* we found the no links argument*/
		else if(!strcmp(variables[x], "nolinks"))
			use_links = FALSE;

		/* we found the no text argument*/
		else if(!strcmp(variables[x], "notext"))
			use_text = FALSE;

		/* we found the no highlights argument*/
		else if(!strcmp(variables[x], "nohighlights"))
			use_highlights = FALSE;

		/* we found the no popups argument*/
		else if(!strcmp(variables[x], "nopopups"))
			display_popups = FALSE;

		/* we found the layer inclusion/exclusion argument */
		else if(!strcmp(variables[x], "layermode")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "include"))
				exclude_layers = FALSE;
			else
				exclude_layers = TRUE;
			}

		/* we found the layer argument */
		else if(!strcmp(variables[x], "layer")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			strip_html_brackets(variables[x]);
			add_layer(variables[x]);
			}
		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



/* top of page */
void display_page_header(void) {
	char temp_buffer[MAX_INPUT_BUFFER];
	int zoom;
	int zoom_width, zoom_height;
	int zoom_width_granularity = 0;
	int zoom_height_granularity = 0;
	int current_zoom_granularity = 0;
	hostgroup *temp_hostgroup;
	struct layer *temp_layer;
	int found = 0;


	if(create_type != CREATE_HTML)
		return;

	if(display_header == TRUE) {

		/* begin top table */
		printf("<table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top>\n");

		if(show_all_hosts == TRUE)
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Network Map For All Hosts");
		else
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Network Map For Host <I>%s</I>", host_name);
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		display_info_table(temp_buffer, TRUE, &current_authdata);

		printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='linkBox'>\n");
		printf("<TR><TD CLASS='linkBox'>\n");

		if(show_all_hosts == FALSE) {
#ifdef LEGACY_GRAPHICAL_CGIS
			printf("<a href='%s?host=all&max_width=%d&max_height=%d'>View Status Map For All Hosts</a><BR>", STATUSMAP_CGI, max_image_width, max_image_height);
#else
			printf("<a href='%s?host=all&max_width=%d&max_height=%d'>View Status Map For All Hosts</a><BR>", LEGACY_STATUSMAP_CGI, max_image_width, max_image_height);
#endif
			printf("<a href='%s?host=%s'>View Status Detail For This Host</a><BR>\n", STATUS_CGI, url_encode(host_name));
			}
		printf("<a href='%s?host=all'>View Status Detail For All Hosts</a><BR>\n", STATUS_CGI);
		printf("<a href='%s?hostgroup=all'>View Status Overview For All Hosts</a>\n", STATUS_CGI);

		printf("</TD></TR>\n");
		printf("</TABLE>\n");

		printf("</td>\n");



		/* center column of top row */
		printf("<td align=center valign=center>\n");

		/* print image size and scaling info */
#ifdef DEBUG
		printf("<p><div align=center><font size=-1>\n");
		printf("[ Raw Image Size: %d x %d pixels | Scaling Factor: %1.2lf | Scaled Image Size: %d x %d pixels ]", canvas_width, canvas_height, scaling_factor, (int)(canvas_width * scaling_factor), (int)(canvas_height * scaling_factor));
		printf("</font></div></p>\n");

		printf("<p><div align=center><font size=-1>\n");
		printf("[ Canvas_x: %d | Canvas_y: %d | Canvas_width: %d | Canvas_height: %d ]", canvas_x, canvas_y, canvas_width, canvas_height);
		printf("</font></div></p>\n");
#endif

		/* zoom links */
		if(user_supplied_canvas == FALSE && strcmp(host_name, "all") && display_header == TRUE) {

			printf("<p><div align=center>\n");

			zoom_width_granularity = ((total_image_width - MINIMUM_PROXIMITY_WIDTH) / 11);
			if(zoom_width_granularity == 0)
				zoom_width_granularity = 1;
			zoom_height_granularity = ((total_image_height - MINIMUM_PROXIMITY_HEIGHT) / 11);

			if(proximity_width <= 0)
				current_zoom_granularity = 0;
			else
				current_zoom_granularity = (total_image_width - proximity_width) / zoom_width_granularity;
			if(current_zoom_granularity > 10)
				current_zoom_granularity = 10;

			printf("<table border=0 cellpadding=0 cellspacing=2>\n");
			printf("<tr>\n");
			printf("<td valign=center class='zoomTitle'>Zoom Out&nbsp;&nbsp;</td>\n");

			for(zoom = 0; zoom <= 10; zoom++) {

				zoom_width = total_image_width - (zoom * zoom_width_granularity);
				zoom_height = total_image_height - (zoom * zoom_height_granularity);

#ifdef LEGACY_GRAPHICAL_CGIS
				printf("<td valign=center><a href='%s?host=%s&layout=%d&max_width=%d&max_height=%d&proximity_width=%d&proximity_height=%d%s%s", STATUSMAP_CGI, url_encode(host_name), layout_method, max_image_width, max_image_height, zoom_width, zoom_height, (display_header == TRUE) ? "" : "&noheader", (display_popups == FALSE) ? "&nopopups" : "");
#else
				printf("<td valign=center><a href='%s?host=%s&layout=%d&max_width=%d&max_height=%d&proximity_width=%d&proximity_height=%d%s%s", LEGACY_STATUSMAP_CGI, url_encode(host_name), layout_method, max_image_width, max_image_height, zoom_width, zoom_height, (display_header == TRUE) ? "" : "&noheader", (display_popups == FALSE) ? "&nopopups" : "");
#endif
				if(user_supplied_scaling == TRUE)
					printf("&scaling_factor=%2.1f", user_scaling_factor);
				print_layer_url(TRUE);
				printf("'>");
				printf("<img src='%s%s' border=0 alt='%d' title='%d'></a></td>\n", url_images_path, (current_zoom_granularity == zoom) ? ZOOM2_ICON : ZOOM1_ICON, zoom, zoom);
				}

			printf("<td valign=center class='zoomTitle'>&nbsp;&nbsp;Zoom In</td>\n");
			printf("</tr>\n");
			printf("</table>\n");

			printf("</div></p>\n");
			}

		printf("</td>\n");



		/* right hand column of top row */
		printf("<td align=right valign=top>\n");

#ifdef LEGACY_GRAPHICAL_CGIS
		printf("<form method=\"POST\" action=\"%s\">\n", STATUSMAP_CGI);
#else
		printf("<form method=\"POST\" action=\"%s\">\n", LEGACY_STATUSMAP_CGI);
#endif
		printf("<table border=0 CLASS='optBox'>\n");
		printf("<tr><td valign=top>\n");
		printf("<input type='hidden' name='host' value='%s'>\n", escape_string(host_name));
		printf("<input type='hidden' name='layout' value='%d'>\n", layout_method);

		printf("</td><td valign=top>\n");

		printf("<table border=0>\n");

		printf("<tr><td CLASS='optBoxItem'>\n");
		printf("Layout Method:<br>\n");
		printf("<select name='layout'>\n");
#ifndef DUMMY_INSTALL
		printf("<option value=%d %s>User-supplied coords\n", LAYOUT_USER_SUPPLIED, (layout_method == LAYOUT_USER_SUPPLIED) ? "selected" : "");
#endif
		printf("<option value=%d %s>Depth layers\n", LAYOUT_SUBLAYERS, (layout_method == LAYOUT_SUBLAYERS) ? "selected" : "");
		printf("<option value=%d %s>Collapsed tree\n", LAYOUT_COLLAPSED_TREE, (layout_method == LAYOUT_COLLAPSED_TREE) ? "selected" : "");
		printf("<option value=%d %s>Balanced tree\n", LAYOUT_BALANCED_TREE, (layout_method == LAYOUT_BALANCED_TREE) ? "selected" : "");
		printf("<option value=%d %s>Circular\n", LAYOUT_CIRCULAR, (layout_method == LAYOUT_CIRCULAR) ? "selected" : "");
		printf("<option value=%d %s>Circular (Marked Up)\n", LAYOUT_CIRCULAR_MARKUP, (layout_method == LAYOUT_CIRCULAR_MARKUP) ? "selected" : "");
		printf("<option value=%d %s>Circular (Balloon)\n", LAYOUT_CIRCULAR_BALLOON, (layout_method == LAYOUT_CIRCULAR_BALLOON) ? "selected" : "");
		printf("</select>\n");
		printf("</td>\n");
		printf("<td CLASS='optBoxItem'>\n");
		printf("Scaling factor:<br>\n");
		printf("<input type='text' name='scaling_factor' maxlength='5' size='4' value='%2.1f'>\n", (user_supplied_scaling == TRUE) ? user_scaling_factor : 0.0);
		printf("</td></tr>\n");

		/*
		printf("<tr><td CLASS='optBoxItem'>\n");
		printf("Max image width:<br>\n");
		printf("<input type='text' name='max_width' maxlength='5' size='4' value='%d'>\n",max_image_width);
		printf("</td>\n");
		printf("<td CLASS='optBoxItem'>\n");
		printf("Max image height:<br>\n");
		printf("<input type='text' name='max_height' maxlength='5' size='4' value='%d'>\n",max_image_height);
		printf("</td></tr>\n");

		printf("<tr><td CLASS='optBoxItem'>\n");
		printf("Proximity width:<br>\n");
		printf("<input type='text' name='proximity_width' maxlength='5' size='4' value='%d'>\n",proximity_width);
		printf("</td>\n");
		printf("<td CLASS='optBoxItem'>\n");
		printf("Proximity height:<br>\n");
		printf("<input type='text' name='proximity_height' maxlength='5' size='4' value='%d'>\n",proximity_height);
		printf("</td></tr>\n");
		*/

		printf("<input type='hidden' name='max_width' value='%d'>\n", max_image_width);
		printf("<input type='hidden' name='max_height' value='%d'>\n", max_image_height);
		printf("<input type='hidden' name='proximity_width' value='%d'>\n", proximity_width);
		printf("<input type='hidden' name='proximity_height' value='%d'>\n", proximity_height);

		printf("<tr><td CLASS='optBoxItem'>Drawing Layers:<br>\n");
		printf("<select multiple name='layer' size='4'>\n");
		for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {
			if(is_authorized_for_hostgroup(temp_hostgroup, &current_authdata) == FALSE)
				continue;
			found = 0;
			for(temp_layer = layer_list; temp_layer != NULL; temp_layer = temp_layer->next) {
				if(!strcmp(temp_layer->layer_name, temp_hostgroup->group_name)) {
					found = 1;
					break;
					}
				}
			printf("<option value='%s' %s>%s\n", escape_string(temp_hostgroup->group_name), (found == 1) ? "SELECTED" : "", temp_hostgroup->alias);
			}
		printf("</select>\n");
		printf("</td><td CLASS='optBoxItem' valign=top>Layer mode:<br>");
		printf("<input type='radio' name='layermode' value='include' %s>Include<br>\n", (exclude_layers == FALSE) ? "CHECKED" : "");
		printf("<input type='radio' name='layermode' value='exclude' %s>Exclude\n", (exclude_layers == TRUE) ? "CHECKED" : "");
		printf("</td></tr>\n");

		printf("<tr><td CLASS='optBoxItem'>\n");
		printf("Suppress popups:<br>\n");
		printf("<input type='checkbox' name='nopopups' %s>\n", (display_popups == FALSE) ? "CHECKED" : "");
		printf("</td><td CLASS='optBoxItem'>\n");
		printf("<input type='submit' value='Update'>\n");
		printf("</td></tr>\n");

		/* display context-sensitive help */
		printf("<tr><td></td><td align=right valign=bottom>\n");
		display_context_help(CONTEXTHELP_MAP);
		printf("</td></tr>\n");

		printf("</table>\n");

		printf("</td></tr>\n");
		printf("</table>\n");
		printf("</form>\n");

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
		}


	return;
	}



/* top-level map generation... */
void display_map(void) {

	load_background_image();
	calculate_host_coords();
	calculate_total_image_bounds();
	calculate_canvas_bounds();
	calculate_scaling_factor();
	find_eligible_hosts();

	/* display page header */
	display_page_header();

	initialize_graphics();
	draw_background_image();
	draw_background_extras();
	draw_host_links();

	if(create_type == CREATE_HTML)
		printf("<map name='statusmap'>\n");

	draw_hosts();

	if(create_type == CREATE_HTML)
		printf("</map>\n");

	write_graphics();
	cleanup_graphics();


	/* write the URL location for the image we just generated - the web browser will come and get it... */
	if(create_type == CREATE_HTML) {
		printf("<P><DIV ALIGN=center>\n");
#ifdef LEGACY_GRAPHICAL_CGIS
		printf("<img src='%s?host=%s&createimage&time=%llu", STATUSMAP_CGI, url_encode(host_name), (unsigned long long)time(NULL));
#else
		printf("<img src='%s?host=%s&createimage&time=%llu", LEGACY_STATUSMAP_CGI, url_encode(host_name), (unsigned long long)time(NULL));
#endif
		printf("&canvas_x=%d&canvas_y=%d&canvas_width=%d&canvas_height=%d&max_width=%d&max_height=%d&layout=%d%s%s%s", canvas_x, canvas_y, canvas_width, canvas_height, max_image_width, max_image_height, layout_method, (use_links == FALSE) ? "&nolinks" : "", (use_text == FALSE) ? "&notext" : "", (use_highlights == FALSE) ? "&nohighlights" : "");
		print_layer_url(TRUE);
		printf("' width=%d height=%d border=0 name='statusimage' useMap='#statusmap'>\n", (int)(canvas_width * scaling_factor), (int)(canvas_height * scaling_factor));
		printf("</DIV></P>\n");
		}

	return;
	}



/******************************************************************/
/********************* CALCULATION FUNCTIONS **********************/
/******************************************************************/

/* calculates host drawing coordinates */
void calculate_host_coords(void) {
	host *this_host;
	host *temp_host;
	int child_hosts = 0;
	int parent_hosts = 0;
	int max_layer_width = 1;
	int current_child_host = 0;
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

		/* see which hosts we should draw and calculate drawing coords */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

			if(temp_host->have_2d_coords == TRUE)
				temp_host->should_be_drawn = TRUE;
			else
				temp_host->should_be_drawn = FALSE;
			}

		return;
		}


	/*****************************/
	/***** AUTO-LAYOUT MODES *****/
	/*****************************/

	/***** DEPTH LAYER MODE *****/
	if(layout_method == LAYOUT_SUBLAYERS) {

		/* find the "main" host we're displaying */
		if(show_all_hosts == TRUE)
			this_host = NULL;
		else
			this_host = find_host(host_name);

		/* find total number of immediate parents/children for this host */
		child_hosts = number_of_immediate_child_hosts(this_host);
		parent_hosts = number_of_immediate_parent_hosts(this_host);

		if(child_hosts == 0 && parent_hosts == 0)
			max_layer_width = 1;
		else
			max_layer_width = (child_hosts > parent_hosts) ? child_hosts : parent_hosts;

		/* calculate center x coord */
		center_x = (((DEFAULT_NODE_WIDTH * max_layer_width) + (DEFAULT_NODE_HSPACING * (max_layer_width - 1))) / 2) + offset_x;

		/* coords for Nagios icon if necessary */
		if(this_host == NULL || this_host->parent_hosts == NULL) {
			nagios_icon_x = center_x;
			nagios_icon_y = offset_y;
			draw_nagios_icon = TRUE;
			}

		/* do we need to draw a link to parent(s)? */
		if(this_host != NULL && is_host_immediate_child_of_host(NULL, this_host) == FALSE) {
			draw_parent_links = TRUE;
			offset_y += DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;
			}

		/* see which hosts we should draw and calculate drawing coords */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

			/* this is an immediate parent of the "main" host we're drawing */
			if(is_host_immediate_parent_of_host(this_host, temp_host) == TRUE) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_2d_coords = TRUE;
				temp_host->x_2d = center_x - (((parent_hosts * DEFAULT_NODE_WIDTH) + ((parent_hosts - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_parent_host * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
				temp_host->y_2d = offset_y;
				current_parent_host++;
				}

			/* this is the "main" host we're drawing */
			else if(this_host == temp_host) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_2d_coords = TRUE;
				temp_host->x_2d = center_x;
				temp_host->y_2d = DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING + offset_y;
				}

			/* this is an immediate child of the "main" host we're drawing */
			else if(is_host_immediate_child_of_host(this_host, temp_host) == TRUE) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_2d_coords = TRUE;
				temp_host->x_2d = center_x - (((child_hosts * DEFAULT_NODE_WIDTH) + ((child_hosts - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_child_host * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
				if(this_host == NULL)
					temp_host->y_2d = (DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING) + offset_y;
				else
					temp_host->y_2d = ((DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING) * 2) + offset_y;
				current_child_host++;
				if(number_of_immediate_child_hosts(temp_host) > 0) {
					bottom_margin = DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;
					draw_child_links = TRUE;
					}
				}

			/* else do not draw this host */
			else {
				temp_host->should_be_drawn = FALSE;
				temp_host->have_2d_coords = FALSE;
				}
			}
		}



	/***** COLLAPSED TREE MODE *****/
	else if(layout_method == LAYOUT_COLLAPSED_TREE) {

		/* find the "main" host we're displaying  - DO NOT USE THIS (THIS IS THE OLD METHOD) */
		/*
		if(show_all_hosts==TRUE)
			this_host=NULL;
		else
			this_host=find_host(host_name);
		*/

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
		if(this_host != NULL && is_host_immediate_child_of_host(NULL, this_host) == FALSE) {
			draw_parent_links = TRUE;
			offset_y += DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;
			}

		/* see which hosts we should draw and calculate drawing coords */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

			/* this is an immediate parent of the "main" host we're drawing */
			if(is_host_immediate_parent_of_host(this_host, temp_host) == TRUE) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_2d_coords = TRUE;
				temp_host->x_2d = center_x - (((parent_hosts * DEFAULT_NODE_WIDTH) + ((parent_hosts - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_parent_host * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
				temp_host->y_2d = offset_y;
				current_parent_host++;
				}

			/* this is the "main" host we're drawing */
			else if(this_host == temp_host) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_2d_coords = TRUE;
				temp_host->x_2d = center_x;
				temp_host->y_2d = DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING + offset_y;
				}

			/* else do not draw this host (we might if its a child - see below, but assume no for now) */
			else {
				temp_host->should_be_drawn = FALSE;
				temp_host->have_2d_coords = FALSE;
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
					temp_host->have_2d_coords = TRUE;
					temp_host->x_2d = center_x - (((layer_members * DEFAULT_NODE_WIDTH) + ((layer_members - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_layer_member * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
					if(this_host == NULL)
						temp_host->y_2d = ((DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING) * current_layer) + offset_y;
					else
						temp_host->y_2d = ((DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING) * (current_layer + 1)) + offset_y;
					current_layer_member++;
					}
				}
			}

		}


	/***** "BALANCED" TREE MODE *****/
	else if(layout_method == LAYOUT_BALANCED_TREE) {

		/* find the "main" host we're displaying  - DO NOT USE THIS (THIS IS THE OLD METHOD) */
		/*
		if(show_all_hosts==TRUE)
			this_host=NULL;
		else
			this_host=find_host(host_name);
		*/

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
		if(this_host != NULL && is_host_immediate_child_of_host(NULL, this_host) == FALSE) {
			draw_parent_links = TRUE;
			offset_y += DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;
			}

		/* see which hosts we should draw and calculate drawing coords */
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

			/* this is an immediate parent of the "main" host we're drawing */
			if(is_host_immediate_parent_of_host(this_host, temp_host) == TRUE) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_2d_coords = TRUE;
				temp_host->x_2d = center_x - (((parent_hosts * DEFAULT_NODE_WIDTH) + ((parent_hosts - 1) * DEFAULT_NODE_HSPACING)) / 2) + (current_parent_host * (DEFAULT_NODE_WIDTH + DEFAULT_NODE_HSPACING)) + (DEFAULT_NODE_WIDTH / 2);
				temp_host->y_2d = offset_y;
				current_parent_host++;
				}

			/* this is the "main" host we're drawing */
			else if(this_host == temp_host) {
				temp_host->should_be_drawn = TRUE;
				temp_host->have_2d_coords = TRUE;
				temp_host->x_2d = center_x;
				temp_host->y_2d = DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING + offset_y;
				}

			/* else do not draw this host (we might if its a child - see below, but assume no for now) */
			else {
				temp_host->should_be_drawn = FALSE;
				temp_host->have_2d_coords = FALSE;
				}
			}

		/* draw all children hosts */
		calculate_balanced_tree_coords(this_host, center_x, DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING + offset_y);

		}


	/***** CIRCULAR LAYOUT MODE *****/
	else if(layout_method == LAYOUT_CIRCULAR || layout_method == LAYOUT_CIRCULAR_MARKUP || layout_method == LAYOUT_CIRCULAR_BALLOON) {

		/* draw process icon */
		nagios_icon_x = 0;
		nagios_icon_y = 0;
		draw_nagios_icon = TRUE;

		/* calculate coordinates for all hosts */
		calculate_circular_coords();
		}

	return;
	}



/* calculates max possible image dimensions */
void calculate_total_image_bounds(void) {
	host *temp_host;

	total_image_width = 0;
	total_image_height = 0;

	/* check all extended host information entries... */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* only check entries that have 2-D coords specified */
		if(temp_host->have_2d_coords == FALSE)
			continue;

		/* skip hosts we shouldn't be drawing */
		if(temp_host->should_be_drawn == FALSE)
			continue;

		if(temp_host->x_2d > total_image_width)
			total_image_width = temp_host->x_2d;
		if(temp_host->y_2d > total_image_height)
			total_image_height = temp_host->y_2d;

		coordinates_were_specified = TRUE;
		}

	/* add some space for icon size and overlapping text... */
	if(coordinates_were_specified == TRUE) {

		total_image_width += (DEFAULT_NODE_WIDTH * 2);
		total_image_height += DEFAULT_NODE_HEIGHT;

		/* add space for bottom margin if necessary */
		total_image_height += bottom_margin;
		}

	/* image size should be at least as large as dimensions of background image */
	if(total_image_width < background_image_width)
		total_image_width = background_image_width;
	if(total_image_height < background_image_height)
		total_image_height = background_image_height;

	/* we didn't find any hosts that had user-supplied coordinates, so we're going to display a warning */
	if(coordinates_were_specified == FALSE) {
		coordinates_were_specified = FALSE;
		total_image_width = COORDS_WARNING_WIDTH;
		total_image_height = COORDS_WARNING_HEIGHT;
		}

	return;
	}


/* calculates canvas coordinates/dimensions */
void calculate_canvas_bounds(void) {

	if(user_supplied_canvas == FALSE && strcmp(host_name, "all"))
		calculate_canvas_bounds_from_host(host_name);

	/* calculate canvas origin (based on total image bounds) */
	if(canvas_x <= 0 || canvas_width > total_image_width)
		canvas_x = 0;
	if(canvas_y <= 0 || canvas_height > total_image_height)
		canvas_y = 0;

	/* calculate canvas dimensions */
	if(canvas_height <= 0)
		canvas_height = (total_image_height - canvas_y);
	if(canvas_width <= 0)
		canvas_width = (total_image_width - canvas_x);

	if(canvas_x + canvas_width > total_image_width)
		canvas_width = total_image_width - canvas_x;
	if(canvas_y + canvas_height > total_image_height)
		canvas_height = total_image_height - canvas_y;

	return;
	}


/* calculates canvas coordinates/dimensions around a particular host */
void calculate_canvas_bounds_from_host(char *hname) {
	host *temp_host;
	int zoom_width;
	int zoom_height;

	/* find the extended host info */
	temp_host = find_host(hname);
	if(temp_host == NULL)
		return;

	/* make sure we have 2-D coords */
	if(temp_host->have_2d_coords == FALSE)
		return;

	if(max_image_width > 0 && proximity_width > max_image_width)
		zoom_width = max_image_width;
	else
		zoom_width = proximity_width;
	if(max_image_height > 0 && proximity_height > max_image_height)
		zoom_height = max_image_height;
	else
		zoom_height = proximity_height;

	canvas_width = zoom_width;
	if(canvas_width >= total_image_width)
		canvas_x = 0;
	else
		canvas_x = (temp_host->x_2d - (zoom_width / 2));

	canvas_height = zoom_height;
	if(canvas_height >= total_image_height)
		canvas_y = 0;
	else
		canvas_y = (temp_host->y_2d - (zoom_height / 2));


	return;
	}


/* calculates scaling factor used in image generation */
void calculate_scaling_factor(void) {
	double x_scaling = 1.0;
	double y_scaling = 1.0;

	/* calculate horizontal scaling factor */
	if(max_image_width <= 0 || canvas_width <= max_image_width)
		x_scaling = 1.0;
	else
		x_scaling = (double)((double)max_image_width / (double)canvas_width);

	/* calculate vertical scaling factor */
	if(max_image_height <= 0 || canvas_height <= max_image_height)
		y_scaling = 1.0;
	else
		y_scaling = (double)((double)max_image_height / (double)canvas_height);

	/* calculate general scaling factor to use */
	if(x_scaling < y_scaling)
		scaling_factor = x_scaling;
	else
		scaling_factor = y_scaling;

	/*** USER-SUPPLIED SCALING FACTOR ***/
	if(user_supplied_scaling == TRUE)
		scaling_factor = user_scaling_factor;

	return;
	}


/* finds hosts that can be drawn in the canvas area */
void find_eligible_hosts(void) {
	int total_eligible_hosts = 0;
	host *temp_host;

	/* check all extended host information entries... */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* only include hosts that have 2-D coords supplied */
		if(temp_host->have_2d_coords == FALSE)
			temp_host->should_be_drawn = FALSE;

		/* make sure coords are all positive */
		else if(temp_host->x_2d < 0 || temp_host->y_2d < 0)
			temp_host->should_be_drawn = FALSE;

		/* make sure x coordinates fall within canvas bounds */
		else if(temp_host->x_2d < (canvas_x - DEFAULT_NODE_WIDTH) || temp_host->x_2d > (canvas_x + canvas_width))
			temp_host->should_be_drawn = FALSE;

		/* make sure y coordinates fall within canvas bounds */
		else if(temp_host->y_2d < (canvas_y - DEFAULT_NODE_HEIGHT) || temp_host->y_2d > (canvas_y + canvas_height))
			temp_host->should_be_drawn = FALSE;

		/* see if the user is authorized to view the host */
		else if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
			temp_host->should_be_drawn = FALSE;

		/* all checks passed, so we can draw the host! */
		else {
			temp_host->should_be_drawn = TRUE;
			total_eligible_hosts++;
			}
		}

	return;
	}



/******************************************************************/
/*********************** DRAWING FUNCTIONS ************************/
/******************************************************************/


/* loads background image from file */
void load_background_image(void) {
	char temp_buffer[MAX_INPUT_BUFFER];

	/* bail out if we shouldn't be drawing a background image */
	if(layout_method != LAYOUT_USER_SUPPLIED || statusmap_background_image == NULL)
		return;

	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s%s", physical_images_path, statusmap_background_image);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';

	/* read the background image into memory */
	background_image = load_image_from_file(temp_buffer);

	/* grab background image dimensions for calculating total image width later */
	if(background_image != NULL) {
		background_image_width = background_image->sx;
		background_image_height = background_image->sy;
		}

	/* if we are just creating the html, we don't need the image anymore */
	if(create_type == CREATE_HTML && background_image != NULL)
		gdImageDestroy(background_image);

	return;
	}


/* draws background image on drawing canvas */
void draw_background_image(void) {

	/* bail out if we shouldn't be drawing a background image */
	if(create_type == CREATE_HTML || layout_method != LAYOUT_USER_SUPPLIED || statusmap_background_image == NULL)
		return;

	/* bail out if we don't have an image */
	if(background_image == NULL)
		return;

	/* copy the background image to the canvas */
	gdImageCopy(map_image, background_image, 0, 0, canvas_x, canvas_y, canvas_width, canvas_height);

	/* free memory for background image, as we don't need it anymore */
	gdImageDestroy(background_image);

	return;
	}



/* draws background "extras" */
void draw_background_extras(void) {

	/* bail out if we shouldn't be here */
	if(create_type == CREATE_HTML)
		return;

	/* circular layout stuff... */
	if(layout_method == LAYOUT_CIRCULAR_MARKUP) {

		/* draw colored sections... */
		draw_circular_markup();
		}

	return;
	}


/* draws host links */
void draw_host_links(void) {
	host *this_host;
	host *main_host;
	host *parent_host;
	hostsmember *temp_hostsmember;
	int status_color = color_black;
	hoststatus *this_hoststatus;
	hoststatus *parent_hoststatus;
	int child_in_layer_list = FALSE;
	int parent_in_layer_list = FALSE;
	int dotted_line = FALSE;
	int x = 0;
	int y = 0;

	if(create_type == CREATE_HTML)
		return;

	if(use_links == FALSE)
		return;

	/* find the "main" host we're drawing */
	main_host = find_host(host_name);
	if(show_all_hosts == TRUE)
		main_host = NULL;

	/* check all extended host information entries... */
	for(this_host = host_list; this_host != NULL; this_host = this_host->next) {

		/* only draw link if user is authorized to view this host */
		if(is_authorized_for_host(this_host, &current_authdata) == FALSE)
			continue;

		/* this is a "root" host, so draw link to Nagios process icon if using auto-layout mode */
		if(this_host->parent_hosts == NULL && layout_method != LAYOUT_USER_SUPPLIED && draw_nagios_icon == TRUE) {

			x = this_host->x_2d + (DEFAULT_NODE_WIDTH / 2) - canvas_x;
			y = this_host->y_2d + (DEFAULT_NODE_WIDTH / 2) - canvas_y;

			draw_line(x, y, nagios_icon_x + (DEFAULT_NODE_WIDTH / 2) - canvas_x, nagios_icon_y + (DEFAULT_NODE_WIDTH / 2) - canvas_y, color_black);
			}

		/* this is a child of the main host we're drawing in auto-layout mode... */
		if(layout_method != LAYOUT_USER_SUPPLIED && draw_child_links == TRUE && number_of_immediate_child_hosts(this_host) > 0 && is_host_immediate_child_of_host(main_host, this_host) == TRUE) {
			/* determine color to use when drawing links to children  */
			this_hoststatus = find_hoststatus(this_host->name);
			if(this_hoststatus != NULL) {
				if(this_hoststatus->status == SD_HOST_DOWN || this_hoststatus->status == SD_HOST_UNREACHABLE)
					status_color = color_red;
				else
					status_color = color_black;
				}
			else
				status_color = color_black;

			x = this_host->x_2d + (DEFAULT_NODE_WIDTH / 2) - canvas_x;
			y = (this_host->y_2d + (DEFAULT_NODE_WIDTH) / 2) - canvas_y;

			draw_dashed_line(x, y, x, y + DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING, status_color);

			/* draw arrow tips */
			draw_line(x, y + DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING, x - 5, y + DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING - 5, color_black);
			draw_line(x, y + DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING, x + 5, y + DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING - 5, color_black);
			}

		/* this is a parent of the main host we're drawing in auto-layout mode... */
		if(layout_method != LAYOUT_USER_SUPPLIED && draw_parent_links == TRUE && is_host_immediate_child_of_host(this_host, main_host) == TRUE) {

			x = this_host->x_2d + (DEFAULT_NODE_WIDTH / 2) - canvas_x;
			y = this_host->y_2d + (DEFAULT_NODE_WIDTH / 2) - canvas_y;

			draw_dashed_line(x, y, x, y - DEFAULT_NODE_HEIGHT - DEFAULT_NODE_VSPACING, color_black);

			/* draw arrow tips */
			draw_line(x, y - DEFAULT_NODE_HEIGHT - DEFAULT_NODE_VSPACING, x - 5, y - DEFAULT_NODE_HEIGHT - DEFAULT_NODE_VSPACING + 5, color_black);
			draw_line(x, y - DEFAULT_NODE_HEIGHT - DEFAULT_NODE_VSPACING, x + 5, y - DEFAULT_NODE_HEIGHT - DEFAULT_NODE_VSPACING + 5, color_black);
			}

		/* draw links to all parent hosts */
		for(temp_hostsmember = this_host->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

			/* find the parent host config entry */
			parent_host = find_host(temp_hostsmember->host_name);
			if(parent_host == NULL)
				continue;

			/* don't draw the link if we don't have the coords */
			if(parent_host->have_2d_coords == FALSE || this_host->have_2d_coords == FALSE)
				continue;

			/* only draw link if user is authorized for this parent host */
			if(is_authorized_for_host(parent_host, &current_authdata) == FALSE)
				continue;

			/* are the hosts in the layer list? */
			child_in_layer_list = is_host_in_layer_list(this_host);
			parent_in_layer_list = is_host_in_layer_list(parent_host);

			/* use dotted or solid line? */
			/* either the child or parent should not be drawn, so use a dotted line */
			if((child_in_layer_list == TRUE && parent_in_layer_list == FALSE) || (child_in_layer_list == FALSE && parent_in_layer_list == TRUE))
				dotted_line = TRUE;
			/* both hosts should not be drawn, so use a dotted line */
			else if((child_in_layer_list == FALSE && parent_in_layer_list == FALSE && exclude_layers == FALSE) || (child_in_layer_list == TRUE && parent_in_layer_list == TRUE && exclude_layers == TRUE))
				dotted_line = TRUE;
			/* both hosts should be drawn, so use a solid line */
			else
				dotted_line = FALSE;

			/* determine color to use when drawing links to parent host */
			parent_hoststatus = find_hoststatus(parent_host->name);
			if(parent_hoststatus != NULL) {
				if(parent_hoststatus->status == SD_HOST_DOWN || parent_hoststatus->status == SD_HOST_UNREACHABLE)
					status_color = color_red;
				else
					status_color = color_black;
				}
			else
				status_color = color_black;

			/* draw the link */
			if(dotted_line == TRUE)
				draw_dotted_line((this_host->x_2d + (DEFAULT_NODE_WIDTH / 2)) - canvas_x, (this_host->y_2d + (DEFAULT_NODE_WIDTH) / 2) - canvas_y, (parent_host->x_2d + (DEFAULT_NODE_WIDTH / 2)) - canvas_x, (parent_host->y_2d + (DEFAULT_NODE_WIDTH / 2)) - canvas_y, status_color);
			else
				draw_line((this_host->x_2d + (DEFAULT_NODE_WIDTH / 2)) - canvas_x, (this_host->y_2d + (DEFAULT_NODE_WIDTH) / 2) - canvas_y, (parent_host->x_2d + (DEFAULT_NODE_WIDTH / 2)) - canvas_x, (parent_host->y_2d + (DEFAULT_NODE_WIDTH / 2)) - canvas_y, status_color);
			}

		}

	return;
	}



/* draws hosts */
void draw_hosts(void) {
	host *temp_host;
	int x1, x2;
	int y1;
	int has_image = FALSE;
	char image_input_file[MAX_INPUT_BUFFER];
	int current_radius = 0;
	int status_color = color_black;
	hoststatus *temp_hoststatus;
	int in_layer_list = FALSE;
	int average_host_services;
	int host_services;
	double host_services_ratio;
	int outer_radius;
	int inner_radius;
	int time_color = 0;
	time_t current_time;
	int translated_x;
	int translated_y;


	/* user didn't supply any coordinates for hosts, so display a warning */
	if(coordinates_were_specified == FALSE) {

		if(create_type == CREATE_IMAGE) {
			draw_text("You have not supplied any host drawing coordinates, so you cannot use this layout method.", (COORDS_WARNING_WIDTH / 2), 30, color_black);
			draw_text("Read the FAQs for more information on specifying drawing coordinates or select a different layout method.", (COORDS_WARNING_WIDTH / 2), 45, color_black);
			}

		return;
		}

	/* draw Nagios process icon if using auto-layout mode */
	if(layout_method != LAYOUT_USER_SUPPLIED && draw_nagios_icon == TRUE) {

		/* get coords of bounding box */
		x1 = nagios_icon_x - canvas_x;
		x2 = x1 + DEFAULT_NODE_WIDTH;
		y1 = nagios_icon_y - canvas_y;

		/* get the name of the image file to open for the logo */
		snprintf(image_input_file, sizeof(image_input_file) - 1, "%s%s", physical_logo_images_path, NAGIOS_GD2_ICON);
		image_input_file[sizeof(image_input_file) - 1] = '\x0';

		/* read in the image from file... */
		logo_image = load_image_from_file(image_input_file);

		/* copy the logo image to the canvas image... */
		if(logo_image != NULL) {
			gdImageCopy(map_image, logo_image, x1, y1, 0, 0, logo_image->sx, logo_image->sy);
			gdImageDestroy(logo_image);
			}

		/* if we don't have an image, draw a bounding box */
		else {
			draw_line(x1, y1, x1, y1 + DEFAULT_NODE_WIDTH, color_black);
			draw_line(x1, y1 + DEFAULT_NODE_WIDTH, x2, y1 + DEFAULT_NODE_WIDTH, color_black);
			draw_line(x2, y1 + DEFAULT_NODE_WIDTH, x2, y1, color_black);
			draw_line(x2, y1, x1, y1, color_black);
			}

		if(create_type == CREATE_IMAGE)
			draw_text("Nagios Process", x1 + (DEFAULT_NODE_WIDTH / 2), y1 + DEFAULT_NODE_HEIGHT, color_black);
		}

	/* calculate average services per host */
	average_host_services = 4;

	/* draw all hosts... */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* skip hosts that should not be drawn */
		if(temp_host->should_be_drawn == FALSE)
			continue;

		/* is this host in the layer inclusion/exclusion list? */
		in_layer_list = is_host_in_layer_list(temp_host);
		if((in_layer_list == TRUE && exclude_layers == TRUE) || (in_layer_list == FALSE && exclude_layers == FALSE))
			continue;

		/* get coords of host bounding box */
		x1 = temp_host->x_2d - canvas_x;
		x2 = x1 + DEFAULT_NODE_WIDTH;
		y1 = temp_host->y_2d - canvas_y;

		if(create_type == CREATE_IMAGE) {


			temp_hoststatus = find_hoststatus(temp_host->name);
			if(temp_hoststatus != NULL) {
				if(temp_hoststatus->status == SD_HOST_DOWN)
					status_color = color_red;
				else if(temp_hoststatus->status == SD_HOST_UNREACHABLE)
					status_color = color_red;
				else if(temp_hoststatus->status == SD_HOST_UP)
					status_color = color_green;
				else if(temp_hoststatus->status == HOST_PENDING)
					status_color = color_grey;
				}
			else
				status_color = color_black;


			/* use balloons instead of icons... */
			if(layout_method == LAYOUT_CIRCULAR_BALLOON) {

				/* get the number of services associated with the host */
				host_services = number_of_host_services(temp_host);

				if(average_host_services == 0)
					host_services_ratio = 0.0;
				else
					host_services_ratio = (double)((double)host_services / (double)average_host_services);

				/* calculate size of node */
				if(host_services_ratio >= 2.0)
					outer_radius = DEFAULT_NODE_WIDTH;
				else if(host_services_ratio >= 1.5)
					outer_radius = DEFAULT_NODE_WIDTH * 0.8;
				else if(host_services_ratio >= 1.0)
					outer_radius = DEFAULT_NODE_WIDTH * 0.6;
				else if(host_services_ratio >= 0.5)
					outer_radius = DEFAULT_NODE_WIDTH * 0.4;
				else
					outer_radius = DEFAULT_NODE_WIDTH * 0.2;

				/* calculate width of border */
				if(temp_hoststatus == NULL)
					inner_radius = outer_radius;
				else if((temp_hoststatus->status == SD_HOST_DOWN || temp_hoststatus->status == SD_HOST_UNREACHABLE) && temp_hoststatus->problem_has_been_acknowledged == FALSE)
					inner_radius = outer_radius - 3;
				else
					inner_radius = outer_radius;

				/* fill node with color based on how long its been in this state... */
				gdImageArc(map_image, x1 + (DEFAULT_NODE_WIDTH / 2), y1 + (DEFAULT_NODE_WIDTH / 2), outer_radius, outer_radius, 0, 360, color_blue);

				/* determine fill color */
				time(&current_time);
				if(temp_hoststatus == NULL)
					time_color = color_white;
				else if(current_time - temp_hoststatus->last_state_change <= 900)
					time_color = color_orange;
				else if(current_time - temp_hoststatus->last_state_change <= 3600)
					time_color = color_yellow;
				else
					time_color = color_white;

				/* fill node with appropriate time color */
				/* the fill function only works with coordinates that are in bounds of the actual image */
				translated_x = x1 + (DEFAULT_NODE_WIDTH / 2);
				translated_y = y1 + (DEFAULT_NODE_WIDTH / 2);
				if(translated_x > 0 && translated_y > 0 && translated_x < canvas_width && translated_y < canvas_height)
					gdImageFillToBorder(map_image, translated_x, translated_y, color_blue, time_color);

				/* border of node should reflect current state */
				for(current_radius = outer_radius; current_radius >= inner_radius; current_radius--)
					gdImageArc(map_image, x1 + (DEFAULT_NODE_WIDTH / 2), y1 + (DEFAULT_NODE_WIDTH / 2), current_radius, current_radius, 0, 360, status_color);

				/* draw circles around the selected host (if there is one) */
				if(!strcmp(host_name, temp_host->name) && use_highlights == TRUE) {
					for(current_radius = DEFAULT_NODE_WIDTH * 2; current_radius > 0; current_radius -= 10)
						gdImageArc(map_image, x1 + (DEFAULT_NODE_WIDTH / 2), y1 + (DEFAULT_NODE_WIDTH / 2), current_radius, current_radius, 0, 360, status_color);
					}
				}


			/* normal method is to use icons for hosts... */
			else {

				/* draw a target around root hosts (hosts with no parents) */
				if(temp_host != NULL && use_highlights == TRUE) {
					if(temp_host->parent_hosts == NULL) {
						gdImageArc(map_image, x1 + (DEFAULT_NODE_WIDTH / 2), y1 + (DEFAULT_NODE_WIDTH / 2), (DEFAULT_NODE_WIDTH * 2), (DEFAULT_NODE_WIDTH * 2), 0, 360, status_color);
						draw_line(x1 - (DEFAULT_NODE_WIDTH / 2), y1 + (DEFAULT_NODE_WIDTH / 2), x1 + (DEFAULT_NODE_WIDTH * 3 / 2), y1 + (DEFAULT_NODE_WIDTH / 2), status_color);
						draw_line(x1 + (DEFAULT_NODE_WIDTH / 2), y1 - (DEFAULT_NODE_WIDTH / 2), x1 + (DEFAULT_NODE_WIDTH / 2), y1 + (DEFAULT_NODE_WIDTH * 3 / 2), status_color);
						}
					}

				/* draw circles around the selected host (if there is one) */
				if(!strcmp(host_name, temp_host->name) && use_highlights == TRUE) {
					for(current_radius = DEFAULT_NODE_WIDTH * 2; current_radius > 0; current_radius -= 10)
						gdImageArc(map_image, x1 + (DEFAULT_NODE_WIDTH / 2), y1 + (DEFAULT_NODE_WIDTH / 2), current_radius, current_radius, 0, 360, status_color);
					}


				if(temp_host->statusmap_image != NULL)
					has_image = TRUE;
				else
					has_image = FALSE;

				/* load the logo associated with this host */
				if(has_image == TRUE) {

					/* get the name of the image file to open for the logo */
					snprintf(image_input_file, sizeof(image_input_file) - 1, "%s%s", physical_logo_images_path, temp_host->statusmap_image);
					image_input_file[sizeof(image_input_file) - 1] = '\x0';

					/* read in the logo image from file... */
					logo_image = load_image_from_file(image_input_file);

					/* copy the logo image to the canvas image... */
					if(logo_image != NULL) {
						gdImageCopy(map_image, logo_image, x1, y1, 0, 0, logo_image->sx, logo_image->sy);
						gdImageDestroy(logo_image);
						}
					else
						has_image = FALSE;
					}

				/* if the host doesn't have an image associated with it (or the user doesn't have rights to see this host), use the unknown image */
				if(has_image == FALSE) {

					if(unknown_logo_image != NULL)
						gdImageCopy(map_image, unknown_logo_image, x1, y1, 0, 0, unknown_logo_image->sx, unknown_logo_image->sy);

					else {

						/* last ditch effort - draw a host bounding box */
						draw_line(x1, y1, x1, y1 + DEFAULT_NODE_WIDTH, color_black);
						draw_line(x1, y1 + DEFAULT_NODE_WIDTH, x2, y1 + DEFAULT_NODE_WIDTH, color_black);
						draw_line(x2, y1 + DEFAULT_NODE_WIDTH, x2, y1, color_black);
						draw_line(x2, y1, x1, y1, color_black);
						}
					}
				}


			/* draw host name, status, etc. */
			draw_host_text(temp_host->name, x1 + (DEFAULT_NODE_WIDTH / 2), y1 + DEFAULT_NODE_HEIGHT);
			}

		/* we're creating HTML image map... */
		else {
			printf("<AREA shape='rect' ");

			/* coordinates */
			printf("coords='%d,%d,%d,%d' ", (int)(x1 * scaling_factor), (int)(y1 * scaling_factor), (int)((x1 + DEFAULT_NODE_WIDTH)*scaling_factor), (int)((y1 + DEFAULT_NODE_HEIGHT)*scaling_factor));

			/* URL */
			if(!strcmp(host_name, temp_host->name))
				printf("href='%s?host=%s' ", STATUS_CGI, url_encode(temp_host->name));
			else {
#ifdef LEGACY_GRAPHICAL_CGIS
				printf("href='%s?host=%s&layout=%d&max_width=%d&max_height=%d&proximity_width=%d&proximity_height=%d%s%s%s%s%s", STATUSMAP_CGI, url_encode(temp_host->name), layout_method, max_image_width, max_image_height, proximity_width, proximity_height, (display_header == TRUE) ? "" : "&noheader", (use_links == FALSE) ? "&nolinks" : "", (use_text == FALSE) ? "&notext" : "", (use_highlights == FALSE) ? "&nohighlights" : "", (display_popups == FALSE) ? "&nopopups" : "");
#else
				printf("href='%s?host=%s&layout=%d&max_width=%d&max_height=%d&proximity_width=%d&proximity_height=%d%s%s%s%s%s", LEGACY_STATUSMAP_CGI, url_encode(temp_host->name), layout_method, max_image_width, max_image_height, proximity_width, proximity_height, (display_header == TRUE) ? "" : "&noheader", (use_links == FALSE) ? "&nolinks" : "", (use_text == FALSE) ? "&notext" : "", (use_highlights == FALSE) ? "&nohighlights" : "", (display_popups == FALSE) ? "&nopopups" : "");
#endif
				if(user_supplied_scaling == TRUE)
					printf("&scaling_factor=%2.1f", user_scaling_factor);
				print_layer_url(TRUE);
				printf("' ");
				}

			/* popup text */
			if(display_popups == TRUE) {

				printf("onMouseOver='showPopup(\"");
				write_host_popup_text(find_host(temp_host->name));
				printf("\",event)' onMouseOut='hidePopup()'");
				}

			printf(">\n");
			}

		}

	return;
	}


/* draws text */
void draw_text(char *buffer, int x, int y, int text_color) {
	int string_width = 0;
	int string_height = 0;

	/* write the string to the generated image... */
	string_height = gdFontSmall->h;
	string_width = gdFontSmall->w * strlen(buffer);
	if(layout_method != LAYOUT_CIRCULAR_MARKUP)
		gdImageFilledRectangle(map_image, x - (string_width / 2) - 2, y - (2 * string_height), x + (string_width / 2) + 2, y - string_height, color_white);
	gdImageString(map_image, gdFontSmall, x - (string_width / 2), y - (2 * string_height), (unsigned char *)buffer, text_color);

	return;
	}


/* draws host text */
void draw_host_text(char *name, int x, int y) {
	hoststatus *temp_hoststatus;
	int status_color = color_black;
	char temp_buffer[MAX_INPUT_BUFFER];

	if(use_text == FALSE)
		return;

	strncpy(temp_buffer, name, sizeof(temp_buffer) - 1);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';

	/* write the host status string to the generated image... */
	draw_text(temp_buffer, x, y, color_black);

	/* find the status entry for this host */
	temp_hoststatus = find_hoststatus(name);

	/* get the status of the host (pending, up, down, or unreachable) */
	if(temp_hoststatus != NULL) {

		/* draw the status string */
		if(temp_hoststatus->status == SD_HOST_DOWN) {
			strncpy(temp_buffer, "Down", sizeof(temp_buffer));
			status_color = color_red;
			}
		else if(temp_hoststatus->status == SD_HOST_UNREACHABLE) {
			strncpy(temp_buffer, "Unreachable", sizeof(temp_buffer));
			status_color = color_red;
			}
		else if(temp_hoststatus->status == SD_HOST_UP) {
			strncpy(temp_buffer, "Up", sizeof(temp_buffer));
			status_color = color_green;
			}
		else if(temp_hoststatus->status == HOST_PENDING) {
			strncpy(temp_buffer, "Pending", sizeof(temp_buffer));
			status_color = color_grey;
			}
		else {
			strncpy(temp_buffer, "Unknown", sizeof(temp_buffer));
			status_color = color_orange;
			}

		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';

		/* write the host status string to the generated image... */
		draw_text(temp_buffer, x, y + gdFontSmall->h, status_color);
		}

	return;
	}


/* writes popup text for a specific host */
void write_host_popup_text(host *hst) {
	hoststatus *temp_status = NULL;
	hostsmember *temp_hostsmember = NULL;
	char *processed_string = NULL;
	int service_totals;
	char date_time[48];
	time_t current_time;
	time_t t;
	char state_duration[48];
	int days;
	int hours;
	int minutes;
	int seconds;

	if(hst == NULL) {
		printf("Host data not found");
		return;
		}

	/* find the status entry for this host */
	temp_status = find_hoststatus(hst->name);
	if(temp_status == NULL) {
		printf("Host status information not found");
		return;
		}

	/* grab macros */
	grab_host_macros_r(mac, hst);

	/* strip nasty stuff from plugin output */
	sanitize_plugin_output(temp_status->plugin_output);

	printf("<table border=0 cellpadding=0 cellspacing=5>");

	printf("<tr><td><img src=\\\"%s", url_logo_images_path);
	if(hst->icon_image == NULL)
		printf("%s", UNKNOWN_ICON_IMAGE);
	else {
		process_macros_r(mac, hst->icon_image, &processed_string, 0);
		printf("%s", processed_string);
		free(processed_string);
		}
	printf("\\\" border=0 width=40 height=40></td>");
	printf("<td class=\\\"popupText\\\"><i>%s</i></td></tr>", (hst->icon_image_alt == NULL) ? "" : html_encode(hst->icon_image_alt, TRUE));

	printf("<tr><td class=\\\"popupText\\\">Name:</td><td class=\\\"popupText\\\"><b>%s</b></td></tr>", escape_string(hst->name));
	printf("<tr><td class=\\\"popupText\\\">Alias:</td><td class=\\\"popupText\\\"><b>%s</b></td></tr>", escape_string(hst->alias));
	printf("<tr><td class=\\\"popupText\\\">Address:</td><td class=\\\"popupText\\\"><b>%s</b></td></tr>", html_encode(hst->address, TRUE));
	printf("<tr><td class=\\\"popupText\\\">State:</td><td class=\\\"popupText\\\"><b>");

	/* get the status of the host (pending, up, down, or unreachable) */
	if(temp_status->status == SD_HOST_DOWN) {
		printf("<font color=red>Down");
		if(temp_status->problem_has_been_acknowledged == TRUE)
			printf(" (Acknowledged)");
		printf("</font>");
		}

	else if(temp_status->status == SD_HOST_UNREACHABLE) {
		printf("<font color=red>Unreachable");
		if(temp_status->problem_has_been_acknowledged == TRUE)
			printf(" (Acknowledged)");
		printf("</font>");
		}

	else if(temp_status->status == SD_HOST_UP)
		printf("<font color=green>Up</font>");

	else if(temp_status->status == HOST_PENDING)
		printf("Pending");

	printf("</b></td></tr>");
	printf("<tr><td class=\\\"popupText\\\">Status Information:</td><td class=\\\"popupText\\\"><b>%s</b></td></tr>", (temp_status->plugin_output == NULL) ? "" : temp_status->plugin_output);

	current_time = time(NULL);
	if(temp_status->last_state_change == (time_t)0)
		t = current_time - program_start;
	else
		t = current_time - temp_status->last_state_change;
	get_time_breakdown((unsigned long)t, &days, &hours, &minutes, &seconds);
	snprintf(state_duration, sizeof(state_duration) - 1, "%2dd %2dh %2dm %2ds%s", days, hours, minutes, seconds, (temp_status->last_state_change == (time_t)0) ? "+" : "");
	state_duration[sizeof(state_duration) - 1] = '\x0';
	printf("<tr><td class=\\\"popupText\\\">State Duration:</td><td class=\\\"popupText\\\"><b>%s</b></td></tr>", state_duration);

	get_time_string(&temp_status->last_check, date_time, (int)sizeof(date_time), SHORT_DATE_TIME);
	printf("<tr><td class=\\\"popupText\\\">Last Status Check:</td><td class=\\\"popupText\\\"><b>%s</b></td></tr>", (temp_status->last_check == (time_t)0) ? "N/A" : date_time);
	get_time_string(&temp_status->last_state_change, date_time, (int)sizeof(date_time), SHORT_DATE_TIME);
	printf("<tr><td class=\\\"popupText\\\">Last State Change:</td><td class=\\\"popupText\\\"><b>%s</b></td></tr>", (temp_status->last_state_change == (time_t)0) ? "N/A" : date_time);

	printf("<tr><td class=\\\"popupText\\\">Parent Host(s):</td><td class=\\\"popupText\\\"><b>");
	if(hst->parent_hosts == NULL)
		printf("None (This is a root host)");
	else {
		for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next)
			printf("%s%s", (temp_hostsmember == hst->parent_hosts) ? "" : ", ", html_encode(temp_hostsmember->host_name, TRUE));
		}
	printf("</b></td></tr>");

	printf("<tr><td class=\\\"popupText\\\">Immediate Child Hosts:</td><td class=\\\"popupText\\\"><b>");
	printf("%d", number_of_immediate_child_hosts(hst));
	printf("</b></td></tr>");

	printf("</table>");

	printf("<br><b><u>Services:</u></b><br>");

	service_totals = get_servicestatus_count(hst->name, SERVICE_OK);
	if(service_totals > 0)
		printf("- <font color=green>%d ok</font><br>", service_totals);
	service_totals = get_servicestatus_count(hst->name, SERVICE_CRITICAL);
	if(service_totals > 0)
		printf("- <font color=red>%d critical</font><br>", service_totals);
	service_totals = get_servicestatus_count(hst->name, SERVICE_WARNING);
	if(service_totals > 0)
		printf("- <font color=orange>%d warning</font><br>", service_totals);
	service_totals = get_servicestatus_count(hst->name, SERVICE_UNKNOWN);
	if(service_totals > 0)
		printf("- <font color=orange>%d unknown</font><br>", service_totals);
	service_totals = get_servicestatus_count(hst->name, SERVICE_PENDING);
	if(service_totals > 0)
		printf("- %d pending<br>", service_totals);

	return;
	}



/* draws a solid line */
void draw_line(int x1, int y1, int x2, int y2, int color) {

	if(create_type == CREATE_HTML)
		return;

	gdImageLine(map_image, x1, y1, x2, y2, color);

	return;
	}


/* draws a dotted line */
void draw_dotted_line(int x1, int y1, int x2, int y2, int color) {
	int styleDotted[12];

	styleDotted[0] = color;
	styleDotted[1] = gdTransparent;
	styleDotted[2] = gdTransparent;
	styleDotted[3] = gdTransparent;
	styleDotted[4] = gdTransparent;
	styleDotted[5] = gdTransparent;
	styleDotted[6] = color;
	styleDotted[7] = gdTransparent;
	styleDotted[8] = gdTransparent;
	styleDotted[9] = gdTransparent;
	styleDotted[10] = gdTransparent;
	styleDotted[11] = gdTransparent;

	/* sets current style to a dashed line */
	gdImageSetStyle(map_image, styleDotted, 12);

	/* draws a line (dotted) */
	gdImageLine(map_image, x1, y1, x2, y2, gdStyled);

	return;
	}

/* draws a dashed line */
void draw_dashed_line(int x1, int y1, int x2, int y2, int color) {
	int styleDashed[12];

	styleDashed[0] = color;
	styleDashed[1] = color;
	styleDashed[2] = color;
	styleDashed[3] = color;
	styleDashed[4] = gdTransparent;
	styleDashed[5] = gdTransparent;
	styleDashed[6] = color;
	styleDashed[7] = color;
	styleDashed[8] = color;
	styleDashed[9] = color;
	styleDashed[10] = gdTransparent;
	styleDashed[11] = gdTransparent;

	/* sets current style to a dashed line */
	gdImageSetStyle(map_image, styleDashed, 12);

	/* draws a line (dashed) */
	gdImageLine(map_image, x1, y1, x2, y2, gdStyled);

	return;
	}



/******************************************************************/
/*********************** GRAPHICS FUNCTIONS ***********************/
/******************************************************************/

/* initialize graphics */
int initialize_graphics(void) {
	char image_input_file[MAX_INPUT_BUFFER];

	if(create_type == CREATE_HTML)
		return ERROR;

	/* allocate buffer for storing image */
#ifndef HAVE_GDIMAGECREATETRUECOLOR
	map_image = gdImageCreate(canvas_width, canvas_height);
#else
	map_image = gdImageCreateTrueColor(canvas_width, canvas_height);
#endif
	if(map_image == NULL)
		return ERROR;

	/* allocate colors used for drawing */
	color_white = gdImageColorAllocate(map_image, 255, 255, 255);
	color_black = gdImageColorAllocate(map_image, 0, 0, 0);
	color_grey = gdImageColorAllocate(map_image, 128, 128, 128);
	color_lightgrey = gdImageColorAllocate(map_image, 210, 210, 210);
	color_red = gdImageColorAllocate(map_image, 255, 0, 0);
	color_lightred = gdImageColorAllocate(map_image, 215, 175, 175);
	color_green = gdImageColorAllocate(map_image, 0, 175, 0);
	color_lightgreen = gdImageColorAllocate(map_image, 210, 255, 215);
	color_blue = gdImageColorAllocate(map_image, 0, 0, 255);
	color_yellow = gdImageColorAllocate(map_image, 255, 255, 0);
	color_orange = gdImageColorAllocate(map_image, 255, 100, 25);
	color_transparency_index = gdImageColorAllocate(map_image, color_transparency_index_r, color_transparency_index_g, color_transparency_index_b);

	/* set transparency index */
#ifndef HAVE_GDIMAGECREATETRUECOLOR
	gdImageColorTransparent(map_image, color_white);
#else
	gdImageColorTransparent(map_image, color_transparency_index);

	/* set background */
	gdImageFill(map_image, 0, 0, color_transparency_index);
#endif

	/* make sure the graphic is interlaced */
	gdImageInterlace(map_image, 1);

	/* get the path where we will be reading logo images from (GD2 format)... */
	snprintf(physical_logo_images_path, sizeof(physical_logo_images_path) - 1, "%slogos/", physical_images_path);
	physical_logo_images_path[sizeof(physical_logo_images_path) - 1] = '\x0';

	/* load the unknown icon to use for hosts that don't have pretty images associated with them... */
	snprintf(image_input_file, sizeof(image_input_file) - 1, "%s%s", physical_logo_images_path, UNKNOWN_GD2_ICON);
	image_input_file[sizeof(image_input_file) - 1] = '\x0';
	unknown_logo_image = load_image_from_file(image_input_file);

	return OK;
	}



/* loads a graphic image (GD2, JPG or PNG) from file into memory */
gdImagePtr load_image_from_file(char *filename) {
	FILE *fp;
	gdImagePtr im = NULL;
	char *ext;

	/* make sure we were passed a file name */
	if(filename == NULL)
		return NULL;

	/* find the file extension */
	if((ext = rindex(filename, '.')) == NULL)
		return NULL;

	/* open the file for reading (binary mode) */
	fp = fopen(filename, "rb");
	if(fp == NULL)
		return NULL;

	/* attempt to read files in various formats */
	if(!strcasecmp(ext, ".png"))
		im = gdImageCreateFromPng(fp);
	else if(!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg"))
		im = gdImageCreateFromJpeg(fp);
	else if(!strcasecmp(ext, ".xbm"))
		im = gdImageCreateFromXbm(fp);
	else if(!strcasecmp(ext, ".gd2"))
		im = gdImageCreateFromGd2(fp);
	else if(!strcasecmp(ext, ".gd"))
		im = gdImageCreateFromGd(fp);

	/* fall back to GD2 image format */
	else
		im = gdImageCreateFromGd2(fp);

	/* close the file */
	fclose(fp);

	return im;
	}



/* draw graphics */
void write_graphics(void) {
	FILE *image_output_file = NULL;

	if(create_type == CREATE_HTML)
		return;

	/* use STDOUT for writing the image data... */
	image_output_file = stdout;

	/* write the image out in PNG format */
	gdImagePng(map_image, image_output_file);

	/* or we could write the image out in JPG format... */
	/*gdImageJpeg(map_image,image_output_file,99);*/

	return;
	}


/* cleanup graphics resources */
void cleanup_graphics(void) {

	if(create_type == CREATE_HTML)
		return;

	/* free memory allocated to image */
	gdImageDestroy(map_image);

	return;
	}




/******************************************************************/
/************************* MISC FUNCTIONS *************************/
/******************************************************************/


/* write JavaScript code an layer for popup window */
void write_popup_code(void) {
	char *border_color = "#000000";
	char *background_color = "#ffffcc";
	int border = 1;
	int padding = 3;
	int x_offset = 3;
	int y_offset = 3;

	printf("<SCRIPT LANGUAGE='JavaScript'>\n");
	printf("<!--\n");
	printf("// JavaScript popup based on code originally found at http://www.helpmaster.com/htmlhelp/javascript/popjbpopup.htm\n");
	printf("function showPopup(text, eventObj){\n");
	printf("if(!document.all && document.getElementById)\n");
	printf("{ document.all=document.getElementsByTagName(\"*\")}\n");
	printf("ieLayer = 'document.all[\\'popup\\']';\n");
	printf("nnLayer = 'document.layers[\\'popup\\']';\n");
	printf("moLayer = 'document.getElementById(\\'popup\\')';\n");

	printf("if(!(document.all||document.layers||document.documentElement)) return;\n");

	printf("if(document.all) { document.popup=eval(ieLayer); }\n");
	printf("else {\n");
	printf("  if (document.documentElement) document.popup=eval(moLayer);\n");
	printf("  else document.popup=eval(nnLayer);\n");
	printf("}\n");

	printf("var table = \"\";\n");

	printf("if (document.all||document.documentElement){\n");
	printf("table += \"<table bgcolor='%s' border=%d cellpadding=%d cellspacing=0>\";\n", background_color, border, padding);
	printf("table += \"<tr><td>\";\n");
	printf("table += \"<table cellspacing=0 cellpadding=%d>\";\n", padding);
	printf("table += \"<tr><td bgcolor='%s' class='popupText'>\" + text + \"</td></tr>\";\n", background_color);
	printf("table += \"</table></td></tr></table>\"\n");
	printf("document.popup.innerHTML = table;\n");
	printf("document.popup.style.left = document.body.scrollLeft + %d;\n", x_offset);
	printf("document.popup.style.top = document.body.scrollTop + %d;\n", y_offset);
	/*
	printf("document.popup.style.left = (document.all ? eventObj.x : eventObj.layerX) + %d;\n",x_offset);
	printf("document.popup.style.top  = (document.all ? eventObj.y : eventObj.layerY) + %d;\n",y_offset);
	*/

	printf("document.popup.style.visibility = \"visible\";\n");
	printf("} \n");


	printf("else{\n");
	printf("table += \"<table cellpadding=%d border=%d cellspacing=0 bordercolor='%s'>\";\n", padding, border, border_color);
	printf("table += \"<tr><td bgcolor='%s' class='popupText'>\" + text + \"</td></tr></table>\";\n", background_color);
	printf("document.popup.document.open();\n");
	printf("document.popup.document.write(table);\n");
	printf("document.popup.document.close();\n");

	/* set x coordinate */
	printf("document.popup.left = eventObj.layerX + %d;\n", x_offset);

	/* make sure we don't overlap the right side of the screen */
	printf("if(document.popup.left + document.popup.document.width + %d > window.innerWidth) document.popup.left = window.innerWidth - document.popup.document.width - %d - 16;\n", x_offset, x_offset);

	/* set y coordinate */
	printf("document.popup.top  = eventObj.layerY + %d;\n", y_offset);

	/* make sure we don't overlap the bottom edge of the screen */
	printf("if(document.popup.top + document.popup.document.height + %d > window.innerHeight) document.popup.top = window.innerHeight - document.popup.document.height - %d - 16;\n", y_offset, y_offset);

	/* make the popup visible */
	printf("document.popup.visibility = \"visible\";\n");
	printf("}\n");
	printf("}\n");

	printf("function hidePopup(){ \n");
	printf("if (!(document.all || document.layers || document.documentElement)) return;\n");
	printf("if (document.popup == null){ }\n");
	printf("else if (document.all||document.documentElement) document.popup.style.visibility = \"hidden\";\n");
	printf("else document.popup.visibility = \"hidden\";\n");
	printf("document.popup = null;\n");
	printf("}\n");
	printf("//-->\n");

	printf("</SCRIPT>\n");

	return;
	}



/* adds a layer to the list in memory */
int add_layer(char *group_name) {
	struct layer *new_layer;

	if(group_name == NULL)
		return ERROR;

	/* allocate memory for a new layer */
	new_layer = (struct layer *)malloc(sizeof(struct layer));
	if(new_layer == NULL)
		return ERROR;

	new_layer->layer_name = (char *)malloc(strlen(group_name) + 1);
	if(new_layer->layer_name == NULL) {
		free(new_layer);
		return ERROR;
		}

	strcpy(new_layer->layer_name, group_name);

	/* add new layer to head of layer list */
	new_layer->next = layer_list;
	layer_list = new_layer;

	return OK;
	}



/* frees memory allocated to the layer list */
void free_layer_list(void) {
	struct layer *this_layer, *next_layer;

	return;

	for(this_layer = layer_list; layer_list != NULL; this_layer = next_layer) {
		next_layer = this_layer->next;
		free(this_layer->layer_name);
		free(this_layer);
		}

	return;
	}


/* checks to see if a host is in the layer list */
int is_host_in_layer_list(host *hst) {
	hostgroup *temp_hostgroup;
	struct layer *temp_layer;

	if(hst == NULL)
		return FALSE;

	/* check each layer... */
	for(temp_layer = layer_list; temp_layer != NULL; temp_layer = temp_layer->next) {

		/* find the hostgroup */
		temp_hostgroup = find_hostgroup(temp_layer->layer_name);
		if(temp_hostgroup == NULL)
			continue;

		/* is the requested host a member of the hostgroup/layer? */
		if(is_host_member_of_hostgroup(temp_hostgroup, hst) == TRUE)
			return TRUE;
		}

	return FALSE;
	}


/* print layer url info */
void print_layer_url(int get_method) {
	struct layer *temp_layer;

	for(temp_layer = layer_list; temp_layer != NULL; temp_layer = temp_layer->next) {
		if(get_method == TRUE)
			printf("&layer=%s", escape_string(temp_layer->layer_name));
		else
			printf("<input type='hidden' name='layer' value='%s'>\n", escape_string(temp_layer->layer_name));
		}

	if(get_method == TRUE)
		printf("&layermode=%s", (exclude_layers == TRUE) ? "exclude" : "include");
	else
		printf("<input type='hidden' name='layermode' value='%s'>\n", (exclude_layers == TRUE) ? "exclude" : "include");

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



/* calculates number of services associated with a particular service */
int number_of_host_services(host *hst) {
	service *temp_service;
	int total_services = 0;

	if(hst == NULL)
		return 0;

	/* check all the services */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {
		if(!strcmp(temp_service->host_name, hst->name))
			total_services++;
		}

	return total_services;
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

			temp_host->x_2d = current_drawing_x + (((DEFAULT_NODE_WIDTH * this_drawing_width) + (DEFAULT_NODE_HSPACING * (this_drawing_width - 1))) / 2);
			temp_host->y_2d = y + DEFAULT_NODE_HEIGHT + DEFAULT_NODE_VSPACING;
			temp_host->have_2d_coords = TRUE;
			temp_host->should_be_drawn = TRUE;

			current_drawing_x += (this_drawing_width * DEFAULT_NODE_WIDTH) + ((this_drawing_width - 1) * DEFAULT_NODE_HSPACING) + DEFAULT_NODE_HSPACING;

			/* recurse into child host ... */
			calculate_balanced_tree_coords(temp_host, temp_host->x_2d, temp_host->y_2d);
			}

		}

	return;
	}


/* calculate coords of all hosts in circular layout method */
void calculate_circular_coords(void) {
	int min_x = 0;
	int min_y = 0;
	int have_min_x = FALSE;
	int have_min_y = FALSE;
	host *temp_host;

	/* calculate all host coords, starting with first layer */
	calculate_circular_layer_coords(NULL, 0.0, 360.0, 1, CIRCULAR_DRAWING_RADIUS);

	/* adjust all calculated coords so none are negative in x or y axis... */

	/* calculate min x, y coords */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(have_min_x == FALSE || temp_host->x_2d < min_x) {
			have_min_x = TRUE;
			min_x = temp_host->x_2d;
			}
		if(have_min_y == FALSE || temp_host->y_2d < min_y) {
			have_min_y = TRUE;
			min_y = temp_host->y_2d;
			}
		}

	/* offset all drawing coords by the min x,y coords we found */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(min_x < 0)
			temp_host->x_2d -= min_x;
		if(min_y < 0)
			temp_host->y_2d -= min_y;
		}

	if(min_x < 0)
		nagios_icon_x -= min_x;
	if(min_y < 0)
		nagios_icon_y -= min_y;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		temp_host->x_2d += (DEFAULT_NODE_WIDTH / 2);
		temp_host->y_2d += (DEFAULT_NODE_HEIGHT / 2);
		}
	nagios_icon_x += (DEFAULT_NODE_WIDTH / 2);
	nagios_icon_y += (DEFAULT_NODE_HEIGHT / 2);

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

			temp_host->x_2d = (int)x_coord;
			temp_host->y_2d = (int)y_coord;
			temp_host->have_2d_coords = TRUE;
			temp_host->should_be_drawn = TRUE;

			/* recurse into child host ... */
			calculate_circular_layer_coords(temp_host, current_drawing_angle + ((available_angle - clipped_available_angle) / 2), clipped_available_angle, layer + 1, radius + CIRCULAR_DRAWING_RADIUS);

			/* increment current drawing angle */
			current_drawing_angle += available_angle;
			}
		}

	return;
	}



/* draws background "extras" for all hosts in circular markup layout */
void draw_circular_markup(void) {

	/* calculate all host sections, starting with first layer */
	draw_circular_layer_markup(NULL, 0.0, 360.0, 1, CIRCULAR_DRAWING_RADIUS);

	return;
	}


/* draws background "extras" for all hosts in a particular "layer" in circular markup layout */
void draw_circular_layer_markup(host *parent, double start_angle, double useable_angle, int layer, int radius) {
	int parent_drawing_width = 0;
	int this_drawing_width = 0;
	int immediate_children = 0;
	double current_drawing_angle = 0.0;
	double available_angle = 0.0;
	double clipped_available_angle = 0.0;
	double x_coord[4] = {0.0, 0.0, 0.0, 0.0};
	double y_coord[4] = {0.0, 0.0, 0.0, 0.0};
	hoststatus *temp_hoststatus;
	host *temp_host;
	int x_offset = 0;
	int y_offset = 0;
	int center_x = 0;
	int center_y = 0;
	int bgcolor = 0;
	double arc_start_angle = 0.0;
	double arc_end_angle = 0.0;
	int translated_x = 0;
	int translated_y = 0;

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

			/* calculate drawing coords of "leftmost" divider using good ol' geometry... */
			x_coord[0] = -(sin(-current_drawing_angle * (M_PI / 180.0)) * (radius - (CIRCULAR_DRAWING_RADIUS / 2)));
			y_coord[0] = -(sin((90 + current_drawing_angle) * (M_PI / 180.0)) * (radius - (CIRCULAR_DRAWING_RADIUS / 2)));
			x_coord[1] = -(sin(-current_drawing_angle * (M_PI / 180.0)) * (radius + (CIRCULAR_DRAWING_RADIUS / 2)));
			y_coord[1] = -(sin((90 + current_drawing_angle) * (M_PI / 180.0)) * (radius + (CIRCULAR_DRAWING_RADIUS / 2)));

			/* calculate drawing coords of "rightmost" divider using good ol' geometry... */
			x_coord[2] = -(sin((-(current_drawing_angle + available_angle)) * (M_PI / 180.0)) * (radius - (CIRCULAR_DRAWING_RADIUS / 2)));
			y_coord[2] = -(sin((90 + current_drawing_angle + available_angle) * (M_PI / 180.0)) * (radius - (CIRCULAR_DRAWING_RADIUS / 2)));
			x_coord[3] = -(sin((-(current_drawing_angle + available_angle)) * (M_PI / 180.0)) * (radius + (CIRCULAR_DRAWING_RADIUS / 2)));
			y_coord[3] = -(sin((90 + current_drawing_angle + available_angle) * (M_PI / 180.0)) * (radius + (CIRCULAR_DRAWING_RADIUS / 2)));


			x_offset = nagios_icon_x + (DEFAULT_NODE_WIDTH / 2) - canvas_x;
			y_offset = nagios_icon_y + (DEFAULT_NODE_HEIGHT / 2) - canvas_y;

			/* draw "slice" dividers */
			if(immediate_children > 1 || layer > 1) {

				/* draw "leftmost" divider */
				gdImageLine(map_image, (int)x_coord[0] + x_offset, (int)y_coord[0] + y_offset, (int)x_coord[1] + x_offset, (int)y_coord[1] + y_offset, color_lightgrey);

				/* draw "rightmost" divider */
				gdImageLine(map_image, (int)x_coord[2] + x_offset, (int)y_coord[2] + y_offset, (int)x_coord[3] + x_offset, (int)y_coord[3] + y_offset, color_lightgrey);
				}

			/* determine arc drawing angles */
			arc_start_angle = current_drawing_angle - 90.0;
			while(arc_start_angle < 0.0)
				arc_start_angle += 360.0;
			arc_end_angle = arc_start_angle + available_angle;

			/* draw inner arc */
			gdImageArc(map_image, x_offset, y_offset, (radius - (CIRCULAR_DRAWING_RADIUS / 2)) * 2, (radius - (CIRCULAR_DRAWING_RADIUS / 2)) * 2, floor(arc_start_angle), ceil(arc_end_angle), color_lightgrey);

			/* draw outer arc */
			gdImageArc(map_image, x_offset, y_offset, (radius + (CIRCULAR_DRAWING_RADIUS / 2)) * 2, (radius + (CIRCULAR_DRAWING_RADIUS / 2)) * 2, floor(arc_start_angle), ceil(arc_end_angle), color_lightgrey);


			/* determine center of "slice" and fill with appropriate color */
			center_x = -(sin(-(current_drawing_angle + (available_angle / 2.0)) * (M_PI / 180.0)) * (radius));
			center_y = -(sin((90 + current_drawing_angle + (available_angle / 2.0)) * (M_PI / 180.0)) * (radius));
			translated_x = center_x + x_offset;
			translated_y = center_y + y_offset;

			/* determine background color */
			temp_hoststatus = find_hoststatus(temp_host->name);
			if(temp_hoststatus == NULL)
				bgcolor = color_lightgrey;
			else if(temp_hoststatus->status == SD_HOST_DOWN || temp_hoststatus->status == SD_HOST_UNREACHABLE)
				bgcolor = color_lightred;
			else
				bgcolor = color_lightgreen;

			/* fill slice with background color */
			/* the fill function only works with coordinates that are in bounds of the actual image */
			if(translated_x > 0 && translated_y > 0 && translated_x < canvas_width && translated_y < canvas_height)
				gdImageFillToBorder(map_image, translated_x, translated_y, color_lightgrey, bgcolor);

			/* recurse into child host ... */
			draw_circular_layer_markup(temp_host, current_drawing_angle + ((available_angle - clipped_available_angle) / 2), clipped_available_angle, layer + 1, radius + CIRCULAR_DRAWING_RADIUS);

			/* increment current drawing angle */
			current_drawing_angle += available_angle;
			}
		}

	return;
	}
