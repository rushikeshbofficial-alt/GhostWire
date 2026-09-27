#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include "peer.h"

/* Network subsystem initialization & cleanup */
int network_init(void);
void network_cleanup(void);

/* Server socket setup and client acceptance */
int create_server_socket(int port);
int accept_client(int server_fd, PeerInfo *client_peer);

/* Client connection to a remote peer */
int connect_to_peer(const char *ip, int port);

/* Clean socket closure */
void close_socket(int sockfd);

/* Robust stream transmission helpers (handles TCP fragmentation & stream semantics) */
int send_all(int sockfd, const void *buffer, size_t length);
int recv_all(int sockfd, void *buffer, size_t length);

#endif /* NETWORK_H */
