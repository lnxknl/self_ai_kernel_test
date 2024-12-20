#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* Constants */
#define PAGE_SIZE 4096
#define MAX_PAGES 1024
#define MAX_TYPES 8
#define INVALID_SWAP_TYPE (~0UL)

/* Structure definitions */
struct frontswap_page {
    unsigned char *data;
    bool is_valid;
};

struct frontswap_type {
    struct frontswap_page *pages;
    unsigned long num_pages;
    unsigned long stored_pages;
    pthread_mutex_t lock;
    bool is_active;
};

/* Global variables */
static struct frontswap_type *frontswap_types[MAX_TYPES];
static unsigned long frontswap_enabled_types;

/* Helper functions */
static void *zalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

/* Initialize frontswap type */
static int frontswap_init(unsigned type, unsigned long num_pages)
{
    struct frontswap_type *fs_type;

    if (type >= MAX_TYPES || num_pages == 0 || num_pages > MAX_PAGES)
        return -1;

    if (frontswap_types[type])
        return -1;

    fs_type = zalloc(sizeof(*fs_type));
    if (!fs_type)
        return -1;

    fs_type->pages = zalloc(num_pages * sizeof(struct frontswap_page));
    if (!fs_type->pages) {
        free(fs_type);
        return -1;
    }

    fs_type->num_pages = num_pages;
    fs_type->stored_pages = 0;
    fs_type->is_active = true;

    if (pthread_mutex_init(&fs_type->lock, NULL) != 0) {
        free(fs_type->pages);
        free(fs_type);
        return -1;
    }

    frontswap_types[type] = fs_type;
    frontswap_enabled_types++;

    printf("Initialized frontswap type %u with %lu pages\n", type, num_pages);
    return 0;
}

/* Store a page in frontswap */
static int frontswap_store(unsigned type, unsigned long page_id, unsigned char *data)
{
    struct frontswap_type *fs_type;
    int ret = -1;

    if (type >= MAX_TYPES || !data || page_id == INVALID_SWAP_TYPE)
        return -1;

    fs_type = frontswap_types[type];
    if (!fs_type || !fs_type->is_active)
        return -1;

    pthread_mutex_lock(&fs_type->lock);

    if (page_id < fs_type->num_pages && !fs_type->pages[page_id].is_valid) {
        fs_type->pages[page_id].data = malloc(PAGE_SIZE);
        if (fs_type->pages[page_id].data) {
            memcpy(fs_type->pages[page_id].data, data, PAGE_SIZE);
            fs_type->pages[page_id].is_valid = true;
            fs_type->stored_pages++;
            ret = 0;
        }
    }

    pthread_mutex_unlock(&fs_type->lock);
    return ret;
}

/* Load a page from frontswap */
static int frontswap_load(unsigned type, unsigned long page_id, unsigned char *data)
{
    struct frontswap_type *fs_type;
    int ret = -1;

    if (type >= MAX_TYPES || !data || page_id == INVALID_SWAP_TYPE)
        return -1;

    fs_type = frontswap_types[type];
    if (!fs_type || !fs_type->is_active)
        return -1;

    pthread_mutex_lock(&fs_type->lock);

    if (page_id < fs_type->num_pages && fs_type->pages[page_id].is_valid) {
        memcpy(data, fs_type->pages[page_id].data, PAGE_SIZE);
        ret = 0;
    }

    pthread_mutex_unlock(&fs_type->lock);
    return ret;
}

/* Invalidate a page in frontswap */
static void frontswap_invalidate_page(unsigned type, unsigned long page_id)
{
    struct frontswap_type *fs_type;

    if (type >= MAX_TYPES || page_id == INVALID_SWAP_TYPE)
        return;

    fs_type = frontswap_types[type];
    if (!fs_type || !fs_type->is_active)
        return;

    pthread_mutex_lock(&fs_type->lock);

    if (page_id < fs_type->num_pages && fs_type->pages[page_id].is_valid) {
        free(fs_type->pages[page_id].data);
        fs_type->pages[page_id].data = NULL;
        fs_type->pages[page_id].is_valid = false;
        fs_type->stored_pages--;
    }

    pthread_mutex_unlock(&fs_type->lock);
}

/* Invalidate all pages in a frontswap type */
static void frontswap_invalidate_area(unsigned type)
{
    struct frontswap_type *fs_type;
    unsigned long i;

    if (type >= MAX_TYPES)
        return;

    fs_type = frontswap_types[type];
    if (!fs_type || !fs_type->is_active)
        return;

    pthread_mutex_lock(&fs_type->lock);

    for (i = 0; i < fs_type->num_pages; i++) {
        if (fs_type->pages[i].is_valid) {
            free(fs_type->pages[i].data);
            fs_type->pages[i].data = NULL;
            fs_type->pages[i].is_valid = false;
        }
    }
    fs_type->stored_pages = 0;

    pthread_mutex_unlock(&fs_type->lock);
}

/* Print frontswap statistics */
static void print_frontswap_stats(unsigned type)
{
    struct frontswap_type *fs_type;

    if (type >= MAX_TYPES)
        return;

    fs_type = frontswap_types[type];
    if (!fs_type)
        return;

    pthread_mutex_lock(&fs_type->lock);
    printf("\nFrontswap Type %u Statistics:\n", type);
    printf("Total pages: %lu\n", fs_type->num_pages);
    printf("Stored pages: %lu\n", fs_type->stored_pages);
    printf("Free pages: %lu\n", fs_type->num_pages - fs_type->stored_pages);
    printf("Status: %s\n", fs_type->is_active ? "Active" : "Inactive");
    pthread_mutex_unlock(&fs_type->lock);
}

/* Cleanup frontswap type */
static void frontswap_cleanup(unsigned type)
{
    struct frontswap_type *fs_type;
    unsigned long i;

    if (type >= MAX_TYPES)
        return;

    fs_type = frontswap_types[type];
    if (!fs_type)
        return;

    pthread_mutex_lock(&fs_type->lock);

    for (i = 0; i < fs_type->num_pages; i++) {
        if (fs_type->pages[i].is_valid) {
            free(fs_type->pages[i].data);
        }
    }

    free(fs_type->pages);
    pthread_mutex_unlock(&fs_type->lock);
    pthread_mutex_destroy(&fs_type->lock);
    free(fs_type);
    frontswap_types[type] = NULL;
    frontswap_enabled_types--;
}

int main()
{
    unsigned char test_data[PAGE_SIZE];
    unsigned char read_data[PAGE_SIZE];
    int i, ret;

    printf("Frontswap Test Program\n");
    printf("=====================\n\n");

    /* Test 1: Initialize frontswap types */
    printf("Test 1: Initialize frontswap types\n");
    printf("--------------------------------\n");
    ret = frontswap_init(0, 100);
    printf("Initialized type 0: %s\n", ret == 0 ? "Success" : "Failed");
    ret = frontswap_init(1, 50);
    printf("Initialized type 1: %s\n", ret == 0 ? "Success" : "Failed");
    print_frontswap_stats(0);
    print_frontswap_stats(1);

    /* Test 2: Store pages */
    printf("\nTest 2: Store pages\n");
    printf("-----------------\n");
    /* Create test data pattern */
    for (i = 0; i < PAGE_SIZE; i++)
        test_data[i] = i & 0xFF;

    /* Store some pages in type 0 */
    for (i = 0; i < 5; i++) {
        ret = frontswap_store(0, i, test_data);
        printf("Stored page %d in type 0: %s\n", i, ret == 0 ? "Success" : "Failed");
    }
    print_frontswap_stats(0);

    /* Test 3: Load pages */
    printf("\nTest 3: Load pages\n");
    printf("----------------\n");
    for (i = 0; i < 5; i++) {
        memset(read_data, 0, PAGE_SIZE);
        ret = frontswap_load(0, i, read_data);
        printf("Loaded page %d from type 0: %s\n", i, ret == 0 ? "Success" : "Failed");
        if (ret == 0) {
            /* Verify data */
            if (memcmp(test_data, read_data, PAGE_SIZE) == 0)
                printf("Data verification successful for page %d\n", i);
            else
                printf("Data verification failed for page %d\n", i);
        }
    }

    /* Test 4: Invalidate pages */
    printf("\nTest 4: Invalidate pages\n");
    printf("----------------------\n");
    frontswap_invalidate_page(0, 2);
    printf("Invalidated page 2 in type 0\n");
    print_frontswap_stats(0);

    /* Try to load invalidated page */
    ret = frontswap_load(0, 2, read_data);
    printf("Loading invalidated page 2: %s\n", ret == 0 ? "Success" : "Failed");

    /* Test 5: Invalidate area */
    printf("\nTest 5: Invalidate area\n");
    printf("---------------------\n");
    frontswap_invalidate_area(0);
    printf("Invalidated all pages in type 0\n");
    print_frontswap_stats(0);

    /* Cleanup */
    printf("\nCleaning up frontswap\n");
    frontswap_cleanup(0);
    frontswap_cleanup(1);
    printf("Frontswap cleanup complete\n");

    return 0;
}
