#include "list.h"
#include "types.h"
#include "log.h"

struct window_list ** new_empty_list(uint8_t size) {
    return ( struct window_list ** ) umalloc(size * sizeof(struct window_list *));
}

struct window_list * new_empty_node(struct window_list ** list) {
    struct window_list * node;

    if (!list) return NULL;

    // NLOG("{$} Creating a new empty node for the list at %p\n", list);

    node = umalloc(sizeof(struct window_list));

    if (!(*list)) node->prev = node->next = NULL;
    else {
        node->next = *list;
        node->next->prev = node;
        node->prev = NULL;
    }
    *list = node;
    // NLOG("{$} Node %p appended to list %p\n", node, list);
    return node;
}

void move_to_head(struct window_list ** list, struct window_list * node){
    if (!node || !list || !*list) return;

    struct window_list * aux = *list;

    // already on head
    if (node == aux) return;
    else {
        node->prev->next = NULL;
        if (node->next)
            node->next->prev = NULL;
        
        node->next = *list;
        node->next->prev = node;
        node->prev = NULL;
        
        *list = node;
    }
}

void pop_node(struct window_list ** list, struct window_list * node) {
    if (!node || !list || !*list) return;

    struct window_list * aux = *list;

    if (node == aux){
        *list = node->next;
        if (node->next)
            node->next->prev = NULL;
    } else {
        node->prev->next = node->next;
        if (node->next)
            node->next->prev = node->prev;
    }

    ufree(node);
}

void free_node(struct window_list ** list, struct window_list * node) {
    if (!node || !list || !*list) return;
    if (node->window)
        ufree(node->window);
    node->window = NULL;
    pop_node(list, node);
}