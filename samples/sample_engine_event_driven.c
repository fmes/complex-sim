/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
 
#include "cs_engine.h"

#include <stdlib.h>

#define END 5
#define DATA_SIZE_EVEN 20
#define DATA_SIZE_ODD 21

const cs_event_type_t et[7] = {"T0", "T1", "T2", "T3", "T4", "T5", "T6"};

cs_eh_status ev_handler(cs_event_t *ev, cs_event_manager_t *evm){
  //the sequence of events is T0->T2
  //or T1->T3,T4
  cs_event_t *eva;
  int n = 0; int i;
  log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_TRACE, "sample1.handler, T=%li, ev=%s",\
    cs_get_clock(CS_EVM_TIMER(evm)), ev->etype);
  if(CS_EV_IS_TYPE(ev, et[0])){ //T0 --> T2
    n = 1;
    eva = CS_NEW_EVENT(n);
    CS_EVENT_SET_TYPE(eva,et[2]);
  }
  else if(CS_EV_IS_TYPE(ev, et[1])){ //T1 --> T3,T4
    n = 2;
    eva = CS_NEW_EVENT(n);
    CS_EVENT_SET_TYPE(eva,et[3]);  //T3
    CS_EVENT_SET_TYPE(&(eva[1]),et[4]);  //T4
  }

  for(i=0; i<n; i++){
    log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_INFO, "Scheduling event %s @%li", eva[i].etype,cs_get_clock(CS_EVM_TIMER(evm))+1);
    if(cs_evm_schedule_event(&(eva[i]), cs_get_clock(CS_EVM_TIMER(evm))+1 /* next time */, evm)!=0){
      log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "Error scheduling event %s", eva[0].etype);
      return CS_EH_ERROR;
    }
  }
  return CS_EH_NORMAL;
}

int main(int argc, char *argv[]){

  sim_type_t stype = EVENT_DRIVEN;
  cs_engine_t *engine;
  cs_timer_t *timer;
  cs_event_manager_t *evm;
  int i;
  srand(time(NULL));
  cs_event_type_t st = (rand() %2 == 0 ? et[0] : et[1]);

  log4c_init();
  log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_INFO, "*** %s, sim_type=%s",\
    argv[0], (stype==ACTIVITY_SCAN ? "ACTIVITY_SCAN" : "EVENT_DRIVEN"));
  
  //timer initialization
  timer = (cs_timer_t*) calloc(1, sizeof(cs_timer_t));
  cs_init_timer(timer, "CLOCK_TEST");

  //set an event to start with
  cs_event_t *ev_start = CS_NEW_EVENT(1);
  CS_EVENT_SET_TYPE(ev_start,st);

  if((evm = (cs_event_manager_t *) malloc(sizeof(cs_event_manager_t)))==NULL){
    log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "*** %s, Memory error", argv[0]);
      return -1;
	}

  if(cs_init_event_manager(evm, 10, timer, stype)!=0){
    log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "*** %s, error\
      initiating Event manager", argv[0]);
      return -1;
  }

  for(i=0; i<7; i++){
    if(cs_evm_install_handler(et[i], ev_handler, evm)!=0){
      log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "*** %s, error\
      installing handler for event %s", argv[0], et[i]);
      return -1;
      }	
  }

  /* Scheduling of the first event */
  if(cs_evm_schedule_event(ev_start, (cs_clockv) 4, evm)<0){
    log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "*** %s, error\
      scheduling event %s", argv[0], et[0]);
    return -1;
    }

  /* Engine */
  engine = malloc(sizeof(cs_engine_t));

  if(cs_init_engine(engine, 5 /* nworkers */)<0){
    log4c_category_log(log4c_category_get("cs.sample"), LOG4C_PRIORITY_FATAL, "*** %s, error\
      init_engine", argv[0]);
    return -1;
  }

  cs_set_sim_type(engine, stype);
  cs_set_event_manager(engine, evm);
  cs_set_timer(engine, timer);

  /* start the simulation */
  cs_sim_start(engine);

  //when function sim_start() returns, the simulation has ended
  cs_engine_fini(engine);
  log4c_fini();
}
