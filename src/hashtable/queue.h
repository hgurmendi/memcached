#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdbool.h>
#include <stdint.h>

#include "dlist.h"

struct Queue {
  // Pointer to the node in the front of the queue (i.e. the one that's coming
  // out first).
  struct DListNode *front;
  // Pointer to the node in the back of the queue (i.e. the one that was last
  // inserted).
  struct DListNode *back;
};

/* Allocates memory for the queue. */
struct Queue *queue_create();

/* Inserts a new node to the back of the queue.
 * The `prev` and `next` members of the node will be modified.
 */
void queue_enqueue(struct Queue *queue, struct DListNode *node);

/* Removes the element from the front of the queue.
 * The `prev` and `next` members of the node will be modified.
 */
struct DListNode *queue_dequeue(struct Queue *queue);

/* Removes the given node from the queue.
 * The `prev` and `next` members of the node will be modified.
 * Assumes that the node belongs in the queue.
 */
void queue_remove_node(struct Queue *queue, struct DListNode *node);

/* Prints the queue to stdout.
 * Elements in the back are to the left, elements in the front are to the right.
 */
void queue_print(struct Queue *queue);

/* Prints the queue to stdout in reverse order.
 * Elements in the front are to the left, elements in the back are to the right.
 */
void queue_print_reverse(struct Queue *queue);

#endif