#!/bin/bash

# Assume each C or S file is standalone

C_FILES=`find . -name "*.c"`
S_FILES=`find . -name "*.S"`

for cfile in $C_FILES; do
  out_name=${cfile%.*}
  gcc -g -no-pie $cfile -o $out_name -static
done

for sfile in $S_FILES; do
  out_name=${sfile%.*}
  as $sfile -o $out_name.o
done