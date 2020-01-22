#include "list.h"
#include "types.h"
#include "log.h"
#include "utils.h"

struct window_list * new_empty_node(struct window_list ** list) {
    struct window_list * node;

    if (!list) return NULL;

    node = umalloc(sizeof(struct window_list));

    if (!(*list)) node->prev = node->next = NULL;
    else {
        node->next = *list;
        node->next->prev = node;
        node->prev = NULL;
    }
    *list = node;
    return node;
}

void bring_to_head(struct window_list ** list, struct window_list * node){
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

struct window_list * pop_node(struct window_list ** list, struct window_list * node) {
    if (!node || !list || !*list) return NULL;

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

    return node;
}

void free_node(struct window_list ** list, struct window_list * node) {
    if (!node || !list || !*list) return;
    if (node->window)
        ufree(node->window);
    node->window = NULL;
    struct window_list * pnode = pop_node(list, node);

    if (pnode) ufree(pnode);
}