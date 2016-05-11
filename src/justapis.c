//
// Created by Andrew Palumbo on 4/11/16.
//

#include "justapis.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/// Used to resolve default callbacks and userdata in ja_perform_request
#define REQUEST_CALLBACK(field) ((callbacks && callbacks->field) ? callbacks->field : gateway->default_request_callbacks.field)
#define REQUEST_CALLBACK_USERDATA(field) ((callbacks && callbacks->field) ? callbacks->field##_userdata : gateway->default_request_callbacks.field##_userdata)

/// The global container for our allocators. defaulted to their stdlib implementations
static struct {
    ja_malloc_callback malloc;
    ja_free_callback free;
    ja_realloc_callback realloc;
    ja_strdup_callback strdup;
    ja_calloc_callback calloc;
} allocators = {
    malloc,
    free,
    realloc,
    strdup,
    calloc
};

/// Sets custom allocators, falling back to stdlib versions for any that aren't provided.
void ja_set_allocators(ja_malloc_callback malloc_callback,
                       ja_free_callback free_callback,
                       ja_realloc_callback realloc_callback,
                       ja_strdup_callback strdup_callback,
                       ja_calloc_callback calloc_callback)
{
    allocators.malloc = malloc_callback ? malloc_callback : malloc;
    allocators.free = free_callback ? free_callback : free;
    allocators.realloc = realloc_callback ? realloc_callback : realloc;
    allocators.strdup = strdup_callback ? strdup_callback : strdup;
    allocators.calloc = calloc_callback ? calloc_callback : calloc;

#if JA_ENABLE_CJSON
    cJSON_Hooks cjson_hooks;
    cjson_hooks.malloc_fn = allocators.malloc;
    cjson_hooks.free_fn = allocators.free;
    cJSON_InitHooks(&cjson_hooks);
#endif
}


/// The context provided to our CURL Write-Body callback
typedef struct {
    CURL* curl;
    const ja_gateway* gateway;
    const ja_request* request;
    ja_response* response;
    ja_request_receive_body_callback external_callback;
    void* external_userdata;
} ja_curl_write_callback_userdata;
/// CURL Write-body callback
static size_t ja_curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

/// The context provided to our CURL Header callback
typedef struct {
    CURL* curl;
    const ja_gateway* gateway;
    const ja_request* request;
    ja_response* response;
    ja_request_receive_headers_callback external_callback;
    void* external_userdata;
} ja_curl_header_callback_userdata;
/// CURL Header callback
static size_t ja_curl_header_callback(char *buffer, size_t size, size_t nitems, void *userdata);

/// The context provided to our CURL Read-Body callback
typedef struct {
    CURL* curl;
    const ja_gateway* gateway;
    const ja_request* request;
    ja_request_send_body_callback external_callback;
    void* external_userdata;
} ja_curl_read_callback_userdata;
/// The CURL Read-body callback
static size_t ja_curl_read_callback(char *buffer, size_t size, size_t nitems, void *instream);

/// Automatically sends the data in request->body_data->buffer with the request [Default]
static size_t ja_request_send_body_from_buffer_callback(
        const struct ja_gateway_t* gateway,
        const ja_request* request,
        void* userdata,
        char* sendBuffer,
        size_t sendBufferSize
);

/// Automatically saves received body data to response->body_data->buffer [Default if JA_ENABLE_CJSON is unset]
static size_t ja_request_receive_body_to_buffer_callback(
        const struct ja_gateway_t* gateway,
        const ja_request* request,
        ja_response* response,
        void* userdata,
        const char* received_data,
        size_t received_data_size);

/// Parses received headers to response->header_data->parsed as ja_key_value_pair items [Default]
static size_t ja_request_receive_headers_to_parsed_callback(
        const struct ja_gateway_t* gateway,
        const ja_request* request,
        ja_response* response,
        void* userdata,
        const char* received_headers_data,
        size_t received_headers_data_size);


static ja_request_error validate_request(const ja_gateway *gateway, const ja_request *request);
static ja_request_error set_curl_request_parameters(CURL *curl, const ja_gateway *gateway, const ja_request *request,
                                                    ja_response *response);

// unused: static char *url_decode(const char *str);
static char *url_encode(const char *str);
static char to_hex(char code);
// unused: static char from_hex(char ch);

static char* build_query_pair_string(const char *key,
                                                 const char *value, bool first);
static char* build_query_string(const ja_gateway *gateway, const ja_request *request);
static char* build_request_url(const ja_gateway* gateway, const ja_request* request);
static struct curl_slist * build_curl_request_headers(const ja_gateway *gateway, const ja_request *request);

static void populate_response_data(CURL *curl, const ja_request *request, ja_response *response);

static ja_response* ja_init_response();


#if JA_ENABLE_RESPONSE_CACHE
/// *****************
/// * Caching
/// *****************

typedef struct cache_item_t {
    long cache_identifier;
    time_t expires_at;
    ja_response* response;
    struct cache_item_t* previous;
    struct cache_item_t* next;
} ja_cache_item;

typedef struct ja_cache_t {
    size_t max_cached_responses;
    size_t current_cached_responses;

    ja_cache_item* cache_item;
} ja_cache;

/// Retrieve a response from the cache
static ja_response* ja_cache_get(struct ja_gateway_t* gateway, const ja_request* request);

/// Save a response to the cache
static void ja_cache_set(struct ja_gateway_t* gateway, const ja_request* request, ja_response* response);

// Generate an intenger identifier from a request (or extract the custom one)
static unsigned long ja_generate_cache_identifier(const ja_gateway* gateway, const ja_request* request);

static ja_cache* ja_init_basic_cache(size_t max_cached_responses);
static void ja_free_basic_cache(ja_cache* basic_cache);
static void ja_add_cache_item(ja_cache* cache, long cache_identifier, time_t expires_at, ja_response* response);
static void ja_remove_cache_item(ja_cache* cache, ja_cache_item* item);

#endif // AP_ENABLE_RESPONSE_CACHE

#if JA_ENABLE_CJSON

static bool ja_response_has_json_body(const ja_response* response);
static void ja_response_parse_json_body(ja_response* response);

#endif
ja_result ja_perform_request(ja_gateway *gateway, const ja_request *request,
                        const ja_request_callbacks *callbacks)
{
    // 0.1 Validate Parameters
    ja_request_error request_error = validate_request(gateway, request);
    if (request_error != ja_request_error_none)
    {
        // Quit immediately on bad parameters. Don't even initialize CURL.
        ja_result result = { request_error, NULL };
        return result;
    }

    // 0.2 Check Cache
#if JA_ENABLE_RESPONSE_CACHE
    ja_response* cachedResponse = ja_cache_get(gateway, request);
    if (cachedResponse)
    {
        ja_result result = { ja_request_error_none, cachedResponse };
        return result;
    }
#endif

    ja_response* response = ja_init_response();

    // 1. Init CURL
    CURL* curl = curl_easy_init();

    // 2. Set CURL callbacks
    // 2.1. Write Callback - incoming body data
    ja_curl_write_callback_userdata write_userdata = {0};
    write_userdata.curl = curl;
    write_userdata.gateway = gateway;
    write_userdata.request = request;
    write_userdata.response = response;
    write_userdata.external_callback = REQUEST_CALLBACK(on_receive_body);
    write_userdata.external_userdata = REQUEST_CALLBACK_USERDATA(on_receive_body);
    if (!write_userdata.external_callback)
    {
        // No override was provided. Use the default
        write_userdata.external_callback = ja_request_receive_body_to_buffer_callback;
        write_userdata.external_userdata = NULL;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ja_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_userdata);

    // 2-b. Headers Callback - incoming headers
    ja_curl_header_callback_userdata headers_userdata = {0};
    headers_userdata.curl = curl;
    headers_userdata.gateway = gateway;
    headers_userdata.request = request;
    headers_userdata.response = response;
    headers_userdata.external_callback = REQUEST_CALLBACK(on_receive_headers);
    headers_userdata.external_userdata = REQUEST_CALLBACK_USERDATA(on_receive_headers);
    if (!headers_userdata.external_callback)
    {
        // No override was provided. Use the default
        headers_userdata.external_callback = ja_request_receive_headers_to_parsed_callback;
        headers_userdata.external_userdata = NULL;
    }
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, ja_curl_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers_userdata);

    // 2-c. Read Callback - outgoing data
    ja_curl_read_callback_userdata read_userdata = {0};
    read_userdata.curl = curl;
    read_userdata.gateway = gateway;
    read_userdata.request = request;
    read_userdata.external_callback = REQUEST_CALLBACK(on_send_body);
    read_userdata.external_userdata = REQUEST_CALLBACK_USERDATA(on_send_body);
    size_t  *sent_body_data_counter = NULL;
    if (!read_userdata.external_callback)
    {
        // No override was provided. Use the default
        sent_body_data_counter = allocators.calloc(1, sizeof(size_t));
        read_userdata.external_callback = ja_request_send_body_from_buffer_callback;
        // Provide a simple counter so the callback can track send progress.
        read_userdata.external_userdata = sent_body_data_counter;
    }
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, ja_curl_read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &read_userdata);

    // 3. Enable Optional Cookie Engine
    if (gateway->cookiejar_path != NULL)
    {
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, gateway->cookiejar_path);
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, gateway->cookiejar_path);
    }

    // 3. Set CURL Request Parameters
    request_error = set_curl_request_parameters(curl, gateway, request, response);
    if (request_error != ja_request_error_none)
    {
        // Quit immediately on bad parameters.

        // Cleanup immediately
        ja_response_free(response);
        curl_easy_cleanup(curl);

        if (sent_body_data_counter)
        {
            allocators.free(sent_body_data_counter);
        }

        ja_result result = { request_error, NULL };
        return result;
    }

    struct curl_slist* headers = build_curl_request_headers(gateway, request);

    if (headers)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    // 4. Perform CURL request
    CURLcode curl_error = curl_easy_perform(curl);

    // 5. Check for errors
    if (curl_error != CURLE_OK)
    {
        request_error = ja_request_error_failed_to_connect;
    }

    if (request_error == ja_request_error_none)
    {
        // 5. Extract any extra information from request or status
        populate_response_data(curl, request, response);

#if JA_ENABLE_CJSON
        if (request->parse_json_response_body_automatically && ja_response_has_json_body(response))
        {
            ja_response_parse_json_body(response);
        }
#endif

#if JA_ENABLE_RESPONSE_CACHE
        // Insert into cache
        ja_cache_set(gateway, request, response);
#endif
    }
    else
    {
        // Clean up the response. Don't return it if the request failed.
        ja_response_free(response);
        response = NULL;
    }

    if (sent_body_data_counter)
    {
        allocators.free(sent_body_data_counter);
    }

    // 8. Cleanup
    curl_easy_cleanup(curl);

    if (headers)
    {
        curl_slist_free_all(headers);
    }

    ja_result result = { request_error, response };
    return result;
}

ja_request_error validate_request(const ja_gateway *gateway, const ja_request *request)
{
    // Make sure there's a baseURL and it looks reasonable
    if (gateway->base_url == NULL)
    {
        return ja_request_error_invalid_gateway;
    }

    size_t baseUrlLength = strlen(gateway->base_url);
    if (baseUrlLength < 8)
    {
        return ja_request_error_invalid_gateway;
    }

    // Make sure there's a path of some sort
    if (request->path == NULL)
    {
        return ja_request_error_invalid_request;
    }

    return ja_request_error_none;
}

char* build_request_url(const ja_gateway* gateway, const ja_request* request)
{
    char* query_string = build_query_string(gateway, request);

    size_t base_url_length = strlen(gateway->base_url);
    size_t path_length = strlen(request->path);
    size_t query_string_length = query_string ? strlen(query_string) : 0;
    size_t total_url_length = base_url_length + path_length + query_string_length + 1;

    char* request_url = allocators.malloc(total_url_length * sizeof(char));
    snprintf(request_url, total_url_length, "%s%s%s", gateway->base_url, request->path, query_string ? query_string : "");

    if (query_string)
    {
        allocators.free(query_string);
    }

    return request_url;
}

ja_request_error set_curl_request_parameters(CURL *curl, const ja_gateway *gateway, const ja_request *request,
                                             ja_response *response)
{
    // Only allow expected protocols on initial request
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);

    // Only allow expected protocols on any redirects
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, request->follow_redirects ? 1 : 0);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10);

    // Set the method
    switch (request->method)
    {
        case (ja_request_method_get):
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
            break;
        case (ja_request_method_post):
            curl_easy_setopt(curl, CURLOPT_HTTPPOST, NULL);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
            break;
        case (ja_request_method_put):
            curl_easy_setopt(curl, CURLOPT_PUT, 1);
            break;
        case (ja_request_method_delete):
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
    }

    // Build the URL, provide to CURL, store in response.
    {
        char* request_url = build_request_url(gateway, request);

        // The response takes ownership
        response->request_url = request_url;

        // Set CURL option
        curl_easy_setopt(curl, CURLOPT_URL, response->request_url);
    }

    return ja_request_error_none;
}

struct curl_slist * build_curl_request_headers(const ja_gateway *gateway, const ja_request *request)
{
    ja_key_value_pair* default_ptr = gateway->default_headers;
    ja_key_value_pair* request_ptr = request->headers;

    char* header_buffer = NULL;
    size_t header_buffer_length = 0;

    struct curl_slist* headers = NULL;

    while(default_ptr)
    {
        ja_key_value_pair* request_scanner = request->headers;
        bool found_override = false;
        while(request_scanner)
        {
            if (0 == strcmp(default_ptr->key, request_scanner->key))
            {
                found_override = true;
                break;
            }
            request_scanner = request_scanner->next;
        }

        const char* value = found_override ? request_scanner->value : default_ptr->value;
        if (value)
        {
            size_t header_length = strlen(default_ptr->key) + 2 + strlen(value) + 1;
            if (header_buffer_length < header_length)
            {
                header_buffer = allocators.realloc(header_buffer, header_length);
                header_buffer_length = header_length;
            }
            snprintf(header_buffer, header_length, "%s: %s", default_ptr->key, value);
        }
        else
        {
            // NULL value means we should delete it, using CURL's "<key>:" syntax
            size_t header_length = strlen(default_ptr->key) + 1 + 1;
            if (header_buffer_length < header_length)
            {
                header_buffer = allocators.realloc(header_buffer, header_length);
                header_buffer_length = header_length;
            }
            snprintf(header_buffer, header_length, "%s:", default_ptr->key);
        }
        headers = curl_slist_append(headers, header_buffer);
        default_ptr = default_ptr->next;
    }
    while(request_ptr)
    {
        ja_key_value_pair* default_scanner = gateway->default_headers;
        bool found_default = false;
        while (default_scanner)
        {
            if (0 == strcmp(request_ptr->key, default_scanner->key))
            {
                found_default = true;
                break;
            }
            default_scanner = default_scanner->next;
        }
        if (!found_default)
        {
            const char* value = request_ptr->value;
            if (value)
            {
                size_t header_length = strlen(request_ptr->key) + 2 + strlen(value) + 1;
                if (header_buffer_length < header_length)
                {
                    header_buffer = allocators.realloc(header_buffer, header_length);
                    header_buffer_length = header_length;
                }
                snprintf(header_buffer, header_length, "%s: %s", request_ptr->key, value);
            }
            else
            {
                // NULL value means we should delete it, using CURL's "<key>:" syntax
                size_t header_length = strlen(request_ptr->key) + 1 + 1;
                if (header_buffer_length < header_length)
                {
                    header_buffer = allocators.realloc(header_buffer, header_length);
                    header_buffer_length = header_length;
                }
                snprintf(header_buffer, header_length, "%s:", request_ptr->key);
            }
            headers = curl_slist_append(headers, header_buffer);
        }
        request_ptr = request_ptr->next;
    }
    if (header_buffer)
    {
        allocators.free(header_buffer);
    }
    return headers;
}


///
/// Simple URL Encoding from http://www.geekhideout.com/urlcode.shtml
///
/* Converts a hex character to its integer value */
/* Commented out as unused */
/* char from_hex(char ch) {
    return (char) (isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10);
} */

/* Converts an integer value to its hex character*/
char to_hex(char code) {
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(const char *str) {
    const char *pstr = str;
    char* buf = allocators.malloc(strlen(str) * 3 + 1);
    char* pbuf = buf;
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
            *pbuf++ = *pstr;
        else if (*pstr == ' ')
            *pbuf++ = '+';
        else
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & (char)15);
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
/** Commented out as its not currently used. */
/*char *url_decode(const char *str) {
    const char* pstr = str;
    char* buf = allocators.malloc(strlen(str) + 1);
    char* pbuf = buf;
    while (*pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        } else if (*pstr == '+') {
            *pbuf++ = ' ';
        } else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}*/

char* build_query_pair_string(const char *key, const char *value, bool first)
{
    char* escaped_key = url_encode(key);
    char* escaped_value = value ? url_encode(value) : NULL;

    const char* connector = first ? "?" : "&";
    size_t connectorLength = strlen(connector);

    size_t keyLength = escaped_key ? strlen(escaped_key) : 0;
    size_t valueLength = escaped_value ? strlen(escaped_value) : 0;

    size_t totalLength = value ? connectorLength + keyLength + 1 + valueLength + 1 : connectorLength + keyLength + 1;

    char* dest = allocators.malloc(totalLength);
    if (!dest)
    {
        return NULL;
    }

    if (!escaped_value)
    {
        // There is no value. Just write key
        snprintf(dest, totalLength+1, "%s%s", connector, escaped_key);
    }
    else
    {
        // There is a value. Write key=value
        snprintf(dest, totalLength+1, "%s%s=%s", connector, escaped_key, escaped_value);
    }

    if (escaped_key)
    {
        allocators.free(escaped_key);
    }
    if (escaped_value)
    {
        allocators.free(escaped_value);
    }

    return dest;
}

char* build_query_string(const ja_gateway *gateway, const ja_request *request)
{
    if (!gateway->default_query_parameters && !request->query_params)
    {
        return NULL;
    }
    ja_key_value_pair* default_ptr = gateway->default_query_parameters;
    ja_key_value_pair* request_ptr = request->query_params;
    // Iterate through the list that starts with default_ptr
    //  For each item, check if it's in request query params.
    //     If so, write request pair
    //     If not, write default pair
    // Iterate through list that starts with request_ptr
    //  For each item, check if it's in the default query params.
    //     If so, ignore.
    //     If not, write request pair


    ///
    /// Allocate a container in which we can store temporary key-pair strings as they come back
    /// from build_query_pair_string.
    ///
    /// We at memory usage of 2x query length * sizeof(char) + max_total_query_pairs*sizeof(char*)
    ///
    /// We could instead realloc our dest for each segment, to minimize peak memory usage, but
    /// fragmentation or waste would be even higher.
    size_t max_total_query_pairs = 0;
    ja_key_value_pair* default_scanner = default_ptr;
    while(default_scanner)
    {
        max_total_query_pairs += 1;
        default_scanner = default_scanner->next;
    }
    ja_key_value_pair* request_scanner = request_ptr;
    while (request_scanner)
    {
        max_total_query_pairs += 1;
        request_scanner = request_scanner->next;
    }
    char ** query_pair_strings = allocators.calloc(max_total_query_pairs, sizeof(char*));
    long actual_total_query_pairs = 0;

    bool first = true;
    while (default_ptr)
    {
        request_scanner = request->query_params;
        bool found_override = false;
        while (request_scanner)
        {
            if (0 == strcmp(request_scanner->key, default_ptr->key))
            {
                found_override = true;
                break;
            }
            request_scanner = request_scanner->next;
        }
        const char* value = found_override ? request_scanner->value : default_ptr->value;

        char* query_pair_string = build_query_pair_string(default_ptr->key, value, first);
        first = false;

        if (query_pair_string != NULL)
        {
            query_pair_strings[actual_total_query_pairs] = query_pair_string;
            actual_total_query_pairs += 1;
        }

        default_ptr = default_ptr->next;
    }

    while (request_ptr)
    {
        default_scanner = gateway->default_query_parameters;
        bool found_default = false;
        while (default_scanner)
        {
            if (0 == strcmp(default_scanner->key, request_ptr->key))
            {
                found_default = true;
                break;
            }
            default_scanner = default_scanner->next;
        }
        if (!found_default)
        {
            // This key is unique to the request, which means it wasn't yet written. Do so now.
            char* query_pair_string = build_query_pair_string(request_ptr->key,
                                                              request_ptr->value, first);
            first = false;
            if (NULL != query_pair_string)
            {
                query_pair_strings[actual_total_query_pairs] = query_pair_string;
                actual_total_query_pairs += 1;
            }
        }
        request_ptr = request_ptr->next;
    }

    // Now build the string
    size_t total_string_length = 0;
    for (int i=0; i < actual_total_query_pairs; ++i)
    {
        total_string_length += strlen(query_pair_strings[i]);
    }

    char* query_string = allocators.malloc(total_string_length + 1);
    query_string[0] = '\0';
    for (int i=0; i < actual_total_query_pairs; ++i) {
        strncat(query_string, query_pair_strings[i], strlen(query_pair_strings[i]));
        allocators.free(query_pair_strings[i]);
        query_pair_strings[i] = NULL;
    }
    allocators.free(query_pair_strings);
    return query_string;
}


void populate_response_data(CURL *curl, const ja_request *request, ja_response *response)
{
    // Get status code - CURLINFO_RESPONSE_CODE
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);

    // Get Final URL - CURLINFO_EFFECTIVE_URL
    char* effective_url;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);

    response->resolved_url = effective_url ? allocators.strdup(effective_url) : NULL;
}

/// Appends an new ja_key_value_pair to the library-managed list, creating head if head is NULL. Returns the head.
ja_key_value_pair* ja_key_value_list_append(ja_key_value_pair* head, const char* key, const char* value)
{
    ja_key_value_pair* item = allocators.malloc(sizeof(ja_key_value_pair));
    item->key = key ? allocators.strdup(key) : NULL;
    item->value = value ? allocators.strdup(value) : NULL;
    item->next = NULL;

    if (head)
    {
        head->next = item;
    }
    else
    {
        head = item;
    }
    return head;
}

void ja_key_value_list_free(ja_key_value_pair* head)
{
    ja_key_value_pair* next = NULL;
    while (head)
    {
        next = head->next;
        if (head->key)
        {
            allocators.free(head->key);
        }
        if (head->value)
        {
            allocators.free(head->value);
        }
        allocators.free(head);
        head = next;
    }
}

ja_simple_buffer* ja_simple_buffer_append(ja_simple_buffer* buffer, const char* data, size_t length)
{
    if (data == NULL || length == 0)
    {
        return buffer;
    }
    if (!buffer)
    {
        buffer = allocators.calloc(1, sizeof(ja_simple_buffer));
    }
    if (!buffer->data)
    {
        // Allocate storage for all data, plus a convenient null terminator
        buffer->data = allocators.malloc((length+1) * sizeof(char));
    }
    else
    {
        // Allocate storage for all data, plus a convenient null terminator
        buffer->data = allocators.realloc(buffer->data, (buffer->data_length + length + 1) * sizeof(char));
    }
    memcpy(buffer->data + buffer->data_length, data, length);
    buffer->data_length += length;

    // Always null terminate, for convenience. The first data_length bytes are raw storage, as provided.
    buffer->data[buffer->data_length] = '\0';

    return buffer;
}

void ja_simple_buffer_free(ja_simple_buffer* buffer)
{
    if (buffer)
    {
        if (buffer->data)
        {
            allocators.free(buffer->data);
        }
        allocators.free(buffer);
    }
}

ja_response* ja_init_response()
{
    ja_response* response = allocators.calloc(1, sizeof(ja_response));

    return response;
}

void ja_response_free(ja_response *response)
{
#if JA_ENABLE_RESPONSE_CACHE
    if (response->cache_references > 0)
    {
        return;
    }
#endif

    if (response->request_url)
    {
        allocators.free(response->request_url);
    }
    if (response->resolved_url)
    {
        allocators.free(response->resolved_url);
    }
    if (response->body_type == ja_response_body_type_buffer && response->body_data.buffer)
    {
        ja_simple_buffer_free(response->body_data.buffer);
    }
#if JA_ENABLE_CJSON
    else if (response->body_type == ja_response_body_type_json && response->body_data.json)
    {
        allocators.free(response->body_data.json);
    }
#endif

    else if (response->header_type == ja_response_header_type_parsed && response->header_data.parsed)
    {
        ja_key_value_list_free(response->header_data.parsed);
    }

    allocators.free(response);
}

ja_request* ja_request_init(ja_request_method method, const char *path)
{
    ja_request* request = allocators.calloc(1, sizeof(ja_request));

    request->method = method;
    request->path = allocators.strdup(path);

    return request;
}

void ja_request_free(ja_request *request)
{
    if (request->path)
    {
        allocators.free(request->path);
    }
    if (request->headers)
    {
        ja_key_value_list_free(request->headers);
    }
    if (request->query_params)
    {
        ja_key_value_list_free(request->query_params);
    }
    if (request->body_type == ja_request_body_type_buffer && request->body_data.buffer)
    {
        ja_simple_buffer_free(request->body_data.buffer);
    }

    allocators.free(request);
}

ja_gateway* ja_gateway_init(const char *base_url, const char *cookiejar_path)
{
    ja_gateway* gateway = allocators.calloc(1, sizeof(ja_gateway));

    gateway->base_url = allocators.strdup(base_url);
    gateway->cookiejar_path = cookiejar_path ? allocators.strdup(cookiejar_path) : NULL;

    return gateway;
}

void ja_gateway_free(ja_gateway *gateway)
{
    if (gateway->default_headers)
    {
        ja_key_value_list_free(gateway->default_headers);
    }
    if (gateway->default_query_parameters)
    {
        ja_key_value_list_free(gateway->default_query_parameters);
    }
    if (gateway->cookiejar_path)
    {
        allocators.free(gateway->cookiejar_path);
    }
    if (gateway->cache)
    {
        ja_free_basic_cache(gateway->cache);
    }
    allocators.free(gateway->base_url);
    allocators.free(gateway);
}

/**
 * Called by CURL. Dispatches to our caller using APRequestCallback signature and marshalling.
 */
size_t ja_curl_write_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    ja_curl_write_callback_userdata* cd = (ja_curl_write_callback_userdata*)userdata;

    // Extract any extra information from request or status
    populate_response_data(cd->curl, cd->request, cd->response);

    return cd->external_callback
           ? cd->external_callback(cd->gateway, cd->request, cd->response, cd->external_userdata, buffer, size*nitems)
           : size*nitems; // Do nothing, but signal to CURL that we're okay with that
}

/**
 * Called by CURL. Dispatches to our caller using APRequestCallback signature and marshalling.
 */
size_t ja_curl_read_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    ja_curl_read_callback_userdata* cd = (ja_curl_read_callback_userdata*)userdata;

    return cd->external_callback
           ? cd->external_callback(cd->gateway, cd->request, cd->external_userdata, buffer, size*nitems)
           : size*nitems; // Do nothing, but signal to CURL that we're okay with that.
}

/**
 * Called by CURL. Dispatches to our caller using APRequestCallback signature and marshalling.
 */
size_t ja_curl_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    ja_curl_header_callback_userdata* cd = (ja_curl_header_callback_userdata*)userdata;

    // Extract any extra information from request or status
    populate_response_data(cd->curl, cd->request, cd->response);

    return cd->external_callback
           ? cd->external_callback(cd->gateway, cd->request, cd->response, cd->external_userdata, buffer, size*nitems)
           : size*nitems; // Do nothing, but signal to CURL that we're okay with that.
}

void ja_request_set_body(ja_request *request, char *data, size_t data_length)
{
    if (request->body_type == ja_request_body_type_buffer && request->body_data.buffer)
    {
        // Free what's already there
        ja_simple_buffer_free(request->body_data.buffer);
        request->body_type = ja_request_body_type_userdata;
        request->body_data.userdata = NULL;
        ja_request_set_header(request, "Content-Length", NULL);
    }

    if (data)
    {
        request->body_type = ja_request_body_type_buffer;
        request->body_data.buffer = ja_simple_buffer_append(request->body_data.buffer, data, data_length);
        char contentLengthString[data_length / 10 + 2];
        snprintf(contentLengthString, data_length / 10 + 2, "%ld", data_length);
        ja_request_set_header(request, "Content-Length", contentLengthString);
    }
}

#if JA_ENABLE_RESPONSE_CACHE

void ja_gateway_enable_cache(ja_gateway* gateway, size_t max_cached_responses)
{
    gateway->cache = ja_init_basic_cache(max_cached_responses);
}

ja_cache* ja_init_basic_cache(size_t max_cached_responses)
{
    ja_cache* cache = allocators.calloc(1, sizeof(ja_cache));
    cache->max_cached_responses = max_cached_responses;
    return cache;
}

void ja_free_basic_cache(ja_cache* cache)
{
    while (cache->cache_item)
    {
        ja_remove_cache_item(cache, cache->cache_item);
    }
    allocators.free(cache);
}

void ja_remove_cache_item(ja_cache* cache, ja_cache_item* item)
{
    // Splice the list
    if (item->previous)
    {
        item->previous->next = item->next;
    }
    if (item->next)
    {
        item->next->previous = item->previous;
    }

    // Decrement the cache reference count. If it's now zero, clean up!
    item->response->cache_references -= 1;
    if (0 == item->response->cache_references)
    {
        ja_response_free(item->response);
    }

    // If the item was the head, now the next item will be.
    if (item == cache->cache_item)
    {
        cache->cache_item = item->next;
    }

    allocators.free(item);
    cache->current_cached_responses -= 1;
}

void ja_add_cache_item(ja_cache* cache, long cache_identifier, time_t expires_at, ja_response* response)
{
    if (cache->current_cached_responses >= cache->max_cached_responses)
    {
        // See if we can find and remove an expired cache item. Otherwise, purge tail (oldest insert)
        ja_cache_item* purge_candidate = cache->cache_item;
        while (purge_candidate)
        {
            if (purge_candidate->expires_at < time(NULL))
            {
                ja_remove_cache_item(cache, purge_candidate);
                break;
            }
            else if (NULL == purge_candidate->next)
            {
                // Hit the tail
                ja_remove_cache_item(cache, purge_candidate);
            }
            purge_candidate = cache->cache_item->next;
        }
    }

    if (cache->current_cached_responses >= cache->max_cached_responses)
    {
        // If there's still no room, that means max_cached_responses is actually 0!
        return;
    }

    // Create item
    ja_cache_item* item = allocators.calloc(1, sizeof(ja_cache_item));
    item->cache_identifier = cache_identifier;
    item->expires_at = expires_at;
    item->response = response;
    item->response->cache_references += 1;

    // Insert at front of list
    item->next = cache->cache_item;
    if (cache->cache_item)
    {
        cache->cache_item->previous = item;
    }
    cache->cache_item = item;
    cache->current_cached_responses += 1;
}

ja_response* ja_cache_get(struct ja_gateway_t* gateway, const ja_request* request)
{
    if (!request->allow_cached_response || request->method != ja_request_method_get)
    {
        return NULL;
    }
    ja_cache* cache = gateway->cache;

    long cache_identifier = ja_generate_cache_identifier(gateway, request);
    ja_cache_item* candidate = cache->cache_item;

    while (candidate)
    {
        ja_cache_item* next = candidate->next;
        if (candidate->expires_at < time(NULL))
        {
            // Clean up expired entries as we see them
            ja_remove_cache_item(cache, candidate);
            candidate = next;
            continue;
        }
        if (candidate->cache_identifier == cache_identifier)
        {
            candidate->response->cache_references += 1;
            return candidate->response;
        }
        candidate = next;
    }
    return NULL;
}

/// Save a response to the cache
void ja_cache_set(struct ja_gateway_t* gateway, const ja_request* request, ja_response* response)
{
    if (request->cache_response_with_expiration == 0 || request->method != ja_request_method_get)
    {
        // Do not cache.
        return;
    }

    ja_cache* cache = gateway->cache;

    long cache_identifier = ja_generate_cache_identifier(gateway, request);
    time_t expires_at = time(NULL) + request->cache_response_with_expiration;

    ja_add_cache_item(cache, cache_identifier, expires_at, response);
}

// Generate an intenger identifier from a request (or extract the custom one)
unsigned long ja_generate_cache_identifier(const ja_gateway* gateway, const ja_request* request)
{
    if (request->custom_cache_identifier != 0)
    {
        return request->custom_cache_identifier;
    }

    // djb2 quick low-collision string hash from http://www.cse.yorku.ca/~oz/hash.html
    unsigned long hash = 5381;
    char* url = build_request_url(gateway, request);
    char* purl = url;
    int character = *purl++;

    while (character)
    {
        hash = ((hash << 5) + hash) + character; /* hash * 33 + c */
        character = *purl++;
    }

    allocators.free(url);

    return hash;
}

#endif

void ja_request_set_header(ja_request* request, const char* key, const char* value)
{
    ja_key_value_pair* previous = NULL;
    ja_key_value_pair* item = request->headers;
    while (item)
    {
        if (0 == strcmp(key, item->key))
        {
            // The header with this key already exists. Update it.
            if (value)
            {
                allocators.free(item->value);
                item->value = allocators.strdup(value);
            }
            else
            {
                if (previous)
                {
                    // Splice this by attaching the next item to the previous
                    previous->next = item->next;
                }
                else
                {
                    // Splice this by assigning the next item as the head
                    request->headers = item->next;
                }
                // Remove this item from the list entirely
                item->next = NULL;
                ja_key_value_list_free(item);
            }
            // Did what was needed. Now return.
            return;
        }
        previous = item;
        item = item->next;
    }

    // We didn't find a match among existing headers. Add it
    if (value)
    {
        request->headers = ja_key_value_list_append(request->headers, key, value);
    }
}

const char* ja_request_get_header(const ja_request* request, const char* key)
{
    ja_key_value_pair* item = request->headers;
    while (item)
    {
        if (0 == strcmp(key, item->key))
        {
            return item->value;
        }
        item = item->next;
    }
    return NULL;
}

void ja_request_set_query_parameter(ja_request* request, const char* key, const char* value)
{
    ja_key_value_pair *item = request->query_params;
    while (item) {
        if (0 == strcmp(key, item->key)) {
            allocators.free(item->value);
            item->value = value ? allocators.strdup(value) : NULL;
            return;
        }
        item = item->next;
    }

    // We didn't find a match among existing query_params. Add it
    request->query_params = ja_key_value_list_append(request->query_params, key, value);
}

void ja_response_set_parsed_header(ja_response* response, const char* key, const char* value)
{
    response->header_type = ja_response_header_type_parsed;

    response->header_data.parsed = ja_key_value_list_append(response->header_data.parsed, key, value);
}

const char* ja_response_get_parsed_header(const ja_response* response, const char* key)
{
    if (response->header_type != ja_response_header_type_parsed)
    {
        return NULL;
    }

    ja_key_value_pair *item = response->header_data.parsed;
    while (item)
    {
        if (0 == strcmp(key, item->key))
        {
            return item->value;
        }
        item = item->next;
    }
    return NULL;
}

#if JA_ENABLE_CJSON

bool ja_response_has_json_body(const ja_response* response)
{
    if (response->body_type == ja_response_body_type_json)
    {
        // It has a json body and it's already been parsed.
        return true;
    }
    if (response->body_type == ja_response_body_type_buffer)
    {
        // It has a body data buffer, check for headers that indicate
        const char* contentType = ja_response_get_parsed_header(response, "Content-Type");
        if (contentType && response->body_data.buffer->data && response->body_data.buffer->data_length > 0
            && (0 == strcmp(contentType, "application/json") || 0 == strcmp(contentType, "text/json")))
        {
            return true;
        }
    }
    return false;
}

void ja_response_parse_json_body(ja_response* response)
{
    if (response->body_type == ja_response_body_type_json)
    {
        // It has a json body and it's already been parsed.
        return;
    }
    if (response->body_type == ja_response_body_type_buffer)
    {
        // It has a body data buffer, check for headers that indicate
        const char *contentType = ja_response_get_parsed_header(response, "Content-Type");
        if (contentType && response->body_data.buffer->data && response->body_data.buffer->data_length > 0
            && (0 == strcmp(contentType, "application/json") || 0 == strcmp(contentType, "text/json")))
        {
            cJSON* json = cJSON_Parse(response->body_data.buffer->data);
            if (json)
            {
                ja_simple_buffer_free(response->body_data.buffer);
                response->body_type = ja_response_body_type_json;
                response->body_data.json = json;
            }
        }
    }
}

void ja_request_set_body_json(ja_request *request, const char *contentType, cJSON *json)
{
    if (json)
    {
        char* data = cJSON_PrintUnformatted(json);
        if (data)
        {
            size_t data_length = strlen(data);
            ja_request_set_body(request, data, data_length);
            allocators.free(data);
        }
        if (contentType)
        {
            ja_request_set_header(request, "Content-Type", contentType);
        }
    }
    else
    {
        ja_request_set_body(request, NULL, 0);
    }
}

#endif

/// Automatically sends the data in request->body_data->buffer with the request [Default]
size_t ja_request_send_body_from_buffer_callback(
        const struct ja_gateway_t* gateway,
        const ja_request* request,
        void* userdata,
        char* send_buffer,
        size_t send_buffer_size
)
{
    if (request->body_type != ja_request_body_type_buffer
        || !request->body_data.buffer
        || !request->body_data.buffer->data)
    {
        // Nothing to send
        return 0;
    }
    size_t* sent_data_ptr = (size_t*)userdata;

    size_t remaining_data = request->body_data.buffer->data_length - *sent_data_ptr;
    size_t sent_data = send_buffer_size < remaining_data ? send_buffer_size : remaining_data;

    // Copy next sendBufferSize data from request->body_data.buffer into sendBuffer
    memcpy(send_buffer, request->body_data.buffer->data + *sent_data_ptr, sent_data);

    /// !!! We need to track how much of the buffer was already sent
    *sent_data_ptr += sent_data;
    return sent_data;
}

/// Automatically saves received body data to response->body_data->buffer [Default if JA_ENABLE_CJSON is unset]
size_t ja_request_receive_body_to_buffer_callback(
        const struct ja_gateway_t* gateway,
        const ja_request* request,
        ja_response* response,
        void* userdata,
        const char* received_data,
        size_t received_data_size)
{
    response->body_type = ja_response_body_type_buffer;

    response->body_data.buffer = ja_simple_buffer_append(response->body_data.buffer, received_data, received_data_size);
    return received_data_size;
}

/// Parses received headers to response->header_data->parsed as ja_key_value_pair items [Default]
size_t ja_request_receive_headers_to_parsed_callback(
        const struct ja_gateway_t* gateway,
        const ja_request* request,
        ja_response* response,
        void* userdata,
        const char* received_headers_data,
        size_t received_headers_data_size)
{
    // Each call contains a complete header, which may not be zero terminated.
    //  Key = received_headers_data, up to and excluding first :
    //  Value = received_headers_data, from first character after first : onwards

    const char* start_of_key = received_headers_data;
    size_t length_of_key = 0;

    const char* first_colon = start_of_key;
    const char* end_of_header = received_headers_data + received_headers_data_size;
    while (first_colon < end_of_header)
    {
        if (*first_colon == ':')
        {
            length_of_key = first_colon - start_of_key;
            break;
        }
        first_colon +=1;
    }
    if (length_of_key == 0)
    {
        // No key!
        return received_headers_data_size;
    }

    const char* start_of_value = first_colon + 1;
    size_t length_of_value = 0;

    // Trim leading whitespace
    while (start_of_value < end_of_header && (*start_of_value == ' ' || *start_of_value == '\t'))
    {
        start_of_value += 1;
    }
    if (start_of_value < end_of_header)
    {
        length_of_value = end_of_header - start_of_value;

        // Trim trailing whitespace
        const char* end_of_value = start_of_value + length_of_value - 1;
        while (length_of_value > 0 &&
                (*end_of_value == ' '
                  || *end_of_value == '\t'
                  || *end_of_value == '\r'
                  || *end_of_value == '\n') )
        {
            length_of_value -= 1;
            end_of_value -= 1;
        }
    }

    char* key = allocators.calloc(length_of_key + 1, sizeof(char));
    char* value = allocators.calloc(length_of_value + 1, sizeof(char));
    memcpy(key, start_of_key, length_of_key);
    memcpy(value, start_of_value, length_of_value);

    ja_response_set_parsed_header(response, key, value);

    allocators.free(key);
    allocators.free(value);

    return received_headers_data_size;
}




