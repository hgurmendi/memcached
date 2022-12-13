#ifndef __DLIST_H__
#define __DLIST_H__

struct DListNode {
  // Previous node in the doubly-linked list. May be NULL.
  struct DListNode *prev;
  // Pointer to arbitrary data. May be NULL.
  void *value;
  // Next node in the doubly-linked list. May be NULL.
  struct DListNode *next;
};

/* Sets proper initial values to the members of the node.
 * Does not allocate memory in any way.
 */
void dlist_node_initialize(struct DListNode *node, void *value);

#endif