#include "app_cli.h"
#include "app_config.h"
#include "app_error.h"
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef struct {
	struct option opt;
	const char * descr;
	const char * arg_name;
} CLIOption;

static const CLIOption my_options[] = {
	{{"auto",			no_argument,	0,	'a'}, "Run full test (DL/UL + auto-locate)", NULL},
	{{"download",		no_argument,	0,	'd'}, "Run download test", NULL},
	{{"upload",			no_argument,	0,	'u'}, "Run upload test", NULL},
	{{"timeout",	required_argument,	0,	't'}, "Set connection timeout", "<ms>"},
	{{"file",		required_argument,	0,	'f'}, "Path to JSON server list", "<path>"},
	{{"server",		required_argument,	0,	's'}, "Specific server URLs", "<urls>"},
	{{"location",	required_argument,	0,	'L'}, "Filter by location", "<locs>"},
	{{"auto-locate",	no_argument,	0,	'g'}, "Auto-detect location", NULL},
	{{"verbose",		no_argument,	0,	'v'}, "Enable verbose output", NULL},
	{{"no-verbose",		no_argument, 	0,	1000}, "Disable verbose output", NULL},
	{{"debug",			no_argument, 	0,	1001}, "Enable debug output", NULL},
	{{"help",			no_argument,	0,	'h'}, "Show this help message", NULL},
	{{0, 0, 0, 0}, NULL, NULL}
};

static app_error_t add_to_list(char *** list, size_t * count, const char * arg);
app_error_t parse_args(Config * config, int argc, char *argv[])
{
	// extract 'struct option' from my_options for getopt
	struct option long_opts[sizeof(my_options) / sizeof(my_options[0])];
	for(int i = 0; i < (int)(sizeof(my_options) / sizeof(my_options[0])); i++) {
		long_opts[i] = my_options[i].opt;
	}

	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			config->show_help = true;
			return APP_SUCCESS;
		}
	}
	int opt;
	int option_index = 0;
	opterr = 0;
	while ((opt = getopt_long(argc, argv, ":adut:vs:f:gL:", long_opts, &option_index)) != -1) {
		switch (opt) {
			case 'a':
				config->run_download_test = true;
				config->run_upload_test = true;
				config->detect_location = true;
				break;
			case 'd':
				config->run_download_test = true;
				break;
			case 'u':
				config->run_upload_test = true;
				break;
			case 't':
				config->timeout_ms = atoi(optarg);
				if(config->timeout_ms == 0) {
					app_log(LOG_ERROR, "Invalid timeout value: %s", optarg);
					return APP_ERR_ARGUMENTS;
				}
				break;
			case 'v':
				config->is_verbose = true;
				break;
			case 'f':
				if(config->servers_file != NULL)
					free(config->servers_file); // free default file
				config->servers_file = strdup(optarg);
				config->custom_server_list = true;
				if(config->servers_file == NULL) {
					app_log(LOG_ERROR, "%s: %s", __func__, strerror(errno));
					return APP_ERR_MALLOC;
				}
				break;
			case 's':
				if(add_to_list(&config->server_urls, &config->server_url_count, optarg) != APP_SUCCESS)
					return APP_ERR_MALLOC;
				break;
			case 1000: // no-verbose
				config->is_verbose = false;
				break;
			case 1001: // debug
				config->is_debug = true;
				break;
			case 'g':
				config->detect_location = true;
				break;
			case 'L':
				if(add_to_list(&config->selected_locations, &config->selected_locations_count, optarg) != APP_SUCCESS)
					return APP_ERR_MALLOC;
				break;
			case ':':
				app_log(LOG_ERROR, "Option -%c requires an argument", optopt);
				return APP_ERR_ARGUMENTS;

			case '?': 
				app_log(LOG_ERROR, "Unknown option or invalid usage");
				return APP_ERR_ARGUMENTS;
			default:
				break;
		}
	}
	return APP_SUCCESS;
}
static app_error_t add_to_list(char *** list, size_t * count, const char * arg)
{
	char * input_copy = strdup(arg);
	if(!input_copy) return APP_ERR_MALLOC;

	char * token = strtok(input_copy, ",");
	while (token != NULL) {
		char **temp = realloc(*list, (*count + 1) * sizeof(char *));
		if(!temp) {
			free(input_copy);
			return APP_ERR_MALLOC;
		}
		*list = temp;
		(*list)[*count] = strdup(token);
		(*count)++;
		token = strtok(NULL, ",");
	}
	free(input_copy);
	return APP_SUCCESS;
}
void show_help(const char * prog_name)
{
	printf("Usage: %s [options]\n\nOptions:\n", prog_name);
	
	for(int i = 0; my_options[i].opt.name != NULL; i++) {
		char opt_str[64];
		
		if(my_options[i].opt.has_arg == required_argument) {
			sprintf(opt_str, "-%c, --%s %s", 
					my_options[i].opt.val, 
					my_options[i].opt.name, 
					my_options[i].arg_name ? my_options[i].arg_name : "ARG");
		} else {
			if(my_options[i].opt.val < 128 && my_options[i].opt.val > 32) {
				sprintf(opt_str, "-%c, --%s", my_options[i].opt.val, my_options[i].opt.name);
			} else {
				sprintf(opt_str, "    --%s", my_options[i].opt.name);
			}
		}
		
		printf("  %-30s %s\n", opt_str, my_options[i].descr);
	}
	return;
}