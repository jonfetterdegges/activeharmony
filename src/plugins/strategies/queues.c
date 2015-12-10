#include "queues.h"

#include <string.h> 

pqueue_node_t *pqueue_pop(pqueue_node_t *queue){
  if(queue != NULL){
    hpoint_t item = queue->point;
    pqueue_node_t *tmp = queue;
    queue = queue->next;
    free(tmp->idx);
    hpoint_fini(&tmp->point);
    free(tmp);
  }
  return queue;
}


void print_pqueue(pqueue_node_t *queue){
  int currPoint = 0;
  while(queue != NULL){
    fprintf(stderr, "%d) Exp = %d Perf = %.5f\n", currPoint, queue->expanded, queue->perf);
    currPoint++;
    queue = queue->next;
  }
}


pqueue_node_t *pqueue_beam_cut(pqueue_node_t *queue, int width, int depth, int *new_width, int *new_depth){
  int curr_width = 0;
  int curr_depth = 0;

  pqueue_node_t *iter = queue;
  pqueue_node_t *prev = NULL;

  while(curr_depth != depth && iter != NULL){
    //unexpanded condition
    if(iter->expanded == 0){
      //THIS SHOULD NEVER HAPPEN
      //in order for the algorithm to work we need width > 0 and thus 
      //we will never go in the if when iter == head and prev == NULL
      if(curr_width == width){
        //Delete the node
        pqueue_node_t *tmp = iter ;
        iter = iter->next;
        free(tmp->idx);
        hpoint_fini(&tmp->point);
        free(tmp);
        prev->next = iter;
      }else{
        curr_width++;
        curr_depth++;
        prev = iter;
        iter = iter->next;
      }
    }else{
      curr_depth++;
      prev = iter;
      iter = iter->next;
    }
  }
  if(curr_depth == depth && iter != NULL){
    //delete all other nodes
    while(iter->next != NULL){
      pqueue_node_t *tmp = iter->next ;
      iter->next = iter->next->next;
      free(tmp->idx);
      hpoint_fini(&tmp->point);
      free(tmp);
    }
  }
  *new_depth = curr_depth;
  *new_width = curr_width;
  return queue;
}

pqueue_node_t *pqueue_beam_next_to_expand(pqueue_node_t *queue){
  pqueue_node_t *iter = queue;
  while(iter != NULL && iter->expanded == 1){
    iter = iter->next;
  }
  return iter;
}

pqueue_node_t *pqueue_push(pqueue_node_t *queue, const hpoint_t* point, double perf, unsigned long *idx, int idx_size, int expanded){
  pqueue_node_t *new_node = (pqueue_node_t *)malloc(sizeof(pqueue_node_t));
  new_node->point = HPOINT_INITIALIZER;
  if (new_node == NULL){
    exit(-1);
  }
  hpoint_copy(&new_node->point, point);
  new_node->perf = perf;
  new_node->expanded = expanded;
  new_node->idx = malloc(idx_size * sizeof(unsigned long));
  memcpy(new_node->idx, idx, idx_size*sizeof(unsigned long));
  new_node->next = NULL;


  //Insertion sort!
  if(queue == NULL || queue->perf > new_node->perf){
    new_node->next = queue;
    queue = new_node;
    return queue;
  }else{
    pqueue_node_t *prev = queue;
    pqueue_node_t *iter = queue->next;
    while(iter != NULL && iter->perf < new_node->perf){
      prev = iter;
      iter = iter->next;
    }
    new_node->next = iter;
    prev->next = new_node;
  }
  return queue;
}

queue_node_t *queue_pop(queue_node_t *queue){
  if(queue != NULL){
    hpoint_t item = queue->point;
    queue_node_t *tmp = queue;
    queue = queue->next;
    free(tmp->idx);
    hpoint_fini(&tmp->point);
    free(tmp);
  }
  return queue;
}

queue_node_t *queue_push_back(queue_node_t *queue, hpoint_t* point, unsigned long *idx, int idx_size){
  queue_node_t *new_node = (queue_node_t *)malloc(sizeof(queue_node_t));
  new_node->point = HPOINT_INITIALIZER;
  if (new_node == NULL){
    exit(-1);
  }
  hpoint_copy(&new_node->point, point);
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
      return 1;
    }else{
      iter = iter->next;
    }
  }
  return 0;
}