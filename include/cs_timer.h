/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _CS_TIMER__

#define _CS_TIMER__

#include <pthread.h>

typedef long int cs_clockv;
typedef cs_clockv* cs_clockp;

typedef struct cs_timer_s {
    int stop;
    int n_sync;
    int sync;
    cs_clockv clock;
    pthread_mutex_t *mutex;
    const char *id;
    pthread_cond_t *cond;
}cs_timer_t;

/*
* Init the timer.
* @param timer a valid pointer to the timer to initiate.
* @param name the name of the timer to initiate.
* @param the number of agents which the timer expects to synchronize
*/
void _cs_init_timer(cs_timer_t *timer, const char *name, int nsync);

/**
* Init the timer.
* @param timer a valid pointer to the timer to initiate.
* @param name the name of the timer to initiate.
*/
void cs_init_timer(cs_timer_t *timer, const char *name);

/**
* Read the current value of the clock. 
* @param timer
*/
cs_clockv cs_get_clock(cs_timer_t *timer);
/**
* Release the resources hold by the timer.
* @param timer a valid pointer to the timer
*/
void cs_destroy_timer(cs_timer_t *timer);
/**
* Wait for the next time of clock.
*/
int cs_time_sync(cs_timer_t *timer); 

void cs_time_stop(cs_timer_t *timer);

void cs_set_nsync(cs_timer_t *timer, int nsync);
#endif
