#include <petuum_ps/server/adarevision_server_table_logic.hpp>
#include <gflags/gflags.h>
#include <petuum_ps_common/storage/dense_row.hpp>

DEFINE_double(init_step_size, 0.1, "init step size");
DEFINE_uint64(old_grad_upper_bound, 10000, "gradient upper bound");
DEFINE_string(random_init, "guassian", "initialize server row");

namespace petuum {

AdaRevisionServerTableLogic::~AdaRevisionServerTableLogic() {
  if (gen_) delete gen_;
  if (dist_) delete dist_;
}

void AdaRevisionServerTableLogic::Init(const TableInfo &table_info,
                                       ApplyRowBatchIncFunc RowBatchInc) {
  table_info_ = table_info;
  init_step_size = FLAGS_init_step_size;

  deltas_.resize(table_info.row_capacity, 0);
  col_ids_.resize(table_info.row_capacity, 0);
  for (int i = 0; i < col_ids_.size(); ++i) {
    col_ids_[i] = i;
  }

  if (FLAGS_random_init == "guassian") {
    //LOG(INFO) << "use guassian init";
    gen_ = new std::mt19937(12345);
    dist_ = new std::normal_distribution<float>(0, 0.1);
  }
  RowBatchInc_ = RowBatchInc;
}

void AdaRevisionServerTableLogic::ServerRowCreated(int32_t row_id,
                                                   ServerRow *server_row) {
  adarevision_info_.insert(
      std::make_pair(row_id, AdaRevisionRow(table_info_.row_capacity)));

  if (FLAGS_random_init == "guassian") {
    for (int i = 0; i < table_info_.row_capacity; ++i) {
      deltas_[i] = (*dist_)((*gen_));
    }
    RowBatchInc_(col_ids_.data(), deltas_.data(), table_info_.row_capacity,
                 server_row);
  }
}

void AdaRevisionServerTableLogic::ApplyRowOpLog(
    int32_t row_id,
    const int32_t *col_ids, const void *updates,
    int32_t num_updates, ServerRow *server_row,
    uint64_t row_version, bool end_of_version) {

  //LOG(INFO) << "row id = " << row_id
  //        << " row version = " << row_version
  //        << " end of version = " << end_of_version;

  auto adarev_iter = adarevision_info_.find(row_id);
  CHECK(adarev_iter != adarevision_info_.end());
  auto &adarev_row = adarev_iter->second;
  CHECK(num_updates == table_info_.row_capacity);

  if (row_version == 0) {
    float old_accum_grad = 0;
    const float *updates_float
        = reinterpret_cast<const float *>(updates);

    for (int i = 0; i < num_updates; ++i) {
      float g_bck = adarev_row.accum_gradients_[i] - old_accum_grad;
      float eta_old = init_step_size / sqrt(adarev_row.z_max_[i]);
      adarev_row.z_[i] += updates_float[i] * updates_float[i] + 2 * updates_float[i] * g_bck;
      adarev_row.z_max_[i] = std::max(adarev_row.z_[i], adarev_row.z_max_[i]);
      float eta = init_step_size / sqrt(adarev_row.z_max_[i]);
      float delta = -(eta * updates_float[i]) + (eta_old - eta) * g_bck;
      //row_data->ApplyIncUnsafe(i, &delta);
      adarev_row.accum_gradients_[i] += updates_float[i];
      deltas_[i] = delta;
      CHECK(delta == delta);
    }
  } else {
    auto old_accum_grad_iter
        = old_accum_gradients_.find(std::make_pair(row_id, row_version));
    CHECK(old_accum_grad_iter != old_accum_gradients_.end());

    auto &old_accum_grad = old_accum_grad_iter->second.first;
    auto &old_accum_grad_client_count
        = old_accum_grad_iter->second.second;

    // assuming dense row with float
    const float *updates_float
        = reinterpret_cast<const float *>(updates);

    for (int i = 0; i < num_updates; ++i) {
      float g_bck = adarev_row.accum_gradients_[i] - old_accum_grad[i];
      float eta_old = init_step_size / sqrt(adarev_row.z_max_[i]);
      adarev_row.z_[i] += updates_float[i] * updates_float[i] + 2 * updates_float[i] * g_bck;
      adarev_row.z_max_[i] = std::max(adarev_row.z_[i], adarev_row.z_max_[i]);
      float eta = init_step_size / sqrt(adarev_row.z_max_[i]);
      float delta = -(eta * updates_float[i]) + (eta_old - eta) * g_bck;
      //row_data->ApplyIncUnsafe(i, &delta);
      adarev_row.accum_gradients_[i] += updates_float[i];
      deltas_[i] = delta;
      CHECK(delta == delta);
    }

    if (end_of_version) {
      old_accum_grad_client_count--;
      if (old_accum_grad_client_count == 0) {
        old_accum_gradients_.erase(old_accum_grad_iter);
      }
    }
  }

  RowBatchInc_(col_ids_.data(), reinterpret_cast<void*>(deltas_.data()),
               num_updates, server_row);
}

void AdaRevisionServerTableLogic::ServerRowSent(
    int32_t row_id, uint64_t version, size_t num_clients) {
  CHECK(num_clients > 0);
  auto adarev_iter = adarevision_info_.find(row_id);
  CHECK(adarev_iter != adarevision_info_.end());
  auto &adarev_row = adarev_iter->second;
  old_accum_gradients_.insert(
      std::make_pair(
          std::make_pair(row_id, version),
          std::make_pair(adarev_row.accum_gradients_, num_clients)));
}

bool AdaRevisionServerTableLogic::AllowSend() {
  return old_accum_gradients_.size() < FLAGS_old_grad_upper_bound;
}

}
