#include "transfer.h"
#include "protocol.h"
#include "network.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Sends a file as binary data over the network in fixed-size chunks of 8192 bytes.
 * Does NOT load the entire file into memory (O(1) memory complexity, O(F) time).
 * Returns 0 on success, or -1 on error.
 */
int send_file(int sockfd, const char *path, long long file_size) {
    if (sockfd < 0 || !path || file_size < 0) {
        log_error("Invalid parameters to send_file");
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        log_error("Cannot open file for reading: '%s'", path);
        return -1;
    }

    char buffer[TRANSFER_BUFFER_SIZE];
    long long total_sent = 0;

    while (total_sent < file_size) {
        size_t to_read = sizeof(buffer);
        if ((file_size - total_sent) < (long long)to_read) {
            to_read = (size_t)(file_size - total_sent);
        }

        size_t bytes_read = fread(buffer, 1, to_read, fp);
        if (bytes_read == 0) {
            if (ferror(fp)) {
                log_error("Read error occurred while reading '%s'", path);
                fclose(fp);
                return -1;
            }
            /* Reached unexpected EOF before expected size */
            break;
        }

        if (send_all(sockfd, buffer, bytes_read) != 0) {
            log_error("Network send failed during file transfer of '%s'", path);
            fclose(fp);
            return -1;
        }

        total_sent += (long long)bytes_read;
    }

    fclose(fp);

    if (total_sent != file_size) {
        log_error("Total sent bytes mismatch for '%s': sent %lld, expected %lld",
                  path, total_sent, file_size);
        return -1;
    }

    return 0;
}

/**
 * Receives a file from the network and writes it directly to disk in fixed 8192-byte chunks.
 * Verifies that the exact expected byte count is received.
 * Returns 0 on success, or -1 on error.
 */
int receive_file(int sockfd, const char *output_path, long long file_size) {
    if (sockfd < 0 || !output_path || file_size < 0) {
        log_error("Invalid parameters to receive_file");
        return -1;
    }

    FILE *fp = fopen(output_path, "wb");
    if (!fp) {
        log_error("Cannot open output file for writing: '%s'", output_path);
        return -1;
    }

    char buffer[TRANSFER_BUFFER_SIZE];
    long long remaining = file_size;
    long long total_received = 0;

    while (remaining > 0) {
        size_t chunk = (remaining > (long long)sizeof(buffer)) ? sizeof(buffer) : (size_t)remaining;

        if (recv_all(sockfd, buffer, chunk) != 0) {
            log_error("Network receive failed while receiving '%s' (got %lld / %lld bytes)",
                      output_path, total_received, file_size);
            fclose(fp);
            remove(output_path);
            return -1;
        }

        size_t written = fwrite(buffer, 1, chunk, fp);
        if (written != chunk) {
            log_error("Disk write failed for '%s'", output_path);
            fclose(fp);
            remove(output_path);
            return -1;
        }

        remaining -= (long long)chunk;
        total_received += (long long)chunk;
    }

    fclose(fp);

    if (total_received != file_size) {
        log_error("Byte verification failed for '%s': received %lld, expected %lld",
                  output_path, total_received, file_size);
        remove(output_path);
        return -1;
    }

    return 0;
}
