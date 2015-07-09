/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "cs_workerpool.h"
#include "cs_concurrence.h"
 
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

const char *status_str(cs_workerpool_status st){
  switch(st){
    case CS_WPS_READY:
      return "CS_WPS_READY";    
    case CS_WPS_WORKING:
      return "CS_WPS_WORKING";
  }
  return NULL;
}

const char *w_status_str(_cs_worker_status st){
  switch(st){
    case CS_WS_READY:
      return "CS_WS_READY";
    case CS_WS_WAITING:
      return "CS_WS_WAITING";
    case CS_WS_WORKING:
      return "CS_WS_WORKING";
    case CS_WS_TSK_ASSIGNED:
      return "CS_WS_TSK_ASSIGNED";
  }

  return NULL;
}

cs_workerpool_status cs_wp_get_status(cs_workerpool_t *wp){
  cs_workerpool_status st;
  _LOCK_MUTEX(wp->wp_mutex);
  st = wp->status;
  _UNLOCK_MUTEX(wp->wp_mutex);
  return st;
}

/* 
* Internal function.
*/
void _cs_wp_set_status(cs_workerpool_t *wp, cs_workerpool_status new_status){
  _LOCK_MUTEX(wp->wp_mutex);
  wp->status = new_status;
  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_TRACE, "Workerpool %li set status to %s", wp->wid, \
    status_str(new_status));
  _UNLOCK_MUTEX(wp->wp_mutex);
}

/*
* Internal function. 
*/
void _cs_wp_set_worker_status(cs_workerpool_t *wp, _cs_worker_status new_status, int windex){
  _LOCK_WORKER(wp, windex);
  wp->workers_data[windex].status = new_status;
  _UNLOCK_WORKER(wp, windex);
}

/*
 * Memfree function, requested by the cs_queue module.
 * Actually it does nothing. 
 */
void _cs_wp_qrelease_func(void *data)
{
  return;
}

/*
* Internal function. 
* It signals the end of a task by increasing 
* the semaphore task_sem.
* It should be called by the worker main 
* function. 
*/
void _cs_wp_signal_task_end(_cs_wp_tsk *tsk){
	if(sem_post(&tsk->task_sem)!=0)
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR, " Worker %u: Unable to increase the semaphore of the task", pthread_self());
}

/*
 * Internal function,
 * used by the wp-controller
 * to assign tsks to each worker.
 */
_cs_wp_tsk *_cs_wp_tsk_dequeue(cs_workerpool_t * wp)
{
	_cs_wp_tsk *tret;
	tret = (_cs_wp_tsk *) cs_queue_dequeue(wp->tsk_res_tb->tsk_q, 1);
	return tret;
}

/*
 * Internal function, used to generate the ids of worker
 * pools. 
 */
cs_wp_wrk_id _cs_wp_new_worker_id()
{
	static cs_wp_wrk_id id = 0;
	return ++id;
}

/*
 * Internal function, used to generate the ids of worker
 * pools.
 */
cs_wp_tsk_id _cs_wp_new_tsk_id()
{
	static cs_wp_tsk_id tsk_id = 0;
	return ++tsk_id;
}


void _cs_wp_tsk_destroy(_cs_wp_tsk *task){
  pthread_mutex_destroy(task->tsk_mutex);
  free(task->tsk_mutex);
  free(task);     
}

/*
 * Internal function, used to generate the ids of worker
 * pools.
 */
cs_wp_id _cs_wp_new_wp_id()
{
	static cs_wp_id wp_id = 0;
	return ++wp_id;
}

/*
 * Internal function.
 * Release the   
 */
void _cs_wp_destroy_worker_data(_cs_wp_worker_data * wd)
{
	sem_destroy(wd->worker_waiting_semaphore);
	pthread_mutex_destroy(wd->worker_mutex);
	free(wd->worker_waiting_semaphore);
	free(wd->worker_mutex);
	free(wd->th_id);
}

/*
 * Internal function.
 * Assigns a tsk to the specified worker.
 **/
int _cs_wp_assign_tsk_to_worker(_cs_wp_tsk * t, int windex, cs_workerpool_t* wp)
{
	assert(t!=NULL);
	assert(windex>=0);
	assert(wp!=NULL);
	if (_LOCK_MUTEX(wp->workers_data[windex].worker_mutex) < 0){
	  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"Unable to lock mutex for worker %d",
				(int) *(wp->workers_data[windex].th_id));
	  exit(-1);
	}

	(wp->workers_data[windex]).assigned_tsk = t;
	(wp->workers_data[windex]).status = CS_WS_TSK_ASSIGNED;

	if (_UNLOCK_MUTEX(wp->workers_data[windex].worker_mutex) < 0) {
	  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"Unable to unlock mutex for worker %d",
				(int) *(wp->workers_data[windex].th_id));
	  exit(-1);
	}

	return 0;
}

/**
 * Internal function, useful to
 * get the status of the worker
 **/
_cs_worker_status _cs_wp_get_worker_status(cs_workerpool_t *wp, int index)
{
	_cs_worker_status status;

	if(_LOCK_MUTEX(wp->workers_data[index].worker_mutex)<0)
		return -1;

	status = wp->workers_data[index].status;

	if(_UNLOCK_MUTEX(wp->workers_data[index].worker_mutex)<0)
		return -1;

	return status;
}

/*
* Simple comparison function.
* It compares two task basing on their ids.  
*/
int _cs_wp_cmp_tsk(const void *t1, const void *t2, void *rb_parama){
	const _cs_wp_tsk *ts1 = t1;
	const _cs_wp_tsk *ts2 = t2;

	log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_TRACE," Comparing task %li and %li.. ", (long int) ts1->tsk_id, (long int) ts2->tsk_id);

	if(ts1->tsk_id>ts2->tsk_id)
		return 1;
	else if(ts1->tsk_id<ts2->tsk_id)
		return -1;
	return 0;
}

/*
* Internal function. Initiates the result/task table. 
*/
int _cs_wp_init_tsk_res_table(_cs_wp_tsk_table *tsk_table, const char *qname, int qsize){
	if(tsk_table==NULL){
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"Passed a null pointer for task table ");
		return -1;
	}

	tsk_table->table_mutex = cs_make_recursive_mutex();
	tsk_table->tsk_res_table = rb_create(_cs_wp_cmp_tsk, NULL, &rb_allocator_default);

	if(!(tsk_table->tsk_q = (cs_queue *) malloc(sizeof(cs_queue)))){
	  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"Unable to allocate memory for the task queue! ");
	  return -1;
	}

	/* Initialize the queue for the tsks */
	if (cs_queue_init(tsk_table->tsk_q, qname, _cs_wp_qrelease_func, qsize) <0){
	  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"workerpool_create(): Unable to initialize the queue of the worker pool");
	  return -1;
	}

	return 0;
}

void cs_wp_destroy(cs_workerpool_t* wp)
{
	cs_wp_shutdown(wp);
	int i = 0;
	free(wp->name);
	free(wp->wp_controller);
	sem_destroy(wp->workerpool_semaphore);
	free(wp->workerpool_semaphore);
	//release data about workers
	for (i = 0; i < wp->nthreads; i++) {
		_cs_wp_destroy_worker_data(&wp->workers_data[i]);
	}
	free(wp->workers_data);
	//release data concerning task table
	pthread_mutex_destroy(wp->tsk_res_tb->table_mutex);
	pthread_mutex_destroy(wp->wp_mutex);
	free(wp->wp_mutex);
	free(wp->tsk_res_tb->table_mutex);
	rb_destroy(wp->tsk_res_tb->tsk_res_table, NULL);
	cs_queue_destroy(wp->tsk_res_tb->tsk_q);
	free(wp->tsk_res_tb->tsk_q);
	free(wp->tsk_res_tb);
	free(wp);
}

/*
 * Internal function. Initiate internal data about a worker (a thread).
 */
int _cs_wp_init_worker(_cs_wp_worker_data * wd, cs_workerpool_t *wp)
{
	wd->worker_waiting_semaphore = (sem_t *) calloc(1, sizeof(sem_t));
	if (wd->worker_waiting_semaphore == NULL
			|| sem_init(wd->worker_waiting_semaphore, 1 /*shared */ ,
					0) != 0) {
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"_cs_wp_init_worker(): Error initiating semaphore of worker");
		return -1;
	}

	wd->worker_mutex = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
	if (wd->worker_mutex == NULL) {
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"_cs_wp_init_worker(): Unable to allocate the worker_mutex");
		return -1;
	}

	pthread_mutex_init(wd->worker_mutex, NULL);
	wd->assigned_tsk = NULL;
	wd->status = CS_WS_READY;
	wd->th_id = (pthread_t *) calloc(1, sizeof(pthread_t));
	wd->shutdown=0;
	wd->wp = wp;
	return 0;
}

/*
 * Internal function.
 * Send a shutdown command of each worker. 
 */
int _cs_wp_shutdown_workers(cs_workerpool_t* wp)
{
	int count = 0;
	int ret_code = 0;
	int w_ret_code = 0;

	//try shutting down the workers..
	for (count = 0; count < (wp->nthreads); count++) {
		//LOCK
		if (_LOCK_MUTEX(wp->workers_data[count].worker_mutex) != 0) {
			ret_code = -1;
			break;
		}
		//set shutdown command in the workers status
		wp->workers_data[count].shutdown = 1;

		//UNLOCK
		if (_UNLOCK_MUTEX(wp->workers_data[count].worker_mutex) != 0) {
			ret_code = -1;
			break;
		}

		if (sem_post(wp->workers_data[count].worker_waiting_semaphore) < 0) {
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"Unable to increase semaphore of worker %d",
					count);
			ret_code = -1;
		}

		/*if (pthread_join(wp->workers_data[count].th_id, &w_ret_code)!=0){
		  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"Unable to join worker %d",
                                        count);
                        ret_code = -1;
		}*/

	}
	return ret_code;
}

/**
 * Internal function.
 * The main-loop of each worker.
 */
void *_cs_wp_worker_behaviour(void *args)
{
	_cs_wp_worker_data *mydata = (_cs_wp_worker_data *) args;
	_cs_wp_tsk *tsk_to_perform;
	cs_wp_tsk_exit_status ex_st;

	log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_TRACE," Worker %u/%s started...", *(mydata->th_id), mydata->wp->name);
	/*
	 * - wait on WP semaphore
	 * - LOCK worker data
	 * - check for shutdown command, if so,
	 * 		unlock worker data and exits
	 * - change status to WORKING
	 * - unlock worker data
	 * - perform the task
	 * - lock worker data
	 * - set status to WAITING and reset the task pointer
	 * - post WP semaphore
	 * - lock the task mutex
	 * - set the task as CS_TSK_RT_DONE and
	 * 		set the exit code in the task results area of the task
	 * - unlock the task data
	 * - signal (by semaphore) that the task has been performed
	 */
	while (1) {
		if (sem_wait(mydata->worker_waiting_semaphore) != 0) {
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"Worker %u, Unable to wait on semaphore",
					(unsigned int) *(mydata->th_id));
			exit(-1);
		}

		assert(mydata->shutdown == 1 || (mydata->status==CS_WS_TSK_ASSIGNED));

		//LOCK ITS OWN DATA (data of this worker)
		if (_LOCK_MUTEX(mydata->worker_mutex) < 0) {
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"Worker %d, Unable to lock shared mutex", (int) *(mydata->th_id));
			exit(-1);
		}

		/* Check whether the shutdown command has been sent by the wp-controller, if there are no tasks assigned, exit */
		if (mydata->shutdown==1 && mydata->assigned_tsk == NULL) {
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_NOTICE,"Worker_activity(): Worker %u / %s, shutting down..", *(mydata->th_id), mydata->wp->name);
			if (_UNLOCK_MUTEX(mydata->worker_mutex) != 0) {
				log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"worker %d, Unable to unlock shared mutex",
						(int) *(mydata->th_id));
				exit(-1);
			}
			break;
		}
		
		assert(mydata->assigned_tsk != NULL);
		assert(mydata->status==CS_WS_TSK_ASSIGNED);

		/* Gets the task to perform */
		tsk_to_perform = mydata->assigned_tsk;

		/* Change its own status*/
		mydata->status = CS_WS_WORKING;

		//UNLOCK DATA
		if (_UNLOCK_MUTEX(mydata->worker_mutex) < 0) {
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"Worker %d, Unable to unlock shared mutex",
					(int) *(mydata->th_id));
			exit(-1);
		}

		//PERFORM THE TASK
		ex_st =  tsk_to_perform->tsk_code(tsk_to_perform->in, &((tsk_to_perform->tsk_res).tsk_res_data));

		//LOCK DATA
		if (_LOCK_MUTEX(mydata->worker_mutex) != 0) {
		  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"worker %d, Unable to lock shared mutex",
		    (int) *(mydata->th_id));
		  exit(-1);
		}

		assert(mydata->status == CS_WS_WORKING);

		/* Set its own state to WAITING */
		mydata->status = CS_WS_WAITING;
		/* Reset the task pointer */
		mydata->assigned_tsk = NULL;

		//increase the global semaphore and UNLOCK IS OWN DATA
		if (sem_post(mydata->workerpool_semaphore) < 0
				|| _UNLOCK_MUTEX(mydata->worker_mutex) != 0) {
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"worker %d, Unable to unlock shared mutex",
					(int) *(mydata->th_id));
			exit(-1);
		}

		pthread_mutex_lock(tsk_to_perform->tsk_mutex); //Get the lock for the task
		tsk_to_perform->status = CS_TSK_RT_DONE; //change the status of the tsk
		tsk_to_perform->tsk_res.tsk_exit_code = ex_st; //set the results for the task
		pthread_mutex_unlock(tsk_to_perform->tsk_mutex); //Release the lock for the task

		/* signal to all the tsk/thread waiting for the completion of the tsk */
		_cs_wp_signal_task_end(tsk_to_perform);
	}

	return NULL;
}

/**
 * Internal function.
 * Create thread and start workers.
 * Starting each worker it's responsability of the pool-manager.
 */
int _cs_wp_start_workers(cs_workerpool_t* wp)
{
	int count;
	for (count = 0; count < wp->nthreads; count++){
	  _cs_wp_set_worker_status(wp, CS_WS_WAITING, count);
	  if (pthread_create(
	    ((wp->workers_data)[count]).th_id,
	    NULL,
	    _cs_wp_worker_behaviour,
	    &(wp->workers_data[count])) != 0){
	      log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"Unable to create worker %d", count);
	      return -1;
	    }
	}
	return 0;
}

/**
 * Internal function.
 * Main-loop of the pool manager.
 */
void *_cs_wp_controller_behaviour(void *args)
{
	cs_workerpool_t*workerpool = (cs_workerpool_t*) args;
	_cs_wp_tsk *task_to_perform;
	int count = 0;
	_cs_worker_status wstatus; 
	int free_worker_index = -1;

	while (1) {
		task_to_perform = _cs_wp_tsk_dequeue(workerpool);

		//if _cs_wp_tsk_dequeue() return NULL, it means
		//that the producer of the tsk want to signal to the consumer
		//that there will be no more tsks (i.e., the producers has invoked
		//close_queue())
		//otherwise, the consumer would remain blocked waiting that at least one tsk
		//is available in the queue

		if (task_to_perform == NULL) {
			log4c_category_log(log4c_category_get("cs.workerpool"),LOG4C_PRIORITY_NOTICE, "Wp-controller/%s: exiting..", workerpool->name);
			break;
		}


		//workerpool_semaphore is initialized to nthreads, if zero (<--> no free workers)
		//then the manager has to wait
		//look for one of the free workers
		if (sem_wait(workerpool->workerpool_semaphore) != 0) {
			perror(strerror(errno));
			exit(-1);
		}

		free_worker_index=-1;
		for (count = 0; count < workerpool->nthreads; count++) {
			wstatus = _cs_wp_get_worker_status(workerpool,count);
			log4c_category_log(log4c_category_get("cs.workerpool"),LOG4C_PRIORITY_TRACE,\
				"analyzing worker %d, status= %s\n", count, w_status_str(wstatus));
			fflush(stderr);
			if (wstatus < 0){
				log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL, "WP-controller: Error getting the status of the worker %d", count);
				exit(-1);
			} else if (wstatus == CS_WS_WAITING){
				log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_TRACE, "WP-controller: selected worker %d, status= %s",\
				  count, w_status_str(wstatus));
				fflush(stderr);
				free_worker_index = count;
				break;
			}
		}
	    
		assert(free_worker_index>=0);

		if (_cs_wp_assign_tsk_to_worker(task_to_perform, free_worker_index, workerpool) < 0) {
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"WP-controller: Unable to assign tsk to the worker %d", free_worker_index);
			exit(-1);
		}

		//notify the selected worker
		if (sem_post(workerpool->workers_data[free_worker_index].worker_waiting_semaphore) < 0) {
			log4c_category_log(log4c_category_get("cs.workerpool"),LOG4C_PRIORITY_ERROR,"WP-controller: unable to notify the worker %d by semaphore",free_worker_index);
			exit(-1);
		}
	}

	if (_cs_wp_shutdown_workers(workerpool) == -1){
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"WP-controller: unable to shutdown the workers");
		exit(-1);
	}

	int *retval[workerpool->nthreads];
	for (count = 0; count < (workerpool->nthreads); count++)
		if (pthread_join (*(workerpool->workers_data[count].th_id),
						(void **) &(retval[count])) != 0)
			log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"WP-controller: unable to join worker [%u]",
					(unsigned int) *(workerpool->workers_data[count]).th_id);
	return NULL;
}

void _cs_wp_tsk_set_status(_cs_wp_tsk *tsk, _cs_wp_tsk_runtime_status new_status){
	if(tsk==NULL)
		return;
	else{
		pthread_mutex_lock(tsk->tsk_mutex);
		tsk->status = new_status;
		pthread_mutex_unlock(tsk->tsk_mutex);
	}
}

_cs_wp_tsk_runtime_status _cs_wp_tsk_get_status(_cs_wp_tsk *tsk){
	_cs_wp_tsk_runtime_status status;
	if(tsk==NULL)
		return CS_TSK_RT_INVALID_TASK;
	else{
		pthread_mutex_lock(tsk->tsk_mutex);
		status = tsk->status;
		pthread_mutex_unlock(tsk->tsk_mutex);
	}
	return status;
}

cs_wp_tsk_res *_cs_wp_get_tsk_res_copy(cs_wp_tsk_res *res){ 
  cs_wp_tsk_res *copy = NULL;
  if(!(copy = (cs_wp_tsk_res *) malloc(sizeof(cs_wp_tsk_res)))){
     log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"cs_wp_get_tsk_res_copy(),\
      unable to alloc result copy for task ");
     return NULL;
  }

  assert(copy!=NULL);

  memcpy(copy, res, sizeof(cs_wp_tsk_res));
  return copy;
}


cs_wp_tsk_res *cs_wp_tsk_get_res(cs_workerpool_t*wp, cs_wp_tsk_id id){
	cs_wp_tsk_res *res = NULL;
	cs_wp_tsk_wait(wp, id);
	_cs_wp_tsk dummy_tsk;
	dummy_tsk.tsk_id = id;
	_cs_wp_tsk *found;

	pthread_mutex_lock(wp->tsk_res_tb->table_mutex);
	found = rb_find(wp->tsk_res_tb->tsk_res_table, &dummy_tsk);

	if(found==NULL){
	    log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"cs_wp_tsk_get_res(), unable to get result for tsk %li.. NOT FOUND", id);
	    pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);
	}
	else if((res=_cs_wp_get_tsk_res_copy(&(found->tsk_res)))==NULL){
	    log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"cs_wp_tsk_get_res(), unable to get result for tsk %li.. memory error", id);
	    pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);
	    return NULL;
	}

	pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);
	return res;
}

cs_wp_tsk_res *cs_wp_tsk_pop_res(cs_workerpool_t*wp, cs_wp_tsk_id id){
	cs_wp_tsk_res *res = NULL;
	cs_wp_tsk_wait(wp, id);
	_cs_wp_tsk dummy_tsk;
	dummy_tsk.tsk_id = id;

	pthread_mutex_lock(wp->tsk_res_tb->table_mutex);
	_cs_wp_tsk *found = rb_delete(wp->tsk_res_tb->tsk_res_table, &dummy_tsk);

	if(found==NULL){
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"cs_wp_tsk_pop_res(), unable to pop the result for tsk %li..NOT FOUND ", id);
		pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);
	}		
		
 	if((res=_cs_wp_get_tsk_res_copy(&found->tsk_res))==NULL){
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"cs_wp_tsk_pop_res(), unable to pop the result for tsk %li.. memory error ", id);
		pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);
		return NULL;
	}

	_cs_wp_tsk_destroy(found);

	pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);
	
	return res;
}

void _cs_wp_wait_task_end(_cs_wp_tsk *tsk){
	if(sem_wait(&tsk->task_sem)!=0)
	  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"_cs_wp_wait_task_end(), unable to wait on the semaphore for the task %u", tsk->tsk_id);
}

void cs_wp_tsk_wait(cs_workerpool_t*wp, cs_wp_tsk_id id){
	_cs_wp_tsk dummy_tsk, *found;
	dummy_tsk.tsk_id = id;
	found = NULL;

	//lock the table
	pthread_mutex_lock(wp->tsk_res_tb->table_mutex);
	found = rb_find(wp->tsk_res_tb->tsk_res_table, &dummy_tsk);
	pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);

	if(found==NULL){
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_WARN,"cs_wp_tsk_wait(), no any task has been found in the task table with "\
				"id %li", id);
		return;
	}

	if(found->status==CS_TSK_RT_DONE)
		return;

	//wait for its end through the semaphore
	_cs_wp_wait_task_end(found);
}

int cs_wp_shutdown(cs_workerpool_t* wp)
{
    _LOCK_MUTEX(wp->wp_mutex);
    if(cs_wp_get_status(wp)!=CS_WPS_WORKING){ //nothing to do
       _UNLOCK_WP(wp);
      return 0;
    }
      
    //close the queue. Thus the pool-manager will
    //find no more tsk, exiting from the main cycle.
    if (cs_queue_close(wp->tsk_res_tb->tsk_q) != 0){
       _UNLOCK_WP(wp);
      return -1;
    }
    int *retval;
    if (pthread_join(*(wp->wp_controller), (void **) &retval) != 0) {
      log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"cs_wp_shutdown(), unable to join on the WP-controller..");
      _UNLOCK_WP(wp);
      return -1;
    } else {
      log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_NOTICE,"%s, cs_wp_shutdown(), done", wp->name);
      free(retval);
    }

    _cs_wp_set_status(wp, CS_WPS_READY);
    _UNLOCK_MUTEX(wp->wp_mutex);
    return 0;
}

int cs_wp_tsk_queue_size(cs_workerpool_t* wp)
{
	return cs_queue_size(wp->tsk_res_tb->tsk_q);
}

int cs_wp_start(cs_workerpool_t* wp)
{
	if(cs_wp_get_status(wp)!=CS_WPS_READY)
		return -1;

	_LOCK_MUTEX(wp->wp_mutex);
	if (_cs_wp_start_workers(wp) != 0){
	  _UNLOCK_MUTEX(wp->wp_mutex);
	  return -1;
	}
	if (pthread_create(wp->wp_controller, NULL, _cs_wp_controller_behaviour, (void *) wp) != 0) {
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"cs_wp_start(),\
unable to initiate the WP-controller");
		_UNLOCK_MUTEX(wp->wp_mutex);
		return -1;
	}
	_cs_wp_set_status(wp, CS_WPS_WORKING);
	_UNLOCK_MUTEX(wp->wp_mutex);
	return 0;
}

cs_wp_tsk_id cs_wp_tsk_enqueue(
  cs_wp_tsk_exit_status (*tsk_code) (cs_data_ptr in, cs_data_ptr *out),
  void *args,
  size_t args_len,
  cs_workerpool_t* wp)
  {
	if(cs_wp_get_status(wp)!=CS_WPS_WORKING){
	  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR," Must start the worker pool before the enqueue of a task");
	  return -1;
	}

	_cs_wp_tsk *tsk = (_cs_wp_tsk *) calloc(1, sizeof(_cs_wp_tsk));
	int old_count;
	tsk->in = (cs_data_ptr) args; //set input data 
	tsk->tsk_code = tsk_code; //set taks_code, i.e. the function pointer
	tsk->args_length = args_len; //set the args len
	tsk->tsk_id = _cs_wp_new_tsk_id(); //set the task id
	tsk->tsk_mutex = cs_make_recursive_mutex(); //set the mutex 
	tsk->status = CS_TSK_RT_WAITING; // set the initial status
	sem_init(&tsk->task_sem, 0, 0); //init the semaphore

	//lock the task table mutex
	pthread_mutex_lock(wp->tsk_res_tb->table_mutex);

	if (cs_queue_enqueue(wp->tsk_res_tb->tsk_q, (void *) tsk, 1) < 0){
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"tsk_enqueue(): Unable to store task %li in queue", tsk->tsk_id);
		pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);
		return -1;
	}

	old_count = wp->tsk_res_tb->tsk_res_table->rb_count;

	/* no duplicate can exist */
	assert(rb_find(wp->tsk_res_tb->tsk_res_table, tsk)==NULL);

	/* insert the task result into the table */
	if(rb_insert(wp->tsk_res_tb->tsk_res_table, tsk)!=NULL)
	  log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR,"tsk_enqueue(): Unable to store task %li in the task table (duplicate element!)", tsk->tsk_id);

	assert(wp->tsk_res_tb->tsk_res_table->rb_count==old_count+1);

	/* The element has been inserted correctly */
	assert(rb_find(wp->tsk_res_tb->tsk_res_table, tsk)!=NULL);

	//unlock the task table mutex
	pthread_mutex_unlock(wp->tsk_res_tb->table_mutex);

	return tsk->tsk_id;
}

int cs_wp_init(int nthreads, int tsk_queue_size, cs_workerpool_t *wp, const char *name)
{
	int count = 0;

	if(wp==NULL){
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_FATAL,"cs_wp_init(), Passed null pointer for worker pool!");
		return -1;
	}

	if (nthreads <= 0) {
		log4c_category_log(log4c_category_get("cs.workerpool"),LOG4C_PRIORITY_ERROR, "cs_wp_init(), the number of workers must be > 0", wp->name);
		return -1;
	}

	wp->nthreads = nthreads;
	wp->wid = _cs_wp_new_wp_id();
	wp->name = (char *) calloc(1,sizeof(char) * (abs((((float) wp->wid) / 10)) + strlen("cs_workerpool") + 4 + (strlen(name))));
	sprintf(wp->name, "cs_workerpool-%d-%s", wp->wid, name);

	/* the wp-controller */
	wp->wp_controller = (pthread_t *) calloc(1, sizeof(pthread_t));

	/* init the semaphore for workers coordination */
	wp->workerpool_semaphore = (sem_t *) calloc(1, sizeof(sem_t));
	if ((wp->workerpool_semaphore) == NULL || sem_init(wp->workerpool_semaphore, /*shared */ 1,
					wp->nthreads) != 0) {
		log4c_category_log(log4c_category_get("cs.workerpool"),LOG4C_PRIORITY_ERROR, "cs_wp_init(), unable to init semaphore for the Worker Pool", wp->name);
		return -1;
	}

	/* allocates workers */
	wp->workers_data = (_cs_wp_worker_data *) calloc(nthreads, sizeof(_cs_wp_worker_data));
	for (count = 0; count < nthreads; count++) {
		_cs_wp_init_worker(&(wp->workers_data)[count], wp);
		(&(wp->workers_data)[count])->workerpool_semaphore =
				wp->workerpool_semaphore;
	}

	/* Initialize the tsk table */
	if(!(wp->tsk_res_tb = (_cs_wp_tsk_table *) malloc(sizeof(_cs_wp_tsk_table))))
		log4c_category_log(log4c_category_get("cs.workerpool"), LOG4C_PRIORITY_ERROR," Unable to malloc tsk results table");
	/* Initialize the task table and the queue */
	_cs_wp_init_tsk_res_table(wp->tsk_res_tb, wp->name, tsk_queue_size);

	/* Initialize the mutex of the worker pool */
	wp->wp_mutex = cs_make_recursive_mutex();

	/* Shares the tsk results table with workers, which will puts the results */
	/* into the table */
	wp->workers_data->tsk_res_tb = wp->tsk_res_tb;

	log4c_category_log(log4c_category_get("cs.workerpool"),LOG4C_PRIORITY_NOTICE, "Worker Pool %s initiated", wp->name);

	_cs_wp_set_status(wp, CS_WPS_READY);
	return 0;
}
