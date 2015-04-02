#!/bin/bash -u

data_filename="/home/jinliang/data/matrixfact_data/data_4K_2K_X.dat.bin.1"
host_filename="../../machinefiles/localserver"

K=40
init_step_size=0.1
step_dec=0.985
lambda=0.05

num_iterations=100
num_iters_per_eval=1
num_workers=1
refresh_freq=-1

# Find other Petuum paths by using the script's path
app_dir=`readlink -f $0 | xargs dirname | xargs dirname`
progname=adagrad_mf
prog_path=$app_dir/bin/$progname
data_file=`readlink -f $data_filename`

loss_file=output_${progname}
loss_file=${loss_file}_${init_step_size}
loss_file=${loss_file}_${num_workers}
loss_file=${loss_file}_${refresh_freq}_${num_iterations}.loss

rm -rf ${loss_file}

GLOG_logtostderr=true \
    GLOG_v=-1 \
    GLOG_minloglevel=0 \
    $prog_path \
    --init_step_size $init_step_size \
    --lambda $lambda \
    --K $K \
    --datafile $data_file \
    --num_iters $num_iterations \
    --num_iters_per_eval $num_iters_per_eval \
    --num_workers $num_workers \
    --refresh_freq ${refresh_freq} \
    --loss_file ${loss_file}
