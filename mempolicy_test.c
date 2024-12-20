#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>

/* Constants */
#define MAX_NUMNODES 64
#define MPOL_DEFAULT     0
#define MPOL_PREFERRED   1
#define MPOL_BIND       2
#define MPOL_INTERLEAVE 3
#define MPOL_LOCAL      4
#define MPOL_MAX        5

#define MPOL_F_STATIC_NODES     (1 << 15)
#define MPOL_F_RELATIVE_NODES   (1 << 14)
#define MPOL_F_NODE            (1 << 13)

/* Structure definitions */
typedef struct {
    unsigned long bits[MAX_NUMNODES / (sizeof(unsigned long) * 8)];
} nodemask_t;

struct mempolicy {
    unsigned short mode;    /* Policy mode */
    unsigned short flags;   /* Optional mode flags */
    nodemask_t nodes;      /* Allowed nodes for allocation */
    int preferred_node;     /* Preferred node for allocation */
    pthread_mutex_t lock;   /* Lock for thread safety */
};

/* Global variables */
static int nr_node_ids = 8;  /* Simulated number of NUMA nodes */
static struct mempolicy default_policy = {
    .mode = MPOL_DEFAULT,
    .flags = 0,
    .preferred_node = -1
};

/* Helper functions */
static void nodes_clear(nodemask_t *dst)
{
    memset(dst, 0, sizeof(nodemask_t));
}

static void nodes_setall(nodemask_t *dst)
{
    memset(dst, 0xFF, sizeof(nodemask_t));
}

static void node_set(int node, nodemask_t *dst)
{
    unsigned long *p = dst->bits + (node / (sizeof(unsigned long) * 8));
    *p |= 1UL << (node % (sizeof(unsigned long) * 8));
}

static void node_clear(int node, nodemask_t *dst)
{
    unsigned long *p = dst->bits + (node / (sizeof(unsigned long) * 8));
    *p &= ~(1UL << (node % (sizeof(unsigned long) * 8)));
}

static bool node_isset(int node, const nodemask_t *src)
{
    const unsigned long *p = src->bits + (node / (sizeof(unsigned long) * 8));
    return (*p >> (node % (sizeof(unsigned long) * 8))) & 1;
}

static int nodes_weight(const nodemask_t *src)
{
    int weight = 0;
    int i;
    
    for (i = 0; i < nr_node_ids; i++) {
        if (node_isset(i, src))
            weight++;
    }
    return weight;
}

/* Core mempolicy functions */
static struct mempolicy *mpol_new(unsigned short mode, unsigned short flags, 
                                const nodemask_t *nodes)
{
    struct mempolicy *pol;
    int ret = 0;

    if (mode >= MPOL_MAX)
        return NULL;

    pol = malloc(sizeof(*pol));
    if (!pol)
        return NULL;

    if (pthread_mutex_init(&pol->lock, NULL) != 0) {
        free(pol);
        return NULL;
    }

    pol->mode = mode;
    pol->flags = flags;
    pol->preferred_node = -1;
    nodes_clear(&pol->nodes);

    if (mode == MPOL_DEFAULT) {
        /* No need to check or copy nodes */
        return pol;
    }

    if (!nodes) {
        ret = -EINVAL;
        goto err;
    }

    switch (mode) {
    case MPOL_PREFERRED:
        /* Find first set node */
        for (int i = 0; i < nr_node_ids; i++) {
            if (node_isset(i, nodes)) {
                pol->preferred_node = i;
                break;
            }
        }
        break;

    case MPOL_BIND:
    case MPOL_INTERLEAVE:
        if (nodes_weight(nodes) == 0) {
            ret = -EINVAL;
            goto err;
        }
        memcpy(&pol->nodes, nodes, sizeof(nodemask_t));
        break;

    case MPOL_LOCAL:
        /* Local policy doesn't need nodes */
        break;
    }

    return pol;

err:
    pthread_mutex_destroy(&pol->lock);
    free(pol);
    return NULL;
}

static void mpol_free(struct mempolicy *pol)
{
    if (!pol || pol == &default_policy)
        return;
    pthread_mutex_destroy(&pol->lock);
    free(pol);
}

static int mpol_set_nodemask(struct mempolicy *pol, const nodemask_t *nodes)
{
    if (!pol || !nodes)
        return -EINVAL;

    pthread_mutex_lock(&pol->lock);

    switch (pol->mode) {
    case MPOL_PREFERRED:
        /* Find first set node */
        pol->preferred_node = -1;
        for (int i = 0; i < nr_node_ids; i++) {
            if (node_isset(i, nodes)) {
                pol->preferred_node = i;
                break;
            }
        }
        break;

    case MPOL_BIND:
    case MPOL_INTERLEAVE:
        if (nodes_weight(nodes) == 0) {
            pthread_mutex_unlock(&pol->lock);
            return -EINVAL;
        }
        memcpy(&pol->nodes, nodes, sizeof(nodemask_t));
        break;

    default:
        pthread_mutex_unlock(&pol->lock);
        return -EINVAL;
    }

    pthread_mutex_unlock(&pol->lock);
    return 0;
}

static void print_nodemask(const char *prefix, const nodemask_t *mask)
{
    printf("%s: [", prefix);
    for (int i = 0; i < nr_node_ids; i++) {
        printf("%c", node_isset(i, mask) ? '1' : '0');
    }
    printf("]\n");
}

static void print_policy(const char *prefix, const struct mempolicy *pol)
{
    static const char *mode_names[] = {
        "DEFAULT", "PREFERRED", "BIND", "INTERLEAVE", "LOCAL"
    };

    printf("\n%s:\n", prefix);
    printf("Mode: %s\n", mode_names[pol->mode]);
    printf("Flags: 0x%x\n", pol->flags);
    if (pol->mode == MPOL_PREFERRED)
        printf("Preferred Node: %d\n", pol->preferred_node);
    else
        print_nodemask("Nodemask", &pol->nodes);
}

int main()
{
    struct mempolicy *pol;
    nodemask_t nodes;
    int ret;

    printf("Memory Policy (mempolicy) Test Program\n");
    printf("====================================\n\n");

    /* Test 1: Create and test default policy */
    printf("Test 1: Default Policy\n");
    printf("---------------------\n");
    pol = mpol_new(MPOL_DEFAULT, 0, NULL);
    if (pol)
        print_policy("Default Policy", pol);
    mpol_free(pol);

    /* Test 2: Preferred policy */
    printf("\nTest 2: Preferred Policy\n");
    printf("----------------------\n");
    nodes_clear(&nodes);
    node_set(2, &nodes);  /* Prefer node 2 */
    pol = mpol_new(MPOL_PREFERRED, 0, &nodes);
    if (pol)
        print_policy("Preferred Policy", pol);
    mpol_free(pol);

    /* Test 3: Bind policy */
    printf("\nTest 3: Bind Policy\n");
    printf("------------------\n");
    nodes_clear(&nodes);
    node_set(0, &nodes);
    node_set(1, &nodes);
    pol = mpol_new(MPOL_BIND, 0, &nodes);
    if (pol)
        print_policy("Bind Policy", pol);

    /* Test 4: Update nodemask */
    printf("\nTest 4: Update Nodemask\n");
    printf("----------------------\n");
    nodes_clear(&nodes);
    node_set(3, &nodes);
    node_set(4, &nodes);
    ret = mpol_set_nodemask(pol, &nodes);
    printf("Update nodemask result: %s\n", ret == 0 ? "Success" : "Failed");
    if (pol)
        print_policy("Updated Bind Policy", pol);
    mpol_free(pol);

    /* Test 5: Interleave policy */
    printf("\nTest 5: Interleave Policy\n");
    printf("------------------------\n");
    nodes_clear(&nodes);
    for (int i = 0; i < 4; i++)
        node_set(i, &nodes);
    pol = mpol_new(MPOL_INTERLEAVE, 0, &nodes);
    if (pol)
        print_policy("Interleave Policy", pol);
    mpol_free(pol);

    /* Test 6: Invalid cases */
    printf("\nTest 6: Invalid Cases\n");
    printf("-------------------\n");
    nodes_clear(&nodes);  /* Empty nodemask */
    pol = mpol_new(MPOL_BIND, 0, &nodes);
    printf("Create policy with empty nodemask: %s\n", 
           pol ? "Unexpected Success" : "Failed as expected");
    mpol_free(pol);

    pol = mpol_new(MPOL_MAX, 0, &nodes);  /* Invalid mode */
    printf("Create policy with invalid mode: %s\n",
           pol ? "Unexpected Success" : "Failed as expected");
    mpol_free(pol);

    printf("\nMemory Policy test complete\n");
    return 0;
}
