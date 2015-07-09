/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef WORKERPOOL_H

#define WORKERPOOL_H

#include "complex_sim.h"
#include "cs_psk.h"
#include "cs_queue.h"
#include "rb.h"

#define _LOCK_WP(wp) _LOCK_MUTEX(((cs_workerpool_t *) wp)->wp_mutex)
#define _UNLOCK_WP(wp) _UNLOCK_MUTEX(((cs_workerpool_t *) wp)->wp_mutex)

#define _LOCK_WORKER(wp, index) _LOCK_MUTEX(((cs_workerpool_t *) wp)->workers_data[index].worker_mutex)
#define _UNLOCK_WORKER(wp, index) _UNLOCK_MUTEX(((cs_workerpool_t *) wp)->workers_data[index].worker_mutex)

/** The id of a (enqueued) task */
typedef long int cs_wp_tsk_id;
/** The id of a worker id */
typedef long int cs_wp_wrk_id;
/** The if of a Worker Pool */
typedef long int cs_wp_id;

/**
 * Exit status of the task.
 */
typedef enum cs_wp_tsk_exit_status_s{
	CS_TSK_SUCCESS,
	CS_TSK_ERROR,
}cs_wp_tsk_exit_status;

/*
 * Status of the worker pool.
 */
typedef enum cs_workerpool_status_s{
	CS_WPS_READY,
	CS_WPS_WORKING,
}cs_workerpool_status;

/*
 * Status of workers.
 */
typedef enum cs_worker_status_s{
	CS_WS_WORKING,
	CS_WS_WAITING,
	CS_WS_TSK_ASSIGNED,
	CS_WS_READY
}_cs_worker_status;

/**
 * Task result.
 * The structure representing the tsk results,
 * to be return to the user(code).
 */
typedef struct cs_wp_tsk_res_s{
	cs_wp_tsk_exit_status tsk_exit_code;
	cs_data_ptr tsk_res_data;
}cs_wp_tsk_res;

/*
* Runtime status of a task.
*/
typedef enum cs_wp_tsk_runtime_status{
	CS_TSK_RT_RUNNING,
	CS_TSK_RT_WAITING,
	CS_TSK_RT_DONE,
	CS_TSK_RT_INVALID_TASK
}_cs_wp_tsk_runtime_status;

/*
 * The tsk structure, to be mantained in a (rb-tree) table.
 */
typedef struct cs_wp_tsk_s {
    cs_wp_tsk_exit_status (*tsk_code) (cs_data_ptr in, cs_data_ptr *out);
    cs_wp_tsk_id tsk_id;
    cs_data_ptr in;
    size_t args_length;
    _cs_wp_tsk_runtime_status status;
    cs_wp_tsk_res tsk_res;
    pthread_mutex_t *tsk_mutex;
    sem_t task_sem;
}_cs_wp_tsk;

/**
 * The table (rb-tree) of the task results.
 */
typedef struct tsk_table_s{
	struct rb_table *tsk_res_table;
	pthread_mutex_t *table_mutex;
	cs_queue *tsk_q;
}_cs_wp_tsk_table;

/*
 * Data mantained for any worker.
 */
typedef struct worker_s {
    //synchronizes the worker with the wp-controller
    sem_t *worker_waiting_semaphore;
    //stores the number of free workers, shared between all workers and the pool manager
    sem_t *workerpool_semaphore;
    //mutex shared by the mutex and the wp-controller
    pthread_mutex_t *worker_mutex;
    //tsk
    _cs_wp_tsk *assigned_tsk;
    //the status of the worker
    _cs_worker_status status;
    //the id of the thread/worker itself
    pthread_t *th_id;		// array storing ids of threads (workers)
    _cs_wp_tsk_table *tsk_res_tb;
    int shutdown;
    struct workerpool_s *wp;
}_cs_wp_worker_data;

/*
 * Data mantained for any Worker pool.
 */
typedef struct workerpool_s {
    cs_workerpool_status status;
    int nthreads;
    /** An indentifier for the worker pool */
    int wid;
    char *name;
    pthread_t *wp_controller;
    pthread_mutex_t *wp_mutex;
    sem_t *workerpool_semaphore;
    _cs_wp_worker_data *workers_data;
    _cs_wp_tsk_table *tsk_res_tb;
}cs_workerpool_t;


/*
 * Init function for the worker pool.
 *
 * @return a integer < 0 when something has been wrong, 0 otherwise.
 * @param nthreads. The number of workers.
 * @param wp. The worker pool object to initialize.
 * @param tsk_queue_size the size of the queue for the tasks.
 * 
 * @see cs_queue
 */
int cs_wp_init(int nthreads, int tsk_queue_size, cs_workerpool_t *wp, const char *name);
/**
 * Destroy the workerpool. If the shutdown was not yet performed, 
 * the caller remains blocked waiting for the completion of all tasks;
 * then the memory for this worker pool will be released.
 *
 * @param wp The worker pool object to destroy.
 */
void cs_wp_destroy(cs_workerpool_t * wp);
/**
 * Start the workerpool
 */
int cs_wp_start(cs_workerpool_t * tp);
/**
 * Shutdown the workerpool.
 */
int cs_wp_shutdown(cs_workerpool_t * wp);
/**
 * Enqueue a new tsk. The tsk will be executed as soon as
 * a new worker is available.
 */
cs_wp_tsk_id cs_wp_tsk_enqueue(
		cs_wp_tsk_exit_status (*tsk_func)(cs_data_ptr in, cs_data_ptr *out),
		void *args,
		size_t args_len,
		cs_workerpool_t *tp);
/** 
 * Get the size of thr task wueue.
 * @return the size of the tsk-queue.
 * @param the worker pool
 */
int cs_wp_tsk_queue_size(cs_workerpool_t * wp);
/**
* Wait for a completion of a task. 
*/
void cs_wp_tsk_wait(cs_workerpool_t *wp, cs_wp_tsk_id id);
/**
* Pop a task result from the task result table.  
*/
cs_wp_tsk_res *cs_wp_tsk_pop_res(cs_workerpool_t *wp, cs_wp_tsk_id id);
/**
* Get a task result from the task result table.
*/
cs_wp_tsk_res *cs_wp_tsk_get_res(cs_workerpool_t *wp, cs_wp_tsk_id id);

/**
* Get the status of the worker pool.
* @return the status of the worker pool.
* @param the pointer to the worker pool.
*/
cs_workerpool_status cs_wp_get_wp_status(cs_workerpool_t *wp);

/*
* Set the status of the worker pool.
* @param the pointer to the worker pool.
* @param new_status the status of the workerpool
*/
void _cs_wp_set_status(cs_workerpool_t *wp, cs_workerpool_status new_status);

#endif
