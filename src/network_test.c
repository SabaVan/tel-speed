#include "network_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <cJSON.h>

static CURL *global_curl_handle = NULL;

static const char *dl_paths[] = {
	"/download?size=1000000000", 
	"/100MB.zip",
	"/speedtest/test.bin",
	"/random3500x3500.jpg",
	"/garbage.php?ckSize=100",
	NULL
};

static const char *ul_paths[] = {
	"/speedtest/upload.php",
	"/speedtest/upload",
	"/ul/upload.php",
	"/upload.php",
	"/",
	NULL
};
struct MemoryStruct {
	char *memory;
	size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;
	char *ptr = realloc(mem->memory, mem->size + realsize + 1);
	if(!ptr) return 0;
	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;
	return realsize;
}

static size_t discard_data_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	(void)ptr;
	(void)userdata;
	return size * nmemb;
}

static size_t upload_dummy_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	(void)userdata;
	size_t total = size * nmemb;
	memset(ptr, 'A', total);
	return total;
}

static bool is_network_disconnected_curl_error(CURLcode code)
{
	return code == CURLE_COULDNT_CONNECT ||
		   code == CURLE_COULDNT_RESOLVE_HOST ||
		   code == CURLE_COULDNT_RESOLVE_PROXY ||
		   code == CURLE_OPERATION_TIMEDOUT;
}

static int progress_callback(void *p, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal;
    (void)ultotal;
	speed_stats_t *stats = (speed_stats_t *)p;
	curl_off_t current_bytes = stats->is_upload ? ulnow : dlnow;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	
	double elapsed = (double)(now.tv_sec - stats->start_time) + 
					 (double)(now.tv_nsec - stats->start_nano) / 1e9;
	double limit_seconds = (double)stats->limit_ms / 1000.0;

	if (elapsed >= limit_seconds) {
		stats->should_stop = true;
		return 1; 
	}

	if (elapsed > 0.1) {
		stats->current_speed_mbps = (double)(current_bytes * 8) / (elapsed * 1000000.0);
		app_log(LOG_STATUS, "%s: %.2f Mbps (Time: %.1fs)", 
				stats->is_upload ? "Uploading" : "Downloading", 
				stats->current_speed_mbps, elapsed);
	}
	return 0;
}

app_error_t network_init(void)
{
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) 
		return APP_ERR_CURL_INIT;
	global_curl_handle = curl_easy_init();
	return global_curl_handle ? APP_SUCCESS : APP_ERR_CURL_INIT;
}

void network_cleanup(void)
{
	if (global_curl_handle) curl_easy_cleanup(global_curl_handle);
	curl_global_cleanup();
}

app_error_t network_get_location(char *buffer, size_t buf_size)
{
	if(!global_curl_handle) return APP_ERR_CURL_INIT;

	struct MemoryStruct chunk = { malloc(1), 0 };
	curl_easy_reset(global_curl_handle);
	curl_easy_setopt(global_curl_handle, CURLOPT_URL, IP_API);
	curl_easy_setopt(global_curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(global_curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(global_curl_handle, CURLOPT_TIMEOUT, 5L);

	app_log(LOG_INFO, "Fetching location from API...");
	CURLcode res = curl_easy_perform(global_curl_handle);
	
	if(res != CURLE_OK) {
		free(chunk.memory);
		if (is_network_disconnected_curl_error(res)) {
			app_log(LOG_ERROR, "Network unavailable while fetching location: %s", curl_easy_strerror(res));
			return APP_ERR_NETWORK_DISCONNECTED;
		}
		return APP_ERR_LOCATION_NOT_FOUND;
	}

	cJSON *json = cJSON_Parse(chunk.memory);
	cJSON *country = cJSON_GetObjectItemCaseSensitive(json, "country");
	
	if(cJSON_IsString(country) && country->valuestring) {
		strncpy(buffer, country->valuestring, buf_size - 1);
	} else {
		strncpy(buffer, "Unknown", buf_size - 1);
	}
	buffer[buf_size - 1] = '\0';


	cJSON_Delete(json);
	free(chunk.memory);
	return APP_SUCCESS;
}
app_error_t network_find_best_server(const struct server_info *srvs, size_t srv_count, const struct server_info **best_srv) {
	if(!srvs || srv_count == 0 || !best_srv) return APP_ERR_ARGUMENTS;
	if(!global_curl_handle) return APP_ERR_CURL_INIT;

	double min_avg_latency = -1.0;
	size_t best_index = 0;
	const int pings_per_server = 1;

	app_log(LOG_STATUS, "Analyzing %zu servers (RTT check)...", srv_count);

	for(size_t i = 0; i < srv_count; i++) {
		double total_latency = 0;
		int successful_pings = 0;
		char latency_url[1024];
		snprintf(latency_url, sizeof(latency_url), "http://%s/latency.txt", srvs[i].host);

		curl_easy_reset(global_curl_handle);
		curl_easy_setopt(global_curl_handle, CURLOPT_URL, latency_url);
		
		curl_easy_setopt(global_curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(global_curl_handle, CURLOPT_USERAGENT, "curl/8.5.0");

		curl_easy_setopt(global_curl_handle, CURLOPT_CONNECTTIMEOUT, 2L);
		curl_easy_setopt(global_curl_handle, CURLOPT_TIMEOUT, 3L);
		curl_easy_setopt(global_curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

		for(int j = 0; j < pings_per_server; j++) {
			struct MemoryStruct chunk = { malloc(1), 0 };
			if (!chunk.memory) continue;

			curl_easy_setopt(global_curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
			
			CURLcode res = curl_easy_perform(global_curl_handle);
			
			if(res == CURLE_OK) {
				long response_code;
				curl_easy_getinfo(global_curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
				
				// FIX 3: Verify HTTP 200 AND the expected handshake string
				if(response_code == 200 && chunk.memory && strstr(chunk.memory, "test=test")) {
					double rtt, connect_time;
					curl_easy_getinfo(global_curl_handle, CURLINFO_STARTTRANSFER_TIME, &rtt);
					curl_easy_getinfo(global_curl_handle, CURLINFO_CONNECT_TIME, &connect_time);

					double actual_ping = rtt - connect_time;
					total_latency += actual_ping;
					successful_pings++;
				}
			} else {
				app_log(LOG_DEBUG, "Server %s ping %d failed: %s", srvs[i].host, j+1, curl_easy_strerror(res));
			}
			free(chunk.memory);
		}

		if(successful_pings > 0) {
			double avg_latency = total_latency / successful_pings;
			app_log(LOG_STATUS, " -> %s: Avg %.2f ms", srvs[i].host, avg_latency * 1000.0);

			if(min_avg_latency < 0 || avg_latency < min_avg_latency) {
				min_avg_latency = avg_latency;
				best_index = i;
			}
		} else {
			app_log(LOG_WARNING, " -> %s: UNREACHABLE", srvs[i].host);
		}
	}

	if(min_avg_latency < 0) return APP_ERR_LOCATION_NOT_FOUND;

	*best_srv = &srvs[best_index];
	app_log(LOG_INFO, "Selected: %s (%.2f ms)", (*best_srv)->host, min_avg_latency * 1000.0);
	
	return APP_SUCCESS;
}
static app_error_t network_discover_path(const char *host, int is_upload, char *out_url, size_t out_size) {
	const char **paths = is_upload ? ul_paths : dl_paths;
	bool network_disconnected = false;
	
	for (int i = 0; paths[i] != NULL; i++) {
		snprintf(out_url, out_size, "http://%s%s", host, paths[i]);
		curl_easy_reset(global_curl_handle);
		curl_easy_setopt(global_curl_handle, CURLOPT_URL, out_url);
		if (is_upload) {
			curl_easy_setopt(global_curl_handle, CURLOPT_POST, 1L);
			curl_easy_setopt(global_curl_handle, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)0);
			curl_easy_setopt(global_curl_handle, CURLOPT_WRITEFUNCTION, discard_data_cb);
		} else {
			curl_easy_setopt(global_curl_handle, CURLOPT_NOBODY, 1L); // Just check headers
		}
		curl_easy_setopt(global_curl_handle, CURLOPT_CONNECTTIMEOUT, 2L);
		curl_easy_setopt(global_curl_handle, CURLOPT_TIMEOUT, 3L);
		curl_easy_setopt(global_curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

		CURLcode res = curl_easy_perform(global_curl_handle);
		if (res == CURLE_OK) {
			long code;
			curl_easy_getinfo(global_curl_handle, CURLINFO_RESPONSE_CODE, &code);
			if (code == 200) {
				app_log(LOG_STATUS, "Discovered path: %s", out_url);
				return APP_SUCCESS;
			}
		} else if (is_network_disconnected_curl_error(res)) {
			network_disconnected = true;
		}
	}

	if (network_disconnected) {
		app_log(LOG_ERROR, "Network unavailable while probing %s paths on %s", is_upload ? "upload" : "download", host);
		return APP_ERR_NETWORK_DISCONNECTED;
	}
	return APP_ERR_NETWORK_TIMEOUT;
}
app_error_t network_measure_speed(const char *host, int is_upload, unsigned int timeout_ms, speed_stats_t *stats)
{
	if (!global_curl_handle || !host || !stats) return APP_ERR_ARGUMENTS;

	char discovered_url[1024];
	app_error_t path_err = network_discover_path(host, is_upload, discovered_url, sizeof(discovered_url));
	if (path_err != APP_SUCCESS) {
		return path_err;
	}

	stats->is_upload = is_upload;
	stats->should_stop = false;
	// Store the limit so the progress_callback knows when to return 1 (abort)
	stats->limit_ms = timeout_ms; 

	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);
	stats->start_time = start.tv_sec;
	stats->start_nano = start.tv_nsec;
	curl_easy_reset(global_curl_handle);
	curl_easy_setopt(global_curl_handle, CURLOPT_URL, discovered_url);
	curl_easy_setopt(global_curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
	
	curl_easy_setopt(global_curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	long curl_timeout_ms = (long)timeout_ms + 1000; // give the callback a safe margin
	curl_easy_setopt(global_curl_handle, CURLOPT_TIMEOUT_MS, curl_timeout_ms);
	curl_easy_setopt(global_curl_handle, CURLOPT_CONNECTTIMEOUT, 5L); 
	
	curl_easy_setopt(global_curl_handle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(global_curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
	curl_easy_setopt(global_curl_handle, CURLOPT_XFERINFODATA, stats);
	curl_easy_setopt(global_curl_handle, CURLOPT_FAILONERROR, 1L);

	struct curl_slist *headers = NULL;
	if (is_upload) {
		headers = curl_slist_append(headers, "Expect:");
		headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
		curl_easy_setopt(global_curl_handle, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(global_curl_handle, CURLOPT_POST, 1L);
		curl_easy_setopt(global_curl_handle, CURLOPT_READFUNCTION, upload_dummy_cb);
		curl_easy_setopt(global_curl_handle, CURLOPT_READDATA, NULL);
		// 2GB target to ensure the pipe stays full until the timeout hits
		curl_easy_setopt(global_curl_handle, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)1024 * 1024 * 1024 * 2);
	} else {
		curl_easy_setopt(global_curl_handle, CURLOPT_WRITEFUNCTION, discard_data_cb);
	}

	CURLcode res = curl_easy_perform(global_curl_handle);
	if (headers) curl_slist_free_all(headers);

	if (res == CURLE_PARTIAL_FILE) {
		app_log(LOG_WARNING, "%s test completed with a partial transfer, using received data", is_upload ? "Upload" : "Download");
		res = CURLE_OK;
	}

	if (res == CURLE_OK || res == CURLE_ABORTED_BY_CALLBACK) {
		double total_time;
		double pretransfer_time = 0.0;
		double starttransfer_time = 0.0;
		curl_off_t total_bytes;
		
		curl_easy_getinfo(global_curl_handle, CURLINFO_TOTAL_TIME, &total_time);
		curl_easy_getinfo(global_curl_handle, CURLINFO_PRETRANSFER_TIME, &pretransfer_time);
		curl_easy_getinfo(global_curl_handle, CURLINFO_STARTTRANSFER_TIME, &starttransfer_time);
		curl_easy_getinfo(global_curl_handle, is_upload ? CURLINFO_SIZE_UPLOAD_T : CURLINFO_SIZE_DOWNLOAD_T, &total_bytes);
		
		double transfer_time = total_time;
		if (is_upload) {
			transfer_time = total_time - pretransfer_time;
		} else {
			transfer_time = total_time - starttransfer_time;
		}
		if (transfer_time < 0.001 || transfer_time > total_time) {
			transfer_time = total_time;
		}
		
		if (transfer_time > 0) {
			stats->final_avg_mbps = (double)(total_bytes * 8) / (transfer_time * 1000000.0);
			stats->last_time = transfer_time;
		} else {
			stats->last_time = 0.0;
		}
		return APP_SUCCESS;
	}

	app_log(LOG_ERROR, "%s test failed: %s", is_upload ? "Upload" : "Download", curl_easy_strerror(res));
	return APP_ERR_NETWORK_TIMEOUT;
}