#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Flex array implementation */
#define FLEX_ARRAY_PART_SIZE 32
#define FLEX_ARRAY_BASE_SIZE 16

struct flex_array_part {
    void *elements[FLEX_ARRAY_PART_SIZE];
};

struct flex_array {
    int element_size;           /* Size of each element */
    unsigned int total_size;    /* Total number of elements */
    unsigned int parts;         /* Number of parts */
    struct flex_array_part *part[0]; /* Array of parts */
};

/* Helper functions */
static inline int flex_array_elements_per_part(void)
{
    return FLEX_ARRAY_PART_SIZE;
}

static unsigned int flex_array_num_parts(unsigned int elements)
{
    return (elements + FLEX_ARRAY_PART_SIZE - 1) / FLEX_ARRAY_PART_SIZE;
}

/* Create a new flex array */
struct flex_array *flex_array_alloc(int element_size, unsigned int total)
{
    struct flex_array *ret;
    size_t size = sizeof(struct flex_array) + 
                  flex_array_num_parts(total) * sizeof(struct flex_array_part *);

    if (element_size <= 0)
        return NULL;

    ret = malloc(size);
    if (!ret)
        return NULL;

    ret->element_size = element_size;
    ret->total_size = total;
    ret->parts = flex_array_num_parts(total);
    memset(ret->part, 0, ret->parts * sizeof(struct flex_array_part *));

    return ret;
}

/* Free a flex array */
void flex_array_free(struct flex_array *fa)
{
    if (!fa)
        return;

    for (unsigned int i = 0; i < fa->parts; i++) {
        if (fa->part[i]) {
            for (int j = 0; j < FLEX_ARRAY_PART_SIZE; j++) {
                free(fa->part[i]->elements[j]);
            }
            free(fa->part[i]);
        }
    }
    free(fa);
}

/* Get element at index */
void *flex_array_get(struct flex_array *fa, unsigned int element_nr)
{
    struct flex_array_part *part;

    if (!fa || element_nr >= fa->total_size)
        return NULL;

    part = fa->part[element_nr / FLEX_ARRAY_PART_SIZE];
    if (!part)
        return NULL;

    return part->elements[element_nr % FLEX_ARRAY_PART_SIZE];
}

/* Put element at index */
int flex_array_put(struct flex_array *fa, unsigned int element_nr, void *element)
{
    struct flex_array_part *part;
    unsigned int part_nr = element_nr / FLEX_ARRAY_PART_SIZE;
    void *new_element;

    if (!fa || element_nr >= fa->total_size)
        return -1;

    if (!fa->part[part_nr]) {
        part = malloc(sizeof(struct flex_array_part));
        if (!part)
            return -1;
        memset(part->elements, 0, sizeof(part->elements));
        fa->part[part_nr] = part;
    }

    new_element = malloc(fa->element_size);
    if (!new_element)
        return -1;

    memcpy(new_element, element, fa->element_size);
    
    /* Free old element if it exists */
    free(fa->part[part_nr]->elements[element_nr % FLEX_ARRAY_PART_SIZE]);
    
    fa->part[part_nr]->elements[element_nr % FLEX_ARRAY_PART_SIZE] = new_element;
    return 0;
}

/* Test structure */
struct test_element {
    int id;
    char name[32];
};

/* Print a test element */
void print_element(struct test_element *elem)
{
    if (elem)
        printf("[%d] %s", elem->id, elem->name);
    else
        printf("(null)");
}

/* Main test program */
int main()
{
    struct flex_array *fa;
    const unsigned int total_elements = 100;
    struct test_element test_data[] = {
        {1, "One"},
        {5, "Five"},
        {10, "Ten"},
        {25, "Twenty-five"},
        {50, "Fifty"},
        {99, "Ninety-nine"}
    };
    int num_test_elements = sizeof(test_data) / sizeof(test_data[0]);

    printf("Flex Array Test Program\n");
    printf("======================\n\n");

    /* Create flex array */
    printf("1. Creating flex array with %u elements...\n", total_elements);
    fa = flex_array_alloc(sizeof(struct test_element), total_elements);
    if (!fa) {
        printf("Failed to allocate flex array!\n");
        return -1;
    }
    printf("Flex array created successfully\n");
    printf("- Element size: %d bytes\n", fa->element_size);
    printf("- Total elements: %u\n", fa->total_size);
    printf("- Number of parts: %u\n\n", fa->parts);

    /* Put test elements */
    printf("2. Inserting test elements...\n");
    for (int i = 0; i < num_test_elements; i++) {
        printf("Inserting element at index %d: ", test_data[i].id);
        if (flex_array_put(fa, test_data[i].id, &test_data[i]) == 0)
            printf("success\n");
        else
            printf("failed\n");
    }
    printf("\n");

    /* Get and verify elements */
    printf("3. Retrieving test elements...\n");
    for (int i = 0; i < num_test_elements; i++) {
        printf("Element at index %d: ", test_data[i].id);
        struct test_element *elem = flex_array_get(fa, test_data[i].id);
        print_element(elem);
        printf("\n");
    }
    printf("\n");

    /* Test boundary conditions */
    printf("4. Testing boundary conditions...\n");
    
    printf("Accessing index -1: ");
    struct test_element *elem = flex_array_get(fa, -1);
    print_element(elem);
    printf("\n");
    
    printf("Accessing index %u (total size): ", total_elements);
    elem = flex_array_get(fa, total_elements);
    print_element(elem);
    printf("\n");
    
    printf("Accessing unset index 42: ");
    elem = flex_array_get(fa, 42);
    print_element(elem);
    printf("\n\n");

    /* Clean up */
    printf("5. Cleaning up...\n");
    flex_array_free(fa);
    printf("Flex array freed\n");

    return 0;
}
