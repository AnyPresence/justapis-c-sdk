//
// Created by Andrew Palumbo on 4/14/16.
//

#ifndef LIBJUSTAPIS_TEST_REQUEST_HANDLERS_H
#define LIBJUSTAPIS_TEST_REQUEST_HANDLERS_H

#include "civetweb/civetweb.h"

int CivetHandler_EchoUri(struct mg_connection* conn, void* cbdata);
int CivetHandler_EchoBody(struct mg_connection* conn, void* cbdata);
int CivetHandler_EchoSpecificHeaderValue(struct mg_connection* conn, void* cbdata);
int CivetHandler_EchoSpecificQueryValue(struct mg_connection* conn, void* cbdata);
int CivetHandler_EchoQueryString(struct mg_connection* conn, void* cbdata);
int CivetHandler_SetHeader(struct mg_connection* conn, void* cbdata);

#endif //LIBJUSTAPIS_TEST_REQUEST_HANDLERS_H
