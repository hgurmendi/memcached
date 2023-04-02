#ifndef __DLINK_NODE_H__
#define __DLINK_NODE_H__

struct Dlink_Node {
  struct Dlink_Node *previous;
  void *data;
  struct Dlink_Node *next;
};

// Allocate memory for a new node with no previous nor next references and
// return a pointer to it.
struct Dlink_Node *dlink_node_create(void *data);

// Insert a new node after the given, existing one, in the doubly linked list.
void dlink_node_insert_after(struct Dlink_Node *existing,
                             struct Dlink_Node *new_node);

// Insert a new node before the given, existing one, in the doubly linked list.
void dlink_node_insert_before(struct Dlink_Node *existing,
                              struct Dlink_Node *new_node);

// Removes the given, existing node from the doubly linked list and returns it.
// Does not destroy the memory allocated for the removed node, nor the memory
// allocated for its content.
struct Dlink_Node *dlink_node_remove(struct Dlink_Node *node);

// Traverses the doubly linked list forward, starting at the given, existing
// node through the `next` links.
void dlink_node_traverse_forward(struct Dlink_Node *start,
                                 void (*func)(void *));

// Traverses the doubly linked list backwards, starting at the given, existing
// node through the `previous` links.
void dlink_node_traverse_backwards(struct Dlink_Node *start,
                                   void (*func)(void *));

// Destroys the memory allocated for the given node.
void dlink_node_destroy(struct Dlink_Node *node);

#endif