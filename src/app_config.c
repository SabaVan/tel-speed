#include "app_config.h"
#include "app_error.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
app_error_t init_config(Config * config)
{
	if(config == NULL) return APP_ERR_ARGUMENTS;
	memset(config, 0, sizeof(Config));
	config->is_verbose = DEFAULT_VERBOSE_MODE;
	config->servers_file = strdup(DEFAULT_SERVER_LIST_FILE);
	if(config->servers_file == NULL) {
		app_log(LOG_ERROR, "%s: %s", __func__, strerror(errno));
		return APP_ERR_MALLOC;
	}
	config->timeout_ms = DEFAULT_TIMEOUT_MS; // default timeout in 15 sec
	return APP_SUCCESS;
}
void destroy_config(Config * config)
{
	if(config == NULL) return;
	if(config->servers_file != NULL)
		free(config->servers_file);
	if(config->server_urls != NULL) {
		for(size_t i = 0; i < config->server_url_count; i++)
			free(config->server_urls[i]);
		free(config->server_urls);
	}
	if(config->selected_locations != NULL) {
		for(size_t i = 0; i < config->selected_locations_count; i++)
			free(config->selected_locations[i]);
		free(config->selected_locations);
	}
	memset(config, 0, sizeof(Config));
	return;
}
void display_config(const Config *config)
{

	app_log(LOG_INFO, "Config -----------------------------------------------------------");

	const char *file_name = config->servers_file ? config->servers_file : "-";
	int file_col_width = (int)strlen(file_name);
	if (file_col_width < 8) file_col_width = 8;

	app_log(LOG_INFO, "%-9s %-10s %-10s %-10s %-*s", 
			"(D/U/L)", "TIMEOUT", "URL_CNT", "LOC_CNT", file_col_width, "SRV_FILE");
	app_log(LOG_INFO, "[%c/%c/%c]   %-10u %-10zu %-10u %-*s",
		config->run_download_test	? 'Y' : 'N',
		config->run_upload_test		? 'Y' : 'N',
		config->detect_location  	? 'Y' : 'N',
		config->timeout_ms,
		config->server_url_count,
		(unsigned int)config->selected_locations_count,
		file_col_width, file_name);
	app_log(LOG_INFO, "------------------------------------------------------------------");
	return;
}
app_error_t validate_config(const Config * config)
{
	if(config->timeout_ms < MIN_TIMEOUT || config->timeout_ms > MAX_TIMEOUT) {
		app_log(LOG_ERROR, "timeout value must be between %d and %d", MIN_TIMEOUT, MAX_TIMEOUT);
		return APP_ERR_ARGUMENTS;
	}
	if(config->server_url_count > 0 && config->custom_server_list) {
		app_log(LOG_ERROR, "Conflict: -f (%s) and -s (custom URLs) provided. Choose one source", 
			config->servers_file);
		return APP_ERR_ARGUMENTS;			
	} 
	if(config->selected_locations_count > 0 && config->detect_location) {
		app_log(LOG_ERROR, "Conflict: -g (--auto-detect) and -L (manual locations) provided. Choose one method");
		return APP_ERR_ARGUMENTS;
	}
	if(config->detect_location && config->server_url_count) {
		app_log(LOG_ERROR, "Conflict: -g (--auto-detect) and -s (manual URLs) provided");
		return APP_ERR_ARGUMENTS;
	}
	if(config->run_download_test || config->run_upload_test) {
		if(config->server_url_count == 0) {
			if(!config->detect_location && config->selected_locations_count == 0) {
				app_log(LOG_ERROR, "No server selection method. Provide server URLs (-s), "
							"specific locations (-L), or enable auto-detection (-g).");
				return APP_ERR_ARGUMENTS;
			}
		}
	} else if(!config->detect_location 	&& !config->show_help) {
		app_log(LOG_ERROR, "No test selected. Please use --download (-d), --upload (-u) or --auto (-a).");
		return APP_ERR_ARGUMENTS;
	}
	if(config->custom_server_list) {
		if(config->servers_file != NULL) {
			if(access(config->servers_file, F_OK) == -1) {
				app_log(LOG_ERROR, "%s", strerror(errno));
				return APP_ERR_FILE_OPEN;
			}
		} else {
			app_log(LOG_ERROR, "%s: File with server list is not set", __func__);
			return APP_ERR_ARGUMENTS;
		}
	}

	return APP_SUCCESS;
}