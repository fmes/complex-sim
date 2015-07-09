/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _CS_CONCURRENCE
#define _CS_CONCURRENCE

#include <pthread.h>

/**
* Build a recursive mutex. It is responsability of the 
* user to destroy the mutex and thereafter free the 
* memory pointed by the pthread_mutex_it object. 
* @return a recursive mutex variable 
*/
pthread_mutex_t *cs_make_recursive_mutex();
/**
* Build a non-recursive mutex. It is responsability of the 
* user to destroy the mutex and thereafter free the 
* memory pointed by the pthread_mutex_it object.
* @return a non-recursive mutex variable
*/
pthread_mutex_t *cs_make_mutex();

#define _LOCK_MUTEX(m) (pthread_mutex_lock(m))
#define _UNLOCK_MUTEX(m) (pthread_mutex_unlock(m))

#endif
