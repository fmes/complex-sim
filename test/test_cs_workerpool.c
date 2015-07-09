/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "cs_workerpool.h"

#include <unistd.h>
#include "log4c.h"
 
#define SL 10

cs_wp_tsk_exit_status test_task(cs_data_ptr in, cs_data_ptr *out){
	int *res = (int*) malloc(sizeof(int));
	*res = (int) 2*(*((int*)in));
	log4c_category_log(log4c_category_get("cs.test"),LOG4C_PRIORITY_NOTICE,"test_task()..in: %d ",\
	 *((int *) in));
	fflush(stderr); fflush(stdout);
//	sleep(*((int *) in));
	*out = res;
	return CS_TSK_SUCCESS;
}

int main(int argc, char *argv[]){
	log4c_init();
	log4c_category_t *log = log4c_category_get("cs.test");

	cs_workerpool_t* wp = (cs_workerpool_t *) malloc(sizeof(cs_workerpool_t));
	if(!wp)
	  log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to malloc the worker pool ");

	if(cs_wp_init(10, 10, wp, "TEST-WP")<0 || cs_wp_start(wp)<0)
	    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to init the worker pool ");

	int *s1 = (int *) malloc(sizeof(int));
	*s1 = SL;

	int *s2 = (int *) malloc(sizeof(int));
	*s2 = SL;

	cs_wp_tsk_id myid1;
	cs_wp_tsk_res *res1;

	cs_wp_tsk_id myid2;
	cs_wp_tsk_res *res2;

	if((myid1=cs_wp_tsk_enqueue(test_task, (void*) s1, sizeof(int), wp))<0)
	    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to enqueue task 1");
	else
	    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Task %li enqueued", myid1);

	if((myid2=cs_wp_tsk_enqueue(test_task, (void*) s2, sizeof(int), wp))<0)
	    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to enqueue task 2");
	else
	    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Task %li enqueued", myid2);

	//wait for the end of the task
	cs_wp_tsk_wait(wp, myid1);
	cs_wp_tsk_wait(wp, myid2);

	//gets the result
	assert((res1 = cs_wp_tsk_get_res(wp, myid1))!=NULL);
	assert((res1 = cs_wp_tsk_pop_res(wp, myid1))!=NULL);

	assert((res2 = cs_wp_tsk_get_res(wp, myid2))!=NULL);
	assert((res2 = cs_wp_tsk_pop_res(wp, myid2))!=NULL);

	assert(res1->tsk_exit_code==CS_TSK_SUCCESS);
	log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Result 1: %d ",  *((int*) res1->tsk_res_data));

	assert(res2->tsk_exit_code==CS_TSK_SUCCESS);
	log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Result 2: %d ",  *((int*) res2->tsk_res_data));

	cs_wp_shutdown(wp);
	cs_wp_destroy(wp);

	log4c_fini();

	return 0;
}
