/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "complex_sim.h"
#include "cs_events.h"
#include "cs_concurrence.h"

void lock_ev_tree(cs_event_manager_t *evm)
{
    pthread_mutex_lock(evm->ev_tree_lock);
}

void unlock_ev_tree(cs_event_manager_t * evm)
{
    pthread_mutex_unlock(evm->ev_tree_lock);
}

void lock_eh_tree(cs_event_manager_t * evm)
{
    pthread_mutex_lock(evm->eh_tree_lock);
}

void unlock_eh_tree(cs_event_manager_t * evm)
{
    pthread_mutex_unlock(evm->eh_tree_lock);
}

int cmp_handler_func(const void *t1, const void *t2, void *param){
  const cs_event_handler_entry_t *eh1 = t1;
  const cs_event_handler_entry_t *eh2 = t2;
  if(t1==NULL || t2==NULL)
    return -1;
  assert(eh1->etype!=NULL && eh2->etype!=NULL);
  return strcmp((const char *) eh1->etype, (const char *) eh2->etype);
}

int cmp_event_list(const void *t1, const void *t2, void *param){
  const cs_event_list_t* el1 = t1;
  const cs_event_list_t* el2 = t2;
  if(t1==NULL || t2==NULL)
    return -1;
  return (el1->scheduled_at > el2->scheduled_at ? 1 : (el1->scheduled_at < el2->scheduled_at ? -1 : 0));
}

void destroy_event_list(void *item, void *param){
  cs_event_list_t *list = item;
  cs_event_list_item_t *elem, *next;
  if(list == NULL || list->count == 0)
    return;

  elem = next = list->head;
  while(elem!=NULL){
    next = elem->enext;
    free(elem);
    elem = next;
  }

  free(list->guards);
  free(item);
}

/*
* Get all event (i.e. an event_list)
* that must be triggered at specified time.
**/
cs_event_list_t *get_events(cs_event_manager_t *evm, cs_clockv time)
{
    cs_event_list_t search, *found;
    search.scheduled_at = time;

    lock_ev_tree(evm);
    found = rb_find(evm->ev_tree, &search);
    unlock_ev_tree(evm);

    return found;
}

/*
* Create a new event list.
*/
cs_event_list_t *new_ev_list(cs_clockv time){
  cs_event_list_t *el = (cs_event_list_t *) calloc(1, sizeof(cs_event_list_t));
  if(!el){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_ERROR, "Memory error while allocating cs_event_list_t");
    return NULL;
  }
  el->head = el->tail = NULL;
  el->count = 0;
  el->scheduled_at = time;
  return el;
}

cs_event_list_item_t *new_ev_list_item(cs_event_t *ev){
  cs_event_list_item_t *el = (cs_event_list_item_t *) calloc(1, sizeof(cs_event_list_item_t));
  if(!el){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_ERROR, "Memory error while allocating cs_event_list_item_t");
    return NULL;
  }
  el->ev = ev;
  el->enext = NULL;
  return el;
}

void _cs_ev_update_guards(cs_event_list_t *ev_list, int nguards, int *hops){
  *hops = MAX(1,floor(ev_list->count/nguards));
  int i,j;
  cs_event_list_item_t *ptr = ev_list->head;
  ev_list->guards = (cs_event_list_item_t **) malloc(sizeof(cs_event_list_item_t *)*nguards);
  for(i=0; i<nguards; i++){
    ev_list->guards[i]=ptr;
    for(j=0; j<*hops; j++)
      if(ptr->enext!=NULL)
	ptr=ptr->enext;
      else
	break;	
  }
}

int cs_init_event_manager(
  cs_event_manager_t *evm,
  int nworkers,
  cs_timer_t *timer,
  sim_type_t type)
{
  if(evm==NULL)
    return -1;

  evm->stype = type;
  evm->running = 0;
  evm->stop = 0;

  //create the event handler table
  if(!(evm->eh_tree = rb_create(cmp_handler_func, NULL, &rb_allocator_default))){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to initiate handler table/tree");
    return -1;
  }
  //create the event-list' tree
  if(!(evm->ev_tree = rb_create(cmp_event_list, NULL, &rb_allocator_default))){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to initiate event tree");
    return -1;
  }

  //create the mutex for the event tree  
  if(!(evm->ev_tree_lock = cs_make_recursive_mutex())){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to initiate event tree mutex");
    return -1;
  }

  //create the mutex for the event handler table
  if(!(evm->eh_tree_lock = cs_make_recursive_mutex())){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to initiate handler table mutex");
    return -1;
  }

  //create the mutex for the event manager
  if(!(evm->mutex = cs_make_recursive_mutex())){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to initiate event manager mutex");
    return -1;
  }

  //create the mutex for waiting the completion of the events 
  if(!(evm->wait_mutex = cs_make_recursive_mutex())){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to initiate event manager wait_mutex");
    return -1;
  }

  evm->timer = timer;
  evm->nworkers = nworkers+1;
  evm->stop_cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
  evm->wait_cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
  evm->last_completion = 0;

  //create the workerpool
  if(!(evm->wp = (cs_workerpool_t *) malloc(sizeof(cs_workerpool_t)))){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Memory error allocating workerpool fot event manager");
    return -1;
  }

  //initiate the workerpool
  if(cs_wp_init(evm->nworkers, evm->nworkers*10, evm->wp, "EVM")<0){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to initiate workerpool for event manager");
    return -1;
  }
  return 0;
}

cs_wp_tsk_exit_status _ev_worker (cs_data_ptr in, cs_data_ptr *out){
  cs_wp_tsk_exit_status myret = CS_TSK_SUCCESS;
  //cs_eh_status ret;
  int c;
  event_worker_data_t *myargs = (event_worker_data_t *) in;
  cs_event_list_item_t *cur_ev = myargs->ev_list->guards[myargs->guard_index];
  for(c=0; c<myargs->num_events; c++){
    if(cur_ev==NULL){
      fprintf(stderr, "\n cur-ev is NULL, c=%d, index=%d", c, myargs->guard_index);
      exit(-1);
    }
    assert(cur_ev!=NULL);
    cs_evm_throw_event(myargs->evm, cur_ev->ev);
    cur_ev = cur_ev->enext;
  }
  //ret = cs_evm_throw_event(myargs->evm, myargs->ev);
  /*
    CS_EH_NORMAL,
    CS_EH_ERROR,
    CS_EH_NO_HANDLER
  */
/*  switch(ret){
    case CS_EH_NO_HANDLER:
      log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_WARN, "No handler for event");
      myret = CS_TSK_ERROR;
      break;
    case CS_EH_ERROR:
      log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_WARN, "Error in event handler");
      myret = CS_TSK_ERROR;
      break;
    case CS_EH_NORMAL:
      myret = CS_TSK_SUCCESS;
      break;      
  }*/
  return myret;
}

/*
* Check whether the controller has marked to be stopped.  
*/
short int _should_stop_controller(cs_event_manager_t *evm){
  short int should_stop;
  pthread_mutex_lock(evm->mutex);
  should_stop = evm->stop;
  if(evm->stop)
    pthread_cond_broadcast(&evm->stop_cond); //signal the caller to the funtion _stop_controller that can go ahead
  pthread_mutex_unlock(evm->mutex);
  return should_stop;
}

/*
* Stop the controller of the event manager.
*/
void _stop_controller(cs_event_manager_t *evm){
  log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_NOTICE, "Trying to stop the event controller..");
  pthread_mutex_lock(evm->mutex);
  evm->stop = 1;
  pthread_mutex_unlock(evm->mutex);
}

void cs_evm_stop_controller(cs_event_manager_t *evm){
  _stop_controller(evm);
}

void _evm_set_not_running(cs_event_manager_t *evm){
  pthread_mutex_lock(evm->mutex);
  evm->running = 0;
  pthread_mutex_unlock(evm->mutex);
}

void _evm_set_running(cs_event_manager_t *evm){
  log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_TRACE, "Event manager set to running..");
  pthread_mutex_lock(evm->mutex);
  evm->running = 1;
  pthread_mutex_unlock(evm->mutex);
}

int cs_evm_running(cs_event_manager_t *evm){
  int ret;
  pthread_mutex_lock(evm->mutex);
  ret = evm->running;
  pthread_mutex_unlock(evm->mutex);
  return ret;
}

void _set_evm_not_running(cs_event_manager_t *evm){
  pthread_mutex_lock(evm->mutex);
  evm->running = 0;
  pthread_mutex_unlock(evm->mutex);
}

void wait_event_completion(cs_event_manager_t *evm, cs_clockv clock){
  _LOCK_MUTEX(evm->wait_mutex);

  if(evm->last_completion<clock)
    pthread_cond_wait(&evm->wait_cond, evm->wait_mutex); //TODO fields

  _UNLOCK_MUTEX(evm->wait_mutex);
}

void _signal_event_completion(cs_event_manager_t *evm, cs_clockv clock){
  _LOCK_MUTEX(evm->wait_mutex);

  evm->last_completion=clock; //TODO field
  pthread_cond_broadcast(&evm->wait_cond);

  _UNLOCK_MUTEX(evm->wait_mutex);
}

/*
* The behaviour of the event controller.
* At each cycle it synchronize for the next tick of clock,
* then it retrieves the set of events scheduled for that time.
*/
cs_wp_tsk_exit_status _ev_controller(cs_data_ptr in, cs_data_ptr *out){
  cs_event_manager_t *evm = (cs_event_manager_t *) in;
  cs_clockv start = evm->start_time;
  cs_clockv cur_time = cs_get_clock(evm->timer);
  cs_event_list_t *ev_list;
  //cs_event_list_item_t *ev_item;
  event_worker_data_t *args;
  cs_wp_tsk_exit_status ex_st = CS_TSK_SUCCESS;
  cs_wp_tsk_id *id_arr;
  //cs_clockv prev_time = cur_time;
  int c;
  int hops;
  int ntasks = evm->nworkers-1;
  int ret_sync;
  int bound;

  _evm_set_running(evm);

  while(1){
    //(cur_time > prev_time ? log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_TRACE, "ev-control: cur-time: %li", (long int) cur_time) : 1);

    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_NOTICE, "(t=%li) - Ev-controller: syncing..", cs_get_clock(evm->timer));
    ret_sync = cs_time_sync(evm->timer); //wait for the next tick of clock
    if(ret_sync<0) //simulation stopped
      break;
    cur_time = cs_get_clock(evm->timer);

    if(_should_stop_controller(evm))
      break;

    else if(start>cur_time)
      continue;

    if((ev_list = pop_events(evm, cur_time))!= NULL && (bound = ev_list->count)>0){
      log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_TRACE, "ev-control: ev-list-count %li", (long int) ev_list->count);
      _cs_ev_update_guards(ev_list, ntasks, &hops); //update guards
      //allocate count arguments for workers
      args = (event_worker_data_t *) calloc(ntasks,sizeof(event_worker_data_t));
      id_arr = (cs_wp_tsk_id *) calloc(ntasks,sizeof(cs_wp_tsk_id));
      c = 0;
      while(c<MIN(ntasks,bound)){ //will schedule evm->nworkers-1 tasks
	args[c].ev_list = ev_list;
	args[c].evm = evm;
	args[c].num_events = (c<ntasks-1 ? hops : ev_list->count-(ntasks-1)*hops);
	args[c].guard_index = c;
	if((id_arr[c] = cs_wp_tsk_enqueue(_ev_worker, (void *) &args[c], sizeof(event_worker_data_t), evm->wp))<0){
	  log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_ERROR,\
	    "Impossible to enqueue task for an event to be raised at %d", ev_list->scheduled_at);
	  ex_st = CS_TSK_ERROR;
	}
	//ev_item = ev_item->enext;
	c++;
      }

      //wait for the completion of the event handlers
      for(c=0; c<MIN(ntasks,bound); c++)
	cs_wp_tsk_wait(evm->wp, id_arr[c]);

      //destroy the event list
      destroy_event_list(ev_list, NULL);

      //frees memory previously allocated
      free(id_arr);
      free(args);
    }

    //signal the completion of events for the current clock
    _signal_event_completion(evm, cur_time);
  }

  _set_evm_not_running(evm);
  
  log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_NOTICE, "Event controller: exiting..");
  return ex_st;
}

/**
* Starts the controller of the Event Manager.
*/
int cs_evm_start_controller(cs_event_manager_t *evm, cs_clockv start_raising){

  /* set data for the controller */
  evm->start_time = (start_raising > 0 ? start_raising : 1);

  if(cs_wp_start(evm->wp)<0){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to start event controller and workers for event manager");
    return -1;
  }

  if(cs_wp_tsk_enqueue(_ev_controller, (void *) evm, sizeof(cs_event_manager_t), evm->wp)<0){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_FATAL, "Impossible to enqueue the event controller for the event manager");
    fflush(stdout);
    return -1;
  }
  return 0;
}

void destroy_handler_entry(void *item, void *param){
  free(item);
}

void _cs_shutdown_event_manager(cs_event_manager_t *evm){
if(cs_evm_running(evm))
    _stop_controller(evm); //the controller is a worker which schedules event handlers

  cs_wp_shutdown(evm->wp); //this forces to wait the completion of event handlers
}

/**
* Deallocate resources of the event manager.
*/
void cs_destroy_event_manager(cs_event_manager_t *evm)
{
  _cs_shutdown_event_manager(evm);

  //destroy workerpool and event controller data
  //free(evm->ev_controller_data);
  if(evm->wp!=NULL)
    cs_wp_destroy(evm->wp); //will shutdown the workerpool if still running

  //destroy data
  rb_destroy(evm->eh_tree, destroy_handler_entry);
  rb_destroy(evm->ev_tree, destroy_event_list);

  //destroy mutexes
  pthread_mutex_destroy(evm->ev_tree_lock);
  free(evm->ev_tree_lock);
  pthread_mutex_destroy(evm->eh_tree_lock);
  free(evm->eh_tree_lock);
}

/**
* Get the handler from the handler table. 
*/
cs_event_handler_t cs_evm_get_handler(cs_event_type_t etype, cs_event_manager_t *evm)
{
  cs_event_handler_entry_t search, *ret;
  search.etype = etype;
  lock_eh_tree(evm);
  ret = (cs_event_handler_entry_t *) rb_find(evm->eh_tree, &search);
  unlock_eh_tree(evm);
  if(ret!=NULL)
    return ret->handler;
  else
    return NULL;
}

/**
* Install an event handler in the table.
**/
int cs_evm_install_handler(
  cs_event_type_t etype,
  cs_event_handler_t handler,
  cs_event_manager_t *evm)
{
  cs_event_handler_entry_t search, *new_entry;
  if(etype==NULL || strlen((const char *) etype)==0){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_ERROR, "cs_evm_install_handler(), parameter etype cannot be null or empty!");
    return -1;
  }

  search.etype = etype;
  lock_eh_tree(evm);

  new_entry = (cs_event_handler_entry_t *) malloc(sizeof(cs_event_handler_entry_t));
  new_entry->handler = handler;
  new_entry->etype = etype;

  if(rb_find(evm->eh_tree, &search)!=NULL)
    rb_replace(evm->eh_tree, handler);  

  else
    rb_insert(evm->eh_tree, new_entry);

  unlock_eh_tree(evm);
  return 0;
}

/**
* Throw the specified event, by the relative handler.
**/
cs_eh_status cs_evm_throw_event(cs_event_manager_t *evm, cs_event_t *ev)
{
  assert(ev!=NULL);
  assert(ev->etype!=NULL);
  cs_event_handler_t handler = cs_evm_get_handler(ev->etype, evm);
  if(handler==NULL){
    log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_ERROR, "cs_evm_throw_event(), no event handle for vent type %s", ev->etype);
    return CS_EH_NO_HANDLER;
  }
  else
    return (handler (ev, evm));
}

cs_event_list_t *pop_events(cs_event_manager_t *evm, cs_clockv time){

    cs_event_list_t search, *found = NULL;
    search.scheduled_at = time;

    //LOCK 
    lock_ev_tree(evm);
    found = rb_delete(evm->ev_tree, &search); //delete the entry from the tree
    /*if(found!=NULL){
      destroy_event_list(found, NULL); //destroy the list of events
      free(found);
    }*/
    //UNLOCK
    unlock_ev_tree(evm);

    return found;
}

void cs_evm_delete_events(cs_event_manager_t *evm, cs_clockv time){
    cs_event_list_t search, *found;
    search.scheduled_at = time;

    //LOCK 
    lock_ev_tree(evm);
    found = rb_delete(evm->ev_tree, &search); //delete the entry from the tree
    if(found!=NULL){
      destroy_event_list(found, NULL); //destroy the list of events
      free(found);
    }
    //UNLOCK
    unlock_ev_tree(evm);
}

/**
* Throw all the event scheduled in the specified time.
**/
int cs_evm_throw_scheduled_events(cs_event_manager_t * evm, cs_clockv time)
{
  int c = 0;
  cs_event_list_t *ev_list;
  cs_event_list_item_t *cur;

  //LOCK
  lock_ev_tree(evm);
  ev_list = get_events(evm, time);
  if(ev_list==NULL || ev_list->count==0){
    unlock_ev_tree(evm);
    return c;
  }

  else{
    assert(ev_list->head!=NULL);
    cur = ev_list->head;
    do{
      cs_evm_throw_event(evm, cur->ev);
      c++;
    }while((cur = cur->enext) != NULL);
  }

  cs_evm_delete_events(evm, time);
  
  //UNLOCK 
  unlock_ev_tree(evm);
  return c;
}

/**
* 
**/
int cs_evm_event_tree_is_empty(cs_event_manager_t * evm)
{
    int ret;
    lock_ev_tree(evm);
    if (evm->ev_tree->rb_count== 0)
	ret = 1;
    else
	ret = 0;
    unlock_ev_tree(evm);
    return ret;
}

int cs_evm_more_events(cs_event_manager_t *evm){
  return (cs_evm_num_events(evm)>0); 
}

/**
* Return the number of events of the event tree.
**/
int cs_evm_num_events(cs_event_manager_t *evm){
  int count = 0;
  int c = 0;
  cs_event_list_t *ev_list;
  struct rb_traverser trav;
  lock_ev_tree(evm);
  count = rb_count(evm->ev_tree);
  if(count==0){    
    unlock_ev_tree(evm);
    return 0;
  }

  rb_t_init(&trav, evm->ev_tree);
  ev_list = rb_t_first(&trav, evm->ev_tree);
  while(count>0 && ev_list!=NULL){
    c+=ev_list->count;
    count--;    
  }
  unlock_ev_tree(evm);
  return c;
}

/*
* Schedule a new event by inserting it in the tree.
*/
int cs_evm_schedule_event(cs_event_t *ev, cs_clockv at, cs_event_manager_t *evm)
{  
    cs_event_list_t search, *found;
    search.scheduled_at = at;
    cs_event_list_item_t *new_item;

    if(ev->etype==NULL){
      log4c_category_log(log4c_category_get("cs.events"), LOG4C_PRIORITY_ERROR, "Event has NULL event-type, doing nothing!");
      return -1;
    }

    new_item = new_ev_list_item(ev);

    //LOCK
    lock_ev_tree(evm);
    found = (cs_event_list_t *) rb_find(evm->ev_tree, &search);
    if(found==NULL){
      found = new_ev_list(at);
      found->head = found->tail = new_item;
      rb_insert(evm->ev_tree, found);
    }

    else{
      found->tail->enext = new_item;
      new_item->enext = NULL;
      found->tail = new_item;      
    }    

    found->count++;
    
    //UNLOCK
    unlock_ev_tree(evm);

    return 0;
}


/**
* Look for the event with the minimum
* schedulation time.
**/
cs_clockv cs_evm_find_nearest_events(cs_event_manager_t * evm)
{
  cs_event_list_t *found;
  struct rb_traverser trav;
  rb_t_init(&trav, evm->ev_tree);

  //LOCK 
  lock_ev_tree(evm);
  found = (cs_event_list_t *) rb_t_first(&trav, evm->ev_tree);
  //UNLOCK
  unlock_ev_tree(evm);

  if(found!=NULL && found->count>0)
    return found->scheduled_at;

  else
    return -1;
}

/**
* Look for the event with the maximum
* schedulation time.
**/
cs_clockv cs_evm_find_farthest_events(cs_event_manager_t *evm)
{
  cs_event_list_t *found;
  struct rb_traverser trav;
  rb_t_init(&trav, evm->ev_tree);

  lock_ev_tree(evm);
  found = (cs_event_list_t *) rb_t_last(&trav, evm->ev_tree);
  unlock_ev_tree(evm);

  if(found!=NULL && found->count>0)
    return found->scheduled_at;

  return -1;
}
