#include "peer.h"
#include <stdio.h>
#include <string.h>

void peer_init(PeerInfo *peer, const char *ip, int port) {
    if (!peer) {
        return;
    }
    if (ip) {
        strncpy(peer->ip, ip, sizeof(peer->ip) - 1);
        peer->ip[sizeof(peer->ip) - 1] = '\0';
    } else {
        peer->ip[0] = '\0';
    }
    peer->port = port;
}

void peer_print(const PeerInfo *peer) {
    if (!peer) {
        printf("(null peer)");
        return;
    }
    printf("%s:%d", peer->ip, peer->port);
}
