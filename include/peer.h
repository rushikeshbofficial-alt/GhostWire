#ifndef PEER_H
#define PEER_H

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

/* Peer endpoint metadata */
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
} PeerInfo;

/* Peer helper functions */
void peer_init(PeerInfo *peer, const char *ip, int port);
void peer_print(const PeerInfo *peer);

#endif /* PEER_H */
