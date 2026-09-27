#ifndef TRANSFER_H
#define TRANSFER_H

#include <stddef.h>

/* File transfer functions using chunked binary I/O */
int send_file(int sockfd, const char *path, long long file_size);
int receive_file(int sockfd, const char *output_path, long long file_size);

#endif /* TRANSFER_H */
