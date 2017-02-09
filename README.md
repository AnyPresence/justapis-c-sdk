# JustAPIs C SDK

## Overview

Lightweight C SDK to connect to a JustAPIs gateway.

## Introduction

The core of the SDK is provided in the following files:

* `include/justapis.h`
* `src/justapis.c`

Optional JSON support is provided using cJSON:

* `include/cJSON.h`
* `src/cJSON.c`

Optional MQTT support is provided using mosquitto:

* `include/mosquitto/lib/*.h`
* `src/mosquitto/lib/*.c`

HTTP requests are handled through **libcurl**.

You may build, install, and test the SDK as a library using CMake, or include the above files directly in your own project. See 'Setup' section for more info.

Three structs capture the core concepts of the SDK: `ja_gateway`, `ja_request`, and `ja_response`.

MQTT API mainly deals with `ja_mqtt_connection` struct.

A `ja_gateway` represents a specific JustAPIs gateway instance and session. You submit `ja_request`s to this gateway, and receive `ja_respose`s or errors in response.

The following features are supported:

* The `ja_gateway` can optionally persist cookies across requests, and even across instances and runs, using a cookiejar file.
* The `ja_gateway` can optionally provide a response cache, allowing repeated requests to return a cached response without making an additional network request.
* The `ja_gateway` can have default Query Parameters and HTTP Headers which will get appended to all outgoing requests. This can be useful for authentication tokens. Individual requests can override the default values with specific values of their own.
* The `ja_gateway` can have default callbacks to use when sending body data with requests, receiving body data in responses, or receiving headers. Alternative callbacks be supplied when performing a request. If no callbacks are provided the SDK provides can manage these operations automatically using simple in-memory buffers for body data and standard parsing of headers.
* With cJSON included, and `JA_ENABLE_CJSON` set to a non-zero value (default), the SDK can automaticaly serialize JSON objects to send in the body of requests, and can automatically deserialize JSON content that's received in responses.
* The entire SDK can use custom memory allocator functions that match the function signature of the stdlib allocator functions. If no custom allocators are provided, stdlib `malloc`, `realloc`, `calloc`, `strdup`, and `free` will be used by default.

### Samples

Currently following sample code is included:
1. `ns-mqtt-demo-cli` and `ns-mqtt-demo-cmake-cli`. Demostrates following:
* Usage of MQTT API.
* Using SDK source directly in Xcode.
* Using CMake to include SDK.


## Dependencies and Setup

### Development/Production

* **CMake 3.5** [Optional, build tool] To build and use the SDK as a static library, you can use CMake 3.5 or higher. However, you can also include the SDK source in your own project. See 'Setup' section for details.

* **libcurl** [Required, library] The SDK currently relies extensively on libcurl for cross-platform HTTP communication. You will need to make sure that the development libraries and headers for libcurl are available to the compiler and linker when building a project that uses this SDK. When using CMake, it will find the libcurl development items automatically if they're installed on your system. Otherwise you have to make sure your project includes headers and links against libcurl. 

Linker Flag: `-lcurl`. See 'Setup' section for details.

* **cJSON** [Optional, source included] The SDK can *optionally* provide simple JSON serialization when sending requests, and JSON deserialization when receiving responses. This feature is enabled by default and the source code for cJSON (`cJSON.h` and `cJSON.c`) is included and built into the library automatically based on `JA_ENABLE_CJSON` flag.

* **mosquitto** [Optional, source included] The SDK can *optionally* provide API to interact with MQTT Broker. This feature is enabled by default and the source code of mosquitto (`include/mosquitto/lib/*.h` and `src/mosquitto/lib/*.h.c`) is included and built into the library automatically based on `JA_ENABLE_MQTT` flag. 
* **pthread** [Optiona] When both `JA_ENABLE_MQTT_WITH_THREADING` and `JA_ENABLE_MQTT` flags are enabled(default behavior), make sure that the development libraries and headers for pthread are available to the compiler and linker when building a project that uses this SDK. When using CMake, it will find the pthread development items automatically if they're installed on your system. Otherwise you have to make sure your project includes headers and links against pthread.

Linker Flag: `-lpthread`. See 'Setup' section for details.

* **Additional Libraries(math etc)**  [Optional, depending on platform] Depending upon platform and compiler(e.g. Ubuntu 16 LTS), you may have to link against additional libraries. 

Linker Flag: `-ldl -lm`. CMakeLists already adds these flags.

### Unit Testing

Unit testing is implemented for this project as a secondary target `runTests` in the CMake listfile. It uses and and includes `CHEAT` as a testing framework and `Civetweb` to mock a JustAPI's HTTP gateway. As long as you meet the requirements for the SDK, above, no further dependencies are needed for testing.

### Installing Dependencies

The dependencies of this SDK are described above.

#### CMake

This is optional and required only if you are building using CMake. You can get CMake from [here](https://cmake.org/download). To use it with this project, you'll need version 3.5 or above.

#### libcurl

Many OS package managers can install libcurl. However, make sure that you install the development headers. For instance, on Raspberry Pi Raspian, and other Debian-like systems, you'll want to install:

```bash
> apt-get install libcurl4-dev
```

Of course, you can also download and install libcurl directly from its [developer site](https://curl.haxx.se/download.html).

#### pthread

**macOS**: Available by default.

**Linux**: Available in most distributions and compilers like `gcc`.

**Windows**: You will have add `pthread` depending upon you compiler/IDE. [See link](https://www.sourceware.org/pthreads-win32/)

### Using the SDK as a Library

1. Use CMake to make your platform-specific build system.
2. Invoke that build system to build and install the SDK library
3. Link against that library (`justapis-c-sdk`) in your own project. See samples for example.

### Including Code Directly

The SDK is intentionally organized in a way that makes it easy for you to include it in your own project without linking it in as a library or relying on CMake.

To take advantage of this:

1. Add `include` and `src` folders in your project. 
2. Make sure to link against `libcurl`
3. Optional: If needed make sure `include/mosquitto/lib` is in your 'Search Paths' so its headers can be included using `#include <xxx>` syntax as well.
4. Optional: If needed make sure `pthread.h` is included in search paths and to link against `pthread`.
5. Optional: If needed make sure link against additional libraries as described above.

## Usage

### Making requests

To make requests to your JustAPIs server, you'll just need to create a `ja_gateway`, submit a `ja_request`.

A simple example is shown here: 

```c

// Create the gateway. Using NULL for the second parameter disables cookie support.
ja_gateway* gateway = ja_gateway_init("http://my-justapi-server.local:5000/", NULL);

// Create a request to GET /foo
ja_request* request = ja_request_init(ja_request_method_get, "/foo");

// Add a query parameter
ja_request_set_query_parameter(request, "id", "123");

// Perform the request. Using NULL for the third parameter indicates we don't need custom body/header callbacks.
ja_result result = ja_perform_request(gateway, request, NULL);

// Inspect the result
if (result.error != ja_request_error_none)
{
// Anything but ja_request_error_none indicates an error.
printf("Received an error: %d", result.error);
}
else
{
// If there was no error, there should be a response. Report its status code
printf("Received a response with HTTP status code: %ld", result.response.status_code);

// Clean up the response, since we're done with it.
ja_response_free(result.response);
}

// Clean up the request
ja_request_free(request);

// Clean up the gateway
ja_gateway_free(gateway);

```

### Response Caching

Response caching is disabled by default.

To use it, you must first enable the gateway's cache:

```c
// Enable caching of up to 25 distinct responses:
ja_gateway_enable_cache(gateway, 25);
```

Later, make you can describe your cache preferences when making a GET request:

```c
// Allow the cache to service this request, if a response is found.
request.allow_cached_response = true;

// If this request hits the network, specify how long to save its response in the cache
request.cache_response_with_expiration = 30; // seconds

// When working with the cache, use the following cache identifier rather than the URL-based hash:
request.custom_cache_identifier = 0xdeadbeef;
```

### Receiving JSON Responses

By default automatic JSON parsing is available, but disabled.

To enable it on a specific request:

```c
request.parse_json_responses_automatically = true;
```

If that flag is set and the SDK encounters a Content-Type of `application/json` or `text/json` in response to a request,
you can access the parsed JSON data as follows:

```c
if (response.body_type == ja_response_body_type_json)
{
cJSON* json = response.json;
}
```

### Cookies

Cookie support is provided by libcurl through cookiejar files. To enable cookies on a gateway, provide a path for curl
to use:

```c

// Create a temp file and get its path using mkstemp
char cookiejar_path[L_tmpnam];
memset(cookiejar_path, 0, sizeof(cookiejar_path));
snprintf(cookiejar_path, L_tmpnam, "%scookierjar.txt-XXXXXX", P_tmpdir);
mkstemp(cookiejar_path);

// Create the gateway and give it a cookiejar to store cookies
gateway = ja_gateway_init("http://localhost:31337", cookiejar_path);

```

### Sending Body Data

On POST and PUT requests, you'll often want to send body data as part of those requests.

You always have the option to set binary body data yourself using `ja_request_set_body`. Make sure to set a Content-Type header on the request as well!

If you've left JSON support enabled, you can alternatively use `ja_request_set_json_body` which will automatically serialize your cJSON structure and set the Content-Type header appropriately.

### Pinning Public Keys for HTTPS

You may want to verify that you're connecting to the expected gateway when using HTTPS by specifying the public key you expect the server to report when the secure connection is made.

To do so, provide the expected public key using the `ja_gateway_set_pinned_public_key_file` function before submitting any requests. You may provide a path to a PEM or DER file, or a specially formatted string of sha256 hashes. For more information on support formats for the key, see the libcurl documentation for [CURLOPT_PINNEDPUBLICKEY](https://curl.haxx.se/libcurl/c/CURLOPT_PINNEDPUBLICKEY.html).

### Memory Allocation

The SDK and libcurl manage memory using the stdlib function by default.

If you want the SDK to use different functions, provide them as follows:

```c
// Instruct the SDK (and cJSON) to use custom allocators
ja_set_allocators(my_malloc, my_free, my_realloc, my_strdup, my_calloc);
```

**Note**: libcurl's allocators are NOT automatically changed when calling this method. If you need libcurl to use custom allocators as well, you'll want to call libcurl's own configuration function:

```c
curl_global_init_mem(my_malloc, my_free, my_realloc, my_strdup, my_calloc);
```

[See](https://curl.haxx.se/libcurl/c/curl_global_init_mem.html) for more information on that function.

### Streamlining the Build

By default, the SDK builds in support for JSON,  Response Caching, Public Key Pinning, MQTT with Threading. If you won't be using these, you can reduce the size of the library by disabling each of them with a build flag.

Make sure to set the build flag to `0` to disable a feature. Any non-zero value will leave the feature enabled. These values can be changed in `CMakeLists.txt` file or `ja_config.h`(when you are including source).

* To disable JSON support, set `JA_ENABLE_CJSON` to `0`
* To disable Caching support, set `JA_ENABLE_RESPONSE_CACHING` to `0`
* To disable Public Key Pinning support, set `JA_ENABLE_PUBLIC_KEY_PINNING` to `0`
* To disable MQTT support, set `JA_ENABLE_MQTT` to `0`
* To disable MQTT with threading support, set `JA_ENABLE_MQTT_WITH_THREADING` to `0`. `pthread` will be required when this is enabled.

## Development

You can clone or fork this repository to make any changes you need to the source code or CMake listfiles for building projects.

You can also include the source files directly into your own project and edit them there.
