#ifndef CATALOG_H
#define CATALOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Singly linked list node representing a shared file */
typedef struct FileEntry {
    char name[256];             /* Filename presented to remote peers */
    char path[512];             /* Full local filesystem path to the file */
    long long size;             /* File size in bytes */
    struct FileEntry *next;     /* Pointer to next node in the catalog list */
} FileEntry;

/* Catalog header structure */
typedef struct {
    FileEntry *head;            /* Head pointer to linked list of FileEntry nodes */
    int count;                  /* Total number of files currently cataloged */
} FileCatalog;

/* Required Data Structure operations */
void catalog_init(FileCatalog *catalog);
int catalog_add(FileCatalog *catalog, const char *name, const char *path, long long size);
FileEntry *catalog_find(const FileCatalog *catalog, const char *name);
void catalog_print(const FileCatalog *catalog);
void catalog_free(FileCatalog *catalog);

/* Directory scanning utility to populate catalog from disk */
int catalog_scan_directory(FileCatalog *catalog, const char *dir_path);

#endif /* CATALOG_H */
