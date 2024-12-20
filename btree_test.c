#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_KEYS 3
#define MIN_KEYS ((MAX_KEYS + 1) / 2)

/* B-tree node structure */
struct btree_node {
    int num_keys;           /* Number of keys currently in node */
    int keys[MAX_KEYS];     /* Array of keys */
    struct btree_node *children[MAX_KEYS + 1];  /* Array of child pointers */
    bool is_leaf;           /* True if node is a leaf, false otherwise */
};

/* B-tree structure */
struct btree {
    struct btree_node *root;
};

/* Create a new B-tree node */
static struct btree_node *btree_node_create(bool is_leaf)
{
    struct btree_node *node = malloc(sizeof(struct btree_node));
    if (node) {
        node->num_keys = 0;
        node->is_leaf = is_leaf;
        memset(node->children, 0, sizeof(node->children));
    }
    return node;
}

/* Initialize B-tree */
static void btree_init(struct btree *tree)
{
    tree->root = btree_node_create(true);
}

/* Split a child node */
static void btree_split_child(struct btree_node *parent, int index, struct btree_node *child)
{
    struct btree_node *new_node = btree_node_create(child->is_leaf);
    int mid = (MAX_KEYS - 1) / 2;

    /* Copy the latter half of child's keys to new node */
    new_node->num_keys = MAX_KEYS - 1 - mid - 1;
    for (int i = 0; i < new_node->num_keys; i++)
        new_node->keys[i] = child->keys[i + mid + 1];

    /* If not leaf, copy the latter half of child's children */
    if (!child->is_leaf) {
        for (int i = 0; i <= new_node->num_keys; i++)
            new_node->children[i] = child->children[i + mid + 1];
    }

    /* Update child's number of keys */
    child->num_keys = mid;

    /* Shift parent's children to make room */
    for (int i = parent->num_keys; i >= index + 1; i--)
        parent->children[i + 1] = parent->children[i];
    parent->children[index + 1] = new_node;

    /* Shift parent's keys and insert middle key */
    for (int i = parent->num_keys - 1; i >= index; i--)
        parent->keys[i + 1] = parent->keys[i];
    parent->keys[index] = child->keys[mid];
    parent->num_keys++;
}

/* Insert a key into a non-full node */
static void btree_insert_nonfull(struct btree_node *node, int key)
{
    int i = node->num_keys - 1;

    if (node->is_leaf) {
        /* Find position and insert key */
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->num_keys++;
    } else {
        /* Find child to recurse on */
        while (i >= 0 && key < node->keys[i])
            i--;
        i++;

        if (node->children[i]->num_keys == MAX_KEYS) {
            btree_split_child(node, i, node->children[i]);
            if (key > node->keys[i])
                i++;
        }
        btree_insert_nonfull(node->children[i], key);
    }
}

/* Insert a key into the B-tree */
static void btree_insert(struct btree *tree, int key)
{
    struct btree_node *root = tree->root;

    if (root->num_keys == MAX_KEYS) {
        /* Create new root */
        struct btree_node *new_root = btree_node_create(false);
        tree->root = new_root;
        new_root->children[0] = root;
        btree_split_child(new_root, 0, root);
        btree_insert_nonfull(new_root, key);
    } else {
        btree_insert_nonfull(root, key);
    }
}

/* Search for a key in a node */
static struct btree_node *btree_search_node(struct btree_node *node, int key, int *index)
{
    int i = 0;
    while (i < node->num_keys && key > node->keys[i])
        i++;

    if (i < node->num_keys && key == node->keys[i]) {
        *index = i;
        return node;
    }

    if (node->is_leaf)
        return NULL;

    return btree_search_node(node->children[i], key, index);
}

/* Search for a key in the B-tree */
static bool btree_search(struct btree *tree, int key)
{
    int index;
    return btree_search_node(tree->root, key, &index) != NULL;
}

/* Print a B-tree node */
static void btree_print_node(struct btree_node *node, int level)
{
    printf("%*s[", level*4, "");
    for (int i = 0; i < node->num_keys; i++) {
        printf("%d", node->keys[i]);
        if (i < node->num_keys - 1)
            printf(" ");
    }
    printf("]\n");

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++)
            btree_print_node(node->children[i], level + 1);
    }
}

/* Print the B-tree */
static void btree_print(struct btree *tree)
{
    printf("B-tree structure:\n");
    btree_print_node(tree->root, 0);
    printf("\n");
}

/* Free a B-tree node and its children */
static void btree_free_node(struct btree_node *node)
{
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++)
            btree_free_node(node->children[i]);
    }
    free(node);
}

/* Free the B-tree */
static void btree_free(struct btree *tree)
{
    btree_free_node(tree->root);
}

int main()
{
    struct btree tree;
    int test_keys[] = {3, 7, 1, 5, 11, 2, 4, 8, 9, 6, 10};
    int num_keys = sizeof(test_keys) / sizeof(test_keys[0]);

    printf("B-tree Test Program\n");
    printf("==================\n\n");

    /* Initialize B-tree */
    printf("1. Initializing B-tree...\n");
    btree_init(&tree);
    printf("Empty tree:\n");
    btree_print(&tree);

    /* Insert keys */
    printf("2. Inserting keys: ");
    for (int i = 0; i < num_keys; i++)
        printf("%d ", test_keys[i]);
    printf("\n\n");

    for (int i = 0; i < num_keys; i++) {
        printf("Inserting %d:\n", test_keys[i]);
        btree_insert(&tree, test_keys[i]);
        btree_print(&tree);
    }

    /* Search for keys */
    printf("3. Testing search operations:\n");
    int search_keys[] = {1, 5, 9, 12, 0};
    for (int i = 0; i < sizeof(search_keys)/sizeof(search_keys[0]); i++) {
        printf("Searching for %d: %s\n", 
               search_keys[i], 
               btree_search(&tree, search_keys[i]) ? "found" : "not found");
    }
    printf("\n");

    /* Clean up */
    printf("4. Cleaning up...\n");
    btree_free(&tree);
    printf("B-tree freed\n");

    return 0;
}
