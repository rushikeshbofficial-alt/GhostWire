#include "catalog.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/**
 * Initializes an empty FileCatalog.
 * Complexity: O(1)
 */
void catalog_init(FileCatalog *catalog) {
    if (!catalog) {
        return;
    }
    catalog->head = NULL;
    catalog->count = 0;
}

/**
 * Creates a new FileEntry node and inserts it at the head of the linked list.
 * Complexity: O(1)
 */
int catalog_add(FileCatalog *catalog, const char *name, const char *path, long long size) {
    if (!catalog || !name || !path) {
        return -1;
    }

    FileEntry *node = (FileEntry *)malloc(sizeof(FileEntry));
    if (!node) {
        log_error("Memory allocation failed for FileEntry '%s'", name);
        return -1;
    }

    /* Safely copy filename and full path */
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';

    strncpy(node->path, path, sizeof(node->path) - 1);
    node->path[sizeof(node->path) - 1] = '\0';

    node->size = size;

    /* Insert at head of linked list */
    node->next = catalog->head;
    catalog->head = node;
    catalog->count++;

    return 0;
}

/**
 * Searches the catalog linked list sequentially by exact filename.
 * Complexity: O(n)
 * Returns pointer to FileEntry if found, or NULL if not found.
 */
FileEntry *catalog_find(const FileCatalog *catalog, const char *name) {
    if (!catalog || !name) {
        return NULL;
    }

    FileEntry *curr = catalog->head;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}

/**
 * Prints all entries in the catalog linked list with indices and file sizes.
 * Complexity: O(n)
 */
void catalog_print(const FileCatalog *catalog) {
    if (!catalog || catalog->count == 0) {
        printf("Catalog is empty (0 files).\n");
        return;
    }

    printf("Catalog (%d files available):\n", catalog->count);
    FileEntry *curr = catalog->head;
    int idx = 1;
    while (curr != NULL) {
        char size_str[32];
        format_size(curr->size, size_str, sizeof(size_str));
        printf("  %d. %s (%lld bytes, %s)\n", idx++, curr->name, curr->size, size_str);
        curr = curr->next;
    }
}

/**
 * Traverses and frees all allocated FileEntry nodes in the linked list.
 * Complexity: O(n)
 */
void catalog_free(FileCatalog *catalog) {
    if (!catalog) {
        return;
    }

    FileEntry *curr = catalog->head;
    while (curr != NULL) {
        FileEntry *next = curr->next;
        free(curr);
        curr = next;
    }

    catalog->head = NULL;
    catalog->count = 0;
}

/**
 * Scans a directory on the local filesystem and populates the catalog linked list.
 * Ignores '.', '..', hidden files (starting with '.'), and subdirectories to keep
 * the design simple and robust for the mid-semester scope.
 * Returns 0 on success, or -1 if directory cannot be opened.
 */
int catalog_scan_directory(FileCatalog *catalog, const char *dir_path) {
    if (!catalog || !dir_path) {
        return -1;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        log_error("Cannot open shared directory: %s", dir_path);
        return -1;
    }

    size_t dlen = strlen(dir_path);
    int has_trailing_slash = (dlen > 0 && (dir_path[dlen - 1] == '/' || dir_path[dlen - 1] == '\\'));

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Ignore '.' and '..' */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Ignore hidden files (starting with '.') */
        if (entry->d_name[0] == '.') {
            continue;
        }

        /* Enforce maximum filename length for catalog storage */
        if (strlen(entry->d_name) >= 256) {
            log_error("Skipping file with excessively long name (%zu bytes): %s",
                      strlen(entry->d_name), entry->d_name);
            continue;
        }

        /* Build full path cleanly without duplicate slashes */
        char full_path[512];
        int written = has_trailing_slash
            ? snprintf(full_path, sizeof(full_path), "%s%s", dir_path, entry->d_name)
            : snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (written < 0 || (size_t)written >= sizeof(full_path)) {
            log_error("Path exceeds buffer length (511 bytes), skipping: %s/%s",
                      dir_path, entry->d_name);
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            /* Could not stat, skip */
            continue;
        }

        /* Mid-semester scope: only include regular files, skip subdirectories */
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        if (catalog_add(catalog, entry->d_name, full_path, (long long)st.st_size) != 0) {
            log_error("Failed to add file to catalog: %s", entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}
