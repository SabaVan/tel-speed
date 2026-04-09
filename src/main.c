#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "app_error.h"
#include "app_config.h"
#include "app_cli.h"
#include "network_test.h"
#include "cJSON.h"

char * read_file_to_string(const char * filename);
app_error_t filter_countries(cJSON *root, char **countries, size_t countries_count, struct server_info ** result, size_t * found_servers);
int main(int argc, char * argv[])
{
	Config config;
	app_error_t stat;
	speed_stats_t speed_stat;
	struct server_info * srvs = NULL;
	const struct server_info * best_srv;
	size_t srv_count = 0;
		
	if((stat = init_config(&config)) != APP_SUCCESS) {
		fprintf(stderr, "%s\n", app_get_full_error(stat));
		exit(stat);
	}
	
	if((stat = parse_args(&config, argc, argv)) != APP_SUCCESS) {
		fprintf(stderr, "%s\n", app_get_full_error(stat));
		goto cleanup;
	}
	if(config.show_help) {
		show_help(argv[0]);
		goto cleanup;
	}
	if((stat = validate_config(&config)) != APP_SUCCESS) {
		fprintf(stderr, "%s\n", app_get_full_error(stat));
		goto cleanup;
	}
	app_log_set_verbose(config.is_verbose);
	app_log_set_debug(config.is_debug);
	// enable result buffering, results will be printed at the end with app_log_flush_results()
	// true if -a was passed, or -du -g
	app_log_set_result(!(config.run_download_test && config.run_upload_test && config.detect_location));

	if(config.is_verbose)
		display_config(&config);

	if((stat = network_init()) != APP_SUCCESS) {
		fprintf(stderr, "%s\n", app_get_full_error(stat));
		goto cleanup;
	}

	if(config.detect_location) {
		char location[100];
		if((stat = network_get_location(location, sizeof(location))) == APP_SUCCESS) {
			config.selected_locations = malloc(sizeof(char *));
			config.selected_locations[0] = strndup(location, 100);
			if(config.selected_locations[0] == NULL) {
				clear_app_error();
				fprintf(stderr, "%s\n", app_get_full_error(APP_ERR_MALLOC));
			}
			config.selected_locations_count = 1;
			app_log(LOG_RESULT, "Detected location: %s", location);
		} else {
			fprintf(stderr, "%s\n", app_get_full_error(stat));
			goto cleanup;
		}
	}
	if((config.run_upload_test || config.run_download_test)) {
		if(config.server_url_count == 0) {
			char * json_string;
			if((json_string = read_file_to_string(config.servers_file)) == NULL) {
				fprintf(stderr, "%s\n", app_get_full_error(-1));
				goto cleanup;
			}

			cJSON * root = cJSON_Parse(json_string);
			free(json_string);
			json_string = NULL;
			if(root == NULL) {
    			const char * error_ptr = cJSON_GetErrorPtr();
				if(error_ptr != NULL) {
					app_log(LOG_ERROR, "JSON parse error before: %s", error_ptr);
				} else {
					app_log(LOG_ERROR, "JSON parse error: Unknown error");
				}
				stat = APP_ERR_JSON_PARSE;
				fprintf(stderr, "%s\n", app_get_full_error(stat));
				goto cleanup;
			}
			app_log(LOG_STATUS, "Filtering by countries..");
			stat = filter_countries(root, config.selected_locations, config.selected_locations_count, &srvs, &srv_count);

			if(stat != APP_SUCCESS) {
				if(stat == APP_ERR_MALLOC && srv_count > 0) {
					app_log(LOG_WARNING, "Memory low: proceeding with %zu servers", srv_count);
					stat = APP_SUCCESS;
				} else {
					fprintf(stderr, "%s\n", app_get_full_error(stat));
					cJSON_Delete(root);
					goto cleanup;
				}
			}

			cJSON_Delete(root);
			root = NULL;
			app_log(LOG_INFO, "Found %zu servers for given location", srv_count);
			if((stat = network_find_best_server(srvs, srv_count, &best_srv))) {
				fprintf(stderr, "%s\n", app_get_full_error(stat));
				goto cleanup;
			}
		} else {
			// need server_info structs for network_find_best_server function
			srvs = malloc(sizeof(struct server_info) * config.server_url_count);
			for(size_t i = 0; i < config.server_url_count; i++) {
				srvs[i].host = strdup(config.server_urls[i]);
				srvs[i].country = NULL;
				srvs[i].id = i;
			}
			srv_count = config.server_url_count;

			if(config.server_url_count == 1)
				best_srv = &srvs[0];
			else {
				if((stat = network_find_best_server(srvs, config.server_url_count, &best_srv))) {
					fprintf(stderr, "%s\n", app_get_full_error(stat));
					goto cleanup;
				}
			}
		}

		if(config.server_url_count != 1)
			app_log(LOG_INFO, "Detected smallest latency for (%s: %s)", 
				best_srv->country == NULL ? "" : best_srv->country, best_srv->host);

		app_log(LOG_RESULT, "Testing speed for %s", best_srv->host);
		
		// download test
		if(config.run_download_test) {
			if((stat = network_measure_speed(best_srv->host, false, config.timeout_ms, &speed_stat))) {
				fprintf(stderr, "%s", app_get_full_error(stat));
				goto cleanup;
			}
			app_log(LOG_RESULT, "Downloading speed %.2f Mbps (%.2fs)", speed_stat.final_avg_mbps, speed_stat.last_time);
		}

		// upload test
		if(config.run_upload_test) {
			if((stat = network_measure_speed(best_srv->host, true, config.timeout_ms, &speed_stat))) {
				fprintf(stderr, "%s", app_get_full_error(stat));
				goto cleanup;
			}
			app_log(LOG_RESULT, "Uploading speed %.2f Mbps (%.2fs)", speed_stat.final_avg_mbps, speed_stat.last_time);
		}
	}

	// prints final results if auto mode was selected
	app_log_flush_results(); 
cleanup:
	app_log_cleanup();
	for(size_t i = 0; i < srv_count; i++) {
		free(srvs[i].country);
		free(srvs[i].host);
	}
	free(srvs);
	destroy_config(&config);
	network_cleanup();
	exit(stat);
}
char * read_file_to_string(const char * filename)
{
	FILE * file = fopen(filename, "rb");
	if(file == NULL) {
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	size_t length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char * buffer = (char *) malloc(length + 1);
	if(buffer == NULL) {
		app_log(LOG_ERROR, "Failed to allocate %lu bytes of memory", length+1);
		fclose(file);
		return NULL; 
	}
	size_t read_size = fread(buffer, 1, length, file);
	if(read_size != length) {
		free(buffer);
		fclose(file);
		return NULL;
	}


	buffer[length] = '\0';

	fclose(file);
	return buffer;
} 

app_error_t filter_countries(cJSON *root, char **countries, size_t countries_count, struct server_info ** result, size_t * found_servers)
{
	if(!root || !countries || !result || !found_servers) {
		app_log(LOG_ERROR, "%s", __func__);
		return APP_ERR_ARGUMENTS;
	}
	size_t servers_size = 50;
	*result = malloc(servers_size * sizeof(struct server_info));
	if (*result == NULL) return APP_ERR_MALLOC;
	cJSON * server_node = NULL;
	size_t found_count = 0;

	cJSON_ArrayForEach(server_node, root) {
		cJSON *country_node = cJSON_GetObjectItemCaseSensitive(server_node, "country");
		
		if(!cJSON_IsString(country_node) || (country_node->valuestring == NULL)) {
			continue;
		}

		for(size_t i = 0; i < countries_count; i++) {
			if(strncasecmp(country_node->valuestring, countries[i], 50) == 0) {
				                
				cJSON * url_node = cJSON_GetObjectItemCaseSensitive(server_node, "host");
				cJSON * id_node = cJSON_GetObjectItemCaseSensitive(server_node, "id");

				if(cJSON_IsString(url_node)) {

					if(found_count >= servers_size) {
						size_t new_size = servers_size + 50;
						void * tmp = realloc(*result, new_size * sizeof(struct server_info));
						if(tmp == NULL) {
							app_log(LOG_WARNING, "Realloc failed: %s", strerror(errno));
							*found_servers = found_count; 
							return APP_ERR_MALLOC;
						}
						servers_size = new_size;
						*result = (struct server_info *)tmp;
					}
					struct server_info * current_server = &((*result)[found_count]);

					current_server->country = strndup(country_node->valuestring, 128);
					current_server->host = strndup(url_node->valuestring, 512);
					
					if(cJSON_IsNumber(id_node)) {
						current_server->id = id_node->valueint;
					}
					// if any strdup has failed
					if(!current_server->country || !current_server->host) {
						free(current_server->country);
						current_server->country = NULL;
						free(current_server->host);
						current_server->host = NULL;
					} else {
						found_count++;
					}
				}
				break;
			}
		}
	}
	*found_servers = found_count;
	return APP_SUCCESS;
}