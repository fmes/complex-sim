/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "rb.h"
#include "cs_workerpool.h"
#include "cs_concurrence.h"
#include "cs_events.h"
#include "cs_timer.h"


#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include <assert.h>
#include <values.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include <igraph/igraph.h>
#include "log4c.h"

#define SL 10
#define MAX_CL 50

#define TTL 5

#define SUCCESS 1
#define FAIL -1
#define CONTINUE 0

#define LOCK(m) pthread_mutex_lock(m)
#define UNLOCK(m) pthread_mutex_unlock(m)

#define min(a,b) (a < b ? a : b)
#define INSERT_CENTROID(cl, n, index) {cl->nids[index]=n->nid; cl->connectivity[index]=n->connectivity; n->cindex=index;}
#define MOVE_CENTROID(cl, old_index, new_index) {cl->nids[new_index]=cl->nids[old_index];\
   cl->connectivity[new_index]=cl->connectivity[old_index];}
#define UPDATE_MINIMUM_CENTROID(cl, i){cl->min_con_index = i; cl->min_con = cl->connectivity[i];}


#define EV_MSG "T"

typedef struct simul_travel_task_res_s{
  int s,f; //success, failures
  long int tot_hops; //total number of hops
}simul_travel_task_res_t; 

typedef struct common_ev_data{
  cs_event_manager_t *evm; //event manager
  igraph_adjlist_t *al; //adjacency matrix
  gsl_rng *r; //random generator
  int ttl; //time to live
  cs_timer_t *clock;
  pthread_mutex_t *lock;
  long int f,s,hops;
}common_ev_data_t;

typedef struct message_ev_data{
  message_t *msg; //message 
  common_ev_data_t *sim_data; //simulation data
}message_ev_data_t;

/*
* The travel bag of each worker.
*/
typedef struct travel_bag_s{
  int nmsg;
  int ttl;
  long int nnodes;
  igraph_adjlist_t *al;
  gsl_rng *r;
  int tindex;
  log4c_category_t *log;
  cs_event_manager_t *evm;
  cs_timer_t *clock;
  common_ev_data_t *sim_data;
}travel_bag_t;


/*
* A message.
*/
typedef struct message_s{
  long int msg_id;
  long int sender;
  long int destination;
  long int current;
  short int hops;
}message_t;

/*
* Generates a netowkr through the 
* Barabasi model implemented into Igraph.
*/
igraph_t *barabasi(int n, int m){

  assert(n>0);

  igraph_t *g;
  if(!(g = (igraph_t *) malloc(sizeof(igraph_t))))
    fprintf(stderr, "\n barabasi(), memory error**\n");

  if(igraph_barabasi_game(/*graph*/g, 
			/*n*/n,
			/*P*/1.5,
			/*m*/m,
			/*outseq*/0,
			/*outpref*/0,
			/*A*/1,
			/*directed*/ 0,
			/*algo*/ IGRAPH_BARABASI_PSUMTREE,
			0)==IGRAPH_EINVAL){
    fprintf(stderr,  "\n**Error barabasi**\n");
    return NULL;
  }

  else 
    return g;
}

/*
* Print the main attributes of a graph. 
*/
void print_graph(igraph_t *g, log4c_category_t *log){
  igraph_vector_t v;
  igraph_vector_init(&v, 0);
  igraph_get_edgelist(g, &v, 0);
//  igraph_vector_print(&v);
log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "n-edges: %li, density: %2.12f", igraph_vector_size(&v), 
    ((double) igraph_vector_size(&v)/ ( ((double) igraph_vcount(g))*((double) igraph_vcount(g)))));
}

/*
* A basic step of a message in the network.
* This is called by simul_travel_task()
* or simula_travel_task_with_events()
*/
int travel(
  message_t *message, 
  igraph_adjlist_t *al,
  gsl_rng *r,
  int ttl){

  long int pos;

  assert(message->current>=0);
  igraph_vector_t *neigh = igraph_adjlist_get(al,message->current);
  assert(igraph_vector_size(neigh)>0);
  //igraph_vector_print(neigh);
  long int sel_index;

  if(message->destination==message->current){
//    fprintf(stderr, "\n Message (%li:%li->%li) success, hops=%d",
//	message->msg_id, message->sender, message->destination, message->hops);
    return SUCCESS;
  }

  else if(message->hops==ttl){
  //  fprintf(stderr, "\n Message (%li:%li->%li) failed", message->msg_id, message->sender, message->destination);
    return FAIL;
  }

  else if(igraph_vector_binsearch(neigh, message->destination, &pos)>0){
    message->current = message->destination;
    message->hops++;
    return CONTINUE;
  }

  else{ //select a random node
    sel_index = gsl_rng_uniform_int(r, igraph_vector_size(neigh));
    message->current = VECTOR(*neigh)[sel_index];
    message->hops++;
    return CONTINUE;
  }
}

/*
* Message initialization.
*/
void init_message(
    message_t *msg,
    gsl_rng * r,
    long int nnodes){
  msg->hops=0;
  msg->current=gsl_rng_uniform_int(r, nnodes);
  assert(msg->current>=0);
  msg->sender=msg->current;
  msg->destination=gsl_rng_uniform_int(r, nnodes);
  assert(msg->destination>=0);
}

/*
* Schedules n messages as events.
*/
cs_wp_tsk_exit_status simul_travel_task_with_events(cs_data_ptr in, cs_data_ptr *out){
  travel_bag_t *tbag = in;
  int i;
  message_t *msg = (message_t *) malloc(sizeof(message_t)*tbag->nmsg);
  int *ret = (int*) malloc(sizeof(int)*tbag->nmsg);
  cs_event_t *ev = (cs_event_t *) malloc(sizeof(cs_event_t)*tbag->nmsg);

  /* Initialize messages */
  for(i=0; i<tbag->nmsg; i++){
    msg[i].msg_id = tbag->tindex*tbag->nmsg+i;
    init_message(&msg[i], tbag->r, tbag->nnodes);
    ret[i] = CONTINUE;
  }

  /* initialize events */
  for(i=0; i<tbag->nmsg; i++){
    ev[i].ev_data = (void*) malloc(sizeof(message_ev_data_t));
    ((message_ev_data_t *) ev[i].ev_data)->msg = &msg[i];
    ((message_ev_data_t *) ev[i].ev_data)->sim_data = tbag->sim_data;
    ev[i].etype = EV_MSG;
  }

  /* schedule events */
  for(i=0; i<tbag->nmsg; i++)
    assert(cs_evm_schedule_event(&ev[i], (cs_clockv) cs_get_clock(tbag->clock)+1, tbag->evm)==0);

  return CS_TSK_SUCCESS;
}

/*
* 
*/
cs_eh_status h_t1(cs_event_t *ev){
  //log4c_category_log(log4c_category_get("cs.test"), LOG4C_PRIORITY_TRACE, "handler_t1(), v=%s", (const char *) ev->ev_data);
  message_ev_data_t *data = ev->ev_data;
  int ret;

  if((ret=travel(data->msg, data->sim_data->al, data->sim_data->r, data->sim_data->ttl))==CONTINUE){
    if(cs_evm_schedule_event(ev, (cs_clockv) cs_get_clock(data->sim_data->clock)+1, data->sim_data->evm)!=0){
      fprintf(stderr, "\n Error scheduling event! \n");
      exit(-1);
    }
  }

  else if(ret==FAIL){
//    fprintf(stderr, "\n Message failure, hops=%d\n", data->msg->hops);
    LOCK(data->sim_data->lock);
    data->sim_data->f++;  
    data->sim_data->hops+=data->msg->hops;
    UNLOCK(data->sim_data->lock);
  }

  else{
    LOCK(data->sim_data->lock);
    data->sim_data->s++;  
    data->sim_data->hops+=data->msg->hops;
    UNLOCK(data->sim_data->lock);
  //  fprintf(stderr, "\n Message success, hops=%d\n", data->msg->hops);
  }

  return CS_EH_NORMAL;
}

cs_wp_tsk_exit_status simul_travel_task(cs_data_ptr in, cs_data_ptr *out){
  travel_bag_t *tbag = in;
  int i, s, f;
  s=f=0;
  long int tot_hops=0;
  message_t *msg = (message_t *) malloc(sizeof(message_t)*tbag->nmsg);
  int *ret = (int*) malloc(sizeof(int)*tbag->nmsg);
  int go = 1;

  for(i=0; i<tbag->nmsg; i++){
    msg[i].msg_id = tbag->tindex*tbag->nmsg+i;
    init_message(&msg[i], tbag->r, tbag->nnodes);
    ret[i] = CONTINUE;
  }
  
  while(go){
    go = 0;
    for(i=0; i<tbag->nmsg; i++)
      if(ret[i] == CONTINUE){
	ret[i] = travel(&msg[i], tbag->al, tbag->r, tbag->ttl);
	go = 1;
      }
  }

  for(i=0; i<tbag->nmsg; i++){
//    log4c_category_log(tbag->log, LOG4C_PRIORITY_NOTICE, "W%d, msg %d: %d", tbag->tindex, msg[i].msg_id, ret[i]);
    if(ret[i]==SUCCESS)
      s++;
    else
      f++;
    tot_hops+=msg[i].hops;
  }

  simul_travel_task_res_t *res = malloc(sizeof(simul_travel_task_res_t));
  res->f = f; res->s = s; res->tot_hops=tot_hops;
  *out = res;
  free(msg);
  free(ret);
  return CS_TSK_SUCCESS;
}

void simulate_travel_with_events(
  igraph_adjlist_t *al,
  long int nnodes,
  gsl_rng *r,
  int nmsg,
  int nw,
  int ttl,
  int nw_ev,
  log4c_category_t *log){

  cs_clockv start = 2;
  cs_timer_t *clock = (cs_timer_t*) calloc(1, sizeof(cs_timer_t));
  cs_init_timer(clock, "CLOCK_TRAVEL", start);

  /* Get a workerpool and schedule events */
  int i;
  int n = floor(nmsg/nw);
  cs_wp_tsk_id tid[nw];
  cs_workerpool_t* wp;
  //cs_wp_tsk_res *res;

  common_ev_data_t *sim_data = (common_ev_data_t *) malloc(sizeof(common_ev_data_t));
  sim_data->s=sim_data->f=sim_data->hops=0;
  sim_data->lock = cs_make_recursive_mutex();

  log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "nw=%d", nw);

  cs_event_manager_t *evm = (cs_event_manager_t *) malloc(sizeof(cs_event_manager_t));
  assert(cs_init_event_manager(evm, nw_ev, clock, EVENT_DRIVEN)==0);
  cs_evm_start_controller(evm, (cs_clockv) 1);

  /* Handler T1*/
  if(cs_evm_install_handler((cs_event_type_t) EV_MSG, h_t1, evm)!=0){
    fprintf(stderr, "\nError setting event handler");
    exit(-1);
  }

  /* The array of travelling bags for the workers */
  travel_bag_t *bags_arr = (travel_bag_t *) malloc(sizeof(travel_bag_t)*nw);

  //set common event data
  sim_data->evm = evm;
  sim_data->al = al;
  sim_data->r = r;
  sim_data->ttl = ttl;
  sim_data->clock = clock;

  for(i=0; i<nw; i++){
    bags_arr[i].r = r;
    bags_arr[i].al = al;
    bags_arr[i].ttl = ttl;
    bags_arr[i].nmsg = (i==nw-1 ? n+nmsg%nw : n);
    bags_arr[i].tindex = i;
    bags_arr[i].nnodes = nnodes;
    bags_arr[i].log = log;
    bags_arr[i].clock = clock;
    bags_arr[i].sim_data = sim_data; //this is common data for events
    bags_arr[i].evm = evm;
  }

  if(!(wp= (cs_workerpool_t *) malloc(sizeof(cs_workerpool_t)))){
    log4c_category_log(log, LOG4C_PRIORITY_ERROR, "Unable to malloc the worker pool ");
    return;
  }

  if(cs_wp_init(nw, nw, wp)<0 || cs_wp_start(wp)<0){
    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to init the worker pool ");
    return;
  }

  for(i=0; i<nw; i++){
    if((tid[i] = cs_wp_tsk_enqueue(simul_travel_task_with_events, &bags_arr[i], sizeof(bags_arr[i]), wp))<0)
      log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to enqueue task 1");
    else
      log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Task %li enqueued", tid[i]);
  }

  for(i=0; i<nw; i++)
    cs_wp_tsk_wait(wp, tid[i]);

  /* Now events (messages) have been scheduled, the event manager 
    will start the flow of events processing and scheduling */
  
  while(cs_evm_more_processing(evm)){
    log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Clock: %li, syncing..", cs_get_clock(clock));
    cs_time_sync(clock);
  }

  cs_time_stop(clock);

  /* release memory */
  //cs_evm_stop_controller(evm);
  cs_destroy_event_manager(evm);

  fprintf(stderr, "\nMsgs %d: s=%li, f=%li, tot_hops=%li", nmsg, sim_data->s, sim_data->f, sim_data->hops);

  log4c_fini();
}

/*
* 
*/
void simulate_travel(
  igraph_adjlist_t *al,
  long int nnodes,
  gsl_rng * r,
  int nmsg,
  int nw,
  int ttl,
  log4c_category_t *log){

  int i;
  int n = floor(nmsg/nw);
  cs_wp_tsk_id tid[nw];
  cs_workerpool_t* wp;
  cs_wp_tsk_res *res;

  log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "nw=%d", nw);

  /* The array of travelling bags for the workers */
  travel_bag_t *bags_arr = (travel_bag_t *) malloc(sizeof(travel_bag_t)*nw);

  for(i=0; i<nw; i++){
    bags_arr[i].r = r;
    bags_arr[i].al = al;
    bags_arr[i].ttl = ttl;
    bags_arr[i].nmsg = (i==nw-1 ? n+nmsg%nw : n);
    bags_arr[i].tindex = i;
    bags_arr[i].nnodes = nnodes;
    bags_arr[i].log = log;
  }

  if(!(wp= (cs_workerpool_t *) malloc(sizeof(cs_workerpool_t)))){
    log4c_category_log(log, LOG4C_PRIORITY_ERROR, "Unable to malloc the worker pool ");
    return;
  }

  if(cs_wp_init(nw, nw, wp)<0 || cs_wp_start(wp)<0){
    log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to init the worker pool ");
    return;
  }

  for(i=0; i<nw; i++){
    if((tid[i] = cs_wp_tsk_enqueue(simul_travel_task, &bags_arr[i], sizeof(bags_arr[i]), wp))<0)
      log4c_category_log(log, LOG4C_PRIORITY_ERROR, " Unable to enqueue task 1");
    else
      log4c_category_log(log, LOG4C_PRIORITY_NOTICE, "Task %li enqueued", tid[i]);
  }

  for(i=0; i<nw; i++)
    cs_wp_tsk_wait(wp, tid[i]);

  for(i=0; i<nw; i++){
    res = cs_wp_tsk_get_res(wp, tid[i]);
    fprintf(stderr, "\nTask %li: s=%d, f=%d, tot_hops=%li", tid[i], ((simul_travel_task_res_t *) res->tsk_res_data)->s, ((simul_travel_task_res_t *) res->tsk_res_data)->f, ((simul_travel_task_res_t *) res->tsk_res_data)->tot_hops);
  }
}

int main(int argc, char *argv[]){
  igraph_t *g;
  gsl_rng * r = gsl_rng_alloc (gsl_rng_taus);
  gsl_rng_set(r, time(NULL));
  time_t a,b;
  long int nnodes;

  igraph_adjlist_t *al;
  //int i;

  log4c_init();
  log4c_category_t *log = log4c_category_get("cs.test.test_cs_scale_free");

  if(argc!=6 && argc!=8){
    fprintf(stderr, "\n Usage: %s <nnodes> <grow_links> <nmsg> <nworkers> <ttl> [events <nw_events>]\n", argv[0]);
    return -1;
  }

  //generates nodes and graph
  a=time(NULL);
  g = barabasi(atoi(argv[1]), atoi(argv[2]));
  b=time(NULL);
  nnodes=igraph_vcount(g);
  print_graph(g,log);
  //gets the adjacency matrix
  al = (igraph_adjlist_t *) malloc(sizeof(igraph_adjlist_t));
  igraph_adjlist_init(g, al, IGRAPH_ALL);

  free(g);

  if(argv[6]!=NULL && strcasecmp(argv[6], "events")==0 && argv[7]!=NULL)
    simulate_travel_with_events(al, nnodes, r, atoi(argv[3]) /* nmsg*/, /* nworkers */ atoi(argv[4]), atoi(argv[5]) /*TTL*/,\
	 atoi(argv[7]) /* nw_events */, log);
  else
    simulate_travel(al, nnodes, r, /* nmsg */ atoi(argv[3]), /* nworkers */ atoi(argv[4]), atoi(argv[5]) /*TTL*/, log);

  fprintf(stderr, "\n Graph Creation: %.4f \n", (double) (b-a));

  return 0;
}
