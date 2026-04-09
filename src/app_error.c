#include "app_error.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
static bool is_verbose = false;
static bool is_debug = false;
static char final_error_msg[1024] = {0}; 
static char dynamic_detail[256];

void app_log_set_verbose(bool enabled) {
    is_verbose = enabled;
}
void app_log_set_debug(bool enabled) {
    is_debug = enabled;
}

const char * app_error_to_string(app_error_t err)
{
	switch (err) 
	{
		case APP_SUCCESS:
			return "Operation completed successfully";

		case APP_ERR_ARGUMENTS:
			return "Invalid arguments";

		case APP_ERR_FILE_OPEN:
			return "Could not open file";

		case APP_ERR_JSON_PARSE:
			return "JSON parsing failed";

		case APP_ERR_CURL_INIT:
			return "Failed to initialize network library (libcurl)";

		case APP_ERR_LOCATION_NOT_FOUND:
			return "Could not determine your location (IP-API error)";

		case APP_ERR_NETWORK_TIMEOUT:
			return "Network request timed out";

		case APP_ERR_NETWORK_DISCONNECTED:
			return "Network connection lost or host unreachable";

		case APP_ERR_NO_SERVER:
			return "No suitable speedtest servers found";

		case APP_ERR_MALLOC:
			return "Memory allocation failed (Out of memory)";

		default:
			return "An unexpected internal error occurred";
	}
}
static void vset_app_error(const char *fmt, va_list args) 
{
	if(!fmt) {
		dynamic_detail[0] = '\0';
		return;
	}
	vsnprintf(dynamic_detail, sizeof(dynamic_detail), fmt, args);
    return;
}

void app_log(LogLevel level, const char *fmt, ...) 
{
	if(!fmt) return;

	va_list args;
	va_start(args, fmt);
	
	if(level == LOG_RESULT) {
		printf("\r\033[K");
		vprintf(fmt, args);
		fprintf(stdout, "\n");
		fflush(stdout);
		va_end(args);
		return;
	}

	if(!is_verbose && (level == LOG_INFO || level == LOG_STATUS)) 
		return;
	if(level == LOG_ERROR) {
		vset_app_error(fmt, args); 
	} else if(level == LOG_DEBUG && is_debug) {
		printf("[DEBUG] ");
		vprintf(fmt, args);
		printf("\n");
		fflush(stdout);
		return;
	} 
	else if (level == LOG_STATUS) {
		printf("\r[WORKING] ");
		vprintf(fmt, args);
		printf("\033[K\r"); 
		fflush(stdout);
	} else {
		FILE * stream = (level == LOG_INFO) ? stdout : stderr;
		fprintf(stream, "\r\033[K");
		fprintf(stream, level == LOG_WARNING ? "[WARNING] " : "[INFO] ");
		vfprintf(stream, fmt, args);
		fprintf(stream, "\033[K\r\n");
		fflush(stream);
	}

	va_end(args);
	return;
}
const char * app_get_full_error(app_error_t err)
{
	const char * base_msg = app_error_to_string(err);
	
	if (dynamic_detail[0] == '\0') 
	{
		return base_msg;
	}

	snprintf(final_error_msg, sizeof(final_error_msg), 
			"[ERROR] %s: (%s)", base_msg, dynamic_detail);

	dynamic_detail[0] = '\0';
	return final_error_msg;
}
void clear_app_error(void)
{
	final_error_msg[0] = '\0';
	dynamic_detail[0] = '\0';
	return;
}