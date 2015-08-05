#!/bin/bash
#set -x

if [[ $# != 2 ]]; then 
  echo "Usage: `basename $0` <dir> <pattern_list_file>"
  exit
fi

dir=$1
filter_pattern_list_file=$2
#file_pattern="file_gen*"
found=1
while read line; do
  for file in `ls $dir/ | grep -E "[[:digit:]]{1,}$"`; do
    #echo "File: $file"
    f=`basename $file`
    found=1
    for p in $line; do
      #echo "Pattern: $p"
      #a line is a list of pattern to be matched on the same file
      grep "$p" $dir/$f >/dev/null 2>&1
      if [[ $? == 1 ]]; then #not found
	found=0
	break
      fi
    done #all pattern have been found on the file
    if [[ $found == 1 ]]; then
      mv -v $dir/$f $dir/$f.filtered
    fi
  done
done < $filter_pattern_list_file
