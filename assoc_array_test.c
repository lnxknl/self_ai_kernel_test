#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Node colors for Red-Black tree */
enum rb_color {
    RB_RED = 0,
    RB_BLACK = 1
};

/* Red-Black tree node structure */
struct rb_node {
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    enum rb_color color;
};

/* Associative array node structure */
struct assoc_array_node {
    struct rb_node rb;     /* Red-black tree node */
    char *key;             /* String key */
    void *value;           /* Associated value */
    size_t key_len;        /* Length of the key */
};

/* Associative array structure */
struct assoc_array {
    struct rb_node *root;  /* Root of the red-black tree */
    size_t count;          /* Number of nodes in the array */
};

/* Initialize an associative array */
void assoc_array_init(struct assoc_array *array)
{
    array->root = NULL;
    array->count = 0;
}

/* Helper function to create a new node */
static struct assoc_array_node *create_node(const char *key, void *value)
{
    struct assoc_array_node *node = malloc(sizeof(*node));
    if (!node)
        return NULL;

    node->key = strdup(key);
    if (!node->key) {
        free(node);
        return NULL;
    }

    node->value = value;
    node->key_len = strlen(key);
    node->rb.parent = NULL;
    node->rb.left = NULL;
    node->rb.right = NULL;
    node->rb.color = RB_RED;

    return node;
}

/* Helper function to free a node */
static void free_node(struct assoc_array_node *node)
{
    free(node->key);
    free(node);
}

/* Helper function to rotate left */
static void rb_rotate_left(struct rb_node **root, struct rb_node *node)
{
    struct rb_node *right = node->right;
    node->right = right->left;

    if (right->left)
        right->left->parent = node;

    right->parent = node->parent;

    if (!node->parent)
        *root = right;
    else if (node == node->parent->left)
        node->parent->left = right;
    else
        node->parent->right = right;

    right->left = node;
    node->parent = right;
}

/* Helper function to rotate right */
static void rb_rotate_right(struct rb_node **root, struct rb_node *node)
{
    struct rb_node *left = node->left;
    node->left = left->right;

    if (left->right)
        left->right->parent = node;

    left->parent = node->parent;

    if (!node->parent)
        *root = left;
    else if (node == node->parent->right)
        node->parent->right = left;
    else
        node->parent->left = left;

    left->right = node;
    node->parent = left;
}

/* Helper function to fix red-black tree properties after insertion */
static void rb_insert_fixup(struct rb_node **root, struct rb_node *node)
{
    struct rb_node *parent, *gparent, *uncle;

    while ((parent = node->parent) && parent->color == RB_RED) {
        gparent = parent->parent;

        if (parent == gparent->left) {
            uncle = gparent->right;

            if (uncle && uncle->color == RB_RED) {
                uncle->color = RB_BLACK;
                parent->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;
                continue;
            }

            if (node == parent->right) {
                rb_rotate_left(root, parent);
                node = parent;
                parent = node->parent;
            }

            parent->color = RB_BLACK;
            gparent->color = RB_RED;
            rb_rotate_right(root, gparent);
        } else {
            uncle = gparent->left;

            if (uncle && uncle->color == RB_RED) {
                uncle->color = RB_BLACK;
                parent->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;
                continue;
            }

            if (node == parent->left) {
                rb_rotate_right(root, parent);
                node = parent;
                parent = node->parent;
            }

            parent->color = RB_BLACK;
            gparent->color = RB_RED;
            rb_rotate_left(root, gparent);
        }
    }

    (*root)->color = RB_BLACK;
}

/* Insert a key-value pair into the array */
int assoc_array_insert(struct assoc_array *array, const char *key, void *value)
{
    struct rb_node **new = &array->root;
    struct rb_node *parent = NULL;
    struct assoc_array_node *node;
    int cmp;

    /* Find insertion point */
    while (*new) {
        parent = *new;
        node = (struct assoc_array_node *)parent;
        cmp = strcmp(key, node->key);

        if (cmp < 0)
            new = &parent->left;
        else if (cmp > 0)
            new = &parent->right;
        else {
            /* Key already exists, update value */
            node->value = value;
            return 0;
        }
    }

    /* Create new node */
    node = create_node(key, value);
    if (!node)
        return -1;

    /* Insert new node */
    node->rb.parent = parent;
    *new = &node->rb;
    rb_insert_fixup(&array->root, &node->rb);
    array->count++;

    return 0;
}

/* Find a node by key */
static struct assoc_array_node *find_node(struct assoc_array *array, const char *key)
{
    struct rb_node *node = array->root;
    int cmp;

    while (node) {
        struct assoc_array_node *curr = (struct assoc_array_node *)node;
        cmp = strcmp(key, curr->key);

        if (cmp < 0)
            node = node->left;
        else if (cmp > 0)
            node = node->right;
        else
            return curr;
    }

    return NULL;
}

/* Look up a value by key */
void *assoc_array_lookup(struct assoc_array *array, const char *key)
{
    struct assoc_array_node *node = find_node(array, key);
    return node ? node->value : NULL;
}

/* Helper function to print the tree recursively */
static void print_tree(struct rb_node *node, int level)
{
    int i;
    if (!node)
        return;

    struct assoc_array_node *curr = (struct assoc_array_node *)node;

    print_tree(node->right, level + 1);

    for (i = 0; i < level; i++)
        printf("    ");
    printf("%s (%s)\n", curr->key, node->color == RB_RED ? "RED" : "BLACK");

    print_tree(node->left, level + 1);
}

/* Print the entire tree */
void assoc_array_print(struct assoc_array *array)
{
    printf("Associative Array Contents:\n");
    print_tree(array->root, 0);
    printf("Total nodes: %zu\n\n", array->count);
}

/* Helper function to free the tree recursively */
static void free_tree(struct rb_node *node)
{
    if (!node)
        return;

    free_tree(node->left);
    free_tree(node->right);
    free_node((struct assoc_array_node *)node);
}

/* Free the entire array */
void assoc_array_free(struct assoc_array *array)
{
    free_tree(array->root);
    array->root = NULL;
    array->count = 0;
}

int main()
{
    struct assoc_array array;
    char *value;

    printf("Associative Array Test Program\n");
    printf("=============================\n\n");

    /* Initialize array */
    printf("1. Initializing associative array...\n");
    assoc_array_init(&array);
    printf("Array initialized\n\n");

    /* Test insertions */
    printf("2. Testing insertions...\n");
    printf("Inserting key-value pairs:\n");
    assoc_array_insert(&array, "apple", "red fruit");
    assoc_array_insert(&array, "banana", "yellow fruit");
    assoc_array_insert(&array, "cherry", "small red fruit");
    assoc_array_insert(&array, "date", "sweet fruit");
    printf("Inserted 4 key-value pairs\n\n");

    /* Print tree structure */
    printf("3. Current tree structure:\n");
    assoc_array_print(&array);

    /* Test lookups */
    printf("4. Testing lookups...\n");
    value = assoc_array_lookup(&array, "apple");
    printf("Looking up 'apple': %s\n", value ? value : "not found");
    
    value = assoc_array_lookup(&array, "banana");
    printf("Looking up 'banana': %s\n", value ? value : "not found");
    
    value = assoc_array_lookup(&array, "grape");
    printf("Looking up 'grape': %s\n", value ? value : "not found");
    printf("\n");

    /* Test update */
    printf("5. Testing value update...\n");
    printf("Updating value for 'apple'\n");
    assoc_array_insert(&array, "apple", "red delicious fruit");
    value = assoc_array_lookup(&array, "apple");
    printf("New value for 'apple': %s\n\n", value ? value : "not found");

    /* Print final tree structure */
    printf("6. Final tree structure:\n");
    assoc_array_print(&array);

    /* Clean up */
    printf("7. Cleaning up...\n");
    assoc_array_free(&array);
    printf("Array freed\n");

    return 0;
}
