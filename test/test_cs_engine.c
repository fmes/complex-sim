/* Copyright (c) 2012, Fabrizio Messina, University of Catania
  cs_wp_tsk_exit_status 
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

#define END 5
#define DATA_SIZE_EVEN 20
#define DATA_SIZE_ODD 21

typedef struct user_data{
  int *data;
  cs_engine_t *engine;
}user_data_t;

cs_eh_status h_t1(cs_event_t *ev){
  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_TRACE, "handler_t1(), v=%s", (const char *) ev->ev_data);
  return CS_EH_NORMAL;
}

cs_eh_status h_t2(cs_event_t *ev){
  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_TRACE, "handler_t2(), v=%s", (const char *) ev->ev_data);
  return CS_EH_NORMAL;
}

int to_even(int data){
  return data*2;
}

int to_odd(int data){
  return data*2+1;
}

cs_wp_tsk_exit_status test_func_user_even(
  cs_data_ptr in,
  cs_data_ptr *out){

  int i;
  int *data;

  cs_tsk_data_t *data_in = (cs_tsk_data_t *) in;  
  cs_timer_t *timer = ((user_data_t *) data_in->data)->engine->timer;
  data = (int*) ((user_data_t *) data_in->data)->data;

  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_DEBUG, "(t=%li) - test_func_user_even, index=%d, offset=%d, [%d,%d]",
    cs_get_clock(timer), data_in->index, data_in->offset, data_in->index, data_in->index+data_in->offset);

  for(i=data_in->index; i<=data_in->index+data_in->offset; i++)
    data[i]=to_even(data[i]);

  return CS_TSK_SUCCESS;
}

cs_wp_tsk_exit_status test_func_user_odd(
  cs_data_ptr in,
  cs_data_ptr *out){

  int i;

  cs_tsk_data_t *data_in = (cs_tsk_data_t *) in;
  cs_timer_t *timer = ((user_data_t *) data_in->data)->engine->timer;
  int *data = (int*) ((user_data_t *) data_in->data)->data;

  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_DEBUG, "(t=%li) - test_func_user_odd, index=%d, offset=%d, [%d,%d]",
    cs_get_clock(timer), data_in->index, data_in->offset, data_in->index, data_in->index+data_in->offset);

  for(i=data_in->index; i<=data_in->index+data_in->offset; i++)
    data[i] = to_odd(data[i]);

  return CS_TSK_SUCCESS;
}

cs_wp_tsk_exit_status test_func_after_step_odd(
  cs_data_ptr in,
  cs_data_ptr *out){

  cs_tsk_data_t *data_in = (cs_tsk_data_t *) in;
  cs_timer_t *timer = ((user_data_t *) data_in->data)->engine->timer;

  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_DEBUG, "(t=%li) - test_func_after_step_odd, index=%d, offset=%d, [%d,%d]",
    cs_get_clock(timer), data_in->index, data_in->offset, data_in->index, data_in->index+data_in->offset);

  return CS_TSK_SUCCESS;
}

int term_function(cs_data_ptr args){
  return (cs_get_clock(((cs_engine_t *) args)->timer))>=END;
}

void test_engine(sim_type_t st){

  log4c_category_log(log4c_category_get("cs.engine"), LOG4C_PRIORITY_INFO, "*** test_engine(), sim_type=%s",\
    (st==ACTIVITY_SCAN ? "ACTIVITY_SCAN" : "EVENT_DRIVEN"));

  user_data_t *udata = (user_data_t *) malloc(sizeof(user_data_t)*2);

  cs_engine_t *engine;
  int i;
  int *data_even = malloc(sizeof(int)*DATA_SIZE_EVEN);
  int *data_odd = malloc(sizeof(int)*DATA_SIZE_ODD);
  
  cs_timer_t *clock = (cs_timer_t*) calloc(1, sizeof(cs_timer_t));
  cs_init_timer(clock, "CLOCK_TEST");
  cs_set_nsync(clock,2);

  cs_event_type_t t1 = "T1";
  cs_event_type_t t2 = "T2";

  cs_event_t ev1, ev2;
  ev1.ev_data = "D1";
  ev2.ev_data = "D2";
  ev1.etype = t1;
  ev2.etype = NULL;

  cs_event_manager_t evm;
  assert(cs_init_event_manager(&evm, 10, clock, st)==0);
  //cs_evm_start_controller(&evm, (cs_clockv) 0);

  /* Handler T1,T2 */
  assert(cs_evm_install_handler(t1, h_t1, &evm)==0);
  assert(cs_evm_install_handler(t2, h_t2, &evm)==0);

  /* Scheduling of some events */
  assert(cs_evm_schedule_event(&ev1, (cs_clockv) 2, &evm)==0);
  assert(cs_evm_num_events(&evm) == 1);

  assert(cs_evm_schedule_event(&ev2, (cs_clockv) 4, &evm)<0);
  assert(cs_evm_num_events(&evm) == 1);

  /* Engine */
  engine = malloc(sizeof(cs_engine_t));
  
  assert(cs_init_engine(engine, 5 /* nworkers */)==0);

  udata[0].engine = engine;
  udata[0].data = (void*) data_even;

  udata[1].engine = engine;
  udata[1].data = (void*) data_odd;

  cs_set_sim_type(engine, st); //also include events
  cs_set_termination_function(engine, term_function, (cs_data_ptr) engine);
  cs_set_event_manager(engine, &evm);
  cs_set_timer(engine, clock);

  for(i=0; i<DATA_SIZE_EVEN; i++)
    data_even[i] = i+1;

  for(i=0; i<DATA_SIZE_ODD; i++)
    data_odd[i] = i+1;

  /* Bind/define some activities */
  cs_bind_sim_activity(
    test_func_user_even, //the task
    engine, //the engine
    (cs_data_ptr) &udata[0], //data
    (size_t) sizeof(udata[0]), //size of data
    DATA_SIZE_EVEN, //number of elements within data
    2, //number of steps between
    USER, "TEST_FUNC_EVEN"); //type of activity

  /* Bind/define some activities */
  cs_bind_sim_activity(
    test_func_user_odd, //the task
    engine, //the engine
    (cs_data_ptr) &udata[1], //data
    (size_t) sizeof(udata[1]), //size of data
    DATA_SIZE_ODD, //number of elements within data
    2, //number of steps between 
    USER, "TEST_FUNC_ODD"); //type of activity

  /* Bind/define some activities, AFTER_STEP */
  cs_bind_sim_activity(
    test_func_after_step_odd, //the task
    engine, //the engine
    (cs_data_ptr) &udata[1], //data
    (size_t) sizeof(udata[1]), //size of data
    DATA_SIZE_ODD, //number of elements within data
    2, //number of steps between
    AFTER_STEP, "TEST_FUNC_AFTER_STEP"); //type of activity

  cs_sim_start(engine);

  //simulation finished
  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_INFO, "Test simulation finished, checking post conditions (ST=%s)",\
    (st==ACTIVITY_SCAN ? "ACTIVITY_SCAN" : "EVENT_DRIVEN"));

  if(st == EVENT_DRIVEN){
    for(i=0; i<DATA_SIZE_EVEN; i++)
      assert(data_even[i]==i+1);
    for(i=0; i<DATA_SIZE_ODD; i++)
      assert(data_odd[i]==i+1);
  }

  else{ //activity scan
    for(i=0; i<DATA_SIZE_EVEN; i++)
	assert(data_even[i]%2==0);

    for(i=0; i<DATA_SIZE_ODD; i++){
      assert(data_odd[i]%2!=0);
    }
  }

  fflush(stdout);
  fflush(stderr);

  cs_engine_fini(engine);
}

int main(int argc, char *argv[]){

  log4c_init();
  test_engine(EVENT_DRIVEN);

  sleep(1);
  test_engine(ACTIVITY_SCAN);
  log4c_fini();
}
