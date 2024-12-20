#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

/* Constants */
#define PAGE_SIZE       4096
#define PAGE_CACHE_SIZE 64
#define MAX_READAHEAD   (PAGE_SIZE * 32)
#define MIN_READAHEAD   (PAGE_SIZE * 2)
#define MAX_FILE_SIZE   (PAGE_SIZE * PAGE_CACHE_SIZE)

/* Structure definitions */
struct page {
    unsigned long index;     /* Page index in file */
    unsigned char *data;     /* Page data */
    bool uptodate;          /* Page is valid */
    pthread_mutex_t lock;    /* Page lock */
};

struct file {
    char *name;                         /* File name */
    unsigned long size;                 /* File size */
    unsigned long ra_pages;             /* Current readahead window size */
    unsigned long pos;                  /* Current read position */
    struct page *page_cache[PAGE_CACHE_SIZE]; /* Page cache */
    pthread_mutex_t lock;               /* File lock */
};

struct readahead_control {
    struct file *file;
    unsigned long start;     /* Start page index */
    unsigned long size;      /* Number of pages to read */
    unsigned long async_size;/* Asynchronous readahead size */
};

/* Global variables */
static unsigned char simulated_disk[MAX_FILE_SIZE];

/* Helper functions */
static struct page *alloc_page(void)
{
    struct page *page = malloc(sizeof(*page));
    if (!page)
        return NULL;

    page->data = malloc(PAGE_SIZE);
    if (!page->data) {
        free(page);
        return NULL;
    }

    if (pthread_mutex_init(&page->lock, NULL) != 0) {
        free(page->data);
        free(page);
        return NULL;
    }

    page->uptodate = false;
    return page;
}

static void free_page(struct page *page)
{
    if (!page)
        return;
    pthread_mutex_destroy(&page->lock);
    free(page->data);
    free(page);
}

/* Simulated disk I/O */
static int read_from_disk(struct file *file, struct page *page)
{
    unsigned long offset = page->index * PAGE_SIZE;
    
    if (offset >= file->size)
        return -EINVAL;

    pthread_mutex_lock(&page->lock);
    
    /* Simulate I/O delay */
    struct timespec ts = {0, 10000000}; /* 10ms */
    nanosleep(&ts, NULL);
    
    unsigned long bytes = PAGE_SIZE;
    if (offset + bytes > file->size)
        bytes = file->size - offset;
        
    memcpy(page->data, simulated_disk + offset, bytes);
    if (bytes < PAGE_SIZE)
        memset(page->data + bytes, 0, PAGE_SIZE - bytes);
        
    page->uptodate = true;
    pthread_mutex_unlock(&page->lock);
    
    return 0;
}

/* Page cache operations */
static struct page *find_or_create_page(struct file *file, unsigned long index)
{
    pthread_mutex_lock(&file->lock);
    
    /* Check if page exists in cache */
    if (file->page_cache[index % PAGE_CACHE_SIZE] &&
        file->page_cache[index % PAGE_CACHE_SIZE]->index == index) {
        struct page *page = file->page_cache[index % PAGE_CACHE_SIZE];
        pthread_mutex_unlock(&file->lock);
        return page;
    }
    
    /* Create new page */
    struct page *page = alloc_page();
    if (!page) {
        pthread_mutex_unlock(&file->lock);
        return NULL;
    }
    
    page->index = index;
    
    /* Free old page if exists */
    if (file->page_cache[index % PAGE_CACHE_SIZE])
        free_page(file->page_cache[index % PAGE_CACHE_SIZE]);
        
    file->page_cache[index % PAGE_CACHE_SIZE] = page;
    pthread_mutex_unlock(&file->lock);
    
    return page;
}

/* Readahead core functions */
static void ondemand_readahead(struct readahead_control *ractl)
{
    unsigned long index = ractl->start;
    unsigned long nr_pages = ractl->size;
    struct file *file = ractl->file;
    
    printf("Readahead: start=%lu, pages=%lu\n", index, nr_pages);
    
    /* Read pages */
    for (unsigned long i = 0; i < nr_pages; i++) {
        struct page *page = find_or_create_page(file, index + i);
        if (!page)
            continue;
            
        if (!page->uptodate)
            read_from_disk(file, page);
    }
}

static unsigned long get_next_readahead_size(struct file *file, unsigned long current_size)
{
    if (current_size >= MAX_READAHEAD)
        return MAX_READAHEAD;
        
    current_size = current_size * 2;
    
    if (current_size > MAX_READAHEAD)
        current_size = MAX_READAHEAD;
        
    return current_size;
}

/* File operations */
static struct file *create_test_file(const char *name, unsigned long size)
{
    struct file *file = malloc(sizeof(*file));
    if (!file)
        return NULL;
        
    file->name = strdup(name);
    file->size = size;
    file->ra_pages = MIN_READAHEAD / PAGE_SIZE;
    file->pos = 0;
    memset(file->page_cache, 0, sizeof(file->page_cache));
    
    if (pthread_mutex_init(&file->lock, NULL) != 0) {
        free(file->name);
        free(file);
        return NULL;
    }
    
    return file;
}

static void free_file(struct file *file)
{
    if (!file)
        return;
        
    for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
        if (file->page_cache[i])
            free_page(file->page_cache[i]);
    }
    
    pthread_mutex_destroy(&file->lock);
    free(file->name);
    free(file);
}

static ssize_t read_file(struct file *file, void *buf, size_t count)
{
    if (file->pos >= file->size)
        return 0;
        
    /* Calculate read parameters */
    unsigned long page_index = file->pos / PAGE_SIZE;
    unsigned long page_offset = file->pos % PAGE_SIZE;
    size_t bytes_to_read = count;
    
    if (file->pos + bytes_to_read > file->size)
        bytes_to_read = file->size - file->pos;
        
    /* Setup readahead */
    struct readahead_control ractl = {
        .file = file,
        .start = page_index,
        .size = (bytes_to_read + PAGE_SIZE - 1) / PAGE_SIZE,
        .async_size = file->ra_pages
    };
    
    /* Perform readahead */
    ondemand_readahead(&ractl);
    
    /* Update readahead window */
    file->ra_pages = get_next_readahead_size(file, file->ra_pages);
    
    /* Copy data to user buffer */
    size_t bytes_read = 0;
    while (bytes_read < bytes_to_read) {
        struct page *page = find_or_create_page(file, page_index);
        if (!page)
            break;
            
        if (!page->uptodate && read_from_disk(file, page) != 0)
            break;
            
        size_t page_bytes = PAGE_SIZE - page_offset;
        if (page_bytes > bytes_to_read - bytes_read)
            page_bytes = bytes_to_read - bytes_read;
            
        memcpy(buf + bytes_read, page->data + page_offset, page_bytes);
        
        bytes_read += page_bytes;
        page_offset = 0;
        page_index++;
    }
    
    file->pos += bytes_read;
    return bytes_read;
}

int main()
{
    printf("File Readahead Test Program\n");
    printf("==========================\n\n");
    
    /* Initialize simulated disk with pattern */
    printf("Initializing test data...\n");
    for (int i = 0; i < MAX_FILE_SIZE; i++)
        simulated_disk[i] = i & 0xFF;
        
    /* Create test file */
    struct file *file = create_test_file("test.dat", MAX_FILE_SIZE);
    if (!file) {
        printf("Failed to create test file\n");
        return 1;
    }
    
    printf("Created test file: size=%lu bytes\n", file->size);
    
    /* Test 1: Sequential read with increasing readahead */
    printf("\nTest 1: Sequential read with readahead\n");
    printf("-------------------------------------\n");
    
    char buffer[PAGE_SIZE * 4];
    size_t total_read = 0;
    int iteration = 0;
    
    while (total_read < file->size && iteration < 5) {
        ssize_t bytes = read_file(file, buffer, sizeof(buffer));
        if (bytes <= 0)
            break;
            
        printf("Read %zd bytes, readahead window: %lu pages\n", 
               bytes, file->ra_pages);
               
        /* Verify data */
        bool data_valid = true;
        for (size_t i = 0; i < bytes; i++) {
            if ((unsigned char)buffer[i] != ((total_read + i) & 0xFF)) {
                data_valid = false;
                break;
            }
        }
        printf("Data verification: %s\n", data_valid ? "PASSED" : "FAILED");
        
        total_read += bytes;
        iteration++;
    }
    
    printf("\nTotal bytes read: %zu\n", total_read);
    
    /* Test 2: Random access */
    printf("\nTest 2: Random access\n");
    printf("-------------------\n");
    
    /* Reset file position */
    file->pos = 0;
    file->ra_pages = MIN_READAHEAD / PAGE_SIZE;
    
    /* Perform some random reads */
    unsigned long positions[] = {
        MAX_FILE_SIZE / 2,
        PAGE_SIZE * 10,
        MAX_FILE_SIZE - PAGE_SIZE,
        0
    };
    
    for (int i = 0; positions[i] < MAX_FILE_SIZE; i++) {
        file->pos = positions[i];
        printf("\nSeeking to position %lu\n", file->pos);
        
        ssize_t bytes = read_file(file, buffer, PAGE_SIZE);
        printf("Read %zd bytes at position %lu\n", bytes, positions[i]);
        
        /* Verify first few bytes */
        bool data_valid = true;
        for (int j = 0; j < 16 && j < bytes; j++) {
            if ((unsigned char)buffer[j] != ((positions[i] + j) & 0xFF)) {
                data_valid = false;
                break;
            }
        }
        printf("Data verification: %s\n", data_valid ? "PASSED" : "FAILED");
    }
    
    /* Cleanup */
    free_file(file);
    printf("\nTest complete\n");
    
    return 0;
}
