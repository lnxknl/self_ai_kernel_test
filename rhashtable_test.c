#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Hash table implementation */
#define HASH_TABLE_MIN_SIZE 4
#define HASH_TABLE_MAX_SIZE 65536
#define HASH_TABLE_LOAD_FACTOR 75    /* 75% */

struct rhash_head {
    struct rhash_head *next;
    uint32_t hash;
};

struct bucket_table {
    unsigned int size;
    unsigned int used;
    struct rhash_head *buckets[];
};

struct rhashtable {
    struct bucket_table *tbl;
    unsigned int key_len;
    unsigned int min_size;
};

/* MurmurHash3 implementation */
static uint32_t murmur3_32(const uint8_t *key, size_t len)
{
    uint32_t h = 0x12345678;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const int nblocks = len / 4;
    const uint32_t *blocks = (const uint32_t *)(key);

    for (int i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];

        k *= c1;
        k = (k << 15) | (k >> 17);
        k *= c2;

        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    const uint8_t *tail = key + nblocks * 4;
    uint32_t k1 = 0;

    switch (len & 3) {
    case 3:
        k1 ^= tail[2] << 16;
    case 2:
        k1 ^= tail[1] << 8;
    case 1:
        k1 ^= tail[0];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h ^= k1;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

/* Create new bucket table */
static struct bucket_table *alloc_bucket_table(unsigned int size)
{
    struct bucket_table *tbl;
    size_t alloc_size;

    alloc_size = sizeof(*tbl) + size * sizeof(tbl->buckets[0]);
    tbl = calloc(1, alloc_size);
    if (!tbl)
        return NULL;

    tbl->size = size;
    tbl->used = 0;
    return tbl;
}

/* Initialize hash table */
int rhashtable_init(struct rhashtable *ht, unsigned int key_len)
{
    struct bucket_table *tbl;

    if (key_len == 0)
        return -1;

    ht->key_len = key_len;
    ht->min_size = HASH_TABLE_MIN_SIZE;

    tbl = alloc_bucket_table(ht->min_size);
    if (!tbl)
        return -1;

    ht->tbl = tbl;
    return 0;
}

/* Insert element into hash table */
int rhashtable_insert(struct rhashtable *ht, struct rhash_head *obj, 
                     const void *key)
{
    struct bucket_table *tbl = ht->tbl;
    uint32_t hash;
    unsigned int bucket;

    hash = murmur3_32(key, ht->key_len);
    bucket = hash % tbl->size;
    obj->hash = hash;

    /* Insert at head of bucket */
    obj->next = tbl->buckets[bucket];
    tbl->buckets[bucket] = obj;
    tbl->used++;

    /* Check if resize needed */
    if (tbl->used * 100 / tbl->size > HASH_TABLE_LOAD_FACTOR) {
        /* In a real implementation, we would resize here */
        printf("Warning: Load factor exceeded, resize needed\n");
    }

    return 0;
}

/* Lookup element in hash table */
struct rhash_head *rhashtable_lookup(struct rhashtable *ht,
                                   const void *key)
{
    struct bucket_table *tbl = ht->tbl;
    uint32_t hash;
    unsigned int bucket;
    struct rhash_head *he;

    hash = murmur3_32(key, ht->key_len);
    bucket = hash % tbl->size;

    for (he = tbl->buckets[bucket]; he; he = he->next) {
        if (he->hash == hash)
            return he;
    }

    return NULL;
}

/* Remove element from hash table */
int rhashtable_remove(struct rhashtable *ht, const void *key)
{
    struct bucket_table *tbl = ht->tbl;
    uint32_t hash;
    unsigned int bucket;
    struct rhash_head *he, *prev = NULL;

    hash = murmur3_32(key, ht->key_len);
    bucket = hash % tbl->size;

    for (he = tbl->buckets[bucket]; he; prev = he, he = he->next) {
        if (he->hash == hash) {
            if (prev)
                prev->next = he->next;
            else
                tbl->buckets[bucket] = he->next;
            tbl->used--;
            return 0;
        }
    }

    return -1;
}

/* Free hash table */
void rhashtable_free(struct rhashtable *ht)
{
    if (ht && ht->tbl)
        free(ht->tbl);
}

/* Test structure */
struct test_obj {
    struct rhash_head node;
    int key;
    char value[32];
};

/* Print hash table statistics */
void print_hashtable_stats(struct rhashtable *ht)
{
    struct bucket_table *tbl = ht->tbl;
    printf("Hash Table Statistics:\n");
    printf("- Table size: %u\n", tbl->size);
    printf("- Elements: %u\n", tbl->used);
    printf("- Load factor: %u%%\n", tbl->used * 100 / tbl->size);
    
    /* Print bucket distribution */
    printf("- Bucket distribution:\n");
    for (unsigned int i = 0; i < tbl->size; i++) {
        unsigned int count = 0;
        struct rhash_head *he;
        for (he = tbl->buckets[i]; he; he = he->next)
            count++;
        if (count > 0)
            printf("  Bucket %u: %u elements\n", i, count);
    }
    printf("\n");
}

int main()
{
    struct rhashtable ht;
    struct test_obj *obj;
    const int num_test_objects = 10;
    struct test_obj test_objects[num_test_objects];

    printf("Resizable Hash Table Test Program\n");
    printf("================================\n\n");

    /* Initialize hash table */
    printf("1. Initializing hash table...\n");
    if (rhashtable_init(&ht, sizeof(int)) != 0) {
        printf("Failed to initialize hash table!\n");
        return -1;
    }
    printf("Hash table initialized successfully\n\n");

    /* Insert test objects */
    printf("2. Inserting test objects...\n");
    for (int i = 0; i < num_test_objects; i++) {
        test_objects[i].key = i * 10;
        snprintf(test_objects[i].value, sizeof(test_objects[i].value),
                "Value-%d", i * 10);
        
        printf("Inserting key %d (\"%s\"): ", 
               test_objects[i].key, test_objects[i].value);
        
        if (rhashtable_insert(&ht, &test_objects[i].node, &test_objects[i].key) == 0)
            printf("success\n");
        else
            printf("failed\n");
    }
    printf("\n");

    /* Print hash table statistics */
    print_hashtable_stats(&ht);

    /* Lookup test */
    printf("3. Testing lookups...\n");
    int test_keys[] = {0, 20, 50, 90, 15};
    for (int i = 0; i < sizeof(test_keys)/sizeof(test_keys[0]); i++) {
        printf("Looking up key %d: ", test_keys[i]);
        struct rhash_head *node = rhashtable_lookup(&ht, &test_keys[i]);
        if (node) {
            obj = (struct test_obj *)((char *)node - offsetof(struct test_obj, node));
            printf("found \"%s\"\n", obj->value);
        } else {
            printf("not found\n");
        }
    }
    printf("\n");

    /* Remove test */
    printf("4. Testing removal...\n");
    int remove_keys[] = {0, 30, 60, 15};
    for (int i = 0; i < sizeof(remove_keys)/sizeof(remove_keys[0]); i++) {
        printf("Removing key %d: ", remove_keys[i]);
        if (rhashtable_remove(&ht, &remove_keys[i]) == 0)
            printf("success\n");
        else
            printf("not found\n");
    }
    printf("\n");

    /* Print final statistics */
    printf("5. Final hash table state:\n");
    print_hashtable_stats(&ht);

    /* Clean up */
    printf("6. Cleaning up...\n");
    rhashtable_free(&ht);
    printf("Hash table freed\n");

    return 0;
}
