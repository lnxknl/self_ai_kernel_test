#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Constants for bit operations */
#define BITS_PER_LONG 64
#define BITOP_WORD(nr) ((nr) / BITS_PER_LONG)
#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG))

/* Bit manipulation macros */
#define test_bit(nr, addr) \
    ((1UL << ((nr) % BITS_PER_LONG)) & \
    (((unsigned long *)addr)[((nr) / BITS_PER_LONG)]))

#define set_bit(nr, addr) \
    ((((unsigned long *)addr)[((nr) / BITS_PER_LONG)] |= \
    (1UL << ((nr) % BITS_PER_LONG))))

#define clear_bit(nr, addr) \
    ((((unsigned long *)addr)[((nr) / BITS_PER_LONG)] &= \
    ~(1UL << ((nr) % BITS_PER_LONG))))

/* Table of number of leading zeros for bytes */
static const uint8_t byte_leading_zeros[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Find first set bit in a word */
static inline unsigned long __ffs(unsigned long word)
{
    int num = 0;

#if BITS_PER_LONG == 64
    if ((word & 0xffffffff) == 0) {
        num += 32;
        word >>= 32;
    }
#endif
    if ((word & 0xffff) == 0) {
        num += 16;
        word >>= 16;
    }
    if ((word & 0xff) == 0) {
        num += 8;
        word >>= 8;
    }
    return num + byte_leading_zeros[~word & 0xff];
}

/* Find first zero bit */
static inline unsigned long ffz(unsigned long word)
{
    return __ffs(~word);
}

/* Find first set bit in an array */
unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
    unsigned long idx;

    for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
        if (addr[idx])
            return min(idx * BITS_PER_LONG + __ffs(addr[idx]), size);
    }

    return size;
}

/* Find first zero bit in an array */
unsigned long find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
    unsigned long idx;

    for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
        if (addr[idx] != ~0UL)
            return min(idx * BITS_PER_LONG + ffz(addr[idx]), size);
    }

    return size;
}

/* Find last set bit in an array */
unsigned long find_last_bit(const unsigned long *addr, unsigned long size)
{
    if (size) {
        unsigned long val = BITS_PER_LONG - 1;
        unsigned long idx = (size - 1) / BITS_PER_LONG;
        unsigned long temp = addr[idx];

        if (!temp)
            return find_last_bit(addr, idx * BITS_PER_LONG);

        temp = ((~temp) >> 1) & temp;
        while (temp) {
            val--;
            temp = ((~temp) >> 1) & temp;
        }
        return min(idx * BITS_PER_LONG + val, size - 1);
    }
    return 0;
}

/* Helper function to print bits */
void print_bits(const unsigned long *addr, unsigned long size)
{
    unsigned long i;
    printf("Bits: ");
    for (i = 0; i < size; i++) {
        printf("%d", test_bit(i, addr) ? 1 : 0);
        if ((i + 1) % 8 == 0)
            printf(" ");
    }
    printf("\n");
}

/* Helper function for min */
static inline unsigned long min(unsigned long a, unsigned long b)
{
    return a < b ? a : b;
}

int main()
{
    /* Test array size in bits */
    const unsigned long test_size = 128;
    /* Number of longs needed to store bits */
    const unsigned long num_longs = (test_size + BITS_PER_LONG - 1) / BITS_PER_LONG;
    
    /* Allocate and initialize test array */
    unsigned long *test_array = calloc(num_longs, sizeof(unsigned long));
    if (!test_array) {
        printf("Memory allocation failed!\n");
        return -1;
    }

    printf("Bit Finding Operations Test Program\n");
    printf("==================================\n\n");

    /* Test 1: Initial state */
    printf("1. Initial state (all zeros):\n");
    print_bits(test_array, test_size);
    printf("First set bit: %lu\n", find_first_bit(test_array, test_size));
    printf("First zero bit: %lu\n", find_first_zero_bit(test_array, test_size));
    printf("Last set bit: %lu\n\n", find_last_bit(test_array, test_size));

    /* Test 2: Set some bits */
    printf("2. Setting bits 5, 23, 45, 67, 89:\n");
    set_bit(5, test_array);
    set_bit(23, test_array);
    set_bit(45, test_array);
    set_bit(67, test_array);
    set_bit(89, test_array);
    print_bits(test_array, test_size);
    printf("First set bit: %lu\n", find_first_bit(test_array, test_size));
    printf("First zero bit: %lu\n", find_first_zero_bit(test_array, test_size));
    printf("Last set bit: %lu\n\n", find_last_bit(test_array, test_size));

    /* Test 3: Clear some bits */
    printf("3. Clearing bits 23 and 67:\n");
    clear_bit(23, test_array);
    clear_bit(67, test_array);
    print_bits(test_array, test_size);
    printf("First set bit: %lu\n", find_first_bit(test_array, test_size));
    printf("First zero bit: %lu\n", find_first_zero_bit(test_array, test_size));
    printf("Last set bit: %lu\n\n", find_last_bit(test_array, test_size));

    /* Test 4: Set all bits */
    printf("4. Setting all bits:\n");
    memset(test_array, 0xFF, num_longs * sizeof(unsigned long));
    print_bits(test_array, test_size);
    printf("First set bit: %lu\n", find_first_bit(test_array, test_size));
    printf("First zero bit: %lu\n", find_first_zero_bit(test_array, test_size));
    printf("Last set bit: %lu\n\n", find_last_bit(test_array, test_size));

    /* Test 5: Individual bit operations */
    printf("5. Testing individual bit operations:\n");
    printf("Bit 45 is set: %s\n", test_bit(45, test_array) ? "yes" : "no");
    clear_bit(45, test_array);
    printf("After clearing bit 45\n");
    printf("Bit 45 is set: %s\n", test_bit(45, test_array) ? "yes" : "no");
    set_bit(45, test_array);
    printf("After setting bit 45\n");
    printf("Bit 45 is set: %s\n\n", test_bit(45, test_array) ? "yes" : "no");

    /* Clean up */
    free(test_array);
    printf("Test completed successfully\n");

    return 0;
}
