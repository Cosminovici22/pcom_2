#include "queue.h"

#include <stdlib.h>
#include "utils.h"

struct queue *create_queue()
{
    struct queue *queue;

    queue = malloc(sizeof(struct queue));
    DIE(queue == NULL, "Error: unable to allocate memeory");

    queue->head = NULL;
    queue->tail = NULL;

    return queue;
}

void enqueue(struct queue *queue, void *data)
{
    struct node *node;

    if (queue == NULL)
        return;

    node = malloc(sizeof(struct node));
    DIE(node == NULL, "Error: unable to allocate memeory");

    node->data = data;
    node->prev = NULL;
    node->next = queue->head;
    if (queue->head != NULL)
        queue->head->prev = node;
    queue->head = node;
    if (queue->tail == NULL)
        queue->tail = node;
}

void *dequeue(struct queue *queue)
{
    void *data;
    struct node *tail;

    if (queue == NULL || queue->tail == NULL)
        return NULL;

    data = queue->tail->data;
    tail = queue->tail;

    if (queue->head == queue->tail)
        queue->head = NULL;
    queue->tail = queue->tail->prev;
    if (queue->tail != NULL)
        queue->tail->next = NULL;
    free(tail);

    return data;
}

void queue_remove_node(struct queue *queue, struct node *node)
{
    if (queue == NULL || node == NULL)
        return;

    if (queue->head == node)
        queue->head = node->next;
    if (queue->tail == node)
        queue->tail = node->prev;

    if (node->prev != NULL)
        node->prev->next = node->next;
    if (node->next != NULL)
        node->next->prev = node->prev;
    free(node);
}
