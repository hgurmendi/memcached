#include <stdio.h>
#include <stdlib.h>

#include "dlink.h"

// Allocate memory for a new, empty, doubly linked list.
struct Dlink *dlink_create() {
  struct Dlink *dlink = malloc(sizeof(struct Dlink));
  if (dlink == NULL) {
    perror("dlink_list_create");
    exit(EXIT_FAILURE);
  }

  dlink->first = NULL;
  dlink->last = NULL;

  return dlink;
}

// Inserts the given node to the given doubly linked list and make it the last
// element.
void dlink_insert_first(struct Dlink *dlink, struct Dlink_Node *new_node) {
  if (dlink->first == NULL) {
    dlink->first = new_node;
    dlink->last = new_node;
    return;
  }

  dlink_node_insert_before(dlink->first, new_node);
  dlink->first = new_node;
}

// Inserts the given node to the given doubly linked list and make it the first
// element.
void dlink_insert_last(struct Dlink *dlink, struct Dlink_Node *new_node) {
  if (dlink->last == NULL) {
    dlink->first = new_node;
    dlink->last = new_node;
    return;
  }

  dlink_node_insert_after(dlink->last, new_node);
  dlink->last = new_node;
}

// Removes the first node from the given doubly linked list and returns a
// pointer to the node with both `next` and `previous` pointers valued `NULL`.
// Returns `NULL` if the doubly linked list is empty.
// Does not destroy the memory allocated for the removed node, nor the memory
// allocated for its content.
struct Dlink_Node *dlink_remove_first(struct Dlink *dlink) {
  struct Dlink_Node *node_to_remove = dlink->first;

  if (node_to_remove == NULL) {
    // The list is empty.
    return NULL;
  }

  if (node_to_remove->next == NULL) {
    // The list has a single node.
    dlink->first = NULL;
    dlink->last = NULL;
  } else {
    dlink->first = node_to_remove->next;
  }

  return dlink_node_remove(node_to_remove);
}

// Removes the last node from the given doubly linked list and returns a
// pointer to the node with both `next` and `previous` pointers valued `NULL`.
// Returns `NULL` if the doubly linked list is empty.
// Does not destroy the memory allocated for the removed node, nor the memory
// allocated for its content.
struct Dlink_Node *dlink_remove_last(struct Dlink *dlink) {
  struct Dlink_Node *node_to_remove = dlink->last;

  if (node_to_remove == NULL) {
    // The list is empty.
    return NULL;
  }

  if (node_to_remove->previous == NULL) {
    // The list has a single node.
    dlink->first = NULL;
    dlink->last = NULL;
  } else {
    dlink->last = node_to_remove->previous;
  }

  return dlink_node_remove(node_to_remove);
}

// Traverses the doubly linked list forward, starting at the first element and
// through the `next` links.
void dlink_traverse_forward(struct Dlink *dlink, void (*func)(void *)) {
  if (dlink->first == NULL) {
    return;
  }
  dlink_node_traverse_forward(dlink->first, func);
}

// Traverses the doubly linked list backwards, starting at the last element and
// through the `previous` links.
void dlink_traverse_backwards(struct Dlink *dlink, void (*func)(void *)) {
  if (dlink->last == NULL) {
    return;
  }
  dlink_node_traverse_backwards(dlink->last, func);
}

// Destroys the memory allocated for the given doubly linked list.
void dlink_destroy(struct Dlink *dlink) { free(dlink); }
