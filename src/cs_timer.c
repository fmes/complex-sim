/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "complex_sim.h"
#include "cs_psk.h"
#include "cs_timer.h"

void _cs_init_timer(cs_timer_t *t, const char *id, int n_sync){
  if(!t)
    return;
  t->id = id;
  if(!(t->mutex = cs_make_recursive_mutex()))
    log4c_category_log(log4c_category_get("cs.timer"), LOG4C_PRIORITY_ERROR, "Impossible to make mutex for timer %s", id);
  if(!(t->cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t))))
    log4c_category_log(log4c_category_get("cs.timer"), LOG4C_PRIORITY_ERROR, "Impossible to allocate condition variable for timer %s", id);
  if(pthread_cond_init(t->cond, NULL)!=0)
    log4c_category_log(log4c_category_get("cs.timer"), LOG4C_PRIORITY_ERROR, "Impossible to initiate the condition variable for the timer %s", t->id);
  if(n_sync>0){
    t->n_sync = n_sync;
    t->sync=0; //when (sync==n-sync) time can go ahead
  }
  else
    log4c_category_log(log4c_category_get("cs.timer"), LOG4C_PRIORITY_ERROR, "Parameter n_sync must be greater than zero", t->id);
  t->clock = 0;
  t->stop=0;
}

void cs_init_timer(cs_timer_t *t, const char *id){
  _cs_init_timer(t, id, 2);
}

void cs_set_nsync(cs_timer_t *t, int nsync){
  int old_nsync;
  pthread_mutex_lock(t->mutex);
  old_nsync=t->n_sync;
  t->n_sync = nsync;
  log4c_category_log(log4c_category_get("cs.timer"), LOG4C_PRIORITY_WARN, "Changing number of sync actors for the timer %s (from %d to %d)", 
    t->id, old_nsync, t->n_sync);
  pthread_mutex_unlock(t->mutex);
}

cs_clockv cs_get_clock(cs_timer_t * t){
  cs_clockv ret;
  pthread_mutex_lock(t->mutex);
  ret = t->clock;
  pthread_mutex_unlock(t->mutex);
  return ret;
}

int cs_time_sync(cs_timer_t *t){
  int ret;
  pthread_mutex_lock(t->mutex);
  if(t->stop==0){
    if(t->sync==t->n_sync){
      log4c_category_log(log4c_category_get("cs.timer"), LOG4C_PRIORITY_ERROR, "It seems that more than %d thread are trying to synchronize on this timer!!", t->n_sync);
      pthread_mutex_unlock(t->mutex);
      return -1;
    }
    t->sync++;
    if(t->sync==t->n_sync){
      t->clock++;
      t->sync=0;
      pthread_cond_broadcast(t->cond);
    }
    else
      pthread_cond_wait(t->cond, t->mutex);

    ret = 0;
  }
  else 
    ret=-1;
  pthread_mutex_unlock(t->mutex);
  return ret;
}

void cs_time_stop(cs_timer_t *t){
  pthread_mutex_lock(t->mutex);
  t->stop=1;
  pthread_cond_broadcast(t->cond);
  pthread_mutex_unlock(t->mutex);  
}

void cs_destroy_timer(cs_timer_t * t){
  pthread_mutex_destroy(t->mutex);
  free(t->mutex);
  free(t);
}
