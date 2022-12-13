#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Allocates memory for the queue. */
struct Queue *queue_create() {
  struct Queue *queue = NULL;

  queue = malloc(sizeof(struct Queue));
  if (queue == NULL) {
    perror("malloc queue_create");
    abort();
  }

  queue->front = NULL;
  queue->back = NULL;

  return queue;
}

/* Inserts a new node to the back of the queue.
 * The `prev` and `next` members of the node will be modified.
 */
void queue_enqueue(struct Queue *queue, struct DListNode *node) {
  node->prev = NULL;
  node->next = queue->back;

  if (queue->back == NULL) {
    queue->back = queue->front = node;
  } else {
    queue->back->prev = node;
    queue->back = node;
  }
}

/* Removes the element from the front of the queue.
 * The `prev` and `next` members of the node will be modified.
 */
struct DListNode *queue_dequeue(struct Queue *queue) {
  struct DListNode *popped_node = queue->front;

  if (popped_node == NULL) {
    return NULL;
  }

  struct DListNode *new_front = popped_node->prev;

  if (new_front == NULL) {
    queue->back = queue->front = NULL;
  } else {
    queue->front = new_front;
    queue->front->next = NULL;
  }

  popped_node->next = popped_node->prev = NULL;
  return popped_node;
}

/* Prints the queue to stdout.
 * Elements in the back are to the left, elements in the front are to the right.
 */
void queue_print(struct Queue *queue) {
  struct DListNode *current_node = queue->back;

  if (current_node == NULL) {
    printf("<Empty queue>\n");
    return;
  }

  printf("X <-> ");
  while (current_node != NULL) {
    char *node_value = (char *)current_node->value;
    printf("%s <-> ", node_value);
    current_node = current_node->next;
  }
  printf("X\n");
}

/* Prints the queue to stdout in reverse order.
 * Elements in the front are to the left, elements in the back are to the right.
 */
void queue_print_reverse(struct Queue *queue) {
  struct DListNode *current_node = queue->front;

  if (current_node == NULL) {
    printf("<Empty queue>\n");
    return;
  }

  printf("X <-> ");
  while (current_node != NULL) {
    char *node_value = (char *)current_node->value;
    printf("%s <-> ", node_value);
    current_node = current_node->prev;
  }
  printf("X\n");
}