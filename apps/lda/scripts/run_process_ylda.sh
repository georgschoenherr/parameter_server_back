#!/bin/bash

data_file=/proj/BigLearning/jinlianw/data/lda_data/yahoo_lda_format
output_file=/proj/BigLearning/jinlianw/data/lda_data/clueweb_ylda_trim_10B.partition.2/ylda
#data_file=/proj/BigLearning/jinlianw/data/lda_data/nytimes/nytimes.ylda
#output_file=/proj/BigLearning/jinlianw/data/lda_data/nytimes/nytimes.ylda.dat


script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`

rm -rf $output_file*

mkdir -p log

cmd="GLOG_logtostderr=true \
GLOG_log_dir=log \
${project_root}/bin/process_ylda \
--data_file=$data_file  \
--output_file=$output_file"

eval $cmd
echo $cmd

# creating backup in case disk stream corrupts the db.
#for i in $(seq 0 $((num_partitions-1)))
#do
#  echo copying $output_file.$i to $output_file.$i.bak
#  cp -r $output_file.$i $output_file.$i.bak
#done
