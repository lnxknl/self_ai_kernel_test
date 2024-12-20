#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Constants */
#define IO_TLB_SHIFT       11
#define IO_TLB_SIZE        (1 << IO_TLB_SHIFT)
#define IO_TLB_SEGSIZE     256
#define IO_TLB_SEGMENTS    256
#define SLABS_PER_PAGE     1
#define IO_TLB_PAGES       1024
#define IO_TLB_TOTAL_SIZE  (IO_TLB_PAGES * IO_TLB_SIZE)

/* Flags for mapping */
#define DMA_BIDIRECTIONAL  0
#define DMA_TO_DEVICE     1
#define DMA_FROM_DEVICE   2
#define DMA_NONE          3

/* Structure definitions */
struct io_tlb_slot {
    unsigned long orig_addr;
    size_t alloc_size;
    unsigned int list;
    bool used;
};

struct io_tlb_mem {
    void *vaddr;
    unsigned long nslabs;
    unsigned long used;
    struct io_tlb_slot *slots;
};

/* Global variables */
static struct io_tlb_mem io_tlb = {0};
static unsigned char *io_tlb_buffer;
static unsigned int io_tlb_index;

/* Helper functions */
static unsigned long roundup_pow_of_two(unsigned long x)
{
    unsigned long power = 1;
    while (power < x)
        power *= 2;
    return power;
}

/* Initialize SWIOTLB */
static int swiotlb_init(void)
{
    size_t slots_size;

    /* Allocate buffer for SWIOTLB memory */
    io_tlb_buffer = (unsigned char *)malloc(IO_TLB_TOTAL_SIZE);
    if (!io_tlb_buffer) {
        printf("Failed to allocate SWIOTLB buffer\n");
        return -1;
    }

    /* Allocate slots */
    slots_size = IO_TLB_SEGMENTS * sizeof(struct io_tlb_slot);
    io_tlb.slots = (struct io_tlb_slot *)malloc(slots_size);
    if (!io_tlb.slots) {
        printf("Failed to allocate SWIOTLB slots\n");
        free(io_tlb_buffer);
        return -1;
    }

    /* Initialize slots */
    memset(io_tlb.slots, 0, slots_size);
    io_tlb.vaddr = io_tlb_buffer;
    io_tlb.nslabs = IO_TLB_SEGMENTS;
    io_tlb.used = 0;

    printf("SWIOTLB initialized with %lu segments of size %d bytes\n",
           io_tlb.nslabs, IO_TLB_SIZE);
    return 0;
}

/* Find a free slot for mapping */
static int find_free_slot(size_t alloc_size)
{
    unsigned int index = io_tlb_index;
    unsigned int count = 0;
    unsigned int needed_slots = (alloc_size + IO_TLB_SIZE - 1) >> IO_TLB_SHIFT;

    while (count < io_tlb.nslabs) {
        if (!io_tlb.slots[index].used) {
            unsigned int i;
            bool found = true;

            /* Check if we have enough consecutive free slots */
            for (i = 0; i < needed_slots; i++) {
                if (io_tlb.slots[(index + i) % io_tlb.nslabs].used) {
                    found = false;
                    break;
                }
            }

            if (found)
                return index;
        }

        index = (index + 1) % io_tlb.nslabs;
        count++;
    }

    return -1;
}

/* Map a buffer for DMA */
static void *swiotlb_map(void *orig_addr, size_t size, unsigned int direction)
{
    int slot_idx;
    unsigned int needed_slots;
    void *mapping;

    if (!size)
        return NULL;

    /* Round up size to multiple of IO_TLB_SIZE */
    size = roundup_pow_of_two(size);
    if (size > IO_TLB_TOTAL_SIZE) {
        printf("Requested size too large: %zu bytes\n", size);
        return NULL;
    }

    needed_slots = (size + IO_TLB_SIZE - 1) >> IO_TLB_SHIFT;
    slot_idx = find_free_slot(size);

    if (slot_idx < 0) {
        printf("No free slots available\n");
        return NULL;
    }

    /* Mark slots as used */
    for (unsigned int i = 0; i < needed_slots; i++) {
        unsigned int idx = (slot_idx + i) % io_tlb.nslabs;
        io_tlb.slots[idx].used = true;
        io_tlb.slots[idx].orig_addr = (unsigned long)orig_addr + (i * IO_TLB_SIZE);
        io_tlb.slots[idx].alloc_size = (i == 0) ? size : 0;
        io_tlb.slots[idx].list = direction;
    }

    io_tlb.used += needed_slots;
    mapping = io_tlb.vaddr + (slot_idx * IO_TLB_SIZE);

    /* Simulate copying data for TO_DEVICE */
    if (direction == DMA_TO_DEVICE || direction == DMA_BIDIRECTIONAL) {
        memcpy(mapping, orig_addr, size);
    }

    printf("Mapped buffer at %p (size: %zu) to SWIOTLB address %p\n",
           orig_addr, size, mapping);
    return mapping;
}

/* Unmap a previously mapped buffer */
static void swiotlb_unmap(void *mapping, size_t size, unsigned int direction)
{
    unsigned int slot_idx;
    unsigned int needed_slots;

    if (!mapping || !size)
        return;

    /* Find the slot index from the mapping address */
    slot_idx = ((unsigned char *)mapping - io_tlb_buffer) >> IO_TLB_SHIFT;
    if (slot_idx >= io_tlb.nslabs) {
        printf("Invalid mapping address\n");
        return;
    }

    size = roundup_pow_of_two(size);
    needed_slots = (size + IO_TLB_SIZE - 1) >> IO_TLB_SHIFT;

    /* Simulate copying data back for FROM_DEVICE */
    if (direction == DMA_FROM_DEVICE || direction == DMA_BIDIRECTIONAL) {
        void *orig_addr = (void *)io_tlb.slots[slot_idx].orig_addr;
        memcpy(orig_addr, mapping, size);
    }

    /* Mark slots as free */
    for (unsigned int i = 0; i < needed_slots; i++) {
        unsigned int idx = (slot_idx + i) % io_tlb.nslabs;
        io_tlb.slots[idx].used = false;
        io_tlb.slots[idx].orig_addr = 0;
        io_tlb.slots[idx].alloc_size = 0;
        io_tlb.slots[idx].list = DMA_NONE;
    }

    io_tlb.used -= needed_slots;
    printf("Unmapped SWIOTLB address %p (size: %zu)\n", mapping, size);
}

/* Clean up SWIOTLB */
static void swiotlb_cleanup(void)
{
    if (io_tlb_buffer) {
        free(io_tlb_buffer);
        io_tlb_buffer = NULL;
    }
    if (io_tlb.slots) {
        free(io_tlb.slots);
        io_tlb.slots = NULL;
    }
    printf("SWIOTLB cleaned up\n");
}

/* Print SWIOTLB statistics */
static void print_swiotlb_stats(void)
{
    printf("\nSWIOTLB Statistics:\n");
    printf("Total slots: %lu\n", io_tlb.nslabs);
    printf("Used slots: %lu\n", io_tlb.used);
    printf("Free slots: %lu\n", io_tlb.nslabs - io_tlb.used);
    printf("Slot size: %d bytes\n", IO_TLB_SIZE);
    printf("Total memory: %d bytes\n", IO_TLB_TOTAL_SIZE);
}

int main()
{
    printf("SWIOTLB Test Program\n");
    printf("===================\n\n");

    /* Initialize SWIOTLB */
    if (swiotlb_init() != 0) {
        printf("Failed to initialize SWIOTLB\n");
        return -1;
    }

    /* Test 1: Simple mapping and unmapping */
    printf("\nTest 1: Simple mapping and unmapping\n");
    printf("------------------------------------\n");
    char test_buffer1[1024] = "Hello, SWIOTLB!";
    void *mapping1 = swiotlb_map(test_buffer1, sizeof(test_buffer1), DMA_TO_DEVICE);
    if (mapping1) {
        print_swiotlb_stats();
        swiotlb_unmap(mapping1, sizeof(test_buffer1), DMA_TO_DEVICE);
    }

    /* Test 2: Multiple mappings */
    printf("\nTest 2: Multiple mappings\n");
    printf("-------------------------\n");
    char test_buffer2[2048] = "Second buffer";
    char test_buffer3[4096] = "Third buffer";
    
    void *mapping2 = swiotlb_map(test_buffer2, sizeof(test_buffer2), DMA_BIDIRECTIONAL);
    void *mapping3 = swiotlb_map(test_buffer3, sizeof(test_buffer3), DMA_FROM_DEVICE);
    
    if (mapping2 && mapping3) {
        print_swiotlb_stats();
        swiotlb_unmap(mapping2, sizeof(test_buffer2), DMA_BIDIRECTIONAL);
        swiotlb_unmap(mapping3, sizeof(test_buffer3), DMA_FROM_DEVICE);
    }

    /* Test 3: Large allocation */
    printf("\nTest 3: Large allocation\n");
    printf("-----------------------\n");
    char *large_buffer = malloc(IO_TLB_TOTAL_SIZE);
    if (large_buffer) {
        void *mapping4 = swiotlb_map(large_buffer, IO_TLB_TOTAL_SIZE, DMA_TO_DEVICE);
        if (mapping4) {
            print_swiotlb_stats();
            swiotlb_unmap(mapping4, IO_TLB_TOTAL_SIZE, DMA_TO_DEVICE);
        }
        free(large_buffer);
    }

    /* Test 4: Overflow test */
    printf("\nTest 4: Overflow test\n");
    printf("--------------------\n");
    char *overflow_buffer = malloc(IO_TLB_TOTAL_SIZE * 2);
    if (overflow_buffer) {
        void *mapping5 = swiotlb_map(overflow_buffer, IO_TLB_TOTAL_SIZE * 2, DMA_TO_DEVICE);
        if (mapping5) {
            swiotlb_unmap(mapping5, IO_TLB_TOTAL_SIZE * 2, DMA_TO_DEVICE);
        }
        free(overflow_buffer);
    }

    /* Final statistics */
    printf("\nFinal SWIOTLB state:\n");
    printf("-------------------\n");
    print_swiotlb_stats();

    /* Cleanup */
    swiotlb_cleanup();
    return 0;
}
