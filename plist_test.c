#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* Basic list implementation */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *new,
                            struct list_head *prev,
                            struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_for_each_entry(pos, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);    \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* Priority list implementation */
struct plist_head {
    struct list_head node_list;
};

struct plist_node {
    int prio;
    struct list_head prio_list;
    struct list_head node_list;
};

static inline void plist_head_init(struct plist_head *head)
{
    INIT_LIST_HEAD(&head->node_list);
}

static inline void plist_node_init(struct plist_node *node, int prio)
{
    node->prio = prio;
    INIT_LIST_HEAD(&node->prio_list);
    INIT_LIST_HEAD(&node->node_list);
}

static inline int plist_head_empty(const struct plist_head *head)
{
    return list_empty(&head->node_list);
}

static inline int plist_node_empty(const struct plist_node *node)
{
    return list_empty(&node->node_list);
}

void plist_add(struct plist_node *node, struct plist_head *head)
{
    struct plist_node *first, *iter, *prev = NULL;
    struct list_head *node_next = &head->node_list;

    if (plist_head_empty(head))
        goto ins_node;

    first = iter = list_entry(head->node_list.next,
                            struct plist_node, node_list);

    do {
        if (node->prio < iter->prio) {
            node_next = &iter->node_list;
            break;
        }

        prev = iter;
        iter = list_entry(iter->prio_list.next,
                        struct plist_node, prio_list);
    } while (iter != first);

    if (!prev || prev->prio != node->prio)
        list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
    list_add_tail(&node->node_list, node_next);
}

void plist_del(struct plist_node *node, struct plist_head *head)
{
    if (!list_empty(&node->prio_list)) {
        if (node->node_list.next != &head->node_list) {
            struct plist_node *next;

            next = list_entry(node->node_list.next,
                            struct plist_node, node_list);

            if (list_empty(&next->prio_list))
                list_add(&next->prio_list, &node->prio_list);
        }
        list_del_init(&node->prio_list);
    }

    list_del_init(&node->node_list);
}

/* Test structure */
struct task {
    const char *name;
    int priority;
    struct plist_node node;
};

void print_tasks(struct plist_head *head)
{
    struct task *task;
    printf("Tasks in priority order:\n");
    list_for_each_entry(task, &head->node_list, node.node_list) {
        printf("Task '%s' with priority %d\n", 
               task->name, task->priority);
    }
    printf("\n");
}

int main()
{
    struct plist_head head;
    struct task tasks[] = {
        {"Task A", 3, {{0}}},
        {"Task B", 1, {{0}}},
        {"Task C", 4, {{0}}},
        {"Task D", 1, {{0}}},
        {"Task E", 2, {{0}}}
    };
    int i;

    /* Initialize the priority list head */
    plist_head_init(&head);

    /* Initialize and add tasks */
    printf("Adding tasks...\n");
    for (i = 0; i < sizeof(tasks)/sizeof(tasks[0]); i++) {
        plist_node_init(&tasks[i].node, tasks[i].priority);
        plist_add(&tasks[i].node, &head);
        printf("Added '%s' with priority %d\n", 
               tasks[i].name, tasks[i].priority);
    }
    printf("\n");

    /* Print all tasks in priority order */
    print_tasks(&head);

    /* Remove a task */
    printf("Removing Task C...\n");
    plist_del(&tasks[2].node, &head);
    print_tasks(&head);

    /* Add a new task */
    printf("Adding Task C back with priority 0...\n");
    tasks[2].priority = 0;
    plist_node_init(&tasks[2].node, tasks[2].priority);
    plist_add(&tasks[2].node, &head);
    print_tasks(&head);

    return 0;
}
