#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* Protocol Constants */
#define MSG_REQUEST_FILE 1
#define STATUS_NOT_FOUND 0
#define STATUS_OK 1
#define MAX_FILENAME 255
#define TRANSFER_BUFFER_SIZE 8192

/* 64-bit Network Byte Order conversion helpers */
uint64_t hton64_compat(uint64_t val);
uint64_t ntoh64_compat(uint64_t val);

/* Protocol Request/Response Functions */
int send_request(int sockfd, const char *filename);
int receive_request(int sockfd, char *filename_out, size_t max_len);
int send_response(int sockfd, uint8_t status, uint64_t file_size);
int receive_response(int sockfd, uint8_t *status_out, uint64_t *file_size_out);

#endif /* PROTOCOL_H */
