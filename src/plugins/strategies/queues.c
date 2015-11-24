#include "queues.h"

#include <string.h> 

queue_node_t *queue_pop(queue_node_t *queue){
    if(queue != NULL){
        hpoint_t item = queue->point;
        queue_node_t *tmp = queue;
        queue = queue->next;
        free(tmp->idx);
        free(tmp);
    }
    return queue;
}

queue_node_t *queue_push_back(queue_node_t *queue, hpoint_t point, unsigned long *idx, int idx_size){
    queue_node_t *new_node = (queue_node_t *)malloc(sizeof(queue_node_t));
    if (new_node == NULL){
        exit(-1);
    }
    new_node->point = point;
    new_node->idx = malloc(idx_size * sizeof(unsigned long));
    memcpy(new_node->idx, idx, idx_size*sizeof(unsigned long));
    new_node->next = NULL;

    queue_node_t *find_last = queue;
    if(queue != NULL){
        while(find_last->next != NULL){
            find_last = find_last->next;
        }
        find_last->next = new_node;
    }else{
        queue = new_node;
    }
    return queue;
}


visited_node_t *vqueue_push_back(visited_node_t *queue, unsigned long *idx, int idx_size){

    visited_node_t *new_node = (visited_node_t *)malloc(sizeof(visited_node_t));
    if (new_node == NULL){
        exit(-1);
    }
    new_node->idx = malloc(idx_size * sizeof(unsigned long));
    memcpy(new_node->idx, idx, idx_size*sizeof(unsigned long));
    new_node->next = NULL;

    visited_node_t *find_last = queue;
    if(queue != NULL){
        while(find_last->next != NULL){
            find_last = find_last->next;
        }
        find_last->next = new_node;
    }else{
        queue = new_node;
    }
    return queue;
}

int vqueue_contains(visited_node_t *queue, unsigned long *idx, int idx_size){
    visited_node_t *iter = queue;
    while(iter != NULL){
        if(memcmp(iter->idx, idx, idx_size*sizeof(unsigned long)) == 0){
            return 0;
        }else{
            iter = iter->next;
        }
    }
    return -1;
}