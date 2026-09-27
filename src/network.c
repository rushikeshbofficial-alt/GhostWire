#include "network.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    typedef int socklen_t;
    #define SOCKET_TYPE SOCKET
    #define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <signal.h>
    typedef int SOCKET_TYPE;
    #define IS_INVALID_SOCKET(s) ((s) < 0)
    #define INVALID_SOCKET (-1)
#endif

int network_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (res != 0) {
        log_error("WSAStartup failed with error: %d", res);
        return -1;
    }
#else
    /* Ignore SIGPIPE so send() returns EPIPE on peer disconnect instead of terminating process */
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif
#endif
    return 0;
}

void network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

void close_socket(int sockfd) {
    if (sockfd < 0) {
        return;
    }
#ifdef _WIN32
    closesocket((SOCKET_TYPE)sockfd);
#else
    close(sockfd);
#endif
}

int create_server_socket(int port) {
    if (port <= 0 || port > 65535) {
        log_error("Invalid server port: %d (must be between 1 and 65535)", port);
        return -1;
    }

    SOCKET_TYPE server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (IS_INVALID_SOCKET(server_sock)) {
        log_error("Failed to create server socket");
        return -1;
    }

    /* Enable SO_REUSEADDR so port can be immediately rebound */
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) != 0) {
        log_error("setsockopt(SO_REUSEADDR) failed");
    }
#else
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        log_error("setsockopt(SO_REUSEADDR) failed");
    }
#endif

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t)port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        log_error("Failed to bind server socket to port %d", port);
        close_socket((int)server_sock);
        return -1;
    }

    if (listen(server_sock, 10) != 0) {
        log_error("Failed to listen on server socket");
        close_socket((int)server_sock);
        return -1;
    }

    return (int)server_sock;
}

int accept_client(int server_fd, PeerInfo *client_peer) {
    if (server_fd < 0) {
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));

    SOCKET_TYPE client_sock = accept((SOCKET_TYPE)server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (IS_INVALID_SOCKET(client_sock)) {
        return -1;
    }

    if (client_peer) {
        char *ip_str = inet_ntoa(client_addr.sin_addr);
        if (ip_str) {
            strncpy(client_peer->ip, ip_str, sizeof(client_peer->ip) - 1);
            client_peer->ip[sizeof(client_peer->ip) - 1] = '\0';
        } else {
            strncpy(client_peer->ip, "unknown", sizeof(client_peer->ip) - 1);
            client_peer->ip[sizeof(client_peer->ip) - 1] = '\0';
        }
        client_peer->port = ntohs(client_addr.sin_port);
    }

    return (int)client_sock;
}

int connect_to_peer(const char *ip, int port) {
    if (!ip || port <= 0 || port > 65535) {
        log_error("Invalid connection parameters: %s:%d", ip ? ip : "(null)", port);
        return -1;
    }

    SOCKET_TYPE sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (IS_INVALID_SOCKET(sock)) {
        log_error("Failed to create client socket");
        return -1;
    }

    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons((uint16_t)port);

    unsigned long addr = inet_addr(ip);
    if (addr != INADDR_NONE || strcmp(ip, "255.255.255.255") == 0) {
        peer_addr.sin_addr.s_addr = addr;
    } else {
        /* Fallback: try hostname resolution if not a numeric IPv4 address (e.g. localhost) */
        struct hostent *he = gethostbyname(ip);
        if (he && he->h_addr_list[0]) {
            memcpy(&peer_addr.sin_addr, he->h_addr_list[0], sizeof(peer_addr.sin_addr));
        } else {
            log_error("Cannot resolve peer address: '%s'", ip);
            close_socket((int)sock);
            return -1;
        }
    }

    if (connect(sock, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) != 0) {
        log_error("Failed to connect to %s:%d", ip, port);
        close_socket((int)sock);
        return -1;
    }

    return (int)sock;
}

/**
 * Loops around send() until all requested bytes are sent or an error occurs.
 * Section 8.1 requirement.
 */
int send_all(int sockfd, const void *buffer, size_t length) {
    if (sockfd < 0 || !buffer) {
        return -1;
    }

    const char *ptr = (const char *)buffer;
    size_t total_sent = 0;

    while (total_sent < length) {
        size_t to_send = length - total_sent;
        int chunk = (to_send > 65536) ? 65536 : (int)to_send;

#if defined(MSG_NOSIGNAL)
        int sent = send((SOCKET_TYPE)sockfd, ptr + total_sent, chunk, MSG_NOSIGNAL);
#else
        int sent = send((SOCKET_TYPE)sockfd, ptr + total_sent, chunk, 0);
#endif

        if (sent < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            log_error("send_all failed after %zu of %zu bytes sent", total_sent, length);
            return -1;
        }
        if (sent == 0) {
            log_error("send_all socket closed prematurely after %zu of %zu bytes sent", total_sent, length);
            return -1;
        }
        total_sent += (size_t)sent;
    }

    return 0;
}

/**
 * Loops around recv() until all requested bytes are received, or peer closes/errors.
 * Section 8.2 requirement.
 */
int recv_all(int sockfd, void *buffer, size_t length) {
    if (sockfd < 0 || !buffer) {
        return -1;
    }

    char *ptr = (char *)buffer;
    size_t total_received = 0;

    while (total_received < length) {
        size_t to_recv = length - total_received;
        int chunk = (to_recv > 65536) ? 65536 : (int)to_recv;

        int received = recv((SOCKET_TYPE)sockfd, ptr + total_received, chunk, 0);
        if (received < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            log_error("recv_all error after %zu of %zu bytes received", total_received, length);
            return -1;
        }
        if (received == 0) {
            /* Unexpected EOF / peer closed before complete message */
            log_error("Peer closed connection prematurely (received %zu of %zu bytes)",
                      total_received, length);
            return -1;
        }
        total_received += (size_t)received;
    }

    return 0;
}
