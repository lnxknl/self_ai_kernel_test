#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

/* Constants */
#define MAX_NODES 100
#define MAX_LISTS 4
#define NODE_DATA_SIZE 64

/* Structure definitions */
struct list_head {
    struct list_head *next, *prev;
};

struct list_lru_node {
    struct list_head lru;    /* LRU list node */
    struct list_head node;   /* Item list node */
    unsigned long access_time;
    char data[NODE_DATA_SIZE];
    bool active;
};

struct list_lru {
    struct list_head list;   /* List to be managed */
    size_t nr_items;        /* Number of items in list */
    pthread_mutex_t lock;    /* Protects the list */
    const char *name;       /* Name for debugging */
};

/* List manipulation helpers */
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
    entry->next = NULL;
    entry->prev = NULL;
}

static inline bool list_empty(const struct list_head *head)
{
    return head->next == head;
}

/* LRU list operations */
static int list_lru_init(struct list_lru *lru, const char *name)
{
    if (!lru)
        return -1;

    INIT_LIST_HEAD(&lru->list);
    lru->nr_items = 0;
    lru->name = name;

    if (pthread_mutex_init(&lru->lock, NULL) != 0)
        return -1;

    return 0;
}

static void list_lru_destroy(struct list_lru *lru)
{
    if (!lru)
        return;

    pthread_mutex_lock(&lru->lock);
    while (!list_empty(&lru->list)) {
        struct list_head *entry = lru->list.next;
        struct list_lru_node *node = (struct list_lru_node *)entry;
        list_del(&node->lru);
        free(node);
    }
    pthread_mutex_unlock(&lru->lock);
    pthread_mutex_destroy(&lru->lock);
}

static bool list_lru_add(struct list_lru *lru, struct list_lru_node *item)
{
    if (!lru || !item)
        return false;

    pthread_mutex_lock(&lru->lock);
    list_add_tail(&item->lru, &lru->list);
    item->active = true;
    item->access_time = time(NULL);
    lru->nr_items++;
    pthread_mutex_unlock(&lru->lock);

    return true;
}

static bool list_lru_del(struct list_lru *lru, struct list_lru_node *item)
{
    if (!lru || !item || !item->active)
        return false;

    pthread_mutex_lock(&lru->lock);
    list_del(&item->lru);
    item->active = false;
    lru->nr_items--;
    pthread_mutex_unlock(&lru->lock);

    return true;
}

/* Move to front of LRU on access */
static void list_lru_touch(struct list_lru *lru, struct list_lru_node *item)
{
    if (!lru || !item || !item->active)
        return;

    pthread_mutex_lock(&lru->lock);
    list_del(&item->lru);
    list_add(&item->lru, &lru->list);
    item->access_time = time(NULL);
    pthread_mutex_unlock(&lru->lock);
}

/* Get least recently used item */
static struct list_lru_node *list_lru_get_tail(struct list_lru *lru)
{
    struct list_lru_node *node = NULL;

    if (!lru)
        return NULL;

    pthread_mutex_lock(&lru->lock);
    if (!list_empty(&lru->list)) {
        struct list_head *tail = lru->list.prev;
        node = (struct list_lru_node *)tail;
    }
    pthread_mutex_unlock(&lru->lock);

    return node;
}

/* Print LRU statistics */
static void print_lru_stats(struct list_lru *lru)
{
    if (!lru)
        return;

    pthread_mutex_lock(&lru->lock);
    printf("\nLRU List Statistics (%s):\n", lru->name);
    printf("Number of items: %zu\n", lru->nr_items);
    
    if (!list_empty(&lru->list)) {
        printf("Items from most to least recently used:\n");
        struct list_head *pos;
        int count = 0;
        list_for_each(pos, &lru->list) {
            struct list_lru_node *node = (struct list_lru_node *)pos;
            printf("  %d. %s (accessed: %lu)\n", ++count, node->data, node->access_time);
        }
    }
    pthread_mutex_unlock(&lru->lock);
}

/* Helper macro for list iteration */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* Create a new LRU node */
static struct list_lru_node *create_lru_node(const char *data)
{
    struct list_lru_node *node = malloc(sizeof(*node));
    if (!node)
        return NULL;

    INIT_LIST_HEAD(&node->lru);
    INIT_LIST_HEAD(&node->node);
    strncpy(node->data, data, NODE_DATA_SIZE - 1);
    node->data[NODE_DATA_SIZE - 1] = '\0';
    node->active = false;
    node->access_time = time(NULL);

    return node;
}

int main()
{
    struct list_lru lru;
    struct list_lru_node *nodes[MAX_NODES];
    int i;

    printf("List LRU Test Program\n");
    printf("====================\n\n");

    /* Initialize LRU list */
    if (list_lru_init(&lru, "test_lru") != 0) {
        printf("Failed to initialize LRU list\n");
        return -1;
    }

    printf("LRU list initialized\n");

    /* Test 1: Add items to LRU */
    printf("\nTest 1: Adding items to LRU\n");
    printf("---------------------------\n");
    for (i = 0; i < 5; i++) {
        char data[NODE_DATA_SIZE];
        snprintf(data, sizeof(data), "Item %d", i);
        nodes[i] = create_lru_node(data);
        if (nodes[i] && list_lru_add(&lru, nodes[i])) {
            printf("Added: %s\n", nodes[i]->data);
        }
    }
    print_lru_stats(&lru);

    /* Test 2: Access some items */
    printf("\nTest 2: Accessing items (moving to front)\n");
    printf("----------------------------------------\n");
    /* Access items in reverse order */
    for (i = 4; i >= 0; i--) {
        if (nodes[i]) {
            printf("Accessing: %s\n", nodes[i]->data);
            list_lru_touch(&lru, nodes[i]);
        }
    }
    print_lru_stats(&lru);

    /* Test 3: Remove some items */
    printf("\nTest 3: Removing items\n");
    printf("----------------------\n");
    for (i = 0; i < 3; i++) {
        if (nodes[i]) {
            printf("Removing: %s\n", nodes[i]->data);
            if (list_lru_del(&lru, nodes[i])) {
                free(nodes[i]);
                nodes[i] = NULL;
            }
        }
    }
    print_lru_stats(&lru);

    /* Test 4: Get least recently used item */
    printf("\nTest 4: Get least recently used item\n");
    printf("-----------------------------------\n");
    struct list_lru_node *lru_item = list_lru_get_tail(&lru);
    if (lru_item) {
        printf("Least recently used item: %s\n", lru_item->data);
    }

    /* Test 5: Add more items after removal */
    printf("\nTest 5: Adding more items\n");
    printf("------------------------\n");
    for (i = 5; i < 8; i++) {
        char data[NODE_DATA_SIZE];
        snprintf(data, sizeof(data), "New Item %d", i);
        nodes[i] = create_lru_node(data);
        if (nodes[i] && list_lru_add(&lru, nodes[i])) {
            printf("Added: %s\n", nodes[i]->data);
        }
    }
    print_lru_stats(&lru);

    /* Cleanup */
    printf("\nCleaning up LRU list\n");
    list_lru_destroy(&lru);
    for (i = 0; i < MAX_NODES; i++) {
        if (nodes[i]) {
            free(nodes[i]);
            nodes[i] = NULL;
        }
    }
    printf("LRU list destroyed\n");

    return 0;
}
