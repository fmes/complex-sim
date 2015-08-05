#!/bin/bash
set -x

if [[ $# < 2 ]]; then
	echo "Usage: `basename $0` <dir> <pattern> [antipattern]"
	exit
fi

dir=$1
pattern=$2

if [[ ! -z $3 ]]; then
  antipattern=$3
  cmd="ls -1 $dir | grep  -E \"$pattern\" | grep -v -E \"$antipattern\" | grep -v file_list"
else
  cmd="ls -1 $dir | grep  -E \"$pattern\" | grep -v file_list"
fi

absdir=$(cd $dir; pwd);
flist=$absdir/file_list
echo -n "" > $flist
echo $cmd
for i in `eval "$cmd"`; do
    echo $absdir/$i >> $flist
	
done
