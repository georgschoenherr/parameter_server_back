#!/bin/bash -u


#bash file inputs
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"
outpath_tag="KDDLR"

#master input
parm_file=/usr0/home/gschoenh/Dropbox/Work/Projects/ChallengingNetworks/parmFiles/imnetParmsKDDLR.txt

#app-specific inputs
#training
train_file=/usr0/home/gschoenh/BigData/imnet
global_data=false
force_global_file_names=true
num_train_data=480  # interesting
num_epochs=10  #interesting
num_batches_per_epoch=1
ignore_nan=true
#model
lambda=0
learning_rate=0.001
learning_rate_search=false
decay_rate=0.99
lr_and_decay_search=true
sparse_weight=false
add_immediately=true
top_mu=0.01
bottom_mu=0.001
top_decay_rate=0.99
bottom_decay_rate=0.999
#testing
test_file=/usr0/home/gschoenh/BigData/imnet
perform_test=true
num_epochs_per_eval=1
num_test_data=9600
out_cols="Epoch:Train-0-1:Train-Entropy:Train-obj:Test-0-1:Time:waitPercentage"
num_train_eval=4800
num_test_eval=9600
target_error=-1
error_field=6
#checkpoint/restart
use_weight_file=false
weight_file=/tank/projects/biglearning/jinlianw/data/mlr_data/imagenet_llc.weight
num_secs_per_checkpoint=200000
#table
w_table_num_cols=500

# System parameters:
host_filename="../../machinefiles/localserver"
consistency_model="SSPPush"
num_table_threads=4
num_comm_channels_per_client=1
table_staleness=0

#Obscure System Parms:
bg_idle_milli=2
# Total bandwidth: bandwidth_mbps * num_comm_channels_per_client * 2
client_bandwidth_mbps=0
server_bandwidth_mbps=0
# bandwidth / oplog_push_upper_bound should be > miliseconds.
thread_oplog_batch_size=21504000
server_idle_milli=2
update_sort_policy=Random
row_candidate_factor=5
append_only_buffer_capacity=$((1024*1024*4))
append_only_buffer_pool_size=3
bg_apply_append_oplog_freq=64
client_send_oplog_upper_bound=1000
server_push_row_upper_bound=500
row_oplog_type=0
oplog_type=Dense
process_storage_type=BoundedDense
no_oplog_replay=true
numa_opt=false
numa_policy=Even
naive_table_oplog_meta=false
suppression_on=false
use_approx_sort=false

# Figure out the paths.
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=mlr_main
multiname=mlr_multi
prog_path=$app_dir/mlr/${progname}
multi_path=$app_dir/mlr/${multiname}
system_path=$app_dir/mlr/
host_file=$(readlink -f $host_filename)

# Parse hostfile
host_list=`cat $host_file | awk '{ print $2 }'`
unique_host_list=`cat $host_file | awk '{ print $2 }' | uniq`
num_unique_hosts=`cat $host_file | awk '{ print $2 }' | uniq | wc -l`
host_list=`cat $host_file | awk '{ print $2 }'`
num_hosts=`cat $host_file | awk '{ print $2 }' | wc -l`

output_dir="${app_dir}/mlr/output"
#output_dir="${output_dir}/mlr.${outpath_tag}.S${table_staleness}.E${num_epochs}"
#output_dir="${output_dir}.M${num_unique_hosts}"
#output_dir="${output_dir}.T${num_table_threads}"
#output_dir="${output_dir}.B${num_batches_per_epoch}.${consistency_model}.${learning_rate}_full"

output_file_prefix=$output_dir/$outpath_tag  # prefix for program outputs
log_dir=$output_dir/${outpath_tag}_logs
stats_path=${output_dir}/${outpath_tag}_mlr_stats.yaml
echo $stats_path

# Kill previous instances of this program
echo "Killing previous instances of '$multiname' on servers, please wait..."
for ip in $unique_host_list; do
    echo "killing ".$ip
  ssh $ssh_options $ip \
    killall -q $multiname
done
echo "All done!"
# exit

# Spawn program instances
client_id=0
for ip in $host_list; do
  echo Running master process on $ip
  log_path=${log_dir}.${client_id}

  numa_index=$(( client_id%num_unique_hosts ))

  cmd="rm -rf ${log_path}; mkdir -p ${log_path}; \
GLOG_logtostderr=true \
    GLOG_log_dir=$log_path \
      GLOG_v=-1 \
      GLOG_minloglevel=0 \
      GLOG_vmodule="" \
      $multi_path \
    --stats_path ${stats_path}\
    --num_clients $num_hosts \
    --num_comm_channels_per_client $num_comm_channels_per_client \
    --init_thread_access_table=false \
    --num_table_threads ${num_table_threads} \
    --client_id $client_id \
    --hostfile ${host_file} \
    --consistency_model $consistency_model \
    --client_bandwidth_mbps $client_bandwidth_mbps \
    --server_bandwidth_mbps $server_bandwidth_mbps \
    --bg_idle_milli $bg_idle_milli \
    --thread_oplog_batch_size $thread_oplog_batch_size \
    --row_candidate_factor ${row_candidate_factor}
    --server_idle_milli $server_idle_milli \
    --update_sort_policy $update_sort_policy \
    --numa_opt=${numa_opt} \
    --numa_index ${numa_index} \
    --numa_policy ${numa_policy} \
    --naive_table_oplog_meta=${naive_table_oplog_meta} \
    --suppression_on=${suppression_on} \
    --use_approx_sort=${use_approx_sort} \
    --table_staleness $table_staleness \
    --row_type 0 \
    --row_oplog_type ${row_oplog_type} \
    --oplog_dense_serialized \
    --oplog_type ${oplog_type} \
    --append_only_oplog_type DenseBatchInc \
    --append_only_buffer_capacity ${append_only_buffer_capacity} \
    --append_only_buffer_pool_size ${append_only_buffer_pool_size} \
    --bg_apply_append_oplog_freq ${bg_apply_append_oplog_freq} \
    --process_storage_type ${process_storage_type} \
    --no_oplog_replay=${no_oplog_replay} \
    --client_send_oplog_upper_bound ${client_send_oplog_upper_bound} \
    --server_push_row_upper_bound ${server_push_row_upper_bound} \
    --parm_file=$parm_file \
    --train_file=$train_file \
    --global_data=$global_data \
    --force_global_file_names=$force_global_file_names \
    --num_train_data=$num_train_data \
    --num_epochs=$num_epochs \
    --num_batches_per_epoch=$num_batches_per_epoch \
    --ignore_nan=$ignore_nan \
    --lambda=$lambda \
    --learning_rate=$learning_rate \
    --learning_rate_search=$learning_rate_search \
    --decay_rate=$decay_rate \
    --lr_and_decay_search=$lr_and_decay_search \
    --sparse_weight=${sparse_weight} \
    --add_immediately=${add_immediately} \
    --top_mu=$top_mu \
    --bottom_mu=$bottom_mu \
    --top_decay_rate=$top_decay_rate \
    --bottom_decay_rate=$bottom_decay_rate \
    --test_file=$test_file \
    --perform_test=$perform_test \
    --num_epochs_per_eval=$num_epochs_per_eval \
    --num_test_data=$num_test_data \
    --out_cols=$out_cols \
    --num_train_eval=$num_train_eval \
    --num_test_eval=$num_test_eval \
    --target_error=$target_error \
    --error_field=$error_field \
    --use_weight_file=$use_weight_file \
    --weight_file=$weight_file \
    --num_secs_per_checkpoint=${num_secs_per_checkpoint} \
    --w_table_num_cols=$w_table_num_cols \
    --output_file_prefix=$output_file_prefix \
    --prog_path=$prog_path \
    --system_path=$system_path"

  ssh $ssh_options $ip $cmd &
  #eval $cmd  # Use this to run locally (on one machine).
  #echo $cmd   # echo the cmd for just the first machine.
  #exit

  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    #echo $cmd   # echo the cmd for just the first machine.
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
  exit
done
