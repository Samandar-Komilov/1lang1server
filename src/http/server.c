/**
 * @file    HTTPServer.c
 * @author  Samandar Komil
 * @date    17 May 2025
 * @brief   HTTPServer methods and utility function implementations
 *
 */

#include "server.h"

int launch(HTTPServer *self)
{
    char s[INET6_ADDRSTRLEN];

    if (bind(self->server->socket, (struct sockaddr *)&self->server->address,
             sizeof(self->server->address)) < 0)
    {
        return SOCKET_BIND_ERROR;
    }
    if ((listen(self->server->socket, self->server->queue)) < 0)
    {
        return SOCKET_LISTEN_ERROR;
    }

    // Initialize epoll
    self->epoll_fd = epoll_create1(0);
    if (self->epoll_fd == -1)
    {
        LOG("ERROR", "Failed to initialize epoll instance.");
        exit(1);
    }

    // Initialize connections
    self->connections = calloc(MAX_CONNECTIONS, sizeof(Connection));
    if (!self->connections)
    {
        LOG("ERROR", "Failed to allocate memory for connections.");
        close(self->epoll_fd);
        return -1;
    }
    self->active_count = 0;

    // Add server socket to epoll
    struct epoll_event ev, events[MAX_EPOLL_EVENTS];
    ev.events  = EPOLLIN;
    ev.data.fd = self->server->socket;
    if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, self->server->socket, &ev) == -1)
    {
        close(self->epoll_fd);
        close(self->server->socket);
        LOG("ERROR", "Failed to add server socket to epoll event loop.");
        return -1;
    }

    LOG("INFO", "Waiting for connections on port %d", self->server->port);

    while (1)
    {
        int n_ready = epoll_wait(self->epoll_fd, events, MAX_EPOLL_EVENTS, 60);
        if (n_ready == -1)
        {
            LOG("ERROR", "Failed to wait for epoll events.");
            continue;
        }

        for (int i = 0; i < n_ready; i++)
        {
            if (events[i].data.fd == self->server->socket)
            {
                // Accept a new connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                int client_fd =
                    accept(self->server->socket, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd == -1)
                {
                    LOG("ERROR", "Failed to accept a new connection.");
                    close(client_fd);
                    continue;
                }

                // Find free connection slot from connections pool
                Connection *conn = NULL;
                for (size_t j = 0; j < MAX_CONNECTIONS; j++)
                {
                    if (self->connections[j].socket == 0)
                    {
                        conn = &self->connections[j];
                        break;
                    }
                }
                if (!conn || self->active_count >= MAX_CONNECTIONS)
                {
                    LOG("ERROR", "No free connection slots available.");
                    close(client_fd);
                    continue;
                }

                // ---------------------

                if (init_connection(conn, client_fd, self->epoll_fd) < 0)
                {
                    LOG("ERROR", "Failed to initialize a connection.");
                    close(client_fd);
                    continue;
                }
                self->active_count++;
                // --------------------

                // Set socket nonblocking
                int flags = fcntl(client_fd, F_GETFL, 0);
                if (flags == -1 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK))
                {
                    LOG("ERROR", "Failed to set socket nonblocking.");
                    free(conn->buffer);
                    close(client_fd);
                    conn->socket = 0;
                    self->active_count--;
                    continue;
                }

                // Add to epoll
                ev.events   = EPOLLIN;
                ev.data.ptr = conn;
                if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                {
                    LOG("ERROR", "Failed to add client socket to epoll event loop.");
                    free(conn->buffer);
                    close(client_fd);
                    conn->socket = 0;
                    self->active_count--;
                    continue;
                }

                inet_ntop(AF_INET, &client_addr.sin_addr, s, sizeof(s));
                LOG("INFO", "Connected: %s:%d, FD: %d", s, ntohs(client_addr.sin_port), client_fd);
            }
            else
            {
                // Handle client data
                Connection *conn = (Connection *)events[i].data.ptr;
                int client_fd    = conn->socket;

                // ---------------------------------
                while (1)
                {
                    int bytes_read = recv(client_fd, conn->buffer + conn->buffer_len,
                                          conn->buffer_size - conn->buffer_len, 0);

                    printf("recv(%d, conn->buffer + %ld, %ld, 0);\n", client_fd, conn->buffer_len,
                           conn->buffer_size - conn->buffer_len);
                    printf("Buffer: %s\n", conn->buffer);

                    conn->buffer[conn->buffer_len + bytes_read] = '\0';
                    if (bytes_read < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            // No more data for now. Socket is still open
                            LOG("DEBUG", "EAGAIN || EWOULDBLOCK - No more data for now.");
                            break;
                        }
                        else
                        {
                            // Couldn't read data from client, error
                            // TODO: set timeout
                            LOG("ERROR", "Failed to read data from client using recv().");
                            conn->state = CONN_ERROR;
                            break;
                        }
                    }
                    else if (bytes_read == 0)
                    {
                        // Client intentionally closed the connection
                        LOG("INFO", "Client FD %d intentionally closed connection (EOF)",
                            client_fd);
                        conn->state = CONN_CLOSING;

                        if (conn->buffer_len > 0)
                        {
                            // If we read something to buffer, continue processing
                            LOG("DEBUG", "Client sent partial request and disconnected.");
                            break;
                        }
                        else
                        {
                            // Otherwise, Client just connected, and disconnected without sending
                            // anything - close connection
                            LOG("DEBUG", "Client disconnected without sending anything.");
                            conn->state = CONN_CLOSING;
                            // free_connection(conn, client_fd, self->epoll_fd);
                            // self->active_count--;
                            break;
                        }
                        break;
                    }
                    else
                    {
                        // Successfully read some data
                        conn->buffer_len += bytes_read;
                        LOG("DEBUG", "Read %d bytes from socket FD %d", bytes_read, client_fd);

                        // Check if we need to grow buffer
                        if (conn->buffer_len >= conn->buffer_size)
                        {
                            size_t new_size  = conn->buffer_size * 2;
                            char *new_buffer = realloc(conn->buffer, new_size);
                            if (!new_buffer)
                            {
                                LOG("ERROR", "Failed to reallocate buffer for FD %d", client_fd);
                                conn->state = CONN_ERROR;
                                break;
                            }
                            conn->buffer      = new_buffer;
                            conn->buffer_size = new_size;
                            LOG("DEBUG", "Buffer size increased to %ld", new_size);
                        }
                    }
                }

                if (conn->state != CONN_CLOSING || conn->state != CONN_ERROR)
                {
                    // Initialize HTTPRequest for current request
                    if (conn->curr_request == NULL)
                    {
                        conn->curr_request = create_http_request();
                    }
                    conn->state = CONN_PROCESSING;
                    printf("New request initialized, connection state - PROCESSING.\n");

                    // Parse request if data available
                    if (conn->curr_request->state != REQ_PARSE_DONE)
                    {
                        int consumed =
                            parse_http_request(conn->buffer, conn->buffer_len, conn->curr_request);
                        printf("Is request parsed: %d\n", consumed);
                        if (consumed < 0)
                        {
                            LOG("ERROR", "Failed to parse HTTP request.");
                            conn->curr_request->state = REQ_HANDLE_ERROR;
                        }
                        else
                        {
                            LOG("DEBUG", "Successfully parsed HTTP request.");
                            conn->curr_request->state = REQ_PARSE_DONE;
                        }
                    }
                    // Handle request if fully parsed
                    if (conn->curr_request->state == REQ_PARSE_DONE)
                    {
                        printf("Request state: PARSE_DONE\n");
                        LOG("DEBUG", "Fully parsed HTTP request below:");
                        // print_request(&conn->request);

                        HTTPResponse *response = request_handler(conn->curr_request);
                        if (!response)
                        {
                            LOG("ERROR", "Failed to handle HTTP request (no response generated).");
                            conn->curr_request->state = REQ_HANDLE_ERROR;
                        }
                        else
                        {
                            size_t response_len;
                            char *response_str = httpresponse_serialize(response, &response_len);
                            httpresponse_free(response);
                            if (!response_str)
                            {
                                LOG("ERROR", "Failed to serialize HTTP response.");
                                conn->curr_request->state = REQ_HANDLE_ERROR;
                            }
                            else
                            {
                                // Send response
                                conn->state       = CONN_SENDING_RESPONSE;
                                size_t total_sent = 0;
                                while (total_sent < response_len)
                                {
                                    int bytes_sent = send(client_fd, response_str + total_sent,
                                                          response_len - total_sent, 0);
                                    if (bytes_sent <= 0)
                                    {
                                        LOG("ERROR",
                                            "Error while sending response to client socket.");
                                        break;
                                    }
                                    total_sent += bytes_sent;
                                }

                                LOG("DEBUG", "Sent %ld bytes response to client FD %d.", total_sent,
                                    client_fd);
                            }
                            // LOG("DEBUG", "Response string: %s", response_str);
                            free(response_str);
                        }
                    }

                    // Check for keep-alive
                    int keep_alive = 0;
                    for (int j = 0; j < conn->curr_request->header_count; j++)
                    {
                        if (strncmp(conn->curr_request->headers[j].name, "Connection",
                                    conn->curr_request->headers[j].name_len) == 0 &&
                            strncmp(conn->curr_request->headers[j].value, "keep-alive",
                                    conn->curr_request->headers[j].value_len) == 0)
                        {
                            keep_alive = 1;
                            break;
                        }
                    }

                    if (keep_alive)
                    {
                        // Reset for next request
                        reset_connection(conn);
                        LOG("DEBUG", "Connection is keep-alive for client FD %d", client_fd);
                    }
                    else
                    {
                        // Close connection
                        LOG("DEBUG",
                            "Connection is not keep-alive for client FD %d, closing connection...",
                            client_fd);
                        conn->state = CONN_CLOSING;
                    }
                }

                if (conn->state == CONN_CLOSING || conn->state == CONN_ERROR)
                {
                    LOG("DEBUG", "Connection is closing for client FD %d", client_fd);
                    if (conn->curr_request != NULL) free_http_request(conn->curr_request);
                    free_connection(conn, client_fd, self->epoll_fd);
                    epoll_ctl(self->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    self->active_count--;
                }
            }
        }
        // ---------------------------------
    }

    close(self->epoll_fd);
    return 0;
}

HTTPResponse *request_handler(HTTPRequest *request_ptr)
{
    if (request_ptr == NULL) return NULL;
    if (request_ptr->request_line.uri == NULL) return NULL;

    if (request_ptr->request_line.uri_len > 0)
    {
        if (strncmp(request_ptr->request_line.uri, "/static", 7) == 0)
        {
            char filepath[PATH_MAX];
            if (snprintf(filepath, sizeof(filepath), "%s%.*s", realpath(BASE_DIR, NULL),
                         (int)request_ptr->request_line.uri_len, request_ptr->request_line.uri) < 0)
            {
                LOG("ERROR", "Failed to build filepath.");
                char response_buffer[] = "<h1>404 Not Found</h1>";
                HTTPResponse *response = response_builder(404, "Not Found", response_buffer,
                                                          sizeof(response_buffer), "text/html");
                return response;
            };

            int fd = open(filepath, O_RDONLY);
            if (fd == -1)
            {
                LOG("ERROR", "Failed to open file.");
                char response_buffer[] = "<h1>404 Not Found</h1>";
                HTTPResponse *response = response_builder(404, "Not Found", response_buffer,
                                                          sizeof(response_buffer), "text/html");
                return response;
            }

            // Get file size
            struct stat st;
            fstat(fd, &st);
            size_t filesize = st.st_size;

            char *buffer = malloc(filesize);
            memset(buffer, 0, filesize);

            if (!buffer)
            {
                LOG("ERROR", "Failed to allocate buffer.");
                close(fd);
                char response_buffer[] = "<h1>Internal Server Error</h1>";
                return response_builder(500, "Internal Server Error", response_buffer,
                                        sizeof(response_buffer), "text/html");
            }

            size_t total_read = 0;
            while (total_read < filesize)
            {
                size_t bytes = read(fd, buffer + total_read, filesize - total_read);
                if (bytes <= 0)
                {
                    LOG("ERROR", "Failed to read file.");
                    free(buffer);
                    char response_buffer[] = "<h1>Internal Server Error</h1>";
                    return response_builder(500, "Internal Server Error", response_buffer,
                                            sizeof(response_buffer), "text/html");
                }
                total_read += bytes;
            }

            LOG("DEBUG", "Read %zu bytes\nActual filesize: %zu", total_read, filesize);

            if (total_read != filesize)
            {
                LOG("ERROR", "Failed to read file.");
                free(buffer);
                char response_buffer[] = "<h1>Internal Server Error</h1>";
                return response_builder(500, "Internal Server Error", response_buffer,
                                        sizeof(response_buffer), "text/html");
            }

            HTTPResponse *response =
                response_builder(200, "OK", buffer, filesize, get_mime_type(filepath));

            close(fd);
            return response;
        }
        else if (strncmp(request_ptr->request_line.uri, "/api", 4) == 0)
        {
            // TODO: Reverse proxy
            char *api_path = request_ptr->request_line.uri + strlen("/api");
            if (*api_path == '\0') api_path = "/";

            char proxy_request[INITIAL_BUFFER_SIZE];
            int proxy_request_len =
                snprintf(proxy_request, sizeof(proxy_request),
                         "%s %s HTTP/1.1\r\n"
                         "Host: localhost\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: close\r\n"
                         "\r\n",
                         request_ptr->request_line.method, api_path, request_ptr->body_len);

            if (proxy_request_len < 0)
            {
                LOG("ERROR", "Failed to build proxy request.");
                char response_buffer[] = "<h1>Internal Server Error</h1>";
                return response_builder(500, "Internal Server Error", response_buffer,
                                        sizeof(response_buffer), "text/html");
            }

            // Then, append the request body to the proxy_request string
            strncat(proxy_request, request_ptr->body, request_ptr->body_len);

            int backend_fd = connect_to_backend("localhost", "8002");
            if (backend_fd == -1)
            {
                LOG("ERROR", "Failed to connect to backend.");
                char response_buffer[] = "<h1>502 Bad Gateway</h1>";
                return response_builder(502, "Bad Gateway", response_buffer,
                                        sizeof(response_buffer), "text/html");
            }

            send(backend_fd, proxy_request, proxy_request_len, 0);

            // Receive response from backend
            char proxy_response[INITIAL_BUFFER_SIZE];
            int proxy_response_len = recv(backend_fd, proxy_response, sizeof(proxy_response), 0);

            if (proxy_response_len < 0)
            {
                LOG("ERROR", "Failed to read from backend.");
                char response_buffer[] = "<h1>502 Bad Gateway</h1>";
                return response_builder(502, "Bad Gateway", response_buffer,
                                        sizeof(response_buffer), "text/html");
            }

            proxy_response[proxy_response_len] = '\0';
            close(backend_fd);

            LOG("DEBUG", "Received %d bytes response from backend.", proxy_response_len);

            // TODO: parse response headers, here we directly return body
            char *body = strstr(proxy_response, "\r\n\r\n");
            if (!body)
                body = proxy_response;
            else
                body += 4;

            HTTPResponse *response =
                response_builder(200, "OK", body, proxy_request_len, "text/html");
            return response;
        }
        else
        {
            LOG("DEBUG", "Request to unknown URI by proxy backend: %s",
                request_ptr->request_line.uri);
            char response_buffer[] = "<h1>404 Not Found</h1>";
            HTTPResponse *response = response_builder(404, "Not Found", response_buffer,
                                                      sizeof(response_buffer), "text/html");
            return response;
        }
    }

    LOG("DEBUG", "Request to unknown URI: %s", request_ptr->request_line.uri);
    char response_buffer[] = "<h1>404 Not Found</h1>";
    HTTPResponse *response =
        response_builder(404, "Not Found", response_buffer, sizeof(response_buffer), "text/html");
    return response;
}

// ---------- UTILS ----------

int init_connection(Connection *conn, int client_fd, int epoll_fd)
{
    if (!conn || client_fd < 0 || epoll_fd < 0) return -1;

    conn->socket = client_fd;
    conn->buffer = (char *)calloc(INITIAL_BUFFER_SIZE, sizeof(char));
    if (!conn->buffer) return -1;

    conn->buffer_size      = INITIAL_BUFFER_SIZE;
    conn->buffer_len       = 0;
    conn->curr_request     = NULL;
    conn->keep_alive       = false;
    conn->requests_handled = 0;
    conn->state            = CONN_ESTABLISHED;
    conn->last_active      = time(NULL);

    return 0;
}

int free_connection(Connection *conn, int client_fd, int epoll_fd)
{
    if (!conn || client_fd < 0 || epoll_fd < 0) return -1;

    if (conn->socket > 0)
    {
        close(conn->socket);
        conn->socket = 0;
    }

    if (conn->buffer)
    {
        free(conn->buffer);
        conn->buffer = NULL;
    }

    conn->buffer_size = 0;

    return OK;
}

int reset_connection(Connection *conn)
{
    memset(conn->buffer, 0, conn->buffer_size);
    conn->state = CONN_ESTABLISHED;

    return OK;
}

int connect_to_backend(const char *host, const char *port)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0)
    {
        LOG("ERROR", "Failed to create socket while connecting to proxy backend.");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0)
    {
        LOG("ERROR", "Failed to connect to proxy backend.");
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sock;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

HTTPServer *httpserver_constructor(int port, char *static_dir, char **proxy_backends,
                                   int backend_count)
{
    HTTPServer *httpserver_ptr = (HTTPServer *)malloc(sizeof(HTTPServer));

    SocketServer *SockServer   = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, port, 10);
    httpserver_ptr->server     = SockServer;
    httpserver_ptr->static_dir = strdup(static_dir);
    httpserver_ptr->proxy_backends = proxy_backends;
    httpserver_ptr->backend_count  = backend_count;
    httpserver_ptr->launch         = launch;

    return httpserver_ptr;
}

void httpserver_destructor(HTTPServer *httpserver_ptr)
{
    if (httpserver_ptr->server != NULL)
    {
        server_destructor(httpserver_ptr->server);
    }
    free(httpserver_ptr->static_dir);
    free(httpserver_ptr->proxy_backends);
    free(httpserver_ptr);
}