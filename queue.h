#ifndef _QUEUE_H
#define _QUEUE_H

struct node {
    void *data;
    struct node *prev, *next;
};

struct queue {
    struct node *head, *tail;
};

struct queue *create_queue(void);
void enqueue(struct queue *queue, void *data);
void *dequeue(struct queue *queue);
void queue_remove_node(struct queue *queue, struct node *node);

#endif
