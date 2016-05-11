//
// Created by Andrew Palumbo on 4/7/16.
//

#ifndef JUSTAPIS_H
#define JUSTAPIS_H


///
/// Build Options
/// ------
///

/// JA_ENABLE_RESPONSE_CACHE
/// 1: Build in support for automatic response caching (default)
/// 0: Do not build in support for automatic response caching
#ifndef JA_ENABLE_RESPONSE_CACHE
  #define JA_ENABLE_RESPONSE_CACHE 1
#endif

/// JA_ENABLE_CJSON
/// 1: Provides convenience features for JSON in requests and responses, using cJSON (default)
/// 0: Removes convenience features for JSON in requests and responses.
#ifndef JA_ENABLE_CJSON
  #define JA_ENABLE_CJSON 1
#endif



///
/// Includes
/// ------
///

#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

#if JA_ENABLE_CJSON
  #include "cJSON.h"
#endif



///
/// Custom Allocators
/// ------
///

/// Signature of functions to be invoked in place of stdlib malloc
typedef void* (*ja_malloc_callback)(size_t size);

/// Signature of functions to be invoked in place of stdlib free
typedef void (*ja_free_callback)(void* ptr);

/// Signature of functions to be invoked in place of stdlib realloc
typedef void* (*ja_realloc_callback)(void* ptr, size_t size);

/// Signature of functions to be invoked in place of stdlib strdup
typedef char* (*ja_strdup_callback)(const char* str);

/// Signature of functions to be invoked in place of stdlib calloc
typedef void* (*ja_calloc_callback)(size_t nmemb, size_t size);

/// Sets allocator callbacks to be used in place for stdlib implementations. These
/// will be used by this library. (and in cJSON)
///
/// NOTE: This library ultimately dispatches requests to libcurl. The allocators you
///       provide here will not be used by libcurl without further action.
///
///       If you want to use custom allocators for libcurl, you must call
///       curl_global_init_mem before using this library, and curl_global_cleanup
///       when done.
///          See https://curl.haxx.se/libcurl/c/curl_global_init_mem.html
///          and https://curl.haxx.se/libcurl/c/curl_global_cleanup.html
void ja_set_allocators(ja_malloc_callback malloc,
                       ja_free_callback free,
                       ja_realloc_callback realloc,
                       ja_strdup_callback strdup,
                       ja_calloc_callback calloc);

///
/// Enums
/// -------

/// Errors that may occur when performing a request
typedef enum  {
    ja_request_error_none = 0,
    ja_request_error_invalid_gateway,
    ja_request_error_invalid_request,
    ja_request_error_failed_to_connect,
    ja_request_error_url_too_long,
    ja_request_error_must_allocate_response
} ja_request_error;

/// Supported HTTP methods
typedef enum {
    ja_request_method_get = 0,
    ja_request_method_post,
    ja_request_method_put,
    ja_request_method_delete
} ja_request_method;

/// The type of data expected in a ja_request.body_data union
typedef enum {
    ja_request_body_type_userdata = 0,
    ja_request_body_type_buffer,
} ja_request_body_type;

/// The type of data expected in a ja_response.body_data union
typedef enum {
    ja_response_body_type_userdata = 0,
    ja_response_body_type_buffer,
#if JA_ENABLE_CJSON
    ja_response_body_type_json,
#endif
} ja_response_body_type;

/// The type of data expected in a ja_response.header_data union
typedef enum {
    ja_response_header_type_userdata = 0,
    ja_response_header_type_parsed
} ja_response_header_type;


///
/// Forward Declarations
/// ------
///

struct ja_gateway_t;

struct ja_request_t;

struct ja_response_t;

struct ja_request_callbacks_t;

#if JA_ENABLE_RESPONSE_CACHE
struct ja_cache_t;
#endif


///
/// Key Value Pair Lists
/// ------
///

/// A linked list used for ordered lists of Query Parameters and Headers
typedef struct _ja_key_value_pair {
    char* key;
    char* value;
    struct _ja_key_value_pair* next;
} ja_key_value_pair;

/// Appends an new ja_key_value_pair to the library-managed list, creating head if head is NULL. Returns the head.
ja_key_value_pair* ja_key_value_list_append(ja_key_value_pair* head, const char* key, const char* value);

/// Frees a library-managed list of key_value_pairs.
void ja_key_value_list_free(ja_key_value_pair* head);


///
/// Simple Buffer
/// ------
/// A Simple Contiguous buffer that's used primarily for request and response bodies.
///

/// Of note, ja_simple_buffer manipulations functions (ja_simple_buffer_append) makes sure
/// that the data member is always NULL-terminated. This allows the field to be used directly
/// as a C String.
///
/// Formally, if data is not NULL:
///   * data[0] - data[data_length-1] will contain data provided through ja_simple_buffer_append
///   * data[data_length] will be '\0', allowing buffer to be used directly as a NULL-terminated string
///   * The allocated size of buffer will be data_length+1

/// The simple buffer object
typedef struct {
    char* data;
    size_t data_length;
} ja_simple_buffer;

/// Appends the provided data to the end of a buffer. If buffer is NULL, creates a new buffer. Returns the buffer.
ja_simple_buffer* ja_simple_buffer_append(ja_simple_buffer* buffer, const char* data, size_t length);

/// Frees resources associated with a simple buffer
void ja_simple_buffer_free(ja_simple_buffer* buffer);


///
/// Request Callbacks
/// ------
///
/// By default, body data provided in POST and PUT requests will automatically be sent to the server,
/// header data received in response to requests will be parsed and stored in the response, and body
/// data received in response to requests will be stored in the response body_data.
///
/// You may override these defaults by providing alternate callbacks in on a ja_gateway or as
/// a parameter to ja_perform_request(). This is especially useful to stream data directly to or
/// from file or in other situations where you want to limit peak memory consumption.

/// Called to send body data associated with a request
/// - userdata is as provided in your ja_request_callback object
/// - send_buffer is the buffer you should fill
/// - send_buffer_size is the size of that buffer
/// and the return value indicates how much data you actually placed in send_buffer
typedef size_t (*ja_request_send_body_callback)(
        const struct ja_gateway_t* gateway,
        const struct ja_request_t* request,
        void* userdata,
        char* send_buffer,
        size_t send_buffer_size
);

/// Called to receive body data coming back from a request
/// - userdata is as provided in your ja_request_callback object
/// - received_data is the incoming data from the server
/// - received_data_size is the size of received_data
/// and the return value indicates how much of the received_data buffer you consumed.
typedef size_t (*ja_request_receive_body_callback)(
        const struct ja_gateway_t* gateway,
        const struct ja_request_t* request,
        struct ja_response_t* response,
        void* userdata,
        const char* received_data,
        size_t received_data_size);

/// Called to receive headers coming back from a request
/// - userdata is as provided in your ja_request_callback object
/// - received_headers_data is single complete line of headers
/// - received_headers_data_size is the size of the received_headers_data buffer
/// and the return value indicates how much of the received_headers_data buffer you consumed.
typedef size_t (*ja_request_receive_headers_callback)(
        const struct ja_gateway_t* gateway,
        const struct ja_request_t* request,
        struct ja_response_t* response,
        void* userdata,
        const char* received_headers_data,
        size_t received_headers_data_size);

/// The collection of callbacks to be called while a request is performed. Any callbacks may be NULL
typedef struct ja_request_callbacks_t {
    ja_request_send_body_callback on_send_body;
    void* on_send_body_userdata;

    ja_request_receive_body_callback on_receive_body;
    void* on_receive_body_userdata;

    ja_request_receive_headers_callback on_receive_headers;
    void* on_receive_headers_userdata;
} ja_request_callbacks;

///
/// Gateway
/// ------
///
/// A Gateway represents a single JustAPI gateway, to which requests will be submitted.

/// The gateway object
typedef struct ja_gateway_t {
    /// The Base URL identifying this gateway.
    char* base_url;

    ja_key_value_pair* default_headers;
    ja_key_value_pair* default_query_parameters;

    ja_request_callbacks default_request_callbacks;

    /// Cookies/sessions associated with will persist in a file at the provided path. When NULL, cookies will
    /// not persist across requests.
    char* cookiejar_path;

#if JA_ENABLE_RESPONSE_CACHE
    struct ja_cache_t* cache;
#endif
} ja_gateway;

/// Creates and returns a new ja_gateway. Must be released using ja_gateway_free when done.
ja_gateway* ja_gateway_init(const char *baseUrl, const char *cookiejar_path);

#if JA_ENABLE_RESPONSE_CACHE
/// Adds a response cache for GET requests.
void ja_gateway_enable_cache(ja_gateway* gateway, size_t max_cached_responses);
#endif

/// Frees a gateway generated by ja_gateway_init
void ja_gateway_free(ja_gateway *gateway);


///
/// Request
/// ------
///
/// A Request describes a specific HTTP request, with a METHOD, PATH and other parameters.

/// The request object
typedef struct ja_request_t {

    /// The HTTP Verb to use
    ja_request_method method;

    /// The path to request, relative to the Gateway's baseURL
    char* path;

    /// A list of query string parameters to append to the path
    ja_key_value_pair* query_params;

    /// A list of HTTP headers to be sent with the request
    ja_key_value_pair* headers;

    /// Type and storage of body data
    ja_request_body_type body_type;
    union {
        void* userdata;
        ja_simple_buffer* buffer;
    } body_data;

    /// Whether to follow redirects suggested by the server or complete the request before doing so.
    bool follow_redirects;

#if JA_ENABLE_CJSON
    /// If true, attempts to automatically parses JSON response body data if response Content-Type is
    /// application/json or text/json
    bool parse_json_response_body_automatically;
#endif

#if JA_ENABLE_RESPONSE_CACHE
    /// Whether to cache responses
    bool allow_cached_response;
    /// How long to cache reponses (0 = do not cache)
    unsigned int cache_response_with_expiration;
    /// A custom identifier to use for caching. If no custom ID is set, HASH(METHOD + PATH + PARAMS) is used
    unsigned long custom_cache_identifier;
#endif
} ja_request;

/// Creates and returns a new request. Must be released using ja_request_free when done.
ja_request* ja_request_init(ja_request_method method, const char *request_url);

/// Adds the provided data to the request as a SimpleBuffer. COPIES THE DATA. pass (request, NULL, 0) to unset.
void ja_request_set_body(ja_request *request, char *data, size_t data_length);

#if JA_ENABLE_CJSON
/// Adds provided cJSON hierarchy as the request body and sets Content-Type. COPIES THE DATA. pass (reqeust, NULL to unset)
void ja_request_set_body_json(ja_request *request, const char *contentType, cJSON *json);
#endif

/// Convenience function to set a specific header on a request. Pass value=NULL to remove the header.
void ja_request_set_header(ja_request* request, const char* key, const char* value);

/// Convenience function to returns a header value on a request.
const char* ja_request_get_header(const ja_request* request, const char* key);

/// Convenience function to set a query parameter on a request.
void ja_request_set_query_parameter(ja_request* request, const char* key, const char* value);

/// Frees a request generated by ja_request_init
void ja_request_free(ja_request *request);


///
/// Response
/// ------
///
/// A response reports the data received from the server when a request is performed.
/// Responses should be released using ja_response_free when no longer needed.

/// The response object
typedef struct ja_response_t {
    /// The URL used to initiate the request
    char *request_url;

    /// The URL associated with any returned data.
    char* resolved_url;

    /// The HTTP Status Code returned by the server for this request
    long status_code;

    /// Type and storage of header data
    ja_response_header_type header_type;
    union {
        void* userdata;
        ja_key_value_pair* parsed;
    } header_data;

    /// Type and storage of body data
    ja_response_body_type body_type;
    union {
        void* userdata;
        ja_simple_buffer* buffer;
#if JA_ENABLE_CJSON
        cJSON* json;
#endif
    } body_data;

#if JA_ENABLE_RESPONSE_CACHE
    /// The number of outstanding references to this response. Managed automatically.
    int cache_references;
#endif
} ja_response;

/// Convenience method to return a parsed header on a response. Returns NULL if header with key not found
const char* ja_response_get_parsed_header(const ja_response* response, const char* key);

/// Frees a response received from ja_perform_request
void ja_response_free(ja_response *response);


///
/// Request Actions
/// ------

/// The result of a request, either and error or response.
/// (response will be NULL unless error == ja_request_error_none)
typedef struct {
    ja_request_error error;
    ja_response* response;
} ja_result;

/// Submits a request to the gateway, and invokes the callback as the request proceeds
ja_result ja_perform_request(ja_gateway *gateway, const ja_request *request,
                        const ja_request_callbacks *callbacks);


#endif //JUSTAPIS_H
