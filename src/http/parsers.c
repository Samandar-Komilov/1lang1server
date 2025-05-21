/**
 * @file    request.c
 * @author  Samandar Komil
 * @date    26 April 2025
 * @brief   HTTP request parser implementations
 *
 */

#include "parsers.h"

int parse_request_line(HTTPRequest *req_t, const char *reqstr, size_t len)
{
    if (!req_t || !reqstr)
    {
        printf("Req parser is not working. %p %p\n", req_t, reqstr);
        return -1;
    }

    // Putting pointers at start and end of the incoming string
    const char *ptr = reqstr;
    const char *end = reqstr + len;

    // Parse method (put pointer to buffer)
    const char *space = memchr(ptr, ' ', end - ptr);
    if (!space) return -1;
    req_t->request_line.method     = (char *)ptr;
    req_t->request_line.method_len = space - ptr;
    ptr                            = space + 1;

    // Parse URI (put pointer to buffer)
    space = memchr(ptr, ' ', end - ptr);
    if (!space) return -1;
    req_t->request_line.uri     = (char *)ptr;
    req_t->request_line.uri_len = space - ptr;
    ptr                         = space + 1;

    // Parse protocol (put pointer to buffer)
    const char *crlf = memmem(ptr, end - ptr, "\r\n", 2);
    if (!crlf) return -1;
    req_t->request_line.protocol     = (char *)ptr;
    req_t->request_line.protocol_len = crlf - ptr;
    ptr                              = crlf + 2;

    LOG("DEBUG", "Request line parsed: %.*s", req_t->request_line.uri_len, req_t->request_line.uri);

    return ptr - reqstr;
}

int parse_header(HTTPHeader *header, const char *line, size_t len)
{
    if (!header || !line)
    {
        printf("Header parser is not working. %p %p\n", header, line);
        return -1;
    }
    const char *ptr = line;
    const char *end = line + len;

    // Find colon
    const char *colon = memchr(ptr, ':', end - ptr);
    if (!colon) return -1;
    header->name     = (char *)ptr;
    header->name_len = colon - ptr;
    ptr              = colon + 1;

    // Skip whitespaces
    while (ptr < end && isspace(*ptr))
        ptr++;

    // Find end of line
    const char *crlf = memmem(ptr, end - ptr, "\r\n", 2);
    if (!crlf) return -1;
    header->value     = (char *)ptr;
    header->value_len = crlf - ptr;
    ptr               = crlf + 2;

    LOG("DEBUG", "Header parsed: %.*s", header->name_len, header->name);

    return ptr - line;
}

int parse_http_request(const char *data, size_t len, HTTPRequest *req)
{
    if (!req || !data)
    {
        printf("Req parser is not working. %p %p\n", req, data);
        return -1;
    }
    const char *ptr = data;
    const char *end = data + len;
    int consumed    = 0;

    consumed = parse_request_line(req, ptr, end - ptr);
    printf("Consumed request_line: %d\n", consumed);
    if (consumed < 0) return -1;
    ptr += consumed;

    req->header_count = 0;
    while (ptr < end && memcmp(ptr, "\r\n", 2) != 0)
    {
        if (req->header_count >= MAX_HEADERS) return -1;
        consumed = parse_header(&req->headers[req->header_count], ptr, end - ptr);
        if (consumed < 0) break;
        ptr += consumed;
        req->header_count++;
    }

    printf("Consumed headers count: %d\n", req->header_count);

    // printf("ptr: %s\n", ptr);

    // if (memcmp(ptr, "\r\n\r\n", 4) == 0)
    // {
    //     printf("No body present.");
    //     req->body     = NULL;
    //     req->body_len = 0;
    //     return 0;
    // }
    // else if (ptr + 2 > end || memcmp(ptr, "\r\n", 2) != 0)
    // {
    //     printf("Invalid HTTP request.");
    //     return -1;
    // }
    // ptr += 2;

    // // Body
    // req->body = (char *)ptr;
    // for (int j = 0; j < req->header_count; j++)
    // {
    //     if (strncmp(req->headers[j].name, "Content-Length", req->headers[j].name_len) == 0)
    //     {
    //         req->body_len = atoi(req->headers[j].value);
    //         break;
    //     }
    // }

    // printf("Consumed body_len: %ld\n", req->body_len);

    LOG("DEBUG", "HTTP request parsed.");

    return 0;
}

void print_request(const HTTPRequest *req)
{
    printf("Method: %.*s\n", (int)req->request_line.method_len, req->request_line.method);
    printf("URI: %.*s\n", (int)req->request_line.uri_len, req->request_line.uri);
    printf("Protocol: %.*s\n", (int)req->request_line.protocol_len, req->request_line.protocol);
    for (int i = 0; i < req->header_count; i++)
    {
        printf("Header: %.*s = %.*s\n", (int)req->headers[i].name_len, req->headers[i].name,
               (int)req->headers[i].value_len, req->headers[i].value);
    }
    if (req->body_len > 0)
    {
        printf("Body: %.*s\n", (int)req->body_len, req->body);
    }
}

const char *get_mime_type(const char *filepath)
{
    const char *ext = strrchr(filepath, '.');
    if (!ext) return "application/octet-stream";

    ext++; // skip the dot
    if (strcmp(ext, "html") == 0) return "text/html";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "png") == 0) return "image/png";
    if (strcmp(ext, "jpg") == 0) return "image/jpeg";
    if (strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif") == 0) return "image/gif";
    if (strcmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcmp(ext, "ico") == 0) return "image/x-icon";
    if (strcmp(ext, "json") == 0) return "application/json";
    if (strcmp(ext, "pdf") == 0) return "application/pdf";

    return "application/octet-stream";
}
