#include <stdio.h>
#include <stdlib.h>

#include "dlist.h"

/* Sets proper initial values to the members of the node.
 * Does not allocate memory in any way.
 */
void dlist_node_initialize(struct DListNode *node, void *value) {
  node->prev = NULL;
  node->next = NULL;
  node->value = value;
}