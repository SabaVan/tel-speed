#ifndef NETWORK_TEST_H
#define NETWORK_TEST_H

#include <curl/curl.h>
#include "app_config.h"
#include "app_error.h"

#define IP_API "http://ip-api.com/json/"
typedef struct {
    time_t start_time;
    long start_nano;
    unsigned int limit_ms;
    curl_off_t last_bytes;
    double last_time;
    double current_speed_mbps;
    double final_avg_mbps;
    int is_upload;
    bool should_stop;
} speed_stats_t;

struct server_info {
    int id;
    char * country;
    //char * city;
    //char * provider;
    char * host;
};

app_error_t network_init(void);
void network_cleanup(void);

app_error_t network_find_best_server(const struct server_info *srvs, size_t srv_count, const struct server_info **best_srv);
app_error_t network_get_location(char * buffer, size_t buf_size);
app_error_t network_measure_speed(const char *host, int is_upload, unsigned int timeout_ms, speed_stats_t *stats);
void display_speed_stats(const speed_stats_t * stats);
#endif