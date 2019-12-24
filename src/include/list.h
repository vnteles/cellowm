#pragma once

struct list * new_empty_node(struct list ** list);
void move_to_head(struct list ** list, struct list * node);

void pop_node(struct list ** list, struct list * node);
void free_node(struct list ** list, struct list * node);
void free_list(struct list ** list);

