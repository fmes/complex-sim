/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "complex_sim.h"

#include "cs_queue.h"
#include "cs_psk.h"

int cs_queue_close(cs_queue * q)
{

    if (_CS_QUEUE_LOCK(q) != 0){
	fprintf(stderr,
                "close_queue(): error while locking on the queue %s..\n",
                q->name);
	return -1;
	}

    q->status = QCLOSED;
    /*must increase the semaphore in order
    /to notify the consumer of the queue*/
    if (sem_post(q->qsem) != 0)
	fprintf(stderr,
		"close_queue(): error while increasing queue semaphore for queue %s..\n",
		q->name);

    if (_CS_QUEUE_UNLOCK(q) != 0)
	return -1;

    return 0;
}

int cs_queue_init(cs_queue *q, const char *name, void (*memfree_func) (void *), unsigned int size)
{
    if(q==NULL){
      log4c_category_log(log4c_category_get("cs.queue"), LOG4C_PRIORITY_FATAL, "Unable to init the queue, passed null pointer");
      return -1;
    }

    q->size = 0;
    q->name = (char *) malloc(sizeof(char) * strlen(name));
    strncpy(q->name, name, strlen(name));
    q->head = q->tail = NULL;
    q->status = QOPENED;
    q->rel_func = memfree_func;

    q->qmutex = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    if (q->qmutex == NULL) {
	log4c_category_log(log4c_category_get("cs.queue"), LOG4C_PRIORITY_FATAL, "Unable to alloc mutex for the queue\n");
	cs_queue_destroy(q);
	return -1;
    }
    pthread_mutex_init(q->qmutex, NULL);

    q->qsem = (sem_t *) calloc(1, sizeof(sem_t));
    q->qlimit_sem = (sem_t *) calloc(1, sizeof(sem_t));
    if (q->qsem == NULL || sem_init(q->qsem, 0, 0) != 0
	|| q->qlimit_sem == NULL
	|| sem_init(q->qlimit_sem, 0, size) != 0) {
	fprintf(stderr, "Error allocating initiating queue semaphores\n");
	cs_queue_destroy(q);
	return -1;
    }
    return 0;
}

int cs_queue_size(cs_queue * q)
{
    int ret;
    pthread_mutex_lock(q->qmutex);
    ret = q->size;
    pthread_mutex_unlock(q->qmutex);
    return ret;
}

void cs_queue_destroy(cs_queue * q)
{
    free(q->name);
    if (q != NULL) {
	if (q->qsem != NULL){
	    sem_destroy(q->qsem);
	    free(q->qsem);
	}
	if (q->qlimit_sem != NULL){
	    sem_destroy(q->qlimit_sem);
	    free(q->qlimit_sem);
	}
	if (q->qmutex != NULL){	     
	    pthread_mutex_destroy(q->qmutex);
	    free(q->qmutex);
	}

	cs_qelem *current = q->tail;
	//qelem *prev; 
	while (current != NULL) {
	    q->rel_func(current->data);
	    //  prev = current;
	    current = current->next;
	    free(current);
	}
    }
}

int cs_queue_enqueue(cs_queue * q, void *data, short int blocking)
{
    //int prev_size;
    cs_qelem *elem;

    if (blocking) {
	if (sem_wait(q->qlimit_sem) != 0) {	//wait in the semaphore
	    fprintf(stderr,
		    "enqueue(): Error waiting in the \"limit\" semaphore\n");
	    return -1;
	}
    } else if (sem_trywait(q->qlimit_sem) != 0)	//queue full
	return QUEUE_FULL;

    //there is space enough, 
    //let's lock the queue

    //begin of critical section
    if (_CS_QUEUE_LOCK(q) != 0)
	return -1;

    //prev_size = q->size;

    if (q->status == QOPENED) {

	//make the new queue element
	elem = (cs_qelem *) calloc(1, sizeof(cs_qelem));

	if (elem == NULL) {	//error allocating memory
	    fprintf(stderr,
		    "Error while allocating memory for the new queue element\n");

	    if (_CS_QUEUE_UNLOCK(q) != 0) {	//error unlocking the queue
		fprintf(stderr, "Error while unlock queue\n");
		return -1;
	    }
	}
	//copy data pointed by data into elem
	elem->data = data;

	elem->next = q->tail;	//1
	elem->prev = NULL;	//3

	if (q->tail != NULL)	//2 queue wasn't empty
	    q->tail->prev = elem;

	else			//queue was empty
	    q->head = q->tail = elem;

	//update the tail of the queue
	q->tail = elem;		//4

	q->size++;

	if (sem_post(q->qsem) != 0) {	//post the semaphore  
	    fprintf(stderr,
		    "enqueue(): Error while increasing semaphore\n");
	    if (_CS_QUEUE_UNLOCK(q) != 0)
		fprintf(stderr,
			"enqueue(): Error while unlocking queue\n");
	    return -1;
	}
	//end of critical section
	if (_CS_QUEUE_UNLOCK(q) != 0) {
	    fprintf(stderr, "enqueue(): Error while unlocking queue\n");
	    return -1;
	}
	return 0;
    }

    else {
	fprintf(stderr,
		"enqueue(): WARN: trying to enqueue in a closed queue, nothing to do..\n");
	//unlock previously locked mutex
	if (_CS_QUEUE_UNLOCK(q) != 0) {
	    fprintf(stderr, "enqueue(): Error while unlocking queue\n");
	    return -1;
	}
	return 0;
    }
}

void *cs_queue_dequeue(cs_queue * q, short int blocking)
{
    void *dataret;
    cs_qelem *elem;
    int old_size;
    int assctl;

    if (blocking) {
	if (sem_wait(q->qsem) != 0) {	//SEM_WAIT
	    fprintf(stderr,
		    "task_dequeue(): Error while decreasing semaphore of task queue\n");
	    return NULL;
	}
    } else if (sem_trywait(q->qsem) != 0)	//queue empty
	return NULL;


    if (_CS_QUEUE_LOCK(q) != 0)	//LOCK
	return NULL;

    //begin of critical section
    if (q->head == NULL && q->status == QCLOSED) {
	if (_CS_QUEUE_UNLOCK(q) != 0)	//UNLOCK
	    fprintf(stderr, "There has been an error unlocking the queue");
	return NULL;
    }

    assert(q->head != NULL);

    old_size = q->size;

    //store the pointer to the data
    dataret = (void *) q->head->data;
    elem = q->head;

    //update the "next" pointer of the prev element
    if (old_size > 1) {
	q->head->prev->next = NULL;
	//update the head of the queue
	q->head = q->head->prev;
    }

    else			//now the queue is empty
	q->tail = q->head = NULL;

    q->size--;
    //thereafter, q->size == sem_getvalue()
    sem_getvalue(q->qsem, &assctl);
    assert(q->size == assctl || q->status==QCLOSED);

    if (sem_post(q->qlimit_sem) != 0) {	//post the semaphore      
	fprintf(stderr,
		"enqueue(): Error while increasing \"limit\" semaphore\n");
	if (_CS_QUEUE_UNLOCK(q) != 0)
	    fprintf(stderr, "enqueue(): Error unlocking queue\n");
	return NULL;
    }

    if (_CS_QUEUE_UNLOCK(q) != 0) {	//UNLOCK
	fprintf(stderr, "There has been an error unlocking the queue");
	return NULL;
    }

    free(elem);
    return dataret;
}
