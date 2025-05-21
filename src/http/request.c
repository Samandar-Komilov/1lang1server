/**
 * @file    request.c
 * @author  Samandar Komil
 * @date    24 April 2025
 * @brief   HTTPRequest struct and method implementations.
 *
 */

#include "request.h"

HTTPRequest *create_http_request(void)
{
    HTTPRequest *req = calloc(1, sizeof(HTTPRequest));

    req->request_line.method       = NULL;
    req->request_line.uri          = NULL;
    req->request_line.protocol     = NULL;
    req->request_line.method_len   = 0;
    req->request_line.uri_len      = 0;
    req->request_line.protocol_len = 0;

    req->headers      = calloc(MAX_HEADERS, sizeof(HTTPHeader));
    req->header_count = 0;

    req->body     = NULL;
    req->body_len = 0;

    req->state = REQ_PARSE_LINE;

    return req;
}

void free_http_request(HTTPRequest *req)
{
    free(req->headers);
    free(req);
}