#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Simplified kfifo implementation */
struct kfifo {
    unsigned char *buffer;
    unsigned int size;
    unsigned int in;
    unsigned int out;
    unsigned int mask;
};

/* Helper macros */
#define min(a, b) ((a) < (b) ? (a) : (b))

/* Check if number is power of 2 */
static inline int is_power_of_2(unsigned int n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

/* Round up to nearest power of 2 */
static inline unsigned int roundup_pow_of_two(unsigned int n)
{
    unsigned int i;
    if (is_power_of_2(n))
        return n;
    
    i = 1;
    while (i < n)
        i <<= 1;
    return i;
}

/* Initialize kfifo */
int kfifo_init(struct kfifo *fifo, unsigned char *buffer, unsigned int size)
{
    if (!is_power_of_2(size))
        return -1;

    fifo->buffer = buffer;
    fifo->size = size;
    fifo->in = 0;
    fifo->out = 0;
    fifo->mask = size - 1;

    return 0;
}

/* Allocate a new kfifo */
struct kfifo *kfifo_alloc(unsigned int size)
{
    struct kfifo *fifo;
    unsigned char *buffer;

    /* Round up size to power of 2 */
    size = roundup_pow_of_two(size);

    fifo = malloc(sizeof(*fifo));
    if (!fifo)
        return NULL;

    buffer = malloc(size);
    if (!buffer) {
        free(fifo);
        return NULL;
    }

    kfifo_init(fifo, buffer, size);
    return fifo;
}

/* Free kfifo */
void kfifo_free(struct kfifo *fifo)
{
    if (fifo) {
        free(fifo->buffer);
        free(fifo);
    }
}

/* Put data into kfifo */
unsigned int kfifo_in(struct kfifo *fifo, const unsigned char *buffer, unsigned int len)
{
    unsigned int l;
    len = min(len, fifo->size - (fifo->in - fifo->out));

    /* First put the data starting from fifo->in to buffer end */
    l = min(len, fifo->size - (fifo->in & fifo->mask));
    memcpy(fifo->buffer + (fifo->in & fifo->mask), buffer, l);

    /* Then put the rest (if any) at the beginning of the buffer */
    memcpy(fifo->buffer, buffer + l, len - l);

    fifo->in += len;
    return len;
}

/* Get data from kfifo */
unsigned int kfifo_out(struct kfifo *fifo, unsigned char *buffer, unsigned int len)
{
    unsigned int l;
    len = min(len, fifo->in - fifo->out);

    /* First get the data from fifo->out until the end of the buffer */
    l = min(len, fifo->size - (fifo->out & fifo->mask));
    memcpy(buffer, fifo->buffer + (fifo->out & fifo->mask), l);

    /* Then get the rest (if any) from the beginning of the buffer */
    memcpy(buffer + l, fifo->buffer, len - l);

    fifo->out += len;

    /* Ensure out is bounded by size */
    if (fifo->out == fifo->in) {
        fifo->out = 0;
        fifo->in = 0;
    }

    return len;
}

/* Get fifo length */
static inline unsigned int kfifo_len(struct kfifo *fifo)
{
    return fifo->in - fifo->out;
}

/* Check if fifo is empty */
static inline int kfifo_is_empty(struct kfifo *fifo)
{
    return fifo->in == fifo->out;
}

/* Check if fifo is full */
static inline int kfifo_is_full(struct kfifo *fifo)
{
    return kfifo_len(fifo) == fifo->size;
}

/* Test functions */
void print_fifo_status(struct kfifo *fifo)
{
    printf("FIFO Status:\n");
    printf("  Size: %u\n", fifo->size);
    printf("  Used: %u\n", kfifo_len(fifo));
    printf("  Free: %u\n", fifo->size - kfifo_len(fifo));
    printf("  Empty: %s\n", kfifo_is_empty(fifo) ? "yes" : "no");
    printf("  Full: %s\n", kfifo_is_full(fifo) ? "yes" : "no");
    printf("  In: %u, Out: %u\n", fifo->in, fifo->out);
}

void print_buffer(const char *label, unsigned char *buffer, unsigned int len)
{
    printf("%s: ", label);
    for (unsigned int i = 0; i < len; i++) {
        printf("%c", buffer[i]);
    }
    printf("\n");
}

int main()
{
    struct kfifo *fifo;
    unsigned char buffer[128];
    unsigned int ret;
    const char *test_data[] = {
        "Hello",
        " FIFO",
        " Test",
        "!"
    };

    printf("KFIFO Test Program\n");
    printf("=================\n\n");

    /* Create a FIFO of size 16 */
    printf("1. Creating FIFO of size 16 bytes...\n");
    fifo = kfifo_alloc(16);
    if (!fifo) {
        printf("Failed to allocate FIFO!\n");
        return -1;
    }
    print_fifo_status(fifo);
    printf("\n");

    /* Test putting data */
    printf("2. Testing data input...\n");
    for (int i = 0; i < 4; i++) {
        printf("Putting: \"%s\"\n", test_data[i]);
        ret = kfifo_in(fifo, (unsigned char *)test_data[i], strlen(test_data[i]));
        printf("Written: %u bytes\n", ret);
        print_fifo_status(fifo);
        printf("\n");
    }

    /* Test getting data */
    printf("3. Testing data output...\n");
    memset(buffer, 0, sizeof(buffer));
    ret = kfifo_out(fifo, buffer, sizeof(buffer));
    printf("Read %u bytes\n", ret);
    print_buffer("Retrieved data", buffer, ret);
    print_fifo_status(fifo);
    printf("\n");

    /* Test wrap-around */
    printf("4. Testing wrap-around...\n");
    const char *wrap_data = "Testing wrap-around data";
    printf("Putting: \"%s\"\n", wrap_data);
    ret = kfifo_in(fifo, (unsigned char *)wrap_data, strlen(wrap_data));
    printf("Written: %u bytes\n", ret);
    print_fifo_status(fifo);
    printf("\n");

    /* Read half of the data */
    printf("5. Reading half of the data...\n");
    memset(buffer, 0, sizeof(buffer));
    ret = kfifo_out(fifo, buffer, ret/2);
    printf("Read %u bytes\n", ret);
    print_buffer("Retrieved data", buffer, ret);
    print_fifo_status(fifo);
    printf("\n");

    /* Read remaining data */
    printf("6. Reading remaining data...\n");
    memset(buffer, 0, sizeof(buffer));
    ret = kfifo_out(fifo, buffer, sizeof(buffer));
    printf("Read %u bytes\n", ret);
    print_buffer("Retrieved data", buffer, ret);
    print_fifo_status(fifo);
    printf("\n");

    /* Clean up */
    printf("7. Cleaning up...\n");
    kfifo_free(fifo);
    printf("FIFO freed\n");

    return 0;
}
