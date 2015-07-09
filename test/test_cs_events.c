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
#include "cs_events.h"

cs_eh_status h_t1(cs_event_t *ev){
  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_NOTICE, "handler_t1(), v=%s", (const char *) ev->ev_data);
  return CS_EH_NORMAL;
}

cs_eh_status h_t2(cs_event_t *ev){
  log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_NOTICE, "handler_t2(), v=%s", (const char *) ev->ev_data);
  return CS_EH_NORMAL;
}

int main(int argc, char *argv[]){

  log4c_init();
  
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
  assert(cs_init_event_manager(&evm, 10, clock, ACTIVITY_SCAN)==0);
  cs_evm_start_controller(&evm, (cs_clockv) 0);

  /* Handler T1,T2 */
  assert(cs_evm_install_handler(t1, h_t1, &evm)==0);
  assert(cs_evm_install_handler(t2, h_t2, &evm)==0);

  /* Scheduling of some eventss */
  assert(cs_evm_schedule_event(&ev1, (cs_clockv) 2, &evm)==0);
  assert(cs_evm_num_events(&evm) == 1);

  assert(cs_evm_schedule_event(&ev2, (cs_clockv) 4, &evm)<0);
  assert(cs_evm_num_events(&evm) == 1);

  /* go ahead with time */
  cs_time_sync(clock); //clock == 1, nothing will happen
  cs_time_sync(clock); //clock == 2, should raise event ev1

  /* schedule event for clock=4 */
  ev2.etype = t2;
  assert(cs_evm_schedule_event(&ev2, (cs_clockv) 4, &evm)==0);

  cs_time_sync(clock); //clock == 3, nothing should happen
  cs_time_sync(clock); //clock == 4, should raise event ev2

  cs_evm_stop_controller(&evm);
  cs_time_stop(clock);

  cs_destroy_event_manager(&evm);
  log4c_fini();

  return 0;
}
