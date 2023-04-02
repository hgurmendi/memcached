#include <stdio.h>
#include <stdlib.h>

#include "dlink_node.h"

// Allocate memory for a new node with no previous nor next references and
// return a pointer to it.
struct Dlink_Node *dlink_node_create(void *data) {
  struct Dlink_Node *new_node;

  new_node = (struct Dlink_Node *)malloc(sizeof(struct Dlink_Node));
  if (new_node == NULL) {
    perror("dlink_node_create");
    exit(EXIT_FAILURE);
  }

  new_node->previous = NULL;
  new_node->next = NULL;
  new_node->data = data;

  return new_node;
}

// Insert a new node after the given, existing one, in the doubly linked list.
void dlink_node_insert_after(struct Dlink_Node *existing,
                             struct Dlink_Node *new_node) {
  struct Dlink_Node *next_node = existing->next;
  existing->next = new_node;
  new_node->previous = existing;
  new_node->next = next_node;
  if (next_node != NULL) {
    next_node->previous = new_node;
  }
}

// Insert a new node before the given, existing one, in the doubly linked list.
void dlink_node_insert_before(struct Dlink_Node *existing,
                              struct Dlink_Node *new_node) {
  struct Dlink_Node *previous_node = existing->previous;
  existing->previous = new_node;
  new_node->next = existing;
  new_node->previous = previous_node;
  if (previous_node != NULL) {
    previous_node->next = new_node;
  }
}

// Removes the given, existing node from the doubly linked list and returns it.
// Does not destroy the memory allocated for the removed node, nor the memory
// allocated for its content.
struct Dlink_Node *dlink_node_remove(struct Dlink_Node *node) {
  struct Dlink_Node *previous_node = node->previous;
  struct Dlink_Node *next_node = node->next;

  if (previous_node != NULL) {
    previous_node->next = next_node;
    node->previous = NULL;
  }

  if (next_node != NULL) {
    next_node->previous = previous_node;
    node->next = NULL;
  }

  return node;
}

// Traverses the doubly linked list forward, starting at the given, existing
// node through the `next` links.
void dlink_node_traverse_forward(struct Dlink_Node *start,
                                 void (*func)(void *)) {
  struct Dlink_Node *current = start;

  while (current != NULL) {
    func(current->data);
    current = current->next;
  }
}

// Traverses the doubly linked list backwards, starting at the given, existing
// node through the `previous` links.
void dlink_node_traverse_backwards(struct Dlink_Node *start,
                                   void (*func)(void *)) {
  struct Dlink_Node *current = start;

  while (current != NULL) {
    func(current->data);
    current = current->previous;
  }
}

// Destroys the memory allocated for the given node.
void dlink_node_destroy(struct Dlink_Node *node) { free(node); }