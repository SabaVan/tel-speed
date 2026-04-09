# tel-speed

A command-line speed testing tool written in C that measures download and upload speeds. The program automatically discovers available test endpoints, selects the best server based on latency, and performs accurate speed measurements with robust error handling.

## Features

- **Download and Upload Testing**: Measure both download and upload speeds with configurable timeouts
- **Automatic Server Discovery**: Probes multiple path patterns to find working test endpoints
- **Best Server Selection**: Analyzes latency across available servers to choose the optimal one
- **Location Detection**: Automatically detects user location via IP geolocation API
- **Country Filtering**: Filter servers by country for targeted testing
- **Robust Error Handling**: Handles network disconnections, timeouts, and partial transfers gracefully
- **Memory Safe**: Valgrind-proof with no memory leaks or errors
- **Clean Output**: Real-time progress updates with proper status line management

## Dependencies

- **libcurl**: For HTTP requests and network operations
- **cJSON**: **(Included in source)** For parsing server list JSON files
- **GCC**: C compiler with support for C99 standard
- **Make**: Build system

## Building

```bash
make
```

## Usage

```bash
./tel-speed [OPTIONS]
```

### Options

- `-d, --download`: Run download speed test
- `-u, --upload`: Run upload speed test
- `-l, --location COUNTRY`: Filter servers by country
- `-s, --server URL`: Specify server URL directly (bypasses server list file)
- `-t, --timeout MS`: Set test timeout in milliseconds (default: 15000)
- `-v, --verbose`: Enable verbose output (default)
- `--debug`: Enable debug logging
- `-h, --help`: Show help message

### Examples

Test download and upload speeds using automatic server selection:
```bash
./tel-speed -a
```

Test against a specific server:
```bash
./tel-speed -d -u -s speedtest.example.com:8080
```

Test with country filtering:
```bash
./tel-speed -du -L Lithuania,Latvia,Estonia
```

## Code Analysis

### Architecture

The program follows a modular design with separate components for different functionalities:

- **main.c**: Program entry point, test orchestration, and server list parsing
- **network_test.c**: Core network operations including speed measurement, path discovery, and server latency testing
- **app_error.c**: Centralized logging and error management with status line handling
- **app_cli.c**: Command-line argument parsing
- **app_config.c**: Configuration initialization and validation
- **cJSON.c**: JSON parsing library (third-party)

### Key Components

#### Speed Measurement
- Uses libcurl for HTTP transfers with progress callbacks
- Calculates speeds by measuring bytes transferred over time
- Subtracts pre-transfer setup time for accurate measurements
- Handles partial file transfers as successful with warnings

#### Server Discovery
- Probes multiple predefined path patterns for download/upload endpoints
- Uses HEAD requests for downloads and POST probes for uploads
- Automatically selects working paths

#### Error Handling
- Comprehensive error codes for different failure scenarios
- Network disconnection detection during path discovery
- Timeout handling with configurable limits
- Memory allocation failure handling

#### Memory Management
- All dynamic allocations are properly freed
- No memory leaks (verified with Valgrind)
- Safe string operations with bounds checking

### Technical Details

- **Language**: C99 with GCC extensions
- **JSON Parsing**: cJSON for server list processing
- **Timing**: High-resolution monotonic clock for accurate measurements
- **Output**: ANSI escape sequences for status line updates
- **Threading**: Single-threaded with asynchronous curl operations

## Server List Format

The program expects a JSON file (`speedtest_server_list.json`) with the following structure:

```json
[
  {
    "country": "Country Name",
    "host": "server.example.com:port",
    "id": 123
  }
]
```
## Example Run
```text
./tel-speed -a
[INFO] Config -----------------------------------------------------------
[INFO] (D/U/L)   TIMEOUT    URL_CNT    LOC_CNT    SRV_FILE                  
[INFO] [Y/Y/Y]   15000      0          0          speedtest_server_list.json
[INFO] ------------------------------------------------------------------
[INFO] Fetching location from API...
[INFO] Detected location: Lithuania
[INFO] Found 12 servers for given location
[WARNING]  -> speedtest.bacloud.com:8080: UNREACHABLE
[INFO] Server speedtest1.ntt.lt:8080 ping 1 failed: Couldn't connect to server
[WARNING]  -> speedtest1.ntt.lt:8080: UNREACHABLE
[INFO] Server speedtest.rackray.eu:8080 ping 1 failed: Timeout was reached
[WARNING]  -> speedtest.rackray.eu:8080: UNREACHABLE
[INFO] Server vln038-speedtest-1.tele2.net:8080 ping 1 failed: Timeout was reached
[WARNING]  -> vln038-speedtest-1.tele2.net:8080: UNREACHABLE
[INFO] Server speedtest-01.cgates.lt:8080 ping 1 failed: Timeout was reached
[WARNING]  -> speedtest-01.cgates.lt:8080: UNREACHABLE
[INFO] Selected: speedtest-vno.init.lt:8080 (1.42 ms)
[INFO] Detected smallest latency for (Lithuania: speedtest-vno.init.lt:8080)
Testing speed for speedtest-vno.init.lt:8080
Downloading speed 59.05 Mbps (15.00s)
Uploading speed 77.57 Mbps (15.00s)
```
