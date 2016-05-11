#include "test_request_handlers.h"
#include <string.h>

int CivetHandler_EchoUri(struct mg_connection* conn, void* cbdata)
{
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    mg_printf(conn, "%s", req_info->request_uri);
    return 1;
}

int CivetHandler_EchoBody(struct mg_connection* conn, void* cbdata)
{
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", cbdata ? (char*)cbdata : "text/html");
    char buf[4096];

    int ret = mg_read(conn, buf, sizeof(buf));
    while (ret > 0) {
        mg_write(conn, buf, ret);
        ret = mg_read(conn, buf, sizeof(buf));
    }
    return 1;
}

int CivetHandler_EchoQueryString(struct mg_connection* conn, void* cbdata)
{
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    mg_printf(conn, "%s", req_info->query_string);
    return 1;
}

int CivetHandler_EchoSpecificHeaderValue(struct mg_connection* conn, void* cbdata)
{
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    for (int i=0; i < req_info->num_headers; ++i)
    {
        if (0 == strcmp(req_info->http_headers[i].name, (const char*)cbdata))
        {
            mg_printf(conn, "%s", req_info->http_headers[i].value);
            break;
        }
    }
    return 1;
}

int CivetHandler_EchoSpecificQueryValue(struct mg_connection* conn, void* cbdata)
{
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");

    char dest[4096];
    if (1 <= mg_get_var(req_info->query_string, strlen(req_info->query_string), (const char*)cbdata, dest, sizeof(dest)))
    {
        mg_printf(conn, "%s", dest);
    }

    return 1;
}

int CivetHandler_SetHeader(struct mg_connection* conn, void* cbdata)
{
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
    mg_printf(conn, "%s\r\n\r\n", cbdata);

    return 1;
}
