#include "common.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[INFO] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
    fflush(stdout);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
}

long long get_file_size(const char *path) {
    if (!path) {
        return -1;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return (long long)st.st_size;
}

int file_exists(const char *path) {
    if (!path) {
        return 0;
    }
    struct stat st;
    return (stat(path, &st) == 0);
}

int is_valid_filename(const char *filename) {
    if (!filename) {
        return 0;
    }
    size_t len = strlen(filename);
    if (len == 0 || len > MAX_FILENAME) {
        return 0;
    }
    /* Reject special directory navigation entries */
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return 0;
    }
    /* Prevent directory traversal attacks */
    for (size_t i = 0; i < len; i++) {
        if (filename[i] == '/' || filename[i] == '\\') {
            return 0;
        }
    }
    return 1;
}

void format_size(long long bytes, char *buffer, size_t buflen) {
    if (!buffer || buflen == 0) {
        return;
    }
    if (bytes < 1024) {
        snprintf(buffer, buflen, "%ld B", (long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buflen, "%.2f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024LL * 1024 * 1024) {
        snprintf(buffer, buflen, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buflen, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

void format_speed(long long bytes, double seconds, char *buffer, size_t buflen) {
    if (!buffer || buflen == 0) {
        return;
    }
    if (seconds <= 0.0) {
        seconds = 0.000001; /* Prevent divide by zero */
    }
    double bytes_per_sec = (double)bytes / seconds;
    if (bytes_per_sec < 1024.0) {
        snprintf(buffer, buflen, "%.2f B/s", bytes_per_sec);
    } else if (bytes_per_sec < 1024.0 * 1024.0) {
        snprintf(buffer, buflen, "%.2f KB/s", bytes_per_sec / 1024.0);
    } else {
        snprintf(buffer, buflen, "%.2f MB/s", bytes_per_sec / (1024.0 * 1024.0));
    }
}

double get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}
