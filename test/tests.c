#include <stdio.h>
#include "../include/justapis.h"
#include "civetweb/civetweb.h"
#include "test_request_handlers.h"
#include "test_helpers.h"
#include "cheat/cheat.h"
#include "cheat/cheats.h"

CHEAT_DECLARE(
        struct mg_callbacks mg_callbacks = {0};
        struct mg_context* ctx = NULL;

        const char *options[] = {"document_root",
                                 "",
                                 "listening_ports",
                                 "31337",
                                 "request_timeout_ms",
                                 "10000",
                                 "error_log_file",
                                 "error.log",
                                 0};

        ja_gateway* gateway;
        char cookiejar_path[L_tmpnam];
)

CHEAT_SET_UP(
        // Make sure everything is reset
        memset(cookiejar_path, 0, sizeof(cookiejar_path));

        // Point the gateway to the Civet server and give it a cookiejar to store cookies

        snprintf(cookiejar_path, L_tmpnam, "%scookierjar.txt-XXXXXX", P_tmpdir);
        mkstemp(cookiejar_path);

        gateway = ja_gateway_init("http://localhost:31337", cookiejar_path);

        ctx = mg_start(&mg_callbacks, 0, options);
)

CHEAT_TEAR_DOWN(
        mg_stop(ctx);
        ctx = NULL;
        ja_gateway_free(gateway);
        remove(cookiejar_path);

        memset(&mg_callbacks, 0, sizeof(struct mg_callbacks));
)

CHEAT_TEST(cheat_works,
           cheat_assert(true);
)

CHEAT_TEST(civetweb_works,
           mg_set_request_handler(ctx, "/", CivetHandler_EchoUri, NULL);
)

CHEAT_TEST(path_concat_works,
           ja_request* request = ja_request_init(ja_request_method_get, "/test");


           mg_set_request_handler(ctx, "**", CivetHandler_EchoUri, NULL);

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "/test");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(single_request_query_params_work,

           ja_request* request = ja_request_init(ja_request_method_get, "/");

           request->query_params = ja_key_value_list_append(NULL, "a", "foobar");

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificQueryValue, "a");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "foobar");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(multiple_request_query_params_work,

           ja_request* request = ja_request_init(ja_request_method_get, "/");

           ja_key_value_pair* params = ja_key_value_list_append(NULL, "a", "foobar");
           params = ja_key_value_list_append(params, "b", "barfood");

           request->query_params = params;

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificQueryValue, "b");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "barfood");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(null_query_param_values_work,
           ja_request* request = ja_request_init(ja_request_method_get, "/");

           request->query_params = ja_key_value_list_append(NULL, "a", NULL);

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificQueryValue, "a");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_userdata);
           cheat_assert_pointer(result.response->body_data.buffer, NULL);

                   ja_response_free(result.response);
                   ja_request_free(request);
)


CHEAT_TEST(default_query_params_work,

           ja_request* request = ja_request_init(ja_request_method_get, "/");

           gateway->default_query_parameters = ja_key_value_list_append(NULL, "a", "foobar");

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificQueryValue, "a");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "foobar");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(default_query_param_override_works,

           ja_request* request = ja_request_init(ja_request_method_get, "/");

           gateway->default_query_parameters = ja_key_value_list_append(NULL, "a", "foobar");
           request->query_params = ja_key_value_list_append(NULL, "a", "barfood");

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificQueryValue, "a");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "barfood");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(single_request_headers_work,
           ja_request* request = ja_request_init(ja_request_method_get, "/");

           request->headers = ja_key_value_list_append(NULL, "a", "foobar");

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificHeaderValue, "a");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "foobar");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(multiple_request_headers_work,
           ja_request* request = ja_request_init(ja_request_method_get, "/");

           request->headers = ja_key_value_list_append(NULL, "a", "foobar");
           request->headers = ja_key_value_list_append(request->headers, "b", "barfood");

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificHeaderValue, "b");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "barfood");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(default_headers_work,
           ja_request* request = ja_request_init(ja_request_method_get, "/");

           gateway->default_headers = ja_key_value_list_append(NULL, "a", "foobar");

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificHeaderValue, "a");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "foobar");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(default_header_override_works,
           ja_request* request = ja_request_init(ja_request_method_get, "/");

           gateway->default_headers = ja_key_value_list_append(NULL, "a", "foobar");
           request->headers = ja_key_value_list_append(NULL, "a", "barfood");

           mg_set_request_handler(ctx, "**", CivetHandler_EchoSpecificHeaderValue, "a");

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result.response->body_data.buffer->data, "barfood");

                   ja_response_free(result.response);
                   ja_request_free(request);
)

CHEAT_TEST(cookie_persists_with_cookiejar,
           mg_set_request_handler(ctx, "/set-header", CivetHandler_SetHeader, "Set-Cookie: foo=bar");
           mg_set_request_handler(ctx, "/get-header", CivetHandler_EchoSpecificHeaderValue, "Cookie");

           ja_request* request1 = ja_request_init(ja_request_method_get, "/set-header");
           ja_request* request2 = ja_request_init(ja_request_method_get, "/get-header");

           ja_result result1 = ja_perform_request(gateway, request1, NULL);
           ja_result result2 = ja_perform_request(gateway, request2, NULL);

           cheat_assert_int(result1.error, ja_request_error_none);
           cheat_assert_int(result2.error, ja_request_error_none);
           cheat_assert_int(result2.response->body_type, ja_response_body_type_buffer);
           cheat_assert_string(result2.response->body_data.buffer->data, "foo=bar");

                   ja_response_free(result1.response);
                   ja_response_free(result2.response);
                   ja_request_free(request1);
                   ja_request_free(request2);
)

CHEAT_TEST(repeat_get_hits_cache,
           mg_set_request_handler(ctx, "**", CivetHandler_EchoUri, NULL);

           ja_gateway_enable_cache(gateway, 10);

           ja_request* request = ja_request_init(ja_request_method_get, "/test");
           request->allow_cached_response = true;
           request->cache_response_with_expiration = 1000;

           ja_result result1 = ja_perform_request(gateway, request, NULL);
           ja_result result2 = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result1.error, ja_request_error_none);
           cheat_assert_int(result2.error, ja_request_error_none);
           cheat_assert_not_pointer(result2.response, NULL);
           cheat_assert(result2.response->cache_references > 0);

                   ja_response_free(result1.response);
                   ja_response_free(result2.response);
                   ja_request_free(request);
)

CHEAT_TEST(repeat_post_ignores_cache,
           mg_set_request_handler(ctx, "**", CivetHandler_EchoUri, NULL);

           ja_gateway_enable_cache(gateway, 10);

           ja_request* request = ja_request_init(ja_request_method_post, "/test");
           request->allow_cached_response = true;
           request->cache_response_with_expiration = 1000;

           ja_result result1 = ja_perform_request(gateway, request, NULL);
           ja_result result2 = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result1.error, ja_request_error_none);
           cheat_assert_int(result2.error, ja_request_error_none);
           cheat_assert_not_pointer(result2.response, NULL);
           cheat_assert_int(result1.response->cache_references, 0);
           cheat_assert_int(result2.response->cache_references, 0);

                   ja_response_free(result1.response);
                   ja_response_free(result2.response);
                   ja_request_free(request);
)

CHEAT_TEST(distinct_get_ignores_cache,
           mg_set_request_handler(ctx, "**", CivetHandler_EchoUri, NULL);

           ja_gateway_enable_cache(gateway, 10);

           ja_request* request1 = ja_request_init(ja_request_method_get, "/test");
           request1->allow_cached_response = true;
           request1->cache_response_with_expiration = 1000;

           ja_request* request2 = ja_request_init(ja_request_method_get, "/different-est");
           request2->allow_cached_response = true;
           request2->cache_response_with_expiration = 1000;

           ja_result result1 = ja_perform_request(gateway, request1, NULL);
           ja_result result2 = ja_perform_request(gateway, request2, NULL);

           cheat_assert_int(result1.error, ja_request_error_none);
           cheat_assert_int(result2.error, ja_request_error_none);
           cheat_assert_not_pointer(result2.response, NULL);
           cheat_assert_not_pointer(result1.response, result2.response);

                   ja_response_free(result1.response);
                   ja_response_free(result2.response);
                   ja_request_free(request1);
                   ja_request_free(request2);
)

CHEAT_TEST(send_json_body_works,
           mg_set_request_handler(ctx, "**", CivetHandler_EchoBody, NULL);

           ja_request* request = ja_request_init(ja_request_method_post, "/test");
           cJSON* json = cJSON_CreateObject();
           cJSON_AddBoolToObject(json, "test_bool", true);
           cJSON_AddNumberToObject(json, "test_number", 4);
           cJSON_AddStringToObject(json, "test_string", "test!");
                   cheat_assert_not_pointer(json, NULL);


                   ja_request_set_body_json(request, "application/json", json);

                   const char* expected_json = "{\"test_bool\":true,\"test_number\":4,\"test_string\":\"test!\"}";

           cheat_assert_int(request->body_type, ja_request_body_type_buffer);
           cheat_assert_not_pointer(request->body_data.buffer, NULL);
           cheat_assert_not_pointer(request->body_data.buffer->data, NULL);
           cheat_assert_string(request->body_data.buffer->data, expected_json);

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);

           // We did NOT set parse_json_response_body_automatically, so it should be a buffer!
           cheat_assert_int(result.response->body_type, ja_response_body_type_buffer);
           cheat_assert_not_pointer(result.response->body_data.buffer, NULL);
           cheat_assert_string(result.response->body_data.buffer->data, expected_json);

                   ja_response_free(result.response);
                   ja_request_free(request);
           cJSON_Delete(json);
)

CHEAT_TEST(receive_json_body_works,
           mg_set_request_handler(ctx, "**", CivetHandler_EchoBody, "application/json");

           ja_request* request = ja_request_init(ja_request_method_post, "/test");
           request->parse_json_response_body_automatically = true;
           cJSON* json = cJSON_CreateObject();
           cJSON_AddBoolToObject(json, "test_bool", true);
           cJSON_AddNumberToObject(json, "test_number", 4);
           cJSON_AddStringToObject(json, "test_string", "test!");
           cheat_assert_not_pointer(json, NULL);

                   ja_request_set_body_json(request, "application/json", json);

           const char* expected_json = "{\"test_bool\":true,\"test_number\":4,\"test_string\":\"test!\"}";

           cheat_assert_int(request->body_type, ja_request_body_type_buffer);
           cheat_assert_not_pointer(request->body_data.buffer, NULL);
           cheat_assert_not_pointer(request->body_data.buffer->data, NULL);
           cheat_assert_string(request->body_data.buffer->data, expected_json);

           ja_result result = ja_perform_request(gateway, request, NULL);

           cheat_assert_int(result.error, ja_request_error_none);
           cheat_assert_not_pointer(result.response, NULL);

           // We DID set parse_json_response_body_automatically, so it should be json!
           cheat_assert_int(result.response->body_type, ja_response_body_type_json);
           cheat_assert_not_pointer(result.response->body_data.json, NULL);

           cJSON* responseJson = result.response->body_data.json;
           cheat_assert_int(cJSON_GetObjectItem(responseJson, "test_bool")->valueint, true);
           cheat_assert_int(cJSON_GetObjectItem(responseJson,"test_number")->valueint, 4);
           cheat_assert_string(cJSON_GetObjectItem(responseJson,"test_string")->valuestring, "test!");

           cJSON_Delete(json);
                   ja_request_free(request);
                   ja_response_free(result.response);
)