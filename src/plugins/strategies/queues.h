#include "hperf.h"
#include "hutil.h"
#include "hpoint.h"

#include <stdlib.h>
#include <string.h>

/*
	Fifo queue
*/
typedef struct queue_node {
    struct queue_node *next;
    unsigned long *idx;
    hpoint_t point;
} queue_node_t;


queue_node_t *queue_pop(queue_node_t *queue);

queue_node_t *queue_push_back(queue_node_t *queue, hpoint_t point, unsigned long *idx, int idx_size);

/*
	Visited list
*/
typedef struct visited_node {
    struct visited_node *next;
    unsigned long *idx;
} visited_node_t;

visited_node_t *vqueue_push_back(visited_node_t *queue, unsigned long *idx, int idx_size);
int vqueue_contains(visited_node_t *queue, unsigned long *idx, int idx_size);