#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* List implementation */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *new,
                            struct list_head *prev,
                            struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

static inline void list_move(struct list_head *list, struct list_head *head)
{
    list_del(list);
    list_add(list, head);
}

#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* Hash table implementation */
#define HASH_SIZE 16
#define HASH_MASK (HASH_SIZE - 1)

struct hlist_node {
    struct hlist_node *next;
    struct list_head *prev;
};

struct hlist_head {
    struct hlist_node *first;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT

static inline void INIT_HLIST_HEAD(struct hlist_head *h)
{
    h->first = NULL;
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    n->next = h->first;
    if (h->first)
        h->first->prev = (struct list_head *)n;
    h->first = n;
    n->prev = (struct list_head *)h;
}

static inline void hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct list_head *pprev = n->prev;
    
    if (pprev) {
        if (next)
            next->prev = pprev;
        *(struct hlist_node **)pprev = next;
    }
}

/* LRU Cache implementation */
struct lru_cache {
    unsigned int max_size;
    unsigned int size;
    struct list_head lru_list;
    struct hlist_head hash_table[HASH_SIZE];
};

struct cache_entry {
    int key;
    int value;
    struct list_head lru;
    struct hlist_node hash;
};

/* Initialize LRU cache */
void lru_cache_init(struct lru_cache *cache, unsigned int max_size)
{
    int i;
    cache->max_size = max_size;
    cache->size = 0;
    INIT_LIST_HEAD(&cache->lru_list);
    for (i = 0; i < HASH_SIZE; i++)
        INIT_HLIST_HEAD(&cache->hash_table[i]);
}

/* Get hash bucket for key */
static inline struct hlist_head *get_hash_bucket(struct lru_cache *cache, int key)
{
    return &cache->hash_table[key & HASH_MASK];
}

/* Find entry by key */
static struct cache_entry *find_entry(struct lru_cache *cache, int key)
{
    struct hlist_head *bucket = get_hash_bucket(cache, key);
    struct hlist_node *node;
    struct cache_entry *entry;

    for (node = bucket->first; node; node = node->next) {
        entry = list_entry(node, struct cache_entry, hash);
        if (entry->key == key)
            return entry;
    }
    return NULL;
}

/* Add new entry to cache */
static void add_entry(struct lru_cache *cache, int key, int value)
{
    struct cache_entry *entry = malloc(sizeof(*entry));
    if (!entry)
        return;

    entry->key = key;
    entry->value = value;
    
    /* Add to hash table */
    hlist_add_head(&entry->hash, get_hash_bucket(cache, key));
    
    /* Add to LRU list */
    list_add(&entry->lru, &cache->lru_list);
    cache->size++;
}

/* Remove least recently used entry */
static void remove_lru_entry(struct lru_cache *cache)
{
    struct list_head *lru_item = cache->lru_list.prev;
    struct cache_entry *entry = list_entry(lru_item, struct cache_entry, lru);
    
    list_del(&entry->lru);
    hlist_del(&entry->hash);
    free(entry);
    cache->size--;
}

/* Get value from cache */
int lru_cache_get(struct lru_cache *cache, int key)
{
    struct cache_entry *entry = find_entry(cache, key);
    if (entry) {
        /* Move to front of LRU list */
        list_move(&entry->lru, &cache->lru_list);
        return entry->value;
    }
    return -1;  /* Cache miss */
}

/* Put value into cache */
void lru_cache_put(struct lru_cache *cache, int key, int value)
{
    struct cache_entry *entry = find_entry(cache, key);
    
    if (entry) {
        /* Update existing entry */
        entry->value = value;
        list_move(&entry->lru, &cache->lru_list);
    } else {
        /* Remove LRU item if cache is full */
        if (cache->size >= cache->max_size)
            remove_lru_entry(cache);
        
        /* Add new entry */
        add_entry(cache, key, value);
    }
}

/* Print cache contents */
void print_cache_contents(struct lru_cache *cache)
{
    struct list_head *pos;
    struct cache_entry *entry;
    int count = 0;

    printf("Cache contents (from most recent to least recent):\n");
    list_for_each(pos, &cache->lru_list) {
        entry = list_entry(pos, struct cache_entry, lru);
        printf("  [%d] Key: %d, Value: %d\n", count++, entry->key, entry->value);
    }
    printf("Cache size: %u/%u\n\n", cache->size, cache->max_size);
}

int main()
{
    struct lru_cache cache;
    int value;

    printf("LRU Cache Test Program\n");
    printf("=====================\n\n");

    /* Initialize cache */
    printf("1. Initializing LRU cache with size 3...\n");
    lru_cache_init(&cache, 3);
    print_cache_contents(&cache);

    /* Test cache insertion */
    printf("2. Testing cache insertion...\n");
    printf("Inserting (1,10), (2,20), (3,30)\n");
    lru_cache_put(&cache, 1, 10);
    lru_cache_put(&cache, 2, 20);
    lru_cache_put(&cache, 3, 30);
    print_cache_contents(&cache);

    /* Test cache access */
    printf("3. Testing cache access...\n");
    printf("Accessing key 2\n");
    value = lru_cache_get(&cache, 2);
    printf("Value for key 2: %d\n", value);
    print_cache_contents(&cache);

    /* Test cache eviction */
    printf("4. Testing cache eviction...\n");
    printf("Inserting (4,40) into full cache\n");
    lru_cache_put(&cache, 4, 40);
    print_cache_contents(&cache);

    /* Test cache update */
    printf("5. Testing cache update...\n");
    printf("Updating key 3 with value 35\n");
    lru_cache_put(&cache, 3, 35);
    print_cache_contents(&cache);

    /* Test cache miss */
    printf("6. Testing cache miss...\n");
    printf("Accessing non-existent key 10\n");
    value = lru_cache_get(&cache, 10);
    printf("Value for key 10: %d (expected: -1)\n", value);
    print_cache_contents(&cache);

    printf("LRU Cache test completed successfully\n");
    return 0;
}
