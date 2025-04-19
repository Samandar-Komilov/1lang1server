# 0️⃣ Version 0: Basic TCP Server

In this chapter, we'll start building bare bones of our web server. We'll define our fundamental `Server` struct with dedicated constructor function `server_constructor`. 
The reason why we are writing a constructor is, C is not an Object-Oriented Language. So we have to use **structs** with **function pointers** to imitate that behaviour.

> Actually, I don't like OOP much, but for now let's start with that. Maybe there is another way.


### Server.h

Let's define our base struct and its constructor function prototype in this [header file](Server.h):

```c
typedef struct {
    int domain;                  // Communication domain: AF_INET for IP4, AF_INET6 for IP6
    int service;                 // Type of service: SOCK_STREAM = TCP, SOCK_DGRAM = UDP
    int protocol;                // Protocol: IPPROTO_TCP = TCP, IPPROTO_UDP = UDP, 0 = default protocol
    int port;                    // Port server listens: 8080 for example
    uint32_t interface;          // IP address the server binds to. INADDR_ANY for all interfaces
    int queue;                   // Maximum length of pending connections
    struct sockaddr_in address;  // Used in bind(), containing internet address information
    int socket;                  // Actual file descriptor returned by socket() syscall. Used for all socket operations. 
} Server;

Server server_constructor(int domain, int service, int protocol, uint32_t interface, int port, int queue);
```

We have to analyze multiple things here:
- What are communication domains? What are `AF_INET`, `AF_INET6`?
- What are TCP and UDP actually?
- Why we separately define service and protocol if each of them are about TCP and UDP only?
- What is an interface? Why we used `uint32_t` type for that?
- How `sockaddr_in` struct is structured?

### Server.c

Now that we already have a basic struct `Server`, [let's define](Server.c) our constructor:

```c
#include "Server.h"

Server server_constructor(int domain, int service, int protocol, uint32_t interface, int port, int queue) {
    Server server;
    server.domain = domain;
    server.service = service;
    server.protocol = protocol;
    server.port = port;
    server.interface = interface;
    server.queue = queue;

    server.address.sin_family = domain;
    server.address.sin_port = htons(port);
    server.address.sin_addr.s_addr = htonl(interface);

    server.socket = socket(domain, service, protocol);

    if (server.socket == 0){
        perror("Failed to connect socket...");
        exit(1);
    }

    if (bind(server.socket, (struct sockaddr *)&server.address, sizeof(server.address)) < 0) {
        perror("Failed to bind socket...");
        exit(1);
    }

    if ((listen(server.socket, server.queue)) < 0){
        perror("Failed to listen socket...");
        exit(1);
    }

    return server;
}
```

Ahh, again there are many new questions:
- What are `htons()` and `htonl()` functions?
- What are Host Byte Order and Network Byte Order? Little-Endian vs Big-Endian.
- What happens when we call `socket(domain, service, protocol)`? Why we should check for 0?
- What happens when we call `bind(server.socket, ...)`? Why we should check for `< 0`? Why we are casting `server.address`'s address to `struct sockaddr` pointer?
- What happens when we call `listen(server.socket, server.queue)`? Why we should check for `< 0`?