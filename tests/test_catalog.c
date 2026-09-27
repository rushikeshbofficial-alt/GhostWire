#include "catalog.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_tests_passed = 0;
static int g_tests_total = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_total++; \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while (0)

void test_catalog_empty(void) {
    printf("Running Test: Catalog Empty (T01)\n");
    FileCatalog cat;
    catalog_init(&cat);

    TEST_ASSERT(cat.head == NULL, "Initial catalog head must be NULL");
    TEST_ASSERT(cat.count == 0, "Initial catalog count must be 0");
    TEST_ASSERT(catalog_find(&cat, "nonexistent.txt") == NULL, "Search in empty catalog must return NULL");

    catalog_free(&cat);
}

void test_catalog_add_and_find(void) {
    printf("Running Test: Catalog Add and Find (T02, T03, T04)\n");
    FileCatalog cat;
    catalog_init(&cat);

    int r1 = catalog_add(&cat, "file1.txt", "/shared/file1.txt", 100);
    int r2 = catalog_add(&cat, "file2.jpg", "/shared/file2.jpg", 20480);
    int r3 = catalog_add(&cat, "file3.pdf", "/shared/file3.pdf", 1048576);

    TEST_ASSERT(r1 == 0 && r2 == 0 && r3 == 0, "catalog_add returned success (0) for 3 items");
    TEST_ASSERT(cat.count == 3, "catalog count is exactly 3");

    /* T03: Find success */
    FileEntry *e1 = catalog_find(&cat, "file1.txt");
    TEST_ASSERT(e1 != NULL, "catalog_find found file1.txt");
    if (e1) {
        TEST_ASSERT(strcmp(e1->name, "file1.txt") == 0, "e1->name matches file1.txt");
        TEST_ASSERT(strcmp(e1->path, "/shared/file1.txt") == 0, "e1->path matches /shared/file1.txt");
        TEST_ASSERT(e1->size == 100, "e1->size is 100");
    }

    FileEntry *e2 = catalog_find(&cat, "file2.jpg");
    TEST_ASSERT(e2 != NULL, "catalog_find found file2.jpg");
    if (e2) {
        TEST_ASSERT(e2->size == 20480, "e2->size is 20480");
    }

    FileEntry *e3 = catalog_find(&cat, "file3.pdf");
    TEST_ASSERT(e3 != NULL, "catalog_find found file3.pdf");
    if (e3) {
        TEST_ASSERT(e3->size == 1048576, "e3->size is 1048576");
    }

    /* Verify LIFO head-insertion (O(1)) */
    TEST_ASSERT(cat.head != NULL && strcmp(cat.head->name, "file3.pdf") == 0, "Last added node is at head of list (O(1) insertion)");

    /* T04: Find failure */
    FileEntry *missing = catalog_find(&cat, "ghost.mp4");
    TEST_ASSERT(missing == NULL, "catalog_find returns NULL for nonexistent file");

    /* Edge case: NULL catalog or NULL filename */
    TEST_ASSERT(catalog_find(NULL, "file1.txt") == NULL, "catalog_find handles NULL catalog safely");
    TEST_ASSERT(catalog_find(&cat, NULL) == NULL, "catalog_find handles NULL filename safely");

    catalog_free(&cat);
    TEST_ASSERT(cat.head == NULL, "After catalog_free, head is NULL");
    TEST_ASSERT(cat.count == 0, "After catalog_free, count is 0");
}

void test_catalog_scan(void) {
    printf("Running Test: Catalog Scan Directory\n");
    FileCatalog cat;
    catalog_init(&cat);

    /* Scan existing shared directory */
    int res = catalog_scan_directory(&cat, "shared");
    TEST_ASSERT(res == 0, "catalog_scan_directory succeeds on shared");
    printf("  Found %d file(s) in shared/ directory\n", cat.count);
    catalog_free(&cat);

    /* Test directory with trailing slash */
    catalog_init(&cat);
    res = catalog_scan_directory(&cat, "shared/");
    TEST_ASSERT(res == 0, "catalog_scan_directory succeeds on shared/ with trailing slash");
    if (cat.head) {
        /* Ensure no double slash was generated */
        TEST_ASSERT(strstr(cat.head->path, "//") == NULL, "Path does not contain duplicate slashes");
    }
    catalog_free(&cat);

    /* Test nonexistent directory returns -1 */
    catalog_init(&cat);
    res = catalog_scan_directory(&cat, "nonexistent_dir_12345");
    TEST_ASSERT(res == -1, "catalog_scan_directory returns -1 on nonexistent directory");
    TEST_ASSERT(cat.count == 0, "Catalog count remains 0 on scan error");
    catalog_free(&cat);

    /* Test NULL directory path returns -1 */
    res = catalog_scan_directory(&cat, NULL);
    TEST_ASSERT(res == -1, "catalog_scan_directory returns -1 for NULL directory path");

    /* Test NULL catalog pointer returns -1 */
    res = catalog_scan_directory(NULL, "shared");
    TEST_ASSERT(res == -1, "catalog_scan_directory returns -1 for NULL catalog");
}

void test_catalog_stress_and_edge_cases(void) {
    printf("Running Test: Catalog Stress and Robustness\n");
    FileCatalog cat;
    catalog_init(&cat);

    /* Add 50 entries and verify sequential integrity */
    for (int i = 0; i < 50; i++) {
        char name[64];
        char path[128];
        snprintf(name, sizeof(name), "doc_%03d.bin", i);
        snprintf(path, sizeof(path), "/shared/doc_%03d.bin", i);
        int ret = catalog_add(&cat, name, path, (long long)(i * 1024));
        assert(ret == 0);
    }
    TEST_ASSERT(cat.count == 50, "Added 50 items successfully");

    /* Lookup first, middle, and last added items */
    FileEntry *first_added = catalog_find(&cat, "doc_000.bin");
    TEST_ASSERT(first_added != NULL && first_added->size == 0, "Found tail node (first added)");

    FileEntry *mid_added = catalog_find(&cat, "doc_025.bin");
    TEST_ASSERT(mid_added != NULL && mid_added->size == 25 * 1024, "Found middle node");

    FileEntry *last_added = catalog_find(&cat, "doc_049.bin");
    TEST_ASSERT(last_added != NULL && last_added->size == 49 * 1024, "Found head node (last added)");

    /* Test NULL parameter guards on catalog_add */
    TEST_ASSERT(catalog_add(NULL, "a", "b", 1) == -1, "catalog_add rejects NULL catalog");
    TEST_ASSERT(catalog_add(&cat, NULL, "b", 1) == -1, "catalog_add rejects NULL name");
    TEST_ASSERT(catalog_add(&cat, "a", NULL, 1) == -1, "catalog_add rejects NULL path");

    /* Verify catalog_free safely frees 50 nodes */
    catalog_free(&cat);
    TEST_ASSERT(cat.head == NULL && cat.count == 0, "catalog_free reset head to NULL and count to 0");

    /* Safe double-free check */
    catalog_free(&cat);
    TEST_ASSERT(cat.head == NULL && cat.count == 0, "Double catalog_free is safe");

    /* NULL catalog_free check */
    catalog_free(NULL);
    catalog_print(NULL);
    catalog_init(NULL);
    TEST_ASSERT(1, "NULL catalog operations do not crash");
}

int main(void) {
    printf("========================================\n");
    printf("   P2P File Catalog Data Structure Tests \n");
    printf("========================================\n");

    test_catalog_empty();
    test_catalog_add_and_find();
    test_catalog_scan();
    test_catalog_stress_and_edge_cases();

    printf("========================================\n");
    printf("Summary: %d / %d tests passed.\n", g_tests_passed, g_tests_total);
    printf("========================================\n");

    return (g_tests_passed == g_tests_total) ? 0 : 1;
}
