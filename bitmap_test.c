#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Bitmap implementation */
#define BITS_PER_LONG 32
#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr) ((nr) / BITS_PER_LONG)
#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))
#define small_const_nbits(nbits) ((nbits) <= BITS_PER_LONG)

/* Set bit n in bitmap */
static inline void set_bit(int nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);
    *p |= mask;
}

/* Clear bit n in bitmap */
static inline void clear_bit(int nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);
    *p &= ~mask;
}

/* Test bit n in bitmap */
static inline int test_bit(int nr, const unsigned long *addr)
{
    return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}

/* Find first set bit in bitmap */
static inline unsigned long _find_first_bit(const unsigned long *addr, unsigned long size)
{
    unsigned long idx;

    for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
        if (addr[idx])
            return min(idx * BITS_PER_LONG + __builtin_ctz(addr[idx]), size);
    }

    return size;
}

/* Find first zero bit in bitmap */
static inline unsigned long _find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
    unsigned long idx;

    for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
        if (addr[idx] != ~0UL)
            return min(idx * BITS_PER_LONG + __builtin_ctz(~addr[idx]), size);
    }

    return size;
}

/* Find next set bit in bitmap */
static inline unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
                    unsigned long offset)
{
    if (offset >= size)
        return size;

    offset = _find_first_bit(addr + BIT_WORD(offset),
                size - offset) + offset;
    return min(offset, size);
}

/* Find next zero bit in bitmap */
static inline unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size,
                        unsigned long offset)
{
    if (offset >= size)
        return size;

    offset = _find_first_zero_bit(addr + BIT_WORD(offset),
                    size - offset) + offset;
    return min(offset, size);
}

/* Set bits in bitmap */
void bitmap_set(unsigned long *map, unsigned int start, int len)
{
    unsigned long *p = map + BIT_WORD(start);
    const unsigned int size = start + len;
    int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
    unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

    while (len - bits_to_set >= 0) {
        *p |= mask_to_set;
        len -= bits_to_set;
        bits_to_set = BITS_PER_LONG;
        mask_to_set = ~0UL;
        p++;
    }
    if (len) {
        mask_to_set &= BITMAP_LAST_WORD_MASK(size);
        *p |= mask_to_set;
    }
}

/* Clear bits in bitmap */
void bitmap_clear(unsigned long *map, unsigned int start, int len)
{
    unsigned long *p = map + BIT_WORD(start);
    const unsigned int size = start + len;
    int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
    unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

    while (len - bits_to_clear >= 0) {
        *p &= ~mask_to_clear;
        len -= bits_to_clear;
        bits_to_clear = BITS_PER_LONG;
        mask_to_clear = ~0UL;
        p++;
    }
    if (len) {
        mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
        *p &= ~mask_to_clear;
    }
}

/* Print bitmap for debugging */
void print_bitmap(const unsigned long *map, unsigned int bits)
{
    unsigned int i;
    printf("Bitmap (%u bits): ", bits);
    for (i = 0; i < bits; i++) {
        printf("%d", test_bit(i, map) ? 1 : 0);
        if ((i + 1) % 8 == 0)
            printf(" ");
    }
    printf("\n");
}

/* Test program */
int main()
{
    /* Allocate bitmap for 64 bits */
    const unsigned int bits = 64;
    const unsigned int words = (bits + BITS_PER_LONG - 1) / BITS_PER_LONG;
    unsigned long *bitmap = calloc(words, sizeof(unsigned long));
    
    if (!bitmap) {
        printf("Failed to allocate bitmap!\n");
        return -1;
    }

    printf("Bitmap Test Program\n");
    printf("==================\n\n");

    /* Test 1: Set individual bits */
    printf("1. Testing individual bit operations:\n");
    printf("Setting bits 0, 5, 10, 31, 32, 63\n");
    set_bit(0, bitmap);
    set_bit(5, bitmap);
    set_bit(10, bitmap);
    set_bit(31, bitmap);
    set_bit(32, bitmap);
    set_bit(63, bitmap);
    print_bitmap(bitmap, bits);
    printf("\n");

    /* Test 2: Test bit operations */
    printf("2. Testing bit test operations:\n");
    printf("Bit 0 is set: %s\n", test_bit(0, bitmap) ? "yes" : "no");
    printf("Bit 1 is set: %s\n", test_bit(1, bitmap) ? "yes" : "no");
    printf("Bit 5 is set: %s\n", test_bit(5, bitmap) ? "yes" : "no");
    printf("Bit 63 is set: %s\n", test_bit(63, bitmap) ? "yes" : "no");
    printf("\n");

    /* Test 3: Clear bits */
    printf("3. Testing bit clear operations:\n");
    printf("Clearing bits 5 and 32\n");
    clear_bit(5, bitmap);
    clear_bit(32, bitmap);
    print_bitmap(bitmap, bits);
    printf("\n");

    /* Test 4: Find first set/zero bit */
    printf("4. Testing find operations:\n");
    printf("First set bit: %lu\n", _find_first_bit(bitmap, bits));
    printf("First zero bit: %lu\n", _find_first_zero_bit(bitmap, bits));
    printf("\n");

    /* Test 5: Find next set/zero bit */
    printf("5. Testing find next operations:\n");
    printf("Next set bit after 0: %lu\n", find_next_bit(bitmap, bits, 1));
    printf("Next zero bit after 0: %lu\n", find_next_zero_bit(bitmap, bits, 1));
    printf("\n");

    /* Test 6: Set range of bits */
    printf("6. Testing bitmap_set operation:\n");
    printf("Setting bits 15-25\n");
    bitmap_set(bitmap, 15, 11);  /* 11 bits starting at position 15 */
    print_bitmap(bitmap, bits);
    printf("\n");

    /* Test 7: Clear range of bits */
    printf("7. Testing bitmap_clear operation:\n");
    printf("Clearing bits 20-30\n");
    bitmap_clear(bitmap, 20, 11);  /* 11 bits starting at position 20 */
    print_bitmap(bitmap, bits);
    printf("\n");

    /* Clean up */
    free(bitmap);
    printf("Test completed.\n");

    return 0;
}
