#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Scatter-gather list implementation */
struct scatterlist {
    unsigned long page_link;
    unsigned int offset;
    unsigned int length;
    unsigned long dma_address;
};

#define SG_CHAIN        0x01UL
#define SG_END         0x02UL
#define SG_PAGE_LINK_MASK (~0x3UL)

/* Initialize a scatter list entry */
static inline void sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
    memset(sgl, 0, sizeof(*sgl) * nents);
}

/* Set entry's page reference */
static inline void sg_set_page(struct scatterlist *sg, void *page,
                             unsigned int length, unsigned int offset)
{
    sg->page_link = (unsigned long)page;
    sg->offset = offset;
    sg->length = length;
}

/* Mark the end of a scatter list */
static inline void sg_mark_end(struct scatterlist *sg)
{
    sg->page_link |= SG_END;
}

/* Mark an entry as chained */
static inline void sg_chain(struct scatterlist *prv, struct scatterlist *nxt)
{
    prv->page_link = ((unsigned long)nxt | SG_CHAIN);
}

/* Get the page reference from an entry */
static inline void *sg_page(struct scatterlist *sg)
{
    return (void *)(sg->page_link & SG_PAGE_LINK_MASK);
}

/* Check if an entry is the end */
static inline bool sg_is_end(struct scatterlist *sg)
{
    return (sg->page_link & SG_END) != 0;
}

/* Check if an entry is chained */
static inline bool sg_is_chain(struct scatterlist *sg)
{
    return (sg->page_link & SG_CHAIN) != 0;
}

/* Get the next entry in a scatter list */
static inline struct scatterlist *sg_next(struct scatterlist *sg)
{
    if (sg_is_chain(sg))
        return (struct scatterlist *)(sg->page_link & SG_PAGE_LINK_MASK);
    return sg + 1;
}

/* Calculate total length of a scatter list */
static unsigned int sg_total_length(struct scatterlist *sgl)
{
    struct scatterlist *sg;
    unsigned int total = 0;

    for (sg = sgl; !sg_is_end(sg); sg = sg_next(sg))
        total += sg->length;

    return total;
}

/* Print scatter list entry details */
static void print_sg_entry(struct scatterlist *sg, int index)
{
    printf("Entry %d:\n", index);
    printf("  Page: %p\n", sg_page(sg));
    printf("  Offset: %u\n", sg->offset);
    printf("  Length: %u\n", sg->length);
    printf("  Flags: %s%s\n",
           sg_is_chain(sg) ? "CHAIN " : "",
           sg_is_end(sg) ? "END" : "");
    printf("\n");
}

/* Test buffer structure */
struct test_buffer {
    char data[64];
};

int main()
{
    const int num_entries = 5;
    struct scatterlist sg[num_entries];
    struct test_buffer buffers[num_entries];
    int i;

    printf("Scatter-Gather List Test Program\n");
    printf("===============================\n\n");

    /* Initialize test buffers */
    printf("1. Initializing test buffers...\n");
    for (i = 0; i < num_entries; i++) {
        snprintf(buffers[i].data, sizeof(buffers[i].data),
                "Test buffer %d data", i);
    }
    printf("Test buffers initialized\n\n");

    /* Initialize scatter list */
    printf("2. Initializing scatter list...\n");
    sg_init_table(sg, num_entries);
    printf("Scatter list initialized with %d entries\n\n", num_entries);

    /* Set up scatter list entries */
    printf("3. Setting up scatter list entries...\n");
    for (i = 0; i < num_entries; i++) {
        unsigned int length = strlen(buffers[i].data) + 1;
        sg_set_page(&sg[i], &buffers[i], length, 0);
        printf("Set entry %d with buffer at %p, length %u\n",
               i, &buffers[i], length);
    }
    sg_mark_end(&sg[num_entries - 1]);
    printf("\n");

    /* Test chaining */
    printf("4. Testing scatter list chaining...\n");
    /* Chain entry 1 to entry 3 (skipping entry 2) */
    sg_chain(&sg[1], &sg[3]);
    printf("Chained entry 1 to entry 3\n\n");

    /* Print scatter list details */
    printf("5. Scatter list details:\n");
    for (i = 0; i < num_entries; i++) {
        print_sg_entry(&sg[i], i);
    }

    /* Test scatter list traversal */
    printf("6. Testing scatter list traversal...\n");
    struct scatterlist *sg_ptr;
    i = 0;
    for (sg_ptr = sg; !sg_is_end(sg_ptr); sg_ptr = sg_next(sg_ptr)) {
        printf("Traversal step %d:\n", i++);
        printf("  Buffer content: %s\n",
               ((struct test_buffer *)sg_page(sg_ptr))->data);
    }
    printf("\n");

    /* Calculate total length */
    printf("7. Testing total length calculation...\n");
    unsigned int total = sg_total_length(sg);
    printf("Total scatter list length: %u bytes\n\n", total);

    /* Test data access */
    printf("8. Testing data access through scatter list...\n");
    for (sg_ptr = sg; !sg_is_end(sg_ptr); sg_ptr = sg_next(sg_ptr)) {
        struct test_buffer *buf = sg_page(sg_ptr);
        printf("Accessing buffer at %p: %s\n", buf, buf->data);
    }

    printf("\nScatter-Gather List test completed successfully\n");
    return 0;
}
