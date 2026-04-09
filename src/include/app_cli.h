#ifndef APP_CLI_H
#define APP_CLI_H
#include "app_config.h"
#include "app_error.h"

app_error_t parse_args(Config * config, const int argc, char * argv[]);
void show_help(const char * prog_name);
#endif