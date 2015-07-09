/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _CS_EVENTS_
#define  _CS_EVENTS_

#include "complex_sim.h"
#include "cs_timer.h"
#include "cs_workerpool.h"
#include "rb.h"
#include <pthread.h>

#define CS_EVM_TIMER(evm) ((evm)->timer)
#define CS_NEW_EVENT(n) (cs_event_t *) malloc(n*sizeof(cs_event_t))
#define CS_EVENT_SET_TYPE(ev, type) ((ev)->etype = type)
#define CS_EVENT_GET_TYPE(ev) ((ev)->etype)
#define CS_EV_IS_TYPE(ev,type) (strcmp(type, (ev)->etype) == 0 ? 1 : 0)
#define CS_EV_SET_DATA(ev, data) {(ev)->ev_data = data}
#define CS_EV_GET_DATA(ev) ((ev)->ev_data)

typedef const char* cs_event_type_t;

/**
* The code returned by an event handler.
*/
typedef enum cs_eh_status_e{
  CS_EH_NORMAL,
  CS_EH_ERROR,
  CS_EH_NO_HANDLER
}cs_eh_status;

/**
 * Structure representing an event, with
 * type and associated data.
 */
typedef struct cs_event_s{
    cs_event_type_t etype;
    cs_data_ptr ev_data;
}cs_event_t;

typedef struct cs_event_list_item_s{
  cs_event_t *ev;
  struct cs_event_list_item_s *enext;
}cs_event_list_item_t;

/**
* A list of events. It is intented
* to collect a set of events to be scheduled
* in the same tick of clock.
*/
typedef struct cs_event_list_s{
    cs_event_list_item_t *head, *tail;
    cs_clockv scheduled_at;
    int count;
    cs_event_list_item_t **guards;
}cs_event_list_t;

/*
* The event handler table.
*/
typedef struct rb_table cs_event_handler_table_t;

/*
* The event lists' tree.
*/
typedef struct rb_table cs_event_tree_t;

/**
* The event manager object.
*/
typedef struct cs_event_manager_s{
  cs_event_handler_table_t *eh_tree;
  pthread_mutex_t *eh_tree_lock;
  pthread_mutex_t *ev_tree_lock;

  cs_event_tree_t *ev_tree;
  cs_timer_t *timer;

  cs_workerpool_t *wp;
  int nworkers;

  /* data about the event controller */
  cs_clockv start_time;
  cs_clockv last_completion;
  pthread_mutex_t *mutex;
  pthread_mutex_t *wait_mutex;
  short int stop;
  short int running;
  sim_type_t stype;
  pthread_cond_t stop_cond;
  pthread_cond_t wait_cond;
  

  /* data about the events in the tree */
  int nguards; //== number of workers for event management
} cs_event_manager_t;

/**
* The definition of the event handler.
*/
typedef cs_eh_status (*cs_event_handler_t) (cs_event_t * ev, cs_event_manager_t *evm);


/*
* An element of the table of the event handler.
*/
typedef struct cs_event_handler_entry{
    cs_event_handler_t handler;
    cs_event_type_t etype;
}cs_event_handler_entry_t;

typedef struct event_worker_data_s{
//  cs_event_t *ev;
  int guard_index, num_events;
  cs_event_list_t *ev_list;
  cs_event_manager_t *evm;
}event_worker_data_t;

/**
* Release the memory allocated during the
* initialization of the event manager.
*/
void cs_destroy_event_manager(cs_event_manager_t * evm);

/**
* Initialize an event manager. 
*/
int cs_init_event_manager(cs_event_manager_t *evm, int nworkers, cs_timer_t *timer, sim_type_t type);
/**
* Schedule an event to be raised at clock time. 
* @param ev the event to be raised
* @param time the time at which the event must be raised
* @evm the event manager 
*/
int cs_evm_schedule_event(cs_event_t *ev, cs_clockv time, cs_event_manager_t *evm);

/*
* Get the handler associated to the event key ekey.
* @param etype the type of event to raise
* @param evm the event manager 
*/
cs_event_handler_t cs_evm_get_handler(cs_event_type_t etype, cs_event_manager_t *evm);

/**
* Throw the specified event.
* 
* @param evm the event manager.
* @param ev the event to be raised.
* @return the number of events that has been raised.
*/
int cs_evm_throw_scheduled_events(cs_event_manager_t *evm, cs_clockv time);

/**
* Throw the specified event.
* 
* @param evm the event manager.
* @param ev the event to be raised.
*/
cs_eh_status cs_evm_throw_event(cs_event_manager_t *evm, cs_event_t *ev);

/**
* Associate the specified handler to the
* specified event-type.
* 
* @etype the event type to associate with the handler
* @handler the handler to install for the specified etype.
* @evm the event manager.
*/
int cs_evm_install_handler(cs_event_type_t etype, cs_event_handler_t handler, cs_event_manager_t *evm);

/**
* Return the list of events to be raised 
* at the specified time. 
*
* @param time the time at which the events should be raised.
* @param evm the event manager.
*/
cs_event_list_t *cs_evm_get_events(cs_event_manager_t *evm, cs_clockv time);

/**
* Return and delete the list of events to be raised 
* at the specified time. 
*
* @param time the time at which the events should be raised.
* @param evm the event manager.
*/
cs_event_list_t *pop_events(cs_event_manager_t *evm, cs_clockv time);

/*
* Find the minimum scheduling time in the event manager.
* @param evm the event manager. 
*/
cs_clockv cs_evm_find_nearest_events(cs_event_manager_t * evm);

/**
* Find the maximum scheduling time in the event manager.
* @param evm the event manager.
*/
cs_clockv cs_evm_find_farthest_events(cs_event_manager_t *evm);

/**
* The total number of scheduled events.
* @return the number of events in the event tree;
*/
int cs_evm_num_events(cs_event_manager_t *evm);

/**
* Destroy the event manager, deallocating the
* memory allocated during its initialization.
* @warning all the events will be discarded, as well as the event handler table.
*/
void cs_destroy_event_manager(cs_event_manager_t * evm);

/**
* Starts the controller of the event manager as task of a workerpool.
*/
int cs_evm_start_controller(cs_event_manager_t *evm, cs_clockv start_raising);

/**
* Stop the controller of the event manager.
*/
void cs_evm_stop_controller(cs_event_manager_t *evm);

/**
* Check whether there are events to process.
*/
int cs_evm_more_events(cs_event_manager_t *evm);

/**
* Check whether the event manager is running.
*/
int cs_evm_running(cs_event_manager_t *evm);

/**
* Check whether the event is of a specific type.
* @return zero if the event is of type type, 
* another value otherwise. 
*/
int cs_ev_is_type(cs_event_t *ev, cs_event_type_t *type);

/**
* Wait for the completion of all events 
* for the specified clock value. 
*/
void wait_event_completion(cs_event_manager_t *evm, cs_clockv clock);

#endif
