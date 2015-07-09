/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef ENGINE_H

#define ENGINE_H

#include "complex_sim.h"
#include "cs_timer.h"
#include "cs_workerpool.h"
#include "cs_events.h"
#include "cs_network_runtime.h"
#include "cs_psk.h"

#define ENG_TIMER(engine) (engine->timer)
#define ENG_EVM(engine) (engine->evm)
#define ENG_NET_RT(engine) (engine->net_rt)

#define TSK_DATA_INDEX(tsk) ((tsk)->data_index)
#define TSK_INDEX(tsk) ((tsk)->index)
#define TSK_DATA(tsk) ((tsk)->data)
#define TSK_DATA_OFFSET(tsk) ((tsk)->offset)
#define TSK_ENGINE(tsk) ((cs_engine_t *) (tsk)->engine)
#define TSK_SET_RESULT(outptr, result) ((*outptr) = (result))
#define TSK_TOT(tsk) (tsk->tot_tasks);

/**
* The type of the activity.
*/
typedef enum activity_type{
  AFTER_STEP,
  USER
}activity_type_e;

typedef struct cs_tsk_data_e{
  cs_data_ptr data;
  int data_index;
  int index;
  /* The user task should operate on data from data[index] to
   * data[index+offset]
   */
  int offset;

  void *engine;
  int tot_tasks;
}cs_tsk_data_t;


/**
* User-defined activity, to be performed during the simulation.
*/
typedef struct cs_user_activity_s{
  cs_wp_tsk_exit_status (*tsk_code) (cs_data_ptr in, cs_data_ptr *out);
  cs_tsk_data_t *tsk_data_arr;
  int nsteps;
  char *name;
  int n_tasks;
  size_t total_data_size;
  cs_clockv last_execution_time;
  activity_type_e type;
  struct cs_user_activity_s *next;
}cs_user_activity_t;

/**
* A list of activities to be performed during the simulation.
*/
typedef struct cs_user_activity_list{
  /* Activities */
  cs_user_activity_t *activities;
  cs_user_activity_t *last_act;
  int nact;
  int total_tasks;
}cs_user_activity_list_t;

/**
* The engine structure, opaque for the user.
*/
typedef struct cs_engine_s{

  /* Activities */
  cs_user_activity_list_t *alist; //init

  /* Event manager, network runtime */
  cs_event_manager_t *evm; //set by the user
  cs_network_runtime_t *net_rt; //set by the user

  /* Workerpool, timer, network runtime, etc */
  cs_workerpool_t *wp; //init
  cs_timer_t *timer;  //init or set by the user

  /* Termination function */
  int (*term_func) (cs_data_ptr in); //set by the user
  cs_data_ptr term_func_args; //set by the user

  /* Flags */ 
  sim_type_t st; //init
  int running; //internal flag

  /* Mutex */
  pthread_mutex_t *lock;

}cs_engine_t;

/**
* Bind an activity to be performed between a step of the simulation.
*/
void cs_bind_sim_activity(
  cs_wp_tsk_exit_status (*tsk_code) (cs_data_ptr in, cs_data_ptr *out),
  cs_engine_t *engine,
  cs_data_ptr in,
  size_t data_size,
  int data_n_items,
  int nsteps,
  activity_type_e type,
  const char *aname);

/**
* Init the engine. 
*/
int cs_init_engine(cs_engine_t *engine, int nworkers);

/**
* Set the timer. 
*/
void cs_set_timer(cs_engine_t *engine, cs_timer_t *timer);

/**
* Set the termination function. 
*/
void cs_set_termination_function(cs_engine_t *engine, int (*func) (cs_data_ptr in), cs_data_ptr);

/**
* Set the event manager. 
*/
void cs_set_event_manager(cs_engine_t *engine, cs_event_manager_t *evm);

/**
* Set the network runtime.
*/
void cs_set_network_runtime(cs_engine_t *engine, cs_network_runtime_t *cs_net_rt /* TODO network runtime*/);

/**
* Set the simulation type.
*/
void cs_set_sim_type(cs_engine_t *engine, sim_type_t st);

/*
* Starts a simulation.
*/
void cs_sim_start(cs_engine_t *engine);

/**
* Check whether the simulation is event driven. 
*/
int cs_event_driven_simulation(cs_engine_t *engine);

/**
*
*/
void cs_engine_fini(cs_engine_t *engine);

/**
* 
*/
const char *activity_to_string(activity_type_e type);

#endif
