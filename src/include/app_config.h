#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include "app_error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define DEFAULT_SERVER_LIST_FILE "speedtest_server_list.json"
#define DEFAULT_TIMEOUT_MS 15000
#define DEFAULT_VERBOSE_MODE true

#define MIN_TIMEOUT 1000 	// 1 sec
#define MAX_TIMEOUT 60000 	// 60 sec
typedef struct {
	bool run_download_test 	:1;
	bool run_upload_test 	:1;
	bool detect_location 	:1;
	bool is_verbose 		:1;
	bool is_debug	 		:1;
	bool show_help			:1;
	bool custom_server_list :1; // if -f was passed, when true warning is printed
	int timeout_ms;
    char * servers_file;
    char ** server_urls;
    size_t server_url_count;
    char ** selected_locations;
    size_t selected_locations_count;
} Config;
app_error_t init_config(Config * config);
void destroy_config(Config * config);
void display_config(const Config * config);
app_error_t validate_config(const Config * config);
#endif