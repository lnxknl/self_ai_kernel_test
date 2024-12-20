#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* Constants */
#define MEMPOOL_INIT_SIZE  4
#define MEMPOOL_MAX_SIZE   16
#define TEST_ELEM_SIZE     64

/* Structure definitions */
struct mempool_element {
    void *ptr;
    struct mempool_element *next;
};

struct mempool {
    size_t min_nr;           /* Minimum number of elements */
    size_t curr_nr;          /* Current number of elements */
    size_t elem_size;        /* Size of each element */
    pthread_mutex_t lock;    /* Pool lock */
    struct mempool_element *elements; /* Free elements list */
    void *(*alloc)(size_t); /* Allocation function */
    void (*free)(void *);   /* Free function */
};

/* Helper functions */
static void *mempool_alloc_node(size_t size)
{
    return malloc(size);
}

static void mempool_free_node(void *ptr)
{
    free(ptr);
}

/* Initialize memory pool */
static struct mempool *mempool_create(
    size_t min_nr,
    size_t elem_size,
    void *(*alloc)(size_t),
    void (*free)(void *))
{
    struct mempool *pool;
    size_t i;

    if (!min_nr || !elem_size)
        return NULL;

    pool = malloc(sizeof(*pool));
    if (!pool)
        return NULL;

    pool->min_nr = min_nr;
    pool->curr_nr = 0;
    pool->elem_size = elem_size;
    pool->elements = NULL;
    pool->alloc = alloc ? alloc : mempool_alloc_node;
    pool->free = free ? free : mempool_free_node;

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        return NULL;
    }

    /* Pre-allocate minimum number of elements */
    for (i = 0; i < min_nr; i++) {
        struct mempool_element *elem = malloc(sizeof(*elem));
        if (!elem)
            goto cleanup;

        elem->ptr = pool->alloc(elem_size);
        if (!elem->ptr) {
            free(elem);
            goto cleanup;
        }

        elem->next = pool->elements;
        pool->elements = elem;
        pool->curr_nr++;
    }

    return pool;

cleanup:
    while (pool->elements) {
        struct mempool_element *elem = pool->elements;
        pool->elements = elem->next;
        pool->free(elem->ptr);
        free(elem);
    }
    pthread_mutex_destroy(&pool->lock);
    free(pool);
    return NULL;
}

/* Allocate element from pool */
static void *mempool_alloc(struct mempool *pool)
{
    struct mempool_element *elem;
    void *ptr = NULL;

    if (!pool)
        return NULL;

    pthread_mutex_lock(&pool->lock);

    if (pool->elements) {
        /* Get element from pool */
        elem = pool->elements;
        pool->elements = elem->next;
        ptr = elem->ptr;
        free(elem);
        pool->curr_nr--;
    } else if (pool->curr_nr < MEMPOOL_MAX_SIZE) {
        /* Allocate new element */
        ptr = pool->alloc(pool->elem_size);
        if (ptr)
            pool->curr_nr++;
    }

    pthread_mutex_unlock(&pool->lock);
    return ptr;
}

/* Return element to pool */
static void mempool_free(struct mempool *pool, void *ptr)
{
    struct mempool_element *elem;

    if (!pool || !ptr)
        return;

    pthread_mutex_lock(&pool->lock);

    if (pool->curr_nr < pool->min_nr) {
        /* Add back to pool */
        elem = malloc(sizeof(*elem));
        if (elem) {
            elem->ptr = ptr;
            elem->next = pool->elements;
            pool->elements = elem;
            pool->curr_nr++;
            ptr = NULL;
        }
    }

    if (ptr) {
        /* Free if pool is full */
        pool->free(ptr);
    }

    pthread_mutex_unlock(&pool->lock);
}

/* Destroy memory pool */
static void mempool_destroy(struct mempool *pool)
{
    if (!pool)
        return;

    pthread_mutex_lock(&pool->lock);

    while (pool->elements) {
        struct mempool_element *elem = pool->elements;
        pool->elements = elem->next;
        pool->free(elem->ptr);
        free(elem);
    }

    pthread_mutex_unlock(&pool->lock);
    pthread_mutex_destroy(&pool->lock);
    free(pool);
}

/* Print pool statistics */
static void print_pool_stats(struct mempool *pool)
{
    if (!pool)
        return;

    pthread_mutex_lock(&pool->lock);
    printf("\nMempool Statistics:\n");
    printf("Minimum elements: %zu\n", pool->min_nr);
    printf("Current elements: %zu\n", pool->curr_nr);
    printf("Element size: %zu bytes\n", pool->elem_size);
    pthread_mutex_unlock(&pool->lock);
}

/* Test structure */
struct test_struct {
    int id;
    char data[TEST_ELEM_SIZE - sizeof(int)];
};

int main()
{
    struct mempool *pool;
    struct test_struct *elements[MEMPOOL_MAX_SIZE];
    int i;

    printf("Memory Pool Test Program\n");
    printf("=======================\n\n");

    /* Create memory pool */
    pool = mempool_create(MEMPOOL_INIT_SIZE, sizeof(struct test_struct), NULL, NULL);
    if (!pool) {
        printf("Failed to create memory pool\n");
        return -1;
    }

    printf("Memory pool created with initial size %d\n", MEMPOOL_INIT_SIZE);
    print_pool_stats(pool);

    /* Test 1: Allocate initial elements */
    printf("\nTest 1: Allocate initial elements\n");
    printf("--------------------------------\n");
    for (i = 0; i < MEMPOOL_INIT_SIZE; i++) {
        elements[i] = mempool_alloc(pool);
        if (elements[i]) {
            elements[i]->id = i;
            snprintf(elements[i]->data, sizeof(elements[i]->data),
                    "Element %d data", i);
            printf("Allocated element %d: %s\n", i, elements[i]->data);
        }
    }
    print_pool_stats(pool);

    /* Test 2: Free some elements */
    printf("\nTest 2: Free some elements\n");
    printf("-------------------------\n");
    for (i = 0; i < MEMPOOL_INIT_SIZE / 2; i++) {
        if (elements[i]) {
            printf("Freeing element %d: %s\n", i, elements[i]->data);
            mempool_free(pool, elements[i]);
            elements[i] = NULL;
        }
    }
    print_pool_stats(pool);

    /* Test 3: Allocate beyond initial size */
    printf("\nTest 3: Allocate beyond initial size\n");
    printf("---------------------------------\n");
    for (i = MEMPOOL_INIT_SIZE; i < MEMPOOL_MAX_SIZE; i++) {
        elements[i] = mempool_alloc(pool);
        if (elements[i]) {
            elements[i]->id = i;
            snprintf(elements[i]->data, sizeof(elements[i]->data),
                    "Element %d data", i);
            printf("Allocated element %d: %s\n", i, elements[i]->data);
        }
    }
    print_pool_stats(pool);

    /* Test 4: Try to allocate when pool is full */
    printf("\nTest 4: Try to allocate when pool is full\n");
    printf("--------------------------------------\n");
    void *overflow = mempool_alloc(pool);
    if (!overflow) {
        printf("Successfully prevented overflow allocation\n");
    }
    print_pool_stats(pool);

    /* Test 5: Free all elements */
    printf("\nTest 5: Free all elements\n");
    printf("-----------------------\n");
    for (i = 0; i < MEMPOOL_MAX_SIZE; i++) {
        if (elements[i]) {
            printf("Freeing element %d: %s\n", i, elements[i]->data);
            mempool_free(pool, elements[i]);
            elements[i] = NULL;
        }
    }
    print_pool_stats(pool);

    /* Cleanup */
    printf("\nCleaning up memory pool\n");
    mempool_destroy(pool);
    printf("Memory pool destroyed\n");

    return 0;
}
