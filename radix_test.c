#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* Simplified radix tree implementation for testing */

#define RADIX_TREE_MAP_SHIFT    6
#define RADIX_TREE_MAP_SIZE     (1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK     (RADIX_TREE_MAP_SIZE-1)

struct radix_tree_node {
    void *slots[RADIX_TREE_MAP_SIZE];
    unsigned int height;  /* Height from the bottom */
    unsigned int count;   /* Number of slots in use */
    struct radix_tree_node *parent;
};

struct radix_tree_root {
    unsigned int height;  /* Height from the bottom */
    struct radix_tree_node *rnode;
};

#define RADIX_TREE_INIT()    { .height = 0, .rnode = NULL }

#define RADIX_TREE(name) \
    struct radix_tree_root name = RADIX_TREE_INIT()

static inline void INIT_RADIX_TREE(struct radix_tree_root *root)
{
    root->height = 0;
    root->rnode = NULL;
}

static struct radix_tree_node *radix_tree_node_alloc(void)
{
    struct radix_tree_node *ret;
    ret = calloc(1, sizeof(struct radix_tree_node));
    if (ret) {
        ret->count = 0;
        ret->parent = NULL;
    }
    return ret;
}

static inline void radix_tree_node_free(struct radix_tree_node *node)
{
    free(node);
}

static int radix_tree_extend(struct radix_tree_root *root, unsigned long index)
{
    struct radix_tree_node *node;
    unsigned int height;
    int ret;

    /* Figure out what the height should be */
    height = 0;
    index |= RADIX_TREE_MAP_SIZE - 1;
    while (index > 0) {
        index >>= RADIX_TREE_MAP_SHIFT;
        height++;
    }

    if (height <= root->height)
        return 0;

    /* Increase the height */
    do {
        if (root->rnode == NULL) {
            /* First node */
            node = radix_tree_node_alloc();
            if (node == NULL)
                return -1;
            root->rnode = node;
            root->height = 1;
            continue;
        }

        /* Add a new level */
        node = radix_tree_node_alloc();
        if (node == NULL)
            return -1;

        /* Link the new node */
        node->slots[0] = root->rnode;
        node->height = root->height + 1;
        node->count = 1;
        root->rnode->parent = node;
        root->rnode = node;
        root->height++;
    } while (height > root->height);

    return 0;
}

int radix_tree_insert(struct radix_tree_root *root, unsigned long index, void *item)
{
    struct radix_tree_node *node = NULL, *tmp;
    unsigned int height, shift;
    int offset;
    int ret;

    /* Make sure the tree is high enough */
    ret = radix_tree_extend(root, index);
    if (ret < 0)
        return ret;

    /* Start at the root */
    node = root->rnode;
    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

    /* Walk down the tree to find the slot */
    while (height > 0) {
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;

        if (node->slots[offset] == NULL) {
            /* Need to add a child node */
            if (height == 1) {
                node->slots[offset] = item;
                node->count++;
                return 0;
            }

            tmp = radix_tree_node_alloc();
            if (tmp == NULL)
                return -1;

            tmp->height = height - 1;
            tmp->parent = node;
            node->slots[offset] = tmp;
            node->count++;
        }

        node = node->slots[offset];
        height--;
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    return 0;
}

void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index)
{
    struct radix_tree_node *node;
    unsigned int height, shift;
    void *item = NULL;

    /* Start at root */
    node = root->rnode;
    height = root->height;

    if (node == NULL)
        return NULL;

    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

    do {
        if (height == 0)
            break;

        int offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        item = node->slots[offset];
        if (item == NULL)
            return NULL;

        node = item;
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    } while (height > 0);

    return item;
}

void radix_tree_delete(struct radix_tree_root *root, unsigned long index)
{
    struct radix_tree_node *node = root->rnode;
    struct radix_tree_node *path[64] = { NULL, };
    unsigned int height = root->height;
    unsigned int shift;
    int offset, idx = 0;

    if (node == NULL)
        return;

    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

    /* Walk down the tree saving path */
    while (height > 0) {
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        path[idx++] = node;
        node = node->slots[offset];
        if (node == NULL)
            return;
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    }

    /* Remove the item */
    offset = index & RADIX_TREE_MAP_MASK;
    path[idx-1]->slots[offset] = NULL;
    path[idx-1]->count--;

    /* Clean up empty nodes */
    while (idx > 0) {
        idx--;
        if (path[idx]->count > 0)
            break;
        if (idx > 0) {
            offset = ((index >> (idx * RADIX_TREE_MAP_SHIFT)) &
                     RADIX_TREE_MAP_MASK);
            path[idx-1]->slots[offset] = NULL;
            path[idx-1]->count--;
        } else
            root->rnode = NULL;
        radix_tree_node_free(path[idx]);
    }

    /* Handle height reduction */
    if (root->rnode && root->rnode->count == 1 && root->height > 1) {
        node = root->rnode->slots[0];
        radix_tree_node_free(root->rnode);
        root->rnode = node;
        root->height--;
    }
}

/* Test structure */
struct test_item {
    unsigned long index;
    char *data;
};

void print_tree(struct radix_tree_node *node, int level)
{
    int i;
    if (!node) return;

    for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
        if (node->slots[i]) {
            if (level == 1) {
                struct test_item *item = node->slots[i];
                printf("%*sslot %d: [%lu] %s\n", level*4, "", i, 
                       item->index, item->data);
            } else {
                printf("%*sslot %d: (node)\n", level*4, "", i);
                print_tree(node->slots[i], level - 1);
            }
        }
    }
}

int main()
{
    RADIX_TREE(tree);
    struct test_item items[] = {
        {0, "item 0"},
        {1, "item 1"},
        {64, "item 64"},  /* In second node */
        {128, "item 128"}, /* In third node */
        {4095, "item 4095"} /* Requires height increase */
    };
    int i, num_items = sizeof(items)/sizeof(items[0]);

    printf("Inserting items...\n");
    for (i = 0; i < num_items; i++) {
        int ret = radix_tree_insert(&tree, items[i].index, &items[i]);
        if (ret == 0)
            printf("Inserted [%lu] %s\n", items[i].index, items[i].data);
        else
            printf("Failed to insert [%lu] %s\n", items[i].index, items[i].data);
    }
    printf("\n");

    printf("Tree structure:\n");
    print_tree(tree.rnode, tree.height);
    printf("\n");

    printf("Looking up items...\n");
    for (i = 0; i < 4100; i += 64) {
        struct test_item *item = radix_tree_lookup(&tree, i);
        if (item)
            printf("Found [%lu] %s\n", item->index, item->data);
    }
    printf("\n");

    printf("Deleting items...\n");
    for (i = 0; i < num_items; i++) {
        printf("Deleting [%lu]\n", items[i].index);
        radix_tree_delete(&tree, items[i].index);
        printf("Tree after deletion:\n");
        print_tree(tree.rnode, tree.height);
        printf("\n");
    }

    return 0;
}
