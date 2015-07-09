/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "cs_workerpool.h"

#include <unistd.h>
#include <time.h>
#include "log4c.h"
 
#define SL 10
#define MAX_RAN 40

typedef struct indata_s{
  int index;
  long int niter;
}indata_t;

cs_wp_tsk_exit_status random_accumulator_task(cs_data_ptr in, cs_data_ptr *out){
	indata_t *mydata = (indata_t*) in;
	long int *accum = malloc(sizeof(long int));
	long int nmax = mydata->niter;
	time_t t = time(NULL);
	int i = 0;
	long int comp;
	fprintf(stderr, "\nrandom_accum() - %d", mydata->index);
	for(i=0; i<nmax; i++){
	  comp = rand_r((unsigned int*) &t)%MAX_RAN;
	  *accum+= (i%2 == 0 ? comp : -comp);
	}
	*out = accum;
	return CS_TSK_SUCCESS;
}

int main(int argc, char *argv[]){

	long int ntasks, nw, nmax;
	cs_wp_tsk_id *tsk_ids;
	int c;
	cs_wp_tsk_res *res;
	indata_t *input; 

	if(argc<3){
	  fprintf(stderr, "\n usage: %s <ntasks> <nw> <nmax>\n", argv[0]);
	  return -1;
	}else{
	  ntasks = strtol(argv[1], NULL, 10);
	  assert(ntasks>=0);
	  fprintf(stderr, "\n ntasks: %li", ntasks);
	
	  nw = strtol(argv[2], NULL, 10);
	  assert(nw>=0);
	  fprintf(stderr, "\n nworkers: %li", nw);

	  nmax = strtol(argv[3], NULL, 10);
	  assert(nmax>=0);
	  fprintf(stderr, "\n nmax: %li", nmax);
	}

	log4c_init();
	log4c_category_t *log = log4c_category_get("cs.test.burst");

	cs_workerpool_t* wp = (cs_workerpool_t *) malloc(sizeof(cs_workerpool_t));
	if(!wp)
	  log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to malloc the worker pool ");

	if(cs_wp_init(nw, nw*100, wp)<0 || cs_wp_start(wp)<0)
	    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to init the worker pool ");

	tsk_ids = (cs_wp_tsk_id *) calloc(ntasks,sizeof(cs_wp_tsk_id));
	input = (indata_t*) calloc(ntasks, sizeof(indata_t));
  
	for(c=0; c<ntasks;c++){
	  input[c].index = c;
	  input[c].niter = nmax;
	  if((tsk_ids[c]=cs_wp_tsk_enqueue(random_accumulator_task, (cs_data_ptr *) &input[c], 0, wp))<0)
	    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to enqueue task %d", c);
	  else
	    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Task %li enqueued", c);
	}

	//wait the completion of tasks
	for(c=0; c<ntasks; c++)
	  cs_wp_tsk_wait(wp, tsk_ids[c]);

	//print results
	for(c=0; c<ntasks; c++){
	  assert((res = cs_wp_tsk_pop_res(wp, tsk_ids[c]))!=NULL);
	  log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Result %d: %li ", c, *((long int*) res->tsk_res_data));
	  free(res->tsk_res_data);
	  free(res);
	}

	cs_wp_shutdown(wp);
	cs_wp_destroy(wp);

	log4c_fini();

	return 0;
}
