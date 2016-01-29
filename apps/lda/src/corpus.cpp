#include "corpus.hpp"
#include "context.hpp"

namespace lda {

Corpus::Corpus(int seed):
  iters_per_work_unit_(1),
  gen_(seed) {
  Context& context = Context::get_instance();
  K_ = context.get_int32("num_topics");
  one_K_dist_.reset(new boost::uniform_int<>(1, K_));
  one_K_rng_.reset(new rng_t(gen_, *one_K_dist_));
}

Corpus::Corpus(time_t seed):
  iters_per_work_unit_(1),
  gen_(seed) {
  Context& context = Context::get_instance();
  K_ = context.get_int32("num_topics");
  one_K_dist_.reset(new boost::uniform_int<>(1, K_));
  one_K_rng_.reset(new rng_t(gen_, *one_K_dist_));
}

void Corpus::AddDoc(const uint8_t *doc_data) {
  docs_.emplace_back();
  
  DocumentWordTopics &last_doc = docs_.back();
  last_doc.DeserializeDoc(doc_data);
  last_doc.RandomInitWordTopics(one_K_rng_.get());

  std::unique_lock<std::mutex> ulock(docs_mtx_);
  docs_iter_vector_.clear();
}

void Corpus::RestartWorkUnit(uint32_t iters_per_work_unit) {
  iters_per_work_unit_ = iters_per_work_unit;

  std::unique_lock<std::mutex> ulock(docs_mtx_);
  docs_iter_ = docs_.begin();
    
  num_iters_this_work_unit_ = 0;
}

std::list<DocumentWordTopics>::iterator Corpus::GetOneDoc() {
  std::unique_lock<std::mutex> ulock(docs_mtx_);
 
  if (num_iters_this_work_unit_ == iters_per_work_unit_) {
    return docs_.end();
  }

  std::list<DocumentWordTopics>::iterator doc_iter = docs_iter_;
  ++docs_iter_;

  if (docs_iter_ == docs_.end()) {
    if (num_iters_this_work_unit_ < (iters_per_work_unit_ - 1)) {
      docs_iter_ = docs_.begin();
    }
    ++num_iters_this_work_unit_;
  }
  return doc_iter;
}

std::list<DocumentWordTopics>::iterator Corpus::GetOneDoc(int32_t *num_iters_this_work_unit) {
  std::unique_lock<std::mutex> ulock(docs_mtx_);
 
  *num_iters_this_work_unit = -1;
  if (num_iters_this_work_unit_ == iters_per_work_unit_) {
    return docs_.end();
  }

  std::list<DocumentWordTopics>::iterator doc_iter = docs_iter_;
  ++docs_iter_;

  if (docs_iter_ == docs_.end()) {
    if (num_iters_this_work_unit_ < (iters_per_work_unit_ - 1)) {
      docs_iter_ = docs_.begin();
    }
    ++num_iters_this_work_unit_;
    *num_iters_this_work_unit = num_iters_this_work_unit_;
  }
  return doc_iter;
}

bool Corpus::EndOfWorkUnit(const std::list<DocumentWordTopics>::iterator &iter) {
  if (iter == docs_.end())
    return true;
  return false;
}

size_t Corpus::GetNumDocs() {
  return docs_.size();
}

void Corpus::buildIterVector()
{
  std::unique_lock<std::mutex> ulock(docs_mtx_);
  docs_iter_vector_.clear();
  std::list<DocumentWordTopics>::iterator doc_iter_cache = docs_iter_;
  docs_iter_ = docs_.begin();
  
  while(docs_iter_ != docs_.end())
  {
    docs_iter_vector_.push_back(docs_iter_);
    ++docs_iter_;
  }

  docs_iter_ = doc_iter_cache;
}

std::list<DocumentWordTopics>::iterator Corpus::GetDocById(int32_t docId)
{
  CHECK((int)docs_iter_vector_.size() > docId) << "docId is out of range";
  CHECK(docId >= 0) << "docId is negative";

  return docs_iter_vector_[docId];
}

































}
