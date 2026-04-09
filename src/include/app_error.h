#ifndef APP_ERROR_H
#define APP_ERROR_H
#include <stdarg.h>
#include <stdbool.h>
typedef enum {
	APP_SUCCESS = 0,
	APP_ERR_ARGUMENTS,
	APP_ERR_FILE_OPEN,
	APP_ERR_JSON_PARSE,
	APP_ERR_CURL_INIT,
	APP_ERR_LOCATION_NOT_FOUND,
	APP_ERR_NETWORK_TIMEOUT,
	APP_ERR_NETWORK_DISCONNECTED,
	APP_ERR_NO_SERVER,
	APP_ERR_MALLOC
} app_error_t;

typedef enum {
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
	LOG_STATUS,
	LOG_RESULT,
	LOG_DEBUG
} LogLevel;

void app_log_set_verbose(bool enabled);
void app_log_set_debug(bool enabled);
const char * app_error_to_string(app_error_t err);
const char * app_get_full_error(app_error_t err);
// with LOG_ERROR app_log function saves error message in internal buffer, else prints immediately
void app_log(LogLevel level, const char *fmt, ...);
void clear_app_error(void);

#endif
