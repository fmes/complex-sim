#!/bin/bash

CORE_MAX=8
CORE_MIN=5
DX_MAX=0.001 #E-3
DX_MIN=0.000001 #E-6
DX_STEP=0.1

machine=`hostname -f`
#nc="1 3 5 7"
nc="2 4 6 8"
#dx=`seq $DX_MIN $DX_STEP $DX_MAX`
dx="0.001 0.0001 0.00001 0.000001"

if [[ ! -z $1 ]]; then 
    ts=$1
else
  ts=`date +%s`
fi

echo "dx: [$dx], nc: [$nc]"

bench_file=bench.$ts.$machine.txt

for c in $nc;do
  a=$a" $c"
done
echo "%dx/ncore $a" > $bench_file

for d in $dx; do
  secs=""
  for c in $nc; do
    echo "NCORE=$c"
    echo "DX=$d"
    /usr/bin/time -o `dirname $0`/bench.tmp.txt -f %e `dirname $0`/../test_build/test_heigen $d $c
    secs=$secs" `cat $(dirname $0)/bench.tmp.txt`"
  done
  echo "%$d"$secs >> $bench_file
done
