//
// Implementation of "prioque.h" priority queue functions (c)
// 1985-2023 by Golden G. Richard III, Ph.D. (@nolaforensix)
//
// See prioque.h for update log.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "prioque.h"

#define QUEUE_MAGIC 0xC0FFEEC0FFEE

// global lock on entire package
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

// for init purposes
pthread_mutex_t initial_mutex = PTHREAD_MUTEX_INITIALIZER;

// function prototypes for internal functions


void init_queue(Queue *q, unsigned int elementsize, unsigned int duplicates,
	   int (*compare) (const void *e1, const void *e2), unsigned int priority_is_tag_only) {

  q->magic = QUEUE_MAGIC;
  q->queuelength = 0;
  q->elementsize = elementsize;
  q->queue = NULL;
  q->tail = NULL;
  q->duplicates = duplicates;
  q->compare = compare;
  q->priority_is_tag_only = priority_is_tag_only;
  nolock_rewind_queue(q);
  q->lock = initial_mutex;
}


int queue_initialized(Queue q) {

  return q.magic == QUEUE_MAGIC;

}


void destroy_queue(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c destroy_queue() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  nolock_destroy_queue(q);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
}


void nolock_destroy_queue(Queue *q) {

  Queue_element temp;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_destroy_queue() **\n");
    exit(1);
  }

  if (q != NULL) {
    while (q->queue != NULL) {
      free(q->queue->info);
      temp = q->queue;
      q->queue = q->queue->next;
      free(temp);
      (q->queuelength)--;
    }
  }

  nolock_rewind_queue(q);
}


unsigned int nolock_element_in_queue(Queue *q, void *element) {

  unsigned int found = FALSE;
  
  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_element_in_queue() **\n");
    exit(1);
  }
  
  if (q->queue != NULL) {
    nolock_rewind_queue(q);
    while (! nolock_end_of_queue(q) && ! found) {
      if (q->compare(element, q->current->info) == 0) {
	found = TRUE;
      }
      else {
	nolock_next_element(q);
      }
    }
  }
  if (! found) {
    nolock_rewind_queue(q);
  }

  return found;
}


unsigned int element_in_queue(Queue *q, void *element) {

  unsigned int found;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c element_in_queue() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  found = nolock_element_in_queue(q, element);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

  return found;
}


int nolock_delete_from_queue(Queue *q, void *element) {
  
  unsigned int found;
  
  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_delete_from_queue() **\n");
    exit(1);
  }
  
  found = nolock_element_in_queue(q, element);
  if (found) {
    nolock_delete_current(q);
  }

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

  return found;
}


int delete_from_queue(Queue *q, void *element) {

  unsigned int found;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c delete_from_queue() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  found = nolock_delete_from_queue(q, element);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

  return found;

}


void add_to_queue(Queue *q, void *element, int priority) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c add_to_queue() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  nolock_add_to_queue(q, element, priority);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

}


void nolock_add_to_queue(Queue *q, void *element, int priority) {

  Queue_element new_element, ptr, prev = NULL;
  
  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_add_to_queue() **\n");
    exit(1);
  }

  if (! q->duplicates && ! q->compare) {
    fprintf(stderr, "prioque.c: If duplicates are disallowed, the comparison function must be\nspecified in init_queue().\n");
    exit(1);
  }
  
  if (! q->queue ||
     (q->queue && (q->duplicates || ! nolock_element_in_queue(q, element)))) {

    new_element = (Queue_element) malloc(sizeof(struct _Queue_element));
    if (new_element == NULL) {
      fprintf(stderr, "malloc() failed in function add_to_queue()\n");
      exit(1);
    }
    new_element->info = (void *)malloc(q->elementsize);
    if (new_element->info == NULL) {
      fprintf(stderr, "malloc() failed in function add_to_queue()\n");
      exit(1);
    }

    memcpy(new_element->info, element, q->elementsize);
    new_element->priority = priority;
    (q->queuelength)++;

    if (q->queue == NULL) {             // first element            
      new_element->next = NULL;
      q->queue = new_element;
      q->tail = new_element;
    }
    else if (q->priority_is_tag_only) { // FIFO queue
      new_element->next = NULL;
      q->tail->next = new_element;  
      q->tail = new_element;
    }
    else {                              // priority queue
      ptr = q->queue;
      while (ptr != NULL && priority <= ptr->priority) {
	prev = ptr;
	ptr = ptr->next;
      }

      if (! prev) {   // queue had only one element and new element
		      // has higher priority
	new_element->next = q->queue;
	q->tail = q->queue;
	q->queue = new_element;
      }
      else {         // insert new element 
	new_element->next = prev->next;
	prev->next = new_element;
	if (new_element->next == NULL) {
	  // new tail
	  q->tail = new_element;
	}
      }
    }
    
    nolock_rewind_queue(q);

    //        printf("---------------\n");    
    //        printf("Queue %p now contains:\n", q);
    //        ptr=q->queue;
    //        while (ptr) {
    //          printf("-->%p\n", *((unsigned long *)ptr->info));
    //          ptr=ptr->next;
    //        }
    //	      printf("---------------\n");
	
  }
}


unsigned int empty_queue(Queue *q) {

  unsigned int ret;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c empty_queue() **\n");
    exit(1);
  }
  
  pthread_mutex_lock(&(q->lock));
  
  ret = nolock_empty_queue(q);

  pthread_mutex_unlock(&(q->lock));

  return ret;
}


int nolock_empty_queue(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_empty_queue() **\n");
    exit(1);
  }
  
  return (q->queue == NULL);

}


void *remove_from_front(Queue *q, void *element) {

  void *ret = NULL;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c remove_from_front() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  ret = nolock_nosync_remove_from_front(q, element);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

  return ret;
}


void *remove_from_front_sync(Queue *q, void *element, 
			     atomic_uint *j, int change) {

  void *ret = NULL;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c remove_from_front_sync() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));
  
  ret = nolock_nosync_remove_from_front(q, element);
  
  if (ret) {
    if (change >= 0) {
      atomic_fetch_add(j, change);
    }
    else {
      atomic_fetch_sub(j, change * -1);
    }
  }
  
  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
  
  return ret;
}


void *nolock_nosync_remove_from_front(Queue *q, void *element) {

  Queue_element temp;
  void *ret = NULL;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_nosync_remove_from_front() **\n");
    exit(1);
  }
  
  if (q->queue) {
    memcpy(element, q->queue->info, q->elementsize);
    ret = element;
    free(q->queue->info);
    temp = q->queue;
    q->queue = q->queue->next;
    free(temp);
    (q->queuelength)--;
    if (q->queue == NULL || q->queue->next == NULL) {
      // new tail
      q->tail = q->queue;
    }
  }

  nolock_rewind_queue(q);

  //  if (ret) {
  //    printf("REMOVED %p from queue %p\n", *((unsigned long *)ret), q);
  //    
  //    printf("Queue %p now contains:\n", q);
  //    Queue_element ptr=q->queue;
  //    while (ptr) {
  //      printf("-->%p\n", *((unsigned long *)ptr->info));
  //      ptr = ptr->next;
  //    }
  //    printf("---------------\n");
  //  }
  
  return ret;
}


void *peek_at_current(Queue *q, void *element, int *priority) {

  void *ret = NULL;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c peek_at_current() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  ret = nolock_peek_at_current(q, element, priority);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

  return ret;
}


void *nolock_peek_at_current(Queue *q, void *element, int *priority) {

  void *ret = NULL;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_peek_at_current() **\n");
    exit(1);
  }
  
  if (q->queue && q->current) {
    memcpy(element, (q->current)->info, q->elementsize);
    ret = element;
    if (priority) {
      *priority = (q->current)->priority;
    }
  }
  
  return ret;
}


void *pointer_to_current(Queue *q) {

  void *data = NULL;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c pointer_to_current() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  data = nolock_pointer_to_current(q);
  
  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
  
  return data;
}


void *nolock_pointer_to_current(Queue *q) {

  void *data = NULL;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_pointer_to_current() **\n");
    exit(1);
  }
  
  if (q->queue && q->current) {
    data = (q->current)->info;
  }
  
  return data;
}


int current_priority(Queue *q) {
  
  int priority;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c current_priority() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));
  
  priority = nolock_current_priority(q);
  
  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
  
  return priority;
}


int nolock_current_priority(Queue *q) {

  int priority;
  
  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_current_priority() **\n");
    exit(1);
  }
  
#if defined(CONSISTENCY_CHECKING)
  if (q->queue == NULL || q->current == NULL) {
    fprintf(stderr, "NULL pointer in function current_priority()\n");
    exit(1);
  }
  else
#endif
  {

    priority = (q->current)->priority;
    
    return priority;
  }
}

 
 void nolock_update_current(Queue *q, void *element) {
   
   if (q->magic != QUEUE_MAGIC) {
     fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_update_current() **\n");
     exit(1);
   }
   
#if defined(CONSISTENCY_CHECKING)
   if (q->queue == NULL || q->current == NULL) {
    fprintf(stderr, "NULL pointer in function update_current()\n");
    exit(1);
   }
   else
#endif
     {
       memcpy(q->current->info, element, q->elementsize);
     }
 }
 
 
void update_current(Queue *q, void *element) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c update_current() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  nolock_update_current(q, element);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
}


void delete_current(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c delete_current() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  nolock_delete_current(q);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

}


void nolock_delete_current(Queue *q) {

  Queue_element temp;
  
  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_delete_current() **\n");
    exit(1);
  }

  #if defined(CONSISTENCY_CHECKING)
  if (q->queue == NULL || q->current == NULL) {
    fprintf(stderr, "NULL pointer in prioque function delete_current()\n");
    exit(1);
  }
  else
#endif
  {

    free(q->current->info);
    temp = q->current;

    if (q->previous == NULL) {	// deletion at beginning
      q->queue = q->queue->next;
      q->current = q->queue;
      if (q->queue == NULL || q->queue->next == NULL) {
	// new tail
	q->tail = q->queue;
      }
    }
    else {			// internal deletion 
      q->previous->next = q->current->next;
      q->current = q->previous->next;
      if (q->current == NULL || q->current->next == NULL) {
	// new tail
	q->tail = q->current;
      }
    }

    free(temp);
    (q->queuelength)--;

  }
}


unsigned int end_of_queue(Queue *q) {

  unsigned int ret;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c end_of_queue() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  ret = nolock_end_of_queue(q);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

  return ret;
}

unsigned int nolock_end_of_queue(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c delete_current() **\n");
    exit(1);
  }
  
  return (q->current == NULL);
}


void next_element(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c next_element() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  nolock_next_element(q);

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
}


void nolock_next_element(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_next_element() **\n");
    exit(1);
  }
  
#if defined(CONSISTENCY_CHECKING)
  if (q->queue == NULL) {
    fprintf(stderr, "NULL pointer in function next_element()\n");
    exit(1);
  }
  else if (q->current == NULL) {
    fprintf(stderr,
	    "Advance past end--NULL pointer in function next_element()\n");
    exit(1);
  }
  else
#endif
    {
      q->previous = q->current;
      q->current = q->current->next;
    }
}
 
 

void rewind_queue(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c rewind_queue() **\n");
    exit(1);
  }

  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  nolock_rewind_queue(q);
  
  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
}


void nolock_rewind_queue(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_rewind_queue() **\n");
    exit(1);
  }

  q->current = q->queue;
  q->previous = NULL;

}


unsigned long queue_length(Queue *q) {

  unsigned long ret;
  
  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c queue_length() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));
  
  ret = nolock_queue_length(q);
  
  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
  
  return ret;
}


unsigned long nolock_queue_length(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c nolock_queue_length() **\n");
    exit(1);
  }
  
  return q->queuelength;

}


void copy_queue(Queue *q1, Queue *q2) {

  if (q1->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** FIRST QUEUE NOT INITIALIZED in prioque.c copy_queue() **\n");
    exit(1);
  }

  if (q2->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** SECOND QUEUE NOT INITIALIZED in prioque.c copy_queue() **\n");
    exit(1);
  }

  // to avoid deadlock, this function acquires a global package
  // lock!
  pthread_mutex_lock(&global_lock);

  // lock entire queues q1, q2
  pthread_mutex_lock(&(q1->lock));
  pthread_mutex_lock(&(q2->lock));

  // free elements in q1 before copy 

  nolock_destroy_queue(q1);

  // now make q1 a clone of q2 

  q1->queuelength = 0;
  q1->elementsize = q2->elementsize;
  q1->queue = NULL;
  q1->tail = NULL;
  q1->duplicates = q2->duplicates;
  q1->priority_is_tag_only = q2->priority_is_tag_only;
  q1->compare = q2->compare;

  nolock_rewind_queue(q2);
  while (! nolock_end_of_queue(q2)) {
    nolock_add_to_queue(q1, nolock_pointer_to_current(q2), nolock_current_priority(q2));
    nolock_next_element(q2);
  }
     

  nolock_rewind_queue(q1);
  nolock_rewind_queue(q2);
  
  // release locks on q1, q2
  pthread_mutex_unlock(&(q2->lock));
  pthread_mutex_unlock(&(q1->lock));

  // release global package lock
  pthread_mutex_unlock(&global_lock);
  
}


unsigned int equal_queues(Queue *q1, Queue *q2) {

  Queue_element temp1, temp2;
  unsigned int same = TRUE;

  if (q1->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** FIRST QUEUE NOT INITIALIZED in prioque.c equal_queues() **\n");
    exit(1);
  }

  if (q2->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** SECOND QUEUE NOT INITIALIZED in prioque.c equal_queues() **\n");
    exit(1);
  }

  // to avoid deadlock, this function acquires a global package
  // lock!
  pthread_mutex_lock(&global_lock);

  // lock entire queues q1, q2
  pthread_mutex_lock(&(q1->lock));
  pthread_mutex_lock(&(q2->lock));

  if (q1->queuelength != q2->queuelength || q1->elementsize != q2->elementsize) {
    same = FALSE;
  }
  else {
    temp1 = q1->queue;
    temp2 = q2->queue;
    while (same && temp1 != NULL) {
      same = (!memcmp(temp1->info, temp2->info, q1->elementsize) &&
	      temp1->priority == temp2->priority);
      temp1 = temp1->next;
      temp2 = temp2->next;
    }
  }

  // release locks on q1, q2
  pthread_mutex_unlock(&(q2->lock));
  pthread_mutex_unlock(&(q1->lock));

  // release global package lock
  pthread_mutex_unlock(&global_lock);

  return same;
}


void merge_queues(Queue *q1, Queue *q2) {

  Queue_element temp;
  
  if (q1->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** FIRST QUEUE NOT INITIALIZED in prioque.c merge_queues() **\n");
    exit(1);
  }
  
  if (q2->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** SECOND QUEUE NOT INITIALIZED in prioque.c merge_queues() **\n");
    exit(1);
  }
  
  // to avoid deadlock, this function acquires a global package
  // lock!
  pthread_mutex_lock(&global_lock);

  // lock entire queues q1, q2
  pthread_mutex_lock(&(q1->lock));
  pthread_mutex_lock(&(q2->lock));

  temp = q2->queue;

  while (temp != NULL) {
    nolock_add_to_queue(q1, temp->info, temp->priority);
    temp = temp->next;
  }

  nolock_rewind_queue(q1);

  // release locks on q1, q2
  pthread_mutex_unlock(&(q2->lock));
  pthread_mutex_unlock(&(q1->lock));

  // release global package lock
  pthread_mutex_unlock(&global_lock);

}


unsigned int serialize_queue(Queue *q,
			     int (*serialize_element)(void *element,
						      int *priority,
						      FILE *fp,
						      StateSerialization mode),
			     FILE *fp) {

  unsigned long num_elements;
  unsigned int ret = TRUE;
  int p;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c serialize_queue() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  // write # of elements in queue
  num_elements = nolock_queue_length(q);
  ret = (fwrite(&num_elements, sizeof(unsigned long), 1, fp) == 1);

  if (ret) {
    nolock_rewind_queue(q);
    while (ret && ! nolock_end_of_queue(q)) {
      p = nolock_current_priority(q);
      ret = serialize_element(nolock_pointer_to_current(q),
			      &p, fp, SERIALIZE);
      nolock_next_element(q);
    }
  }

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
  
  return ret;
}


unsigned int deserialize_queue(Queue *q,
			       int (*deserialize_element)(void *element,
							  int *priority,
							  FILE *fp,
							  StateSerialization mode),
			       FILE *fp) {
  
  void *element = NULL;
  int priority;
  unsigned long num_elements;
  unsigned int ret = TRUE;

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c deserialize_queue() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));

  // read # of elements in queue
  ret = (fread(&num_elements, sizeof(unsigned long), 1, fp) == 1);
  
  while (ret && num_elements-- > 0 &&
	 (ret = deserialize_element(&element, &priority, fp, DESERIALIZE) > 0)) {
    nolock_add_to_queue(q, &element, priority);
  }

  // release lock on queue
  pthread_mutex_unlock(&(q->lock));

  return ret;
}


// lock the queue globally, to allow use of nolock_* functions
void lock_queue(Queue *q) {
  
  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c lock_queue() **\n");
    exit(1);
  }
  
  // lock entire queue
  pthread_mutex_lock(&(q->lock));
  
}

// unlock the queue
void unlock_queue(Queue *q) {

  if (q->magic != QUEUE_MAGIC) {
    fprintf(stderr, "** QUEUE NOT INITIALIZED in prioque.c unlock_queue() **\n");
    exit(1);
  }
  
  // release lock on queue
  pthread_mutex_unlock(&(q->lock));
  
}


