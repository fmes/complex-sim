#!/bin/bash
export PYTHONPATH=$PYTHONPATH:`dirname $0`/../lib/pyth
ex=`basename $0`
/usr/bin/python `dirname $0`/`echo $ex | sed -e 's/\.sh/\.py/'` $@
