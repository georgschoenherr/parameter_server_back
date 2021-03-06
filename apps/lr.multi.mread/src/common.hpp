// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.07.14

#pragma once

#include <gflags/gflags.h>
#include <cstdint>

// Globally accessible gflags.
DECLARE_double(init_step_size);
DECLARE_double(step_size_decay);
DECLARE_int32(num_train_data);
DECLARE_int32(feature_dim);
DECLARE_string(train_file);
DECLARE_bool(global_data);
DECLARE_string(test_file);
DECLARE_int32(num_train_eval);
DECLARE_int32(num_test_eval);
DECLARE_bool(perform_test);

DECLARE_int32(num_epochs);
DECLARE_int32(num_batches_per_epoch);
DECLARE_int32(num_epochs_per_eval);
DECLARE_bool(sparse_weight);
DECLARE_double(lambda);

DECLARE_string(output_file_prefix);
DECLARE_int32(num_secs_per_checkpoint);
DECLARE_int32(w_table_num_cols);

DECLARE_int32(num_labels);
DECLARE_string(data_format);
DECLARE_bool(feature_one_based);
DECLARE_bool(label_one_based);
DECLARE_bool(snappy_compressed);
DECLARE_bool(clock_per_minibatch);
DECLARE_int32(num_files_per_client);
DECLARE_int32(num_test_files_per_client);
DECLARE_int32(test_file_st_index);
DECLARE_int32(file_skip);

const int32_t kWTableID = 0;
const int32_t kLossTableID = 1;

namespace mlr {

const int32_t kColIdxLossTableEpoch = 0;
const int32_t kColIdxLossTableBatch = 1;
const int32_t kColIdxLossTableZeroOneLoss = 2;    // training set
const int32_t kColIdxLossTableEntropyLoss = 3;    // training set
const int32_t kColIdxLossTableNumEvalTrain = 4;    // # train point eval
const int32_t kColIdxLossTableTestZeroOneLoss = 5;
const int32_t kColIdxLossTableNumEvalTest = 6;  // # test point eval.
const int32_t kColIdxLossTableTime = 7;
const int32_t kColIdxLossTableRegLoss = 8;    // reg loss (with lambda)

const int32_t kNumColumnsLossTable = 9;

}  // namespace mlr
