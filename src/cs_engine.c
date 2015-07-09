/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "cs_engine.h"
#include <math.h>

/*typedef struct cs_tsk_data_e{
  cs_data_ptr data;
  int data_index;
  int index;
  int offset;
}cs_tsk_data_t;*/

void insert_into_activity_list(cs_user_activity_t *act, cs_user_activity_list_t *alist){

  assert(alist!=NULL);
  assert(act!=NULL);

  /* insert the activity as the head of the list */
  if(alist->last_act != NULL)
    alist->last_act->next = act; //tail
  
  else
    alist->activities = act; //head
  
  alist->last_act = act; //update the tail
  alist->nact++;
  alist->total_tasks+=act->n_tasks;
}

/*
* Register a simulation activity to be performed
* during the simulation into an activity list.
*/
void _cs_bind_sim_activity(
  cs_wp_tsk_exit_status (*tsk_code) (cs_data_ptr in, cs_data_ptr *out), //function to bind
  cs_engine_t *engine,
  cs_data_ptr data_ptr, //pointer to the data
  size_t data_size, //total data size
  int data_n_items, // number of items
  int nsteps, // time between an execution of the function and the next one
  activity_type_e type,
  const char *aname){

    int i, base_offset;
    cs_user_activity_t *act;
    cs_workerpool_t *wp = engine->wp;
    
    int block_dim = floor(data_n_items/wp->nthreads);
    base_offset = block_dim - 1;
    int n_tasks = (data_n_items > wp->nthreads ? wp->nthreads : data_n_items);
    int res = data_n_items%wp->nthreads;

    cs_tsk_data_t *tsk_data = (cs_tsk_data_t *) calloc(n_tasks, sizeof(cs_tsk_data_t));
    if(!tsk_data){
      log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_ERROR, "Memory error");
      return;
    }

    /* Build the activity structure */
    act = malloc(sizeof(cs_user_activity_t));
    if(!act){
      log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_ERROR, "Memory error");
      return;
    }

    /* Set the task data array */
    for(i=0; i<n_tasks; i++){
      tsk_data[i].data = data_ptr;
      tsk_data[i].offset = (i<wp->nthreads-1 ?  base_offset : base_offset + res);
      tsk_data[i].data_index = i*block_dim;
      tsk_data[i].index = i;
      tsk_data[i].engine = engine;
      tsk_data[i].tot_tasks = n_tasks;		
    }

    act->nsteps = nsteps;
    act->n_tasks = n_tasks;
    act->last_execution_time = -1L;
    act->tsk_data_arr = tsk_data;
    act->next = NULL;
    act->type = type;
    act->total_data_size = data_size;
    act->tsk_code = tsk_code;
    act->name = malloc(sizeof(char)*strlen(aname)+1);
    sprintf(act->name, "%s", aname);

    insert_into_activity_list(act, engine->alist);
}

void cs_bind_sim_activity(
  cs_wp_tsk_exit_status (*tsk_code) (cs_data_ptr in, cs_data_ptr *out), //function to bind
  cs_engine_t *engine, //the engine
  cs_data_ptr data_ptr, //pointer to the data
  size_t data_size, //total data size
  int data_n_items, // number of items
  int nsteps, // time between an execution of the function and the next one
  activity_type_e type,
  const char *aname){

   _cs_bind_sim_activity(
  tsk_code,
  engine,
  data_ptr, //pointer to the data
  data_size, //total data size
  data_n_items, // number of items 
  nsteps, // time between an execution of the function and the next one
  type,
  aname);
  }

int _check_already_running(cs_engine_t *engine){
  return (engine->running==1);
/*  if(engine->running==1){
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_ERROR, "Trying o (re)configure the engine, 
      which is already running, doing nothing");
    return -1;
  }

  return 0;*/
}

void cs_set_timer(cs_engine_t *engine, cs_timer_t *timer){
  if(_check_already_running(engine)){
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_WARN, "Trying to (re)set\
	the timer within the engine, but the engine is already running, doing nothing..");
    return;
  }

  if(timer!=NULL)
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_WARN, "Trying to (re)set the timer within the engine ");
  engine->timer = timer;
}

void cs_set_termination_function(cs_engine_t *engine, int (*func) (cs_data_ptr in), cs_data_ptr args){
  if(_check_already_running(engine))
    return;

  engine->term_func = func;
  engine->term_func_args = args;
}

int _check_termination(cs_engine_t *engine){
  return (engine->st==ACTIVITY_SCAN && (engine->term_func(engine->term_func_args)));
}

void cs_set_event_manager(cs_engine_t *engine, cs_event_manager_t *evm){
  if(_check_already_running(engine)){
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_WARN, "Trying to (re)set\
	the event manager within the engine, but the engine is already running, doing nothing..");
    return;
  }

  assert(evm!=NULL);

  log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_WARN, "Setting event manager..");

  engine->evm = evm;
}

void cs_set_network_runtime(cs_engine_t *engine, cs_network_runtime_t *net_rt){
  if(_check_already_running(engine)){
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_WARN, "Trying to (re)set\
	the network runtime within the engine, but the engine is already running, doing nothing..");
    return;
  }

  assert(net_rt!=NULL);

  log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_WARN, "Setting network runtime..");

  engine->net_rt = net_rt;
}

void _loop_activity(cs_engine_t *engine, activity_type_e atype){

  int ischeduled = 0;
  cs_user_activity_t *act;
  cs_wp_tsk_id *ids;
  int act_processed = 0;
  int i;
  cs_clockv cur_time = cs_get_clock(engine->timer);
  if(cur_time <= 0)
    return;

  if(engine->alist->total_tasks>0){
    act = engine->alist->activities; //the first activity
    ids = malloc(sizeof(cs_wp_tsk_id)*engine->alist->total_tasks);
    for(i=0; i<engine->alist->total_tasks; i++)
      ids[i] = -1;
  }

  else{
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_DEBUG, "No any activity to schedule (t=%li)",
      cs_get_clock(engine->timer));
    return;
  }

  //Schedule the activities into the worker pool
  log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_DEBUG, "Scheduling activities (t=%li, type=%s)",
  	cs_get_clock(engine->timer), activity_to_string(atype));

  do{
    /*schedule activities - TODO this code will be left into a separate
     *function to be used also into network runtime module
     * For all activities into the list:
     * 1. check whether the current activity should be performed
     * 2. Schedule the activity n times (n=#workers)
     */
    if((act->last_execution_time<0 || (cur_time-act->last_execution_time)%act->nsteps==0) && act->type == atype){
    	//Schedule the same task into n threads, with n bunchs of data
    	act->last_execution_time = cur_time;
    	log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_DEBUG, "Scheduling activities (%s) (t=%li)", act->name, cur_time);
    	for(i=0; i<act->n_tasks; i++)
	  if((ids[i+ischeduled]=cs_wp_tsk_enqueue(act->tsk_code, (cs_data_ptr) &(act->tsk_data_arr[i]), act->total_data_size, engine->wp))<0)
	    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_ERROR, "Error scheduling activity (t=%li)",
	      cs_get_clock(engine->timer));
	ischeduled+=i;
      }
    }while((act=act->next)!=NULL);

    if(ischeduled>0)
      for(i=0; i<ischeduled; i++)
	cs_wp_tsk_wait(engine->wp, ids[i]);
}

int cs_event_driven_simulation(cs_engine_t *engine){
  return (engine->st == EVENT_DRIVEN);
}

void _cs_stop_actors(cs_engine_t *engine){
  if(engine->evm!=NULL)
    cs_evm_stop_controller(engine->evm);
  cs_time_stop(engine->timer);
}

void _engine_main_loop(cs_engine_t *engine){

  /* ENGINE					    GENERIC ACTOR
  * 1.Start other actors (event manager, etc)
  * 2.While(true)				    1.While(true)
  * 3.	if (term_conditions)
  *	  stop all the actors
  *       sync timer
  * 4.	  break;				    2.	sync_timer
  * 6.  user_activities				    3.  
  * 7.  after_step_activities			    4.  if(received_stop_signal)
  *						    5.	  break
  *						    6.  else //
  *						    7.	  perform activities
  * // Tuser or Tactor 
  * E.g: Tuser = #time is 10
  *	 Tactor = no more events (EVENT DRIVEN)
  *	 Tuser = #nodes = X
  *	 ...
  */

  int stop = 0;
  int events = (engine->evm!=NULL);
  cs_clockv cur_time = cs_get_clock(engine->timer);

  /* Start the event controller, if any */
  if(events && !cs_evm_running(engine->evm)){
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_INFO, "Starting event controller (t=%li)", cs_get_clock(engine->timer));
    if(cs_evm_start_controller(engine->evm, 1)<0)
      log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_ERROR, "while starting event controller (t=%li)", cs_get_clock(engine->timer));
  }

  /* Start the workerpool for activities, if any */
  if(engine->alist->nact>0 && engine->alist->total_tasks>0)
    cs_wp_start(engine->wp);

  //cur_time == 0
  while(!stop){

    //schedule AFTER_STEP activities
    _loop_activity(engine, AFTER_STEP);

    //check termination conditions
    if(_check_termination(engine))
      stop = 1;

    else if(events && !cs_evm_more_events(engine->evm) && cs_event_driven_simulation(engine))
      stop = 1;

    //sync into the timer (i.e. sync with the other actors)
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_INFO, "(t=%li) - Syncing..", cur_time);
    cs_time_sync(engine->timer);
    cur_time = cs_get_clock(engine->timer);

    if(stop){
      _cs_stop_actors(engine);
      break;
    }

    switch(engine->st){
      case ACTIVITY_SCAN:
	_loop_activity(engine, USER);
	break;
      case EVENT_DRIVEN: //nothing to do explicitely
	break;
    }

    //wait for the completion of events
    if(events)
      wait_event_completion(engine->evm, cur_time);
  }

  assert(cur_time == cs_get_clock(engine->timer));
  log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_INFO, "Simulation ended @%li",cs_get_clock(engine->timer));
}

void _destroy_activity_list(cs_user_activity_list_t *alist){
  cs_user_activity_t *act1 = alist->activities;
  cs_user_activity_t *act2;

  while(act1!=NULL){
    act2 = act1->next;
    free(act1->name);
    free(act1);
    act1 = act2;
  }
}

void cs_engine_fini(cs_engine_t *engine){
  if(ENG_EVM(engine)!=NULL)
    cs_destroy_event_manager(engine->evm);

  cs_wp_shutdown(engine->wp);
  cs_wp_destroy(engine->wp);

  _destroy_activity_list(engine->alist);

  pthread_mutex_destroy(engine->lock);
  fflush(stderr); 
  fflush(stdout);
  //TODO free other memory related resources (cs_engine_t)
}

void cs_sim_start(cs_engine_t *engine){

  //lock the mutex

  int nactors = 1; //the engine itself

  if(engine->timer==NULL){
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_ERROR, "Trying to start simulation without any timer/clock");
    fflush(stderr); fflush(stdout);
    exit(-1);
  }

  int c1, c2, c3;
  c1 = engine->alist!=NULL && engine->alist->nact>0;
  c2 = (engine->net_rt != NULL);
  c2 = (engine->evm!=NULL);

  /*
  * There must be almost a user-defined activity, and/or an event manager.
  */
  assert(c1 || c2 || c3);

  nactors+=c2+c3; //The engine itself drives the workepool for the activities

  log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_DEBUG, "Setting nactors as %li", nactors);
  fflush(stderr); fflush(stdout);
  cs_set_nsync(engine->timer, nactors);

  _engine_main_loop(engine);
}

int cs_init_engine(cs_engine_t *engine, int wp_nworkers){

  //init lock, workerpool, and activity list 

  int nw;
  int ret = 0;

  assert(engine!=NULL);

  engine->lock = cs_make_recursive_mutex();

  pthread_mutex_lock(engine->lock);

  nw = (wp_nworkers > 0 ? wp_nworkers : 10);

  engine->alist = malloc(sizeof(cs_user_activity_list_t));
  memset((void*) engine->alist, 0, sizeof(cs_user_activity_list_t));

  engine->wp = malloc(sizeof(cs_workerpool_t));
  if(cs_wp_init(nw, nw*10, engine->wp, "ENG-UACT")<0){
    log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_ERROR, "Error while initiating Worker Pool");
    ret = -1;
  }

  engine->st = ACTIVITY_SCAN;

  engine->net_rt = NULL;
  engine->evm = NULL;
  engine->running = 0;

  pthread_mutex_unlock(engine->lock);

  return ret;
}

void cs_set_sim_type(cs_engine_t *engine, sim_type_t st){
  engine->st = st;
}

const char *activity_to_string(activity_type_e type){
  switch(type){
    case USER:
      return "USER";
      break;
    case AFTER_STEP:
      return "AFTER_STEP";
      break;
  }
}
