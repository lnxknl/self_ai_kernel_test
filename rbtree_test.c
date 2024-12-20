#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* Red-black tree implementation */

#define RB_RED   0
#define RB_BLACK 1

struct rb_node {
    unsigned long  __rb_parent_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
};

struct rb_root {
    struct rb_node *rb_node;
};

#define rb_parent(r)   ((struct rb_node *)((r)->__rb_parent_color & ~3))
#define rb_color(r)    ((r)->__rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
    rb->__rb_parent_color = rb_color(rb) | (unsigned long)p;
}

static inline void rb_set_parent_color(struct rb_node *rb,
                                     struct rb_node *p, int color)
{
    rb->__rb_parent_color = (unsigned long)p | color;
}

static inline void rb_set_black(struct rb_node *rb)
{
    rb->__rb_parent_color |= RB_BLACK;
}

static inline struct rb_node *rb_red_parent(struct rb_node *red)
{
    return (struct rb_node *)red->__rb_parent_color;
}

static inline void
__rb_change_child(struct rb_node *old, struct rb_node *new,
                 struct rb_node *parent, struct rb_root *root)
{
    if (parent) {
        if (parent->rb_left == old)
            parent->rb_left = new;
        else
            parent->rb_right = new;
    } else
        root->rb_node = new;
}

static inline void
__rb_rotate_set_parents(struct rb_node *old, struct rb_node *new,
                       struct rb_root *root, int color)
{
    struct rb_node *parent = rb_parent(old);
    new->__rb_parent_color = old->__rb_parent_color;
    rb_set_parent_color(old, new, color);
    __rb_change_child(old, new, parent, root);
}

void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *parent = rb_red_parent(node), *gparent, *tmp;

    while (1) {
        if (!parent) {
            rb_set_parent_color(node, NULL, RB_BLACK);
            break;
        }
        if (rb_is_black(parent))
            break;

        gparent = rb_red_parent(parent);
        tmp = gparent->rb_right;
        if (parent != tmp) {
            if (tmp && rb_is_red(tmp)) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }
            tmp = parent->rb_right;
            if (node == tmp) {
                parent->rb_right = tmp = node->rb_left;
                node->rb_left = parent;
                if (tmp)
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                rb_set_parent_color(parent, node, RB_RED);
                parent = node;
                tmp = node->rb_right;
            }
            gparent->rb_left = tmp;
            parent->rb_right = gparent;
            if (tmp)
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            __rb_rotate_set_parents(gparent, parent, root, RB_RED);
            break;
        } else {
            tmp = gparent->rb_left;
            if (tmp && rb_is_red(tmp)) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }
            tmp = parent->rb_left;
            if (node == tmp) {
                parent->rb_left = tmp = node->rb_right;
                node->rb_right = parent;
                if (tmp)
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                rb_set_parent_color(parent, node, RB_RED);
                parent = node;
                tmp = node->rb_left;
            }
            gparent->rb_right = tmp;
            parent->rb_left = gparent;
            if (tmp)
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            __rb_rotate_set_parents(gparent, parent, root, RB_RED);
            break;
        }
    }
}

static void rb_set_child(struct rb_node *parent,
                        struct rb_node *child,
                        struct rb_node **link)
{
    *link = child;
    if (child)
        rb_set_parent_color(child, parent, RB_BLACK);
}

void rb_erase(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *child = node->rb_right;
    struct rb_node *tmp = node->rb_left;
    struct rb_node *parent, *rebalance;
    unsigned long pc;

    if (!tmp) {
        pc = node->__rb_parent_color;
        parent = rb_parent(node);
        __rb_change_child(node, child, parent, root);
        if (child) {
            child->__rb_parent_color = pc;
            rebalance = NULL;
        } else
            rebalance = rb_is_black(node) ? parent : NULL;
    } else if (!child) {
        pc = node->__rb_parent_color;
        parent = rb_parent(node);
        __rb_change_child(node, tmp, parent, root);
        rebalance = NULL;
    } else {
        struct rb_node *successor = child, *child2;

        tmp = child->rb_left;
        if (!tmp) {
            parent = successor;
            child2 = successor->rb_right;
        } else {
            do {
                parent = successor;
                successor = tmp;
                tmp = tmp->rb_left;
            } while (tmp);
            child2 = successor->rb_right;
            parent->rb_left = child2;
            successor->rb_right = child;
            rb_set_parent(child, successor);
        }

        tmp = node->rb_left;
        successor->rb_left = tmp;
        rb_set_parent(tmp, successor);

        pc = node->__rb_parent_color;
        tmp = rb_parent(node);
        __rb_change_child(node, successor, tmp, root);

        if (child2) {
            rb_set_parent_color(child2, parent, RB_BLACK);
            rebalance = NULL;
        } else {
            rebalance = rb_is_black(successor) ? parent : NULL;
        }
        successor->__rb_parent_color = pc;
    }
}

/* Test structure */
struct test_node {
    int key;
    struct rb_node node;
};

/* Helper function to insert a node */
void insert_node(struct rb_root *root, struct test_node *data)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        struct test_node *this = rb_entry(*new, struct test_node, node);
        parent = *new;
        
        if (data->key < this->key)
            new = &((*new)->rb_left);
        else if (data->key > this->key)
            new = &((*new)->rb_right);
        else
            return;
    }

    /* Add new node and recolor */
    rb_set_parent_color(&data->node, parent, RB_RED);
    data->node.rb_left = data->node.rb_right = NULL;
    *new = &data->node;
    
    rb_insert_color(&data->node, root);
}

/* Helper function to find a node */
struct test_node *search_node(struct rb_root *root, int key)
{
    struct rb_node *node = root->rb_node;

    while (node) {
        struct test_node *data = rb_entry(node, struct test_node, node);

        if (key < data->key)
            node = node->rb_left;
        else if (key > data->key)
            node = node->rb_right;
        else
            return data;
    }
    return NULL;
}

/* Helper function to print the tree (in-order traversal) */
void print_tree(struct rb_node *node)
{
    if (!node) return;
    
    print_tree(node->rb_left);
    struct test_node *data = rb_entry(node, struct test_node, node);
    printf("%d(%s) ", data->key, rb_is_red(node) ? "R" : "B");
    print_tree(node->rb_right);
}

int main()
{
    struct rb_root root = {NULL};
    struct test_node *nodes;
    int i, num_nodes = 10;
    
    /* Allocate nodes */
    nodes = (struct test_node*)malloc(sizeof(struct test_node) * num_nodes);
    if (!nodes) {
        printf("Memory allocation failed!\n");
        return -1;
    }

    /* Insert some test values */
    printf("Inserting values: ");
    for (i = 0; i < num_nodes; i++) {
        nodes[i].key = i * 2 + 1;  /* Odd numbers */
        insert_node(&root, &nodes[i]);
        printf("%d ", nodes[i].key);
    }
    printf("\n\n");

    /* Print the tree */
    printf("Tree structure (in-order, with colors):\n");
    print_tree(root.rb_node);
    printf("\n\n");

    /* Test searching */
    printf("Searching for values:\n");
    for (i = 0; i < 20; i++) {
        struct test_node *found = search_node(&root, i);
        if (found)
            printf("Found %d\n", i);
        else
            printf("%d not found\n", i);
    }

    /* Test deletion */
    printf("\nDeleting node with key 5\n");
    struct test_node *to_delete = search_node(&root, 5);
    if (to_delete) {
        rb_erase(&to_delete->node, &root);
        printf("Tree after deletion:\n");
        print_tree(root.rb_node);
        printf("\n");
    }

    /* Cleanup */
    free(nodes);
    return 0;
}
