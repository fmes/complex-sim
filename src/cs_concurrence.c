/* Copyright (c) 2012, Fabrizio Messina, University of Catania
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "cs_psk.h"
#include "cs_concurrence.h"

/**
* Creates a recursive mutex 
* (PTHREAD_MUTEX_RECURSIVE_NP).
*/
pthread_mutex_t *cs_make_recursive_mutex()
{
    pthread_mutex_t *m =
	(pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutexattr_t *attr;
    attr = (pthread_mutexattr_t *) calloc(1, sizeof(pthread_mutexattr_t));
    pthread_mutexattr_settype(attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(m, attr);	//default attributes, i.e., fast mutex
    free(attr);
    return m;
}

/*
* Creates a standard (non-recursive) mutex. 
*/
pthread_mutex_t *cs_make_mutex()
{
    pthread_mutex_t *m =
	(pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);	//default attributes, i.e., fast mutex
    return m;
}
