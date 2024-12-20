#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* Simplified kernel structures */
#define TASK_RUNNING    0
#define TASK_INTERRUPTIBLE  1
#define TASK_UNINTERRUPTIBLE    2

/* Process Management */
struct task_struct {
    int pid;
    int state;
    int priority;
    void *stack;
    struct task_struct *next;
};

/* Memory Management */
struct page {
    unsigned long flags;
    void *virtual;
    int count;
};

/* File System */
struct file {
    char *name;
    unsigned long size;
    struct file *next;
};

/* Network */
struct socket {
    int state;
    int type;
    void *ops;
};

/* Global variables */
struct task_struct *current_task = NULL;
struct page *page_list = NULL;
struct file *file_list = NULL;
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

/* Process Management Functions */
struct task_struct *create_task(int pid, int priority) {
    struct task_struct *task = malloc(sizeof(struct task_struct));
    if (!task) return NULL;
    
    task->pid = pid;
    task->state = TASK_RUNNING;
    task->priority = priority;
    task->stack = malloc(4096);  // Simple stack allocation
    task->next = NULL;
    return task;
}

void schedule(void) {
    if (!current_task) return;
    
    printf("Scheduling: PID %d, Priority %d\n", 
           current_task->pid, current_task->priority);
    
    // Simple round-robin
    if (current_task->next) {
        current_task = current_task->next;
    }
}

/* Memory Management Functions */
struct page *alloc_page(void) {
    struct page *new_page = malloc(sizeof(struct page));
    if (!new_page) return NULL;
    
    new_page->flags = 0;
    new_page->virtual = malloc(4096);  // 4KB page
    new_page->count = 1;
    return new_page;
}

void free_page(struct page *page) {
    if (!page) return;
    free(page->virtual);
    free(page);
}

/* File System Functions */
struct file *create_file(const char *name, unsigned long size) {
    struct file *file = malloc(sizeof(struct file));
    if (!file) return NULL;
    
    file->name = strdup(name);
    file->size = size;
    file->next = NULL;
    return file;
}

/* Synchronization Demo */
void *thread_function(void *arg) {
    int thread_id = *(int*)arg;
    
    pthread_mutex_lock(&global_lock);
    printf("Thread %d acquired lock\n", thread_id);
    sleep(1);  // Simulate work
    printf("Thread %d releasing lock\n", thread_id);
    pthread_mutex_unlock(&global_lock);
    
    return NULL;
}

/* Timer Demo */
void timer_callback(void) {
    static int ticks = 0;
    printf("Timer tick: %d\n", ++ticks);
}

/* Main test function */
int main(void) {
    printf("Linux Kernel Components Demo\n\n");

    /* Process Management Test */
    printf("1. Process Management Test\n");
    current_task = create_task(1, 100);
    struct task_struct *task2 = create_task(2, 90);
    current_task->next = task2;
    task2->next = current_task;  // Circular list
    
    for (int i = 0; i < 3; i++) {
        schedule();
    }
    printf("\n");

    /* Memory Management Test */
    printf("2. Memory Management Test\n");
    struct page *page1 = alloc_page();
    if (page1) {
        printf("Page allocated: %p\n", page1->virtual);
        free_page(page1);
        printf("Page freed\n");
    }
    printf("\n");

    /* File System Test */
    printf("3. File System Test\n");
    struct file *file1 = create_file("test.txt", 1024);
    if (file1) {
        printf("File created: %s, size: %lu\n", file1->name, file1->size);
    }
    printf("\n");

    /* Synchronization Test */
    printf("4. Synchronization Test\n");
    pthread_t threads[2];
    int thread_ids[2] = {1, 2};
    
    for (int i = 0; i < 2; i++) {
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }
    
    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("\n");

    /* Timer Test */
    printf("5. Timer Test\n");
    for (int i = 0; i < 3; i++) {
        timer_callback();
        usleep(100000);  // 100ms delay
    }
    printf("\n");

    /* Cleanup */
    free(current_task->stack);
    free(task2->stack);
    free(current_task);
    free(task2);
    free(file1->name);
    free(file1);

    printf("Demo completed successfully\n");
    return 0;
}
