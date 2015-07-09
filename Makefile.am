lib_LTLIBRARIES=libavl.la libcomplexsim.la 

libavl_la_SOURCES=avl-2.0/src/avl.c avl-2.0/src/pavl.c avl-2.0/src/rb.c avl-2.0/src/rtrb.c avl-2.0/src/tbst.c avl-2.0/src/bst.c avl-2.0/src/pbst.c avl-2.0/src/rtavl.c avl-2.0/src/slr.c avl-2.0/src/trb.c avl-2.0/src/bsts.c avl-2.0/src/prb.c avl-2.0/src/rtbst.c avl-2.0/src/tavl.c
libavl_la_LDFLAGS=-shared
libavl_la_CFLAGS=-Iavl-2.0/include

libcomplexsim_la_SOURCES=src/cs_concurrence.c  src/cs_queue.c  src/cs_workerpool.c src/cs_events.c src/cs_timer.c src/cs_engine.c 
libcomplexsim_la_LIBADD=libavl.la
libcomplexsim_la_LDFLAGS=-shared
libcomplexsim_la_CFLAGS=-Iinclude -Iavl-2.0/include

include_HEADERS=include/cs_engine.h include/complex_sim.h include/cs_psk.h include/cs_workerpool.h include/cs_concurrence.h include/cs_queue.h include/cs_events.h include/cs_timer.h avl-2.0/include/avl.h avl-2.0/include/pbst.h  avl-2.0/include/rtavl.h  avl-2.0/include/tavl.h  avl-2.0/include/trb.h avl-2.0/include/bst.h avl-2.0/include/prb.h avl-2.0/include/rtbst.h  avl-2.0/include/tbst.h avl-2.0/include/pavl.h avl-2.0/include/rb.h avl-2.0/include/rtrb.h avl-2.0/include/test.h

ACLOCAL_AMFLAGS=-I m4

#test programs
bin_PROGRAMS = test_cs_events test_cs_engine test_cs_workerpool sample_engine_event_driven sample_engine_activity_driven
test_cs_events_SOURCES=test/test_cs_events.c
test_cs_engine_SOURCES=test/test_cs_engine.c
test_cs_workerpool_SOURCES=test/test_cs_workerpool.c
sample_engine_event_driven_SOURCES=samples/sample_engine_event_driven.c
sample_engine_activity_driven_SOURCES=samples/sample_engine_activity_driven.c
#test_wp_burst_SOURCES=test/test_wp_burst.c
#test_wp_burst_SOURCES=test/test_wp_burst.c
#test_cs_events_LDADD=libcomplexsim.la libavl.la
test_cs_events_LDFLAGS=-L.libs -lcomplexsim -lavl
test_cs_engine_LDFLAGS=-L.libs -lcomplexsim -lavl
test_cs_workerpool_LDFLAGS=-L.libs -lcomplexsim -lavl
sample_engine_event_driven_LDFLAGS=-L.libs -lcomplexsim -lavl
sample_engine_activity_driven_LDFLAGS=-L.libs -lcomplexsim -lavl
#test_wp_burst_LDFLAGS=-L.libs -lcomplexsim -lavl
#test_cs_events_CFLAGS=-l
CFLAGS+=-g -Iinclude -Iavl-2.0/include
