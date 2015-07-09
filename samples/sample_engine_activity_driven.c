/* Copyright (c) 2012, Fabrizio Messina, University of Catania

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
 
#include "complex_sim.h"
#include "cs_psk.h"
#include "cs_engine.h"
#include "cs_workerpool.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CHECK_BOUNDARY(x,b) (x<b ? 0 : 1)
#define MAP(vec,i,x) ((vec)[(i)]+=x)

/*
* User data. Its composition is not known to the user. 
*/
typedef struct user_data_s{
  const char *pathname; 
  int boundary;
  int **map_results;
  int *data; 
  int nelem;
  int ntasks;
}user_data_t;

/*
* The code of the task.
*/
cs_wp_tsk_exit_status data_processing_function(
  cs_data_ptr in,
  cs_data_ptr *out){

  int i;
  int fd;
  int *buf;
  int rd,ret;
  int tot2rd;
  int result[2] = {0,0};
  int myindex;

  /* In the case the task is an activity defined by the bind_sim_activity() function,
   * a casting to cs_tsk_data_t can/must be performed
   */
  cs_tsk_data_t *data_in = (cs_tsk_data_t *) in;
  /*
  * By the macro TSK_ENGINE the reference to the engine can be retrieved.   
  * Macros ENG_* etc allow the programmer to retrieve 
  * pointer to others simulation modules, e.g. event manager and timer.
  */
  cs_timer_t *timer = ENG_TIMER(TSK_ENGINE(data_in));

  /*
  * The index of the task which corresponds to the activity
  */
  myindex = TSK_INDEX(data_in);
  
  log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_DEBUG, "(t=%li) - data_processing_function,\
     (tsk)index = %d, data: [%d,%d]", cs_get_clock(timer), TSK_INDEX(data_in), TSK_DATA_INDEX(data_in), TSK_DATA_INDEX(data_in)+TSK_DATA_OFFSET(data_in));

  //open the file for processing, the type-of TSK_DATA
  if( (fd=open(((user_data_t *) TSK_DATA(data_in))->pathname, O_RDONLY))<0){
    perror("While opening file for reading");
    return CS_TSK_ERROR;
  }

  //rewind to the beginning of the data that must be processed
  if(lseek(fd, TSK_DATA_INDEX(data_in)*(sizeof(int)), SEEK_SET)<0){
    perror("lseek() on file ");
    close(fd);
    return CS_TSK_ERROR;
  }

  //allocate buffer in order to read data to be processed 
  buf = (int *) malloc(sizeof(int)*(TSK_DATA_OFFSET(data_in)+1));

  //read integers from files
  rd = 0;
  tot2rd = (data_in->offset+1)*sizeof(int);
  while(1){
    if((ret=read(fd, &(buf[rd]), tot2rd-rd))<0){
      perror("read() ");
      close(fd);
      return CS_TSK_ERROR;
    }

    rd+=ret;
    if(rd>=tot2rd)
      break;
  }

  //process data
  for(i=0; i<TSK_DATA_OFFSET(data_in)+1; i++)
    MAP(result, CHECK_BOUNDARY(buf[i], ((user_data_t *) TSK_DATA(data_in))->boundary), 1);

  //The task/worker gets access to its own piece of data like a simd program
  ((user_data_t *) TSK_DATA(data_in))->map_results[TSK_INDEX(data_in)][0] = result[0];
  ((user_data_t *) TSK_DATA(data_in))->map_results[TSK_INDEX(data_in)][1] = result[1];

  log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_DEBUG, "(t=%li) - data_processing_function,\
     (tsk)index = %d, data: [%d,%d], res: [%d,%d]", cs_get_clock(timer), TSK_INDEX(data_in), TSK_DATA_INDEX(data_in),
	 TSK_DATA_INDEX(data_in)+TSK_DATA_OFFSET(data_in), ((user_data_t *) TSK_DATA(data_in))->map_results[TSK_INDEX(data_in)][0],
		((user_data_t *) TSK_DATA(data_in))->map_results[TSK_INDEX(data_in)][1]);

  return CS_TSK_SUCCESS;
}

/**
* Final sum of the results.
*/
int *reduce(user_data_t *data){
  int i;
  int *res = calloc(2,sizeof(int));
  for(i=0; i<data->ntasks; i++){
    res[0]+=data->map_results[i][0];
    res[1]+=data->map_results[i][1];
  }

  return res;
}

/*
* A termination function must be defined in the case
* the activity scan simulation model is used. 
* The function takes a set of parameters as argument.
*/
int term_function(cs_data_ptr args){
  int tot = 0; int i;
  for(i=0;i<((user_data_t *)args)->ntasks; i++)
    tot+= ((user_data_t *) args)->map_results[i][0] + ((user_data_t *) args)->map_results[i][1];
  return (tot>=((user_data_t *) args)->nelem);
}

int main(int argc, char *argv[]){

  log4c_init();

  cs_engine_t *engine;
  sim_type_t st = ACTIVITY_SCAN;
  //cs_event_manager_t *evm;
  user_data_t *udata;
  cs_timer_t *clock;
  int *res;
  int ntasks;
  int i;

  if(argc!=5){
    fprintf(stderr, "\n\n Usage: %s <pathname> <nelem> <ntasks> <boundary>\n\n", argv[0]);
    return -1;
  }

  udata = (user_data_t *) calloc(1,sizeof(user_data_t)*2);
  clock = (cs_timer_t*) calloc(1, sizeof(cs_timer_t));
  //init the timer
  cs_init_timer(clock, "CLOCK_TEST");
  //set user data
  udata->pathname = argv[1];
  udata->nelem = atoi(argv[2]);
  udata->ntasks = atoi(argv[3]);
  udata->boundary = atoi(argv[4]);
  udata->map_results = (int **) calloc(udata->ntasks,sizeof(int *));
  for(i=0; i<udata->ntasks; i++)
    udata->map_results[i] = (int *) calloc(2,sizeof(int));
  
  //evm = (cs_event_manager_t *) calloc(1, cs_event_manager_t);

  /*if(cs_init_event_manager(evm, 4, clock, st)!=0){
    log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "While init the event manager");
    return -1;
  }*/

  /* Engine */
  engine = (cs_engine_t *) calloc(1,sizeof(cs_engine_t));

  if(cs_init_engine(engine, udata->ntasks /* nworkers */)!=0){
    log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "While init the engine");
    return -1;
  }

  //set the simulation type, termination function and timer
  cs_set_sim_type(engine, st);
  cs_set_termination_function(engine, term_function, (cs_data_ptr) udata);
  //cs_set_event_manager(engine, &evm);
  cs_set_timer(engine, clock);

  /* Bind/define the processing activity */
  cs_bind_sim_activity(
    data_processing_function, //the task/function to be performed
    engine, //the engine
    (cs_data_ptr) udata, //user's data
    (size_t) sizeof(udata), //size of data
    udata->nelem, //number of elements within data
    1, //number of steps between the last execution of the activity and the next one
    USER, "USER_FUNC"); //type of activity

  cs_sim_start(engine);
  res = reduce(udata);

  log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_INFO, "**Results. File %s composition: %d elem <%d; %d elem >=%d)",
    udata->pathname, res[0], udata->boundary, res[1], udata->boundary);

  fflush(stdout);
  fflush(stderr);

  cs_engine_fini(engine);
}
