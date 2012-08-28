/************************************************************************
 *
 * CONVERTCFG.C - Config File Convertor
 *
 * Copyright (c) 2001-2005 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 08-12-2005
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
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *my_strsep(char **, const char *);

int main(int argc, char **argv) {
	FILE *fp;
	char *temp_ptr;
	char *temp_ptr2;
	char input[8096];
	int notify_recovery;
	int notify_warning;
	int notify_critical;
	int notify_down;
	int notify_unreachable;
	int option;
	int have_template = 0;
	int x = 0, y = 0;
	char *host_name;
	char *service_description;
	char *host_name2;
	char *service_description2;

	if(argc != 3) {
		printf("Nagios Config File Converter\n");
		printf("Written by Ethan Galstad (egalstad@nagios.org)\n");
		printf("Last Modified: 08-12-2005\n");
		printf("\n");
		printf("Usage: %s <config file> <object type>\n", argv[0]);
		printf("\n");
		printf("Valid object types include:\n");
		printf("\n");
		printf("\ttimeperiods\n");
		printf("\tcommands\n");
		printf("\tcontacts\n");
		printf("\tcontactgroups\n");
		printf("\thosts\n");
		printf("\thostgroups\n");
		printf("\thostgroupescalationss\n");
		printf("\tservices\n");
		printf("\tservicedependencies\n");
		printf("\tserviceescalations\n");
		printf("\n");
		printf("\thostextinfo\n");
		printf("\tserviceextinfo\n");
		printf("\n");
		printf("Notes:\n");
		printf("\n");
		printf("This utility is designed to aide you in converting your old 'host'\n");
		printf("config file(s) to the new template-based config file style.  It is\n");
		printf("also capable of converting extended host and service information\n");
		printf("definitions in your old CGI config file.\n");
		printf("\n");
		printf("Supply the name of your old 'host' config file (or your old CGI config\n");
		printf("file if you're converting extended host/service definitions) on the\n");
		printf("command line, along with the type of object you would like to produce\n");
		printf("a new config file for.  Your old config file is not overwritten - new\n");
		printf("configuration data is printed to standard output, so you can redirect it\n");
		printf("wherever you like.\n");
		printf("\n");
		printf("Please note that you can only specify one type of object at a time\n");
		printf("on the command line.\n");
		printf("\n");
		printf("IMPORTANT: This utility will generate Nagios 1.x compliant config files.\n");
		printf("However, the config files are not totally compatible with Nagios 2.x, so\n");
		printf("you will have to do some manual tweaking.\n");
		printf("\n");
		return -1;
		}

	fp = fopen(argv[1], "r");
	if(fp == NULL) {
		printf("Error: Could not open file '%s' for reading.\n", argv[1]);
		return -1;
		}

	for(fgets(input, sizeof(input) - 1, fp); !feof(fp); fgets(input, sizeof(input) - 1, fp)) {

		/* skip blank lines and comments */
		if(input[0] == '#' || input[0] == '\x0' || input[0] == '\n' || input[0] == '\r')
			continue;

		/* timeperiods */
		if(strstr(input, "timeperiod[") && !strcmp(argv[2], "timeperiods")) {

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			temp_ptr = my_strsep(&temp_ptr2, "]");

			printf("# '%s' timeperiod definition\n", temp_ptr);
			printf("define timeperiod{\n");
			/*printf("\tname\t\t%s\n",temp_ptr);*/
			printf("\ttimeperiod_name\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\talias\t\t%s\n", temp_ptr + 1);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tsunday\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tmonday\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\ttuesday\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\twednesday\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tthursday\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tfriday\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tsaturday\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}

		/* commands */
		if(strstr(input, "command[") && !strcmp(argv[2], "commands")) {

			temp_ptr = strtok(input, "[");
			temp_ptr = strtok(NULL, "]");

			printf("# '%s' command definition\n", temp_ptr);
			printf("define command{\n");
			/*printf("\tname\t\t%s\n",temp_ptr);*/
			printf("\tcommand_name\t%s\n", temp_ptr);

			temp_ptr = strtok(NULL, "\n");

			printf("\tcommand_line\t%s\n", temp_ptr + 1);

			printf("\t}\n\n\n");
			}

		/* contacts */
		if(strstr(input, "contact[") && !strcmp(argv[2], "contacts")) {

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			temp_ptr = my_strsep(&temp_ptr2, "]");

			printf("# '%s' contact definition\n", temp_ptr);
			printf("define contact{\n");
			/*printf("\tname\t\t\t\t%s\n",temp_ptr);*/
			printf("\tcontact_name\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\talias\t\t\t\t%s\n", temp_ptr + 1);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tservice_notification_period\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\thost_notification_period\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_recovery = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_critical = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_warning = atoi(temp_ptr);

			option = 0;
			printf("\tservice_notification_options\t");
			if(notify_recovery == 1 || notify_critical == 1 || notify_warning == 1) {
				if(notify_warning == 1) {
					printf("w,u");
					option = 1;
					}
				if(notify_critical == 1) {
					if(option == 1)
						printf(",");
					printf("c");
					option = 1;
					}
				if(notify_recovery == 1) {
					if(option == 1)
						printf(",");
					printf("r");
					}
				}
			else
				printf("n");
			printf("\n");

			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_recovery = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_down = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_unreachable = atoi(temp_ptr);

			option = 0;
			printf("\thost_notification_options\t");
			if(notify_recovery == 1 || notify_down == 1 || notify_unreachable == 1) {
				if(notify_down == 1) {
					printf("d");
					option = 1;
					}
				if(notify_unreachable == 1) {
					if(option == 1)
						printf(",");
					printf("u");
					option = 1;
					}
				if(notify_recovery == 1) {
					if(option == 1)
						printf(",");
					printf("r");
					}
				}
			else
				printf("n");
			printf("\n");

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tservice_notification_commands\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\thost_notification_commands\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\temail\t\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tpager\t\t\t\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}


		/* contactgroups */
		if(strstr(input, "contactgroup[") && !strcmp(argv[2], "contactgroups")) {

			temp_ptr = strtok(input, "[");
			temp_ptr = strtok(NULL, "]");

			printf("# '%s' contact group definition\n", temp_ptr);
			printf("define contactgroup{\n");
			/*printf("\tname\t\t\t%s\n",temp_ptr);*/
			printf("\tcontactgroup_name\t%s\n", temp_ptr);

			temp_ptr = strtok(NULL, ";");
			printf("\talias\t\t\t%s\n", temp_ptr + 1);

			temp_ptr = strtok(NULL, "\n");

			printf("\tmembers\t\t\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}

		/* hosts */
		if(strstr(input, "host[") && !strcmp(argv[2], "hosts")) {

			if(have_template == 0) {

				printf("# Generic host definition template\n");
				printf("define host{\n");
				printf("\tname\t\t\t\tgeneric-host\t; The name of this host template - referenced in other host definitions, used for template recursion/resolution\n");
				printf("\tactive_checks_enabled\t\t1\t; Active host checks are enabled\n");
				printf("\tpassive_checks_enabled\t\t1\t; Passive host checks are enabled/accepted\n");
				printf("\tnotifications_enabled\t\t1\t; Host notifications are enabled\n");
				printf("\tevent_handler_enabled\t\t1\t; Host event handler is enabled\n");
				printf("\tflap_detection_enabled\t\t1\t; Flap detection is enabled\n");
				printf("\tprocess_perf_data\t\t1\t; Process performance data\n");
				printf("\tretain_status_information\t1\t; Retain status information across program restarts\n");
				printf("\tretain_nonstatus_information\t1\t; Retain non-status information across program restarts\n");
				printf("\n");
				printf("\tregister\t\t\t0\t; DONT REGISTER THIS DEFINITION - ITS NOT A REAL HOST, JUST A TEMPLATE!\n");
				printf("\t}\n\n");

				have_template = 1;
				}

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			temp_ptr = my_strsep(&temp_ptr2, "]");

			printf("# '%s' host definition\n", temp_ptr);
			printf("define host{\n");
			printf("\tuse\t\t\tgeneric-host\t\t; Name of host template to use\n\n");
			printf("\thost_name\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\talias\t\t\t%s\n", temp_ptr + 1);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\taddress\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tparents\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tcheck_command\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tmax_check_attempts\t%d\n", atoi(temp_ptr));
			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tnotification_interval\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tnotification_period\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_recovery = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_down = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_unreachable = atoi(temp_ptr);

			option = 0;
			printf("\tnotification_options\t");
			if(notify_recovery == 1 || notify_down == 1 || notify_unreachable == 1) {
				if(notify_down == 1) {
					printf("d");
					option = 1;
					}
				if(notify_unreachable == 1) {
					if(option == 1)
						printf(",");
					printf("u");
					option = 1;
					}
				if(notify_recovery == 1) {
					if(option == 1)
						printf(",");
					printf("r");
					}
				}
			else
				printf("n");
			printf("\n");

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tevent_handler\t\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}

		/* hostgroups */
		if(strstr(input, "hostgroup[") && !strcmp(argv[2], "hostgroups")) {

			temp_ptr = strtok(input, "[");
			temp_ptr = strtok(NULL, "]");

			printf("# '%s' host group definition\n", temp_ptr);
			printf("define hostgroup{\n");
			/*printf("\tname\t\t%s\n",temp_ptr);*/
			printf("\thostgroup_name\t%s\n", temp_ptr);

			temp_ptr = strtok(NULL, ";");
			printf("\talias\t\t%s\n", temp_ptr + 1);

			temp_ptr = strtok(NULL, ";");
			/*printf("\tcontact_groups\t%s\n",temp_ptr);*/

			temp_ptr = strtok(NULL, "\n");

			printf("\tmembers\t\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}


		/* services */
		if(strstr(input, "service[") && !strcmp(argv[2], "services")) {

			if(have_template == 0) {

				printf("# Generic service definition template\n");
				printf("define service{\n");
				printf("\tname\t\t\t\tgeneric-service\t; The 'name' of this service template, referenced in other service definitions\n");
				printf("\tactive_checks_enabled\t\t1\t; Active service checks are enabled\n");
				printf("\tpassive_checks_enabled\t\t1\t; Passive service checks are enabled/accepted\n");
				printf("\tparallelize_check\t\t1\t; Active service checks should be parallelized (disabling this can lead to major performance problems)\n");
				printf("\tobsess_over_service\t\t1\t; We should obsess over this service (if necessary)\n");
				printf("\tcheck_freshness\t\t\t0\t; Default is to NOT check service 'freshness'\n");
				printf("\tnotifications_enabled\t\t1\t; Service notifications are enabled\n");
				printf("\tevent_handler_enabled\t\t1\t; Service event handler is enabled\n");
				printf("\tflap_detection_enabled\t\t1\t; Flap detection is enabled\n");
				printf("\tprocess_perf_data\t\t1\t; Process performance data\n");
				printf("\tretain_status_information\t1\t; Retain status information across program restarts\n");
				printf("\tretain_nonstatus_information\t1\t; Retain non-status information across program restarts\n");
				printf("\n");
				printf("\tregister\t\t\t0\t; DONT REGISTER THIS DEFINITION - ITS NOT A REAL SERVICE, JUST A TEMPLATE!\n");
				printf("\t}\n\n");

				have_template = 1;
				}

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			temp_ptr = my_strsep(&temp_ptr2, "]");

			printf("# Service definition\n");
			printf("define service{\n");
			printf("\tuse\t\t\t\tgeneric-service\t\t; Name of service template to use\n\n");
			printf("\thost_name\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tservice_description\t\t%s\n", temp_ptr + 1);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tis_volatile\t\t\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tcheck_period\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tmax_check_attempts\t\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tnormal_check_interval\t\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tretry_check_interval\t\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tcontact_groups\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tnotification_interval\t\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tnotification_period\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_recovery = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_critical = atoi(temp_ptr);
			temp_ptr = my_strsep(&temp_ptr2, ";");
			notify_warning = atoi(temp_ptr);

			option = 0;
			printf("\tnotification_options\t\t");
			if(notify_recovery == 1 || notify_critical == 1 || notify_warning == 1) {
				if(notify_warning == 1) {
					printf("w,u");
					option = 1;
					}
				if(notify_critical == 1) {
					if(option == 1)
						printf(",");
					printf("c");
					option = 1;
					}
				if(notify_recovery == 1) {
					if(option == 1)
						printf(",");
					printf("r");
					}
				}
			else
				printf("n");
			printf("\n");

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tevent_handler\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tcheck_command\t\t\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}

		/* hostgroup escalations */
		if(strstr(input, "hostgroupescalation[") && !strcmp(argv[2], "hostgroupescalations")) {

			x++;

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			temp_ptr = my_strsep(&temp_ptr2, "]");

			printf("# Hostgroup '%s' escalation definition\n", temp_ptr);
			printf("define hostgroupescalation{\n");

			printf("\thostgroup_name\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, "-");
			printf("\tfirst_notification\t\t%d\n", atoi(temp_ptr + 1));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tlast_notification\t\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tcontact_groups\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			printf("\tnotification_interval\t\t%d\n", atoi(temp_ptr));

			printf("\t}\n\n\n");
			}

		/* service escalations */
		if(strstr(input, "serviceescalation[") && !strcmp(argv[2], "serviceescalations")) {

			x++;

			printf("# Serviceescalation definition\n");
			printf("define serviceescalation{\n");

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			host_name = my_strsep(&temp_ptr2, ";");
			service_description = my_strsep(&temp_ptr2, "]");

			printf("\thost_name\t\t%s\n", host_name);
			printf("\tservice_description\t\t%s\n", service_description);

			temp_ptr = my_strsep(&temp_ptr2, "-");
			printf("\tfirst_notification\t\t%d\n", atoi(temp_ptr + 1));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tlast_notification\t\t%d\n", atoi(temp_ptr));

			temp_ptr = my_strsep(&temp_ptr2, ";");
			printf("\tcontact_groups\t\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			printf("\tnotification_interval\t\t%d\n", atoi(temp_ptr));

			printf("\t}\n\n\n");
			}

		/* service dependencies */
		if(strstr(input, "servicedependency[") && !strcmp(argv[2], "servicedependencies")) {

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			host_name = my_strsep(&temp_ptr2, ";");
			service_description = my_strsep(&temp_ptr2, "]");
			host_name2 = my_strsep(&temp_ptr2, ";") + 1;
			service_description2 = my_strsep(&temp_ptr2, ";");

			temp_ptr = my_strsep(&temp_ptr2, ";");

			x++;

			printf("# Servicedependency definition\n");
			printf("define servicedependency{\n");

			printf("\thost_name\t\t\t%s\n", host_name2);
			printf("\tservice_description\t\t%s\n", service_description2);
			printf("\tdependent_host_name\t\t%s\n", host_name);
			printf("\tdependent_service_description\t%s\n", service_description);

			printf("\texecution_failure_criteria\t");
			for(y = 0; temp_ptr[y] != '\x0'; y++)
				printf("%s%c", (y > 0) ? "," : "", temp_ptr[y]);
			if(y == 0)
				printf("n");
			printf("\t; These are the criteria for which check execution will be suppressed\n");

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");

			printf("\tnotification_failure_criteria\t");
			for(y = 0; temp_ptr[y] != '\x0'; y++)
				printf("%s%c", (y > 0) ? "," : "", temp_ptr[y]);
			if(y == 0)
				printf("n");
			printf("\t; These are the criteria for which notifications will be suppressed\n");
			printf("\t}\n\n\n");
			}


		/* extended host info */
		if(strstr(input, "hostextinfo[") && !strcmp(argv[2], "hostextinfo")) {

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			temp_ptr = my_strsep(&temp_ptr2, "]");

			printf("# '%s' hostextinfo definition\n", temp_ptr);
			printf("define hostextinfo{\n");
			printf("\thost_name\t\t%s\t\t; The name of the host this data is associated with\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");

			if(temp_ptr + 1 != NULL && strcmp(temp_ptr + 1, ""))
				printf("\tnotes_url\t\t\t%s\n", temp_ptr + 1);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\ticon_image\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tvrml_image\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\tstatusmap_image\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\ticon_image_alt\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\t2d_coords\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\t3d_coords\t\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}


		/* extended service info */
		if(strstr(input, "serviceextinfo[") && !strcmp(argv[2], "serviceextinfo")) {

			temp_ptr2 = &input[0];
			temp_ptr = my_strsep(&temp_ptr2, "[");
			temp_ptr = my_strsep(&temp_ptr2, ";");

			printf("# serviceextinfo definition\n", temp_ptr);
			printf("define serviceextinfo{\n");
			printf("\thost_name\t\t%s\t\t; The name of the service this data is associated with\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, "]");
			printf("\tservice_description\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";");

			if(temp_ptr + 1 != NULL && strcmp(temp_ptr + 1, ""))
				printf("\tnotes_url\t\t%s\n", temp_ptr + 1);

			temp_ptr = my_strsep(&temp_ptr2, ";");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\ticon_image\t\t%s\n", temp_ptr);

			temp_ptr = my_strsep(&temp_ptr2, ";\r\n");
			if(temp_ptr != NULL && strcmp(temp_ptr, ""))
				printf("\ticon_image_alt\t\t%s\n", temp_ptr);

			printf("\t}\n\n\n");
			}

		}


	fclose(fp);

	return 0;
	}



/* fixes compiler problems under Solaris, since strsep() isn't included */
/* this code is taken from the glibc source */
char *my_strsep(char **stringp, const char *delim) {
	char *begin, *end;

	begin = *stringp;
	if(begin == NULL)
		return NULL;

	/* A frequent case is when the delimiter string contains only one
	   character.  Here we don't need to call the expensive `strpbrk'
	   function and instead work using `strchr'.  */
	if(delim[0] == '\0' || delim[1] == '\0') {
		char ch = delim[0];

		if(ch == '\0')
			end = NULL;
		else {
			if(*begin == ch)
				end = begin;
			else
				end = strchr(begin + 1, ch);
			}
		}

	else
		/* Find the end of the token.  */
		end = strpbrk(begin, delim);

	if(end) {

		/* Terminate the token and set *STRINGP past NUL character.  */
		*end++ = '\0';
		*stringp = end;
		}
	else
		/* No more delimiters; this is the last token.  */
		*stringp = NULL;

	return begin;
	}


