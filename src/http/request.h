/**
 * @file    request.h
 * @author  Samandar Komil
 * @date    24 April 2025
 * @brief   HTTPRequest struct and method prototypes.
 *
 */

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include "common.h"

typedef struct HTTPRequestLine
{
    char *method;
    char *uri;
    char *protocol;
    size_t method_len;
    size_t uri_len;
    size_t protocol_len;
} HTTPRequestLine;

typedef struct HTTPHeader
{
    char *name;
    char *value;
    size_t name_len;
    size_t value_len;
} HTTPHeader;

typedef struct HTTPRequest
{
    HTTPRequestLine request_line;
    HTTPHeader *headers;
    int header_count;
    char *body;
    size_t body_len;
    HTTPRequestState state;
} HTTPRequest;

HTTPRequest *create_http_request();
void free_http_request(HTTPRequest *req);

#endif