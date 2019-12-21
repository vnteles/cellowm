#include "list.h"
#include "types.h"
#include "log.h"

struct list ** new_empty_list(uint8_t size) {
    return ( struct list ** ) umalloc(size * sizeof(struct list *));
}

struct list * new_empty_node(struct list ** list) {
    struct list * node;
    ELOG("{$} Creating a new empty node for the list at %p\n", list);

    node = umalloc(sizeof(struct list));

    if (!(*list)) node->prev = node->next = NULL;
    else {
        node->next = *list;
        node->next->prev = node;
        node->prev = NULL;
    }
    *list = node;
    ELOG("{$} Node %p appended to list %p\n", node, list);
    return node;
}

void pop_node(struct list ** list, struct list * node) {
    if (!node || !list || !*list) return;

    struct list * aux = *list;

    if (node == *list){
        *list = aux->next;
        if (node->next)
            node->next->prev = NULL;
    } else {
        node->prev->next = node->next;
        if (node->next)
            node->next->prev = node->prev;
    }

    free(node);
}

void free_node(struct list ** list, struct list * node) {
    if (!node || !list || !*list) return;
    if (node->gdata)
        free(node->gdata);
    node->gdata = NULL;
    pop_node(list, node);
}

void free_list(struct list ** list) {
    struct list *node;
    
    for (node = *list; node != NULL; node = node->next){
        free_node(list, node);
    }

}

void list_all(struct list * list) {
    struct list * node;
    unsigned int i;
    for (node = list, i = 1; node; node=node->next, i++) printf("node #%d at %p\n", i, (void *)node);
}