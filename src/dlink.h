#ifndef __DLINK_LIST_H__
#define __DLINK_LIST_H__

#include "dlink_node.h"

struct Dlink {
  struct Dlink_Node *first;
  struct Dlink_Node *last;
};

// Allocate memory for a new, empty, doubly linked list.
struct Dlink *dlink_create();

// Inserts the given node to the given doubly linked list and make it the last
// element.
void dlink_insert_first(struct Dlink *dlink, struct Dlink_Node *new_node);

// Inserts the given node to the given doubly linked list and make it the first
// element.
void dlink_insert_last(struct Dlink *dlink, struct Dlink_Node *new_node);

// Removes the first node from the given doubly linked list and returns a
// pointer to the node with both `next` and `previous` pointers valued `NULL`.
// Returns `NULL` if the doubly linked list is empty.
// Does not destroy the memory allocated for the removed node, nor the memory
// allocated for its content.
struct Dlink_Node *dlink_remove_first(struct Dlink *dlink);

// Removes the last node from the given doubly linked list and returns a
// pointer to the node with both `next` and `previous` pointers valued `NULL`.
// Returns `NULL` if the doubly linked list is empty.
// Does not destroy the memory allocated for the removed node, nor the memory
// allocated for its content.
struct Dlink_Node *dlink_remove_last(struct Dlink *dlink);

// Traverses the doubly linked list forward, starting at the first element and
// through the `next` links.
void dlink_traverse_forward(struct Dlink *dlink, void (*func)(void *));

// Traverses the doubly linked list backwards, starting at the last element and
// through the `previous` links.
void dlink_traverse_backwards(struct Dlink *dlink, void (*func)(void *));

// Destroys the memory allocated for the given doubly linked list.
void dlink_destroy(struct Dlink *dlink);

#endif