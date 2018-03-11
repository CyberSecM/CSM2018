#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define llist_foreach(entry, lhead) \
    for(struct llist_node* node = lhead->head; ; ) { \
        entry = node->data; \
        node = node->next;

#define llist_foreach_end(lhead) \
        if(node == lhead->head) break; \
    }

struct llist_node {
    struct llist_node* next;
    struct llist_node* prev;
    void* data;
};

struct llist {
    struct llist_node* head;
    struct llist_node* tail;
};

static inline void _llist_add(struct llist_node* next, struct llist_node* prev, struct llist_node* new) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void llist_add_head(struct llist* list, struct llist_node* new) {
    struct llist_node* head = list->head;
    struct llist_node* tail = list->tail;
    if(head == NULL) {
        assert(tail == NULL);
        list->head = list->tail = new;
    }
    _llist_add(list->tail, list->head, new);
    list->head = new;
}

static inline void llist_add_tail(struct llist* list, struct llist_node* new) {
    struct llist_node* head = list->head;
    struct llist_node* tail = list->tail;
    if(tail == NULL) {
        assert(head == NULL);
        list->head = list->tail = new;
    }
    _llist_add(list->tail, list->head, new);
    list->tail = new;
}

static inline void llist_add(struct llist* list, void* data) {
    struct llist_node* new;
    new = malloc(sizeof(*new));
    if(new == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    new->data = data;
    llist_add_tail(list, new);
}
