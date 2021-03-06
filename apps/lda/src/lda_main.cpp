// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.03.25

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <petuum_ps_common/include/system_gflags_declare.hpp>
#include <petuum_ps_common/include/table_gflags_declare.hpp>
#include <petuum_ps_common/include/init_table_group_config.hpp>
#include <petuum_ps_common/include/init_table_config.hpp>
#include "lda_engine.hpp"
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>
#include "common.hpp"
#include "corpus.hpp"
#include <iostream>
#include <fstream>

//LDA parms
//data
DEFINE_string(doc_file, "", "File containing document in LibSVM format. Each document is a line.");
DEFINE_int32(num_vocabs, -1, "Number of vocabs.");
DEFINE_int32(max_vocab_id, -1, "Maximum word index, which could be different from num_vocabs if there are unused vocab indices.");
DEFINE_int32(num_topics, 100, "Number of topics.");
//model
DEFINE_double(alpha, 1, "Dirichlet prior on document-topic vectors.");
DEFINE_double(beta, 0.1, "Dirichlet prior on vocab-topic vectors.");
//network
DEFINE_int32(communication_factor, -1, "The factor by which communication is artificially delayed");
DEFINE_int32(virtual_staleness, -1, "Artificial staleness");
DEFINE_bool(is_bipartite, false, "bipartite topology in virtual staleness");
DEFINE_bool(is_local_sync, false, "local styncing in bipartite topology in virtual staleness");
//training
DEFINE_int32(num_work_units, 1, "Number of work units");
DEFINE_int32(compute_ll_interval, 1, "Compute log likelihood over local dataset on every N iterations");
DEFINE_int32(num_iters_per_work_unit, 1, "number of iterations per work unit");
DEFINE_int32(num_clocks_per_work_unit, 1, "number of clocks per work unit");
DEFINE_int32(seed, 0, "random seed for sampling topics. It is not used for initialization");
DEFINE_bool(safe_llh, false, "if true, use the correct global model for llh");

// System Parameters
DEFINE_uint64(word_topic_table_process_cache_capacity, 100000, "Word topic table process cache capacity");

// paths
DEFINE_string(output_file_prefix, "", "LDA results.");
DEFINE_string(signal_file_path, "", "signal_file_path");

// No need to change the following.
DEFINE_int32(word_topic_table_id, 1, "ID within Petuum-PS");
DEFINE_int32(summary_table_id, 2, "ID within Petuum-PS");
DEFINE_int32(llh_table_id, 3, "ID within Petuum-PS");
DEFINE_int32(word_topic_table_global_id, 4, "ID within Petuum-PS"); 
DEFINE_int32(summary_table_global_id, 5, "ID within Petuum-PS"); 

int32_t kSortedVectorMapRowTypeID = 1;
int32_t kDenseRowIntTypeID = 2;
int32_t kDenseRowDoubleTypeID = 3;

template<typename type>
void wls(std::string path, std::vector<type> input, std::ios_base::openmode mode)
{
	std::ofstream stream;
	stream.open(path, mode);
	CHECK(!stream.fail()) << "output file not found";
	int size = (int)input.size();
	for(int i=0;i<size;i++)
	{
		type row = input[i];
		if(i!=0)
			stream << '\n';
		stream << row;
	}
	stream.close();
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "LDA starts here! dense serialize = " << FLAGS_oplog_dense_serialized;

  if(true)
  {
    LOG(INFO) << "doc_file " << FLAGS_doc_file;
    LOG(INFO) << "num_vocabs " << FLAGS_num_vocabs;
    LOG(INFO) << "max_vocab_id " << FLAGS_max_vocab_id;
    LOG(INFO) << "num_topics " << FLAGS_num_topics;
    LOG(INFO) << "alpha " << FLAGS_alpha;
    LOG(INFO) << "beta " << FLAGS_beta;
    LOG(INFO) << "communication_factor " << FLAGS_communication_factor;
    LOG(INFO) << "virtual_staleness " << FLAGS_virtual_staleness;
    LOG(INFO) << "is_bipartite " << FLAGS_is_bipartite;
    LOG(INFO) << "is_local_sync " << FLAGS_is_local_sync;
    LOG(INFO) << "num_work_units " << FLAGS_num_work_units;
    LOG(INFO) << "compute_ll_interval " << FLAGS_compute_ll_interval;
    LOG(INFO) << "num_iters_per_work_unit " << FLAGS_num_iters_per_work_unit;
    LOG(INFO) << "num_clocks_per_work_unit " << FLAGS_num_clocks_per_work_unit;
    LOG(INFO) << "seed " << FLAGS_seed;
    LOG(INFO) << "safe_llh " << FLAGS_safe_llh;
    LOG(INFO) << "word_topic_table_process_cache_capacity " << FLAGS_word_topic_table_process_cache_capacity;
    LOG(INFO) << "output_file_prefix " << FLAGS_output_file_prefix;
    LOG(INFO) << "stats_path " << FLAGS_stats_path;
    LOG(INFO) << "signal_file_path " << FLAGS_signal_file_path;
    LOG(INFO) << "table_staleness " << FLAGS_table_staleness;
    LOG(INFO) << "consistency_model " << FLAGS_consistency_model;
    LOG(INFO) << "client_bandwidth_mbps " << FLAGS_client_bandwidth_mbps;
    LOG(INFO) << "server_bandwidth_mbps " << FLAGS_server_bandwidth_mbps;
    LOG(INFO) << "num_comm_channels_per_client " << FLAGS_num_comm_channels_per_client;
    LOG(INFO) << "num_table_threads " << FLAGS_num_table_threads;
    LOG(INFO) << "hostfile " << FLAGS_hostfile;
  }

  //some checks
  CHECK(FLAGS_communication_factor == -1 || FLAGS_communication_factor >= 1) << "bad range for communication factor";
  CHECK(FLAGS_virtual_staleness == -1 || FLAGS_virtual_staleness >= 1) << "bad range for virtual staleness";
  CHECK(FLAGS_virtual_staleness <= FLAGS_communication_factor) << "must have staleness < comm factor";
  CHECK(FLAGS_virtual_staleness * FLAGS_communication_factor > 0) << "must have virtual staleness and communication factor simultaneously";
  CHECK(FLAGS_communication_factor != -1 || !FLAGS_is_bipartite) << "cannot have communication_factor == -1 and is_bipartite";
  CHECK(!FLAGS_is_local_sync || FLAGS_is_bipartite) << "cannot have local_sync and not is_bipartite";
  CHECK(FLAGS_communication_factor == -1 || FLAGS_num_vocabs == FLAGS_max_vocab_id + 1) << "cannot have empty vocab ids for virtual staleness";
  CHECK(FLAGS_communication_factor == -1 || FLAGS_num_clocks_per_work_unit == 1) << "does not supported fractional iterations with virtual staleness";
  CHECK(FLAGS_communication_factor == -1 || FLAGS_table_staleness == 0) << "cannot have virtual staleness and actual staleness";
  CHECK(FLAGS_communication_factor != -1 || !FLAGS_safe_llh) << "cannot have safe_llh without virtual staleness";

  // Read in data first to get # of vocabs in this partition.
  petuum::TableGroupConfig table_group_config;
  // doc-topic table, summary table, llh table.
  petuum::InitTableGroupConfig(&table_group_config, 5);

  petuum::PSTableGroup::RegisterRow<petuum::SortedVectorMapRow<int32_t> >
      (kSortedVectorMapRowTypeID);
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<int32_t> >
    (kDenseRowIntTypeID);
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<double> >
    (kDenseRowDoubleTypeID);

  LOG(INFO) << "Starting to Init PS";

  // Don't let main thread access table API.
  petuum::PSTableGroup::Init(table_group_config, false);

  STATS_SET_APP_DEFINED_ACCUM_SEC_NAME("sole_compute_sec");
  STATS_SET_APP_DEFINED_VEC_NAME("work_unit_sec");
  STATS_SET_APP_DEFINED_ACCUM_VAL_NAME("nonzero_entries");

  lda::LDAEngine* lda_engine;
  if(FLAGS_virtual_staleness != -1)
    lda_engine = new lda::LDAEngine(0, FLAGS_seed);
  else
    lda_engine = new lda::LDAEngine(time(NULL), FLAGS_seed);
  LOG(INFO) << "Loading data";
  STATS_APP_LOAD_DATA_BEGIN();
  lda_engine->ReadData(FLAGS_doc_file);
  STATS_APP_LOAD_DATA_END();

  LOG(INFO) << "Read data done!";
  LOG(INFO) << "LDA starts here! dense serialize = " << FLAGS_oplog_dense_serialized;

  //word-topic table
  petuum::ClientTableConfig wt_table_config;
  petuum::InitTableConfig(&wt_table_config);
  wt_table_config.table_info.row_capacity = FLAGS_num_topics;
  wt_table_config.table_info.dense_row_oplog_capacity = FLAGS_num_topics;
  wt_table_config.process_cache_capacity =
      FLAGS_word_topic_table_process_cache_capacity;
  wt_table_config.thread_cache_capacity = 1;
  wt_table_config.oplog_capacity = FLAGS_word_topic_table_process_cache_capacity;
  wt_table_config.table_info.row_type = kSortedVectorMapRowTypeID;
  CHECK(petuum::PSTableGroup::CreateTable(
      FLAGS_word_topic_table_id, wt_table_config)) << "Failed to create word-topic table";

  LOG(INFO) << "Created word-topic table";

  //word-topic table global
  petuum::ClientTableConfig wt_table_config_global;
  petuum::InitTableConfig(&wt_table_config_global);
  wt_table_config_global.table_info.row_capacity = FLAGS_num_topics;
  wt_table_config_global.table_info.dense_row_oplog_capacity = FLAGS_num_topics;
  wt_table_config_global.process_cache_capacity = FLAGS_max_vocab_id + 1;
  wt_table_config_global.thread_cache_capacity = 1;
  wt_table_config_global.oplog_capacity = FLAGS_max_vocab_id + 1;
  wt_table_config_global.table_info.row_type = kSortedVectorMapRowTypeID;
  CHECK(petuum::PSTableGroup::CreateTable(
      FLAGS_word_topic_table_global_id, wt_table_config_global)) << "Failed to create global word-topic table";

  LOG(INFO) << "Created global word-topic table";

  // Summary row table (single_row).
  petuum::ClientTableConfig summary_table_config;
  petuum::InitTableConfig(&summary_table_config);
  summary_table_config.table_info.row_capacity = FLAGS_num_topics;
  summary_table_config.table_info.dense_row_oplog_capacity = FLAGS_num_topics;
  summary_table_config.process_storage_type = petuum::BoundedSparse;
  summary_table_config.oplog_type = petuum::Sparse;
  summary_table_config.process_cache_capacity = 1;
  summary_table_config.thread_cache_capacity = 1;
  summary_table_config.oplog_capacity = 1;
  summary_table_config.table_info.row_type = kDenseRowIntTypeID;
  summary_table_config.client_send_oplog_upper_bound = 1;
  summary_table_config.table_info.server_push_row_upper_bound = 1;
  CHECK(petuum::PSTableGroup::CreateTable(
      FLAGS_summary_table_id, summary_table_config)) << "Failed to create summary table";

  LOG(INFO) << "Created summary table";

  // Summary row table (single_row) global.
  petuum::ClientTableConfig summary_table_config_global;
  petuum::InitTableConfig(&summary_table_config_global);
  summary_table_config_global.table_info.row_capacity = FLAGS_num_topics;
  summary_table_config_global.table_info.dense_row_oplog_capacity = FLAGS_num_topics;
  summary_table_config_global.process_storage_type = petuum::BoundedSparse;
  summary_table_config_global.oplog_type = petuum::Sparse;
  summary_table_config_global.process_cache_capacity = 1;
  summary_table_config_global.thread_cache_capacity = 1;
  summary_table_config_global.oplog_capacity = 1;
  summary_table_config_global.table_info.row_type = kDenseRowIntTypeID;
  summary_table_config_global.client_send_oplog_upper_bound = 1;
  summary_table_config_global.table_info.server_push_row_upper_bound = 1;
  CHECK(petuum::PSTableGroup::CreateTable(
      FLAGS_summary_table_global_id, summary_table_config_global)) << "Failed to create global summary table";

  LOG(INFO) << "Created global summary table";

  // Log-likelihood (llh) table. Single column; each column is a complete-llh.
  petuum::ClientTableConfig llh_table_config;
  petuum::InitTableConfig(&llh_table_config);
  llh_table_config.process_cache_capacity = FLAGS_num_work_units*FLAGS_num_iters_per_work_unit;
  llh_table_config.thread_cache_capacity = 1;
  llh_table_config.oplog_capacity = FLAGS_num_work_units*FLAGS_num_iters_per_work_unit;
  llh_table_config.table_info.row_capacity = 3;   // 3 columns: "iter-# llh time".
  llh_table_config.table_info.dense_row_oplog_capacity
      = llh_table_config.table_info.row_capacity;
  llh_table_config.table_info.row_type = kDenseRowDoubleTypeID;
  llh_table_config.process_storage_type = petuum::BoundedSparse;
  llh_table_config.oplog_type = petuum::Sparse;
  llh_table_config.client_send_oplog_upper_bound = 1;
  llh_table_config.table_info.server_push_row_upper_bound = 1;
  CHECK(petuum::PSTableGroup::CreateTable(
      FLAGS_llh_table_id, llh_table_config)) << "Failed to create summary table";

  LOG(INFO) << "Created llh table";

  petuum::PSTableGroup::CreateTableDone();

  // Start LDA
  LOG(INFO) << "Starting LDA with " << FLAGS_num_table_threads << " threads "
            << "on client " << FLAGS_client_id;

  std::vector<std::thread> threads(FLAGS_num_table_threads);
  for (auto& thr : threads) {
    thr = std::thread(&lda::LDAEngine::Start, std::ref(*lda_engine));
  }
  for (auto& thr : threads) {
    thr.join();
  }
  
  delete lda_engine; 
 
  LOG(INFO) << "LDA finished!";
  petuum::PSTableGroup::ShutDown();
  if(FLAGS_signal_file_path != "" && FLAGS_client_id == 0)
    wls<std::string>(FLAGS_signal_file_path, {"done"}, std::ios_base::trunc);
  LOG(INFO) << "LDA shut down!";
  return 0;
}
