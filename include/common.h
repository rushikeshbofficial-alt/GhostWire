#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
    #ifndef __USE_MINGW_ANSI_STDIO
        #define __USE_MINGW_ANSI_STDIO 1
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Logging and error reporting helpers */
void log_info(const char *format, ...);
void log_error(const char *format, ...);

/* File utility functions */
long long get_file_size(const char *path);
int file_exists(const char *path);
int is_valid_filename(const char *filename);

/* Formatting helpers for student-friendly displays */
void format_size(long long bytes, char *buffer, size_t buflen);
void format_speed(long long bytes, double seconds, char *buffer, size_t buflen);

/* High-resolution wall-clock timer returning seconds */
double get_time_seconds(void);

#endif /* COMMON_H */
