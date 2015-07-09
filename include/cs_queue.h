/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CS_QUEUE_H__
#define CS_QUEUE_H__

#include "cs_psk.h"

#define _CS_QUEUE_LOCK(q) (pthread_mutex_lock(((cs_queue *) q)->qmutex))
#define _CS_QUEUE_UNLOCK(q) (pthread_mutex_unlock(((cs_queue *) q)->qmutex))

/**
* The status of the queue. 
*/
typedef enum cs_queue_status_e{
 QOPENED,
 QCLOSED,
 QUEUE_FULL,
 QUEUE_EMPTY
}cs_queue_status;

/**
* Structure representing the element of the queue. 
*/
typedef struct cs_qelem_s {
    /**
    * User-provide data.
    */
    void *data;
    struct cs_qelem_s *next, *prev;
}cs_qelem;

/**
* The queue object. 
*/
typedef struct queue_s {
    int size;
    char *name;
    short int status;
    cs_qelem *head;
    cs_qelem *tail;
    /**
     * Mutual exclusion (enqueue/dequeue)  
     */
    pthread_mutex_t *qmutex;
    /**
     * The value of the semaphore indicates the number of
     * element in the cs_queue.
     */
    sem_t *qsem;
    sem_t *qlimit_sem;

    /**
    * The memfree user-provided function, 
    * used to release data within the elements of the queue.
    */
    void (*rel_func) (void *);
} cs_queue;

/**
* Enqueue data into the queue. 
* @param blocking indicates whether, when the queue is full, the caller will block waiting the insertion 
*  into the queue.
* @param data data to enqueue
* @param q the pointer to the queue
* @return a value < 0 whenever something was wrong, 0 otherwise. 
**/
int cs_queue_enqueue(cs_queue * q, void *data, short int blocking);

/**
* Dequeue an element from the queue. 
* @param blocking indicates whether the caller must block invoking the 
* function
* @param q the pointer to the queue 
* @return the data of the element of the queue, NULL in the case of error or 
* if the queue is empty. 
*/
void *cs_queue_dequeue(cs_queue * q, short int blocking);
/**
* Getter function for the size of the queue. 
* @param q the pointer to the q
* @return the size of the queue. 
*/
int cs_queue_size(cs_queue * q);
/**
* Initiate a new queue.
* @param size the max number of elements that can be referenced by the queue.
* @param rel_func a user-provided function to release the data within 
* an element of the queue once that has been deuqueued. 
* @param name the name of the queue
* @param q the pointer to the queue
* @return a value < 0 whenever something was wrong, 0 otherwise. 
*/
int cs_queue_init(cs_queue *q, const char *name, void (*rel_func) (void *), unsigned int size);
/**
* Close the queue, which thereafter will not accept any more element.
* @return a value < 0 whenever something was wrong, 0 otherwise. 
* @param q the pointer to the queue
*/
int cs_queue_close(cs_queue * q);

/**
* Release the memory allocated for the queue and all its 
* elements. @warning: all the remaining elements will be discarded, 
* and the function rel_func will be called on them.
* @param q the pointer to the queue
*/
void cs_queue_destroy(cs_queue * q);

#endif
