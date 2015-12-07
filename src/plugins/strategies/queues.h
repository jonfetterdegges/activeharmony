#include "hperf.h"
#include "hutil.h"
#include "hpoint.h"

#include <stdlib.h>
#include <string.h>

/*
	Priority queue with insertion sort
*/
typedef struct priority_node{
    struct priority_node *next;
    unsigned long *idx;
    hpoint_t point;
    double perf;
    int expanded;
} pqueue_node_t;

pqueue_node_t *pqueue_pop(pqueue_node_t *queue);

pqueue_node_t *pqueue_beam_cut(pqueue_node_t *queue, int width, int depth, int *new_width, int *new_depth);

pqueue_node_t *pqueue_beam_next_to_expand(pqueue_node_t *queue);

pqueue_node_t *pqueue_push(pqueue_node_t *queue, const hpoint_t* point, double perf, unsigned long *idx, int idx_size, int expanded);

int pqueue_contains(pqueue_node_t *queue, unsigned long *idx, int idx_size);

void print_pqueue(pqueue_node_t *queue);
/*
	Fifo queue
*/
typedef struct queue_node {
    struct queue_node *next;
    unsigned long *idx;
    hpoint_t point;
} queue_node_t;

queue_node_t *queue_pop(queue_node_t *queue);

queue_node_t *queue_push_back(queue_node_t *queue, hpoint_t* point, unsigned long *idx, int idx_size);

/*
	Visited list
*/
typedef struct visited_node {
    struct visited_node *next;
    unsigned long *idx;
} visited_node_t;

visited_node_t *vqueue_push_back(visited_node_t *queue, unsigned long *idx, int idx_size);

int vqueue_contains(visited_node_t *queue, unsigned long *idx, int idx_size);