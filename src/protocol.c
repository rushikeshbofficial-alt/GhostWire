#include "protocol.h"
#include "network.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

/**
 * Checks if the host architecture is little-endian.
 */
static inline int is_host_little_endian(void) {
    uint32_t x = 1;
    return (*(uint8_t *)&x == 1);
}

/**
 * Swaps bytes of a 64-bit unsigned integer.
 */
static inline uint64_t swap64(uint64_t val) {
    return ((val & 0x00000000000000FFULL) << 56) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x00000000FF000000ULL) << 8)  |
           ((val & 0x000000FF00000000ULL) >> 8)  |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0xFF00000000000000ULL) >> 56);
}

/**
 * Portable 64-bit host-to-network endian converter.
 * Converts host byte order to big-endian network byte order.
 */
uint64_t hton64_compat(uint64_t val) {
    if (is_host_little_endian()) {
        return swap64(val);
    }
    return val;
}

/**
 * Portable 64-bit network-to-host endian converter.
 * Converts big-endian network byte order to host byte order.
 */
uint64_t ntoh64_compat(uint64_t val) {
    if (is_host_little_endian()) {
        return swap64(val);
    }
    return val;
}

/**
 * Sends a file request message:
 *   uint8_t  type;       // 1 = MSG_REQUEST_FILE
 *   uint32_t name_len;   // Network byte order
 *   char     name[...];  // Filename string
 *
 * Packed into a single contiguous buffer to prevent TCP fragmentation / Nagle delays.
 */
int send_request(int sockfd, const char *filename) {
    if (sockfd < 0 || !filename) {
        return -1;
    }

    size_t name_len = strlen(filename);
    if (name_len == 0 || name_len > MAX_FILENAME) {
        log_error("Invalid filename length (%zu): must be 1 to %d bytes", name_len, MAX_FILENAME);
        return -1;
    }

    uint8_t packet[1 + 4 + MAX_FILENAME];
    packet[0] = MSG_REQUEST_FILE;

    uint32_t net_len = htonl((uint32_t)name_len);
    memcpy(&packet[1], &net_len, 4);
    memcpy(&packet[5], filename, name_len);

    size_t total_len = 1 + 4 + name_len;
    if (send_all(sockfd, packet, total_len) != 0) {
        log_error("Failed to send request packet to peer");
        return -1;
    }

    return 0;
}

/**
 * Receives and validates a file request message.
 */
int receive_request(int sockfd, char *filename_out, size_t max_len) {
    if (sockfd < 0 || !filename_out || max_len == 0) {
        return -1;
    }

    /* Read header: 1 byte type + 4 bytes name_len */
    uint8_t header[5];
    if (recv_all(sockfd, header, sizeof(header)) != 0) {
        return -1;
    }

    uint8_t type = header[0];
    if (type != MSG_REQUEST_FILE) {
        log_error("Unknown protocol request type: %u (expected %u)", type, MSG_REQUEST_FILE);
        return -1;
    }

    uint32_t net_len = 0;
    memcpy(&net_len, &header[1], 4);
    uint32_t name_len = ntohl(net_len);

    if (name_len == 0 || name_len > MAX_FILENAME || name_len >= max_len) {
        log_error("Invalid filename length in request: %u", name_len);
        return -1;
    }

    if (recv_all(sockfd, filename_out, name_len) != 0) {
        log_error("Failed to receive filename payload from peer");
        return -1;
    }
    filename_out[name_len] = '\0';

    /* Security check: verify no embedded null characters */
    if (strlen(filename_out) != name_len) {
        log_error("Rejected filename with embedded null characters");
        return -1;
    }

    if (!is_valid_filename(filename_out)) {
        log_error("Rejected unsafe or invalid filename in request: '%s'", filename_out);
        return -1;
    }

    return 0;
}

/**
 * Sends a file response message:
 *   uint8_t  status;     // 0 = NOT_FOUND, 1 = OK
 *   uint64_t file_size;  // Network byte order
 *
 * Packed into a single contiguous 9-byte packet.
 */
int send_response(int sockfd, uint8_t status, uint64_t file_size) {
    if (sockfd < 0) {
        return -1;
    }

    uint8_t packet[1 + 8];
    packet[0] = status;

    uint64_t net_size = hton64_compat(file_size);
    memcpy(&packet[1], &net_size, 8);

    if (send_all(sockfd, packet, sizeof(packet)) != 0) {
        log_error("Failed to send response packet to peer");
        return -1;
    }

    return 0;
}

/**
 * Receives and validates a file response message.
 */
int receive_response(int sockfd, uint8_t *status_out, uint64_t *file_size_out) {
    if (sockfd < 0 || !status_out || !file_size_out) {
        return -1;
    }

    uint8_t packet[1 + 8];
    if (recv_all(sockfd, packet, sizeof(packet)) != 0) {
        return -1;
    }

    uint8_t status = packet[0];
    if (status != STATUS_OK && status != STATUS_NOT_FOUND) {
        log_error("Received unknown response status: %u", status);
        return -1;
    }

    uint64_t net_size = 0;
    memcpy(&net_size, &packet[1], 8);

    *status_out = status;
    *file_size_out = ntoh64_compat(net_size);

    return 0;
}
