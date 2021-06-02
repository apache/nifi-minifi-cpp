/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DatabaseContentRepository.h"

#include <memory>
#include <string>

#include "RocksDbStream.h"
#include "rocksdb/merge_operator.h"
#include "utils/GeneralUtils.h"
#include "utils/gsl.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

bool DatabaseContentRepository::initialize(const std::shared_ptr<minifi::Configure> &configuration) {
  std::string value;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, value)) {
    directory_ = value;
  } else {
    directory_ = configuration->getHome() + "/dbcontentrepository";
  }
  rocksdb::Options options;
  options.create_if_missing = true;
  options.use_direct_io_for_flush_and_compaction = true;
  options.use_direct_reads = true;
  options.merge_operator = std::make_shared<StringAppender>();
  options.error_if_exists = false;
  options.max_successive_merges = 0;
  db_ = utils::make_unique<minifi::internal::RocksDatabase>(options, directory_);
  if (db_->open()) {
    logger_->log_debug("NiFi Content DB Repository database open %s success", directory_);
    is_valid_ = true;
  } else {
    logger_->log_error("NiFi Content DB Repository database open %s fail", directory_);
    is_valid_ = false;
  }
  return is_valid_;
}

void DatabaseContentRepository::stop() {
  if (db_) {
    auto opendb = db_->open();
    if (opendb) {
      opendb->FlushWAL(true);
    }
  }
  db_.reset();
}

DatabaseContentRepository::Session::Session(std::shared_ptr<ContentRepository> repository) : ContentSession(std::move(repository)) {}

std::shared_ptr<ContentSession> DatabaseContentRepository::createSession() {
  return std::make_shared<Session>(sharedFromThis());
}

void DatabaseContentRepository::Session::commit() {
  auto dbContentRepository = std::static_pointer_cast<DatabaseContentRepository>(repository_);
  auto opendb = dbContentRepository->db_->open();
  if (!opendb) {
    throw Exception(REPOSITORY_EXCEPTION, "Couldn't open rocksdb database to commit content changes");
  }
  rocksdb::WriteBatch batch;
  for (const auto& resource : managedResources_) {
    auto outStream = dbContentRepository->write(*resource.first, false, &batch);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    if (outStream->write(resource.second->getBuffer(), size) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
    }
  }
  for (const auto& resource : extendedResources_) {
    auto outStream = dbContentRepository->write(*resource.first, true, &batch);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    if (outStream->write(resource.second->getBuffer(), size) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
    }
  }

  rocksdb::WriteOptions options;
  options.sync = true;
  rocksdb::Status status = opendb->Write(options, &batch);
  if (!status.ok()) {
    throw Exception(REPOSITORY_EXCEPTION, "Batch write failed: " + status.ToString());
  }

  managedResources_.clear();
  extendedResources_.clear();
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::write(const minifi::ResourceClaim &claim, bool append) {
  return write(claim, append, nullptr);
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::read(const minifi::ResourceClaim &claim) {
  // the traditional approach with these has been to return -1 from the stream; however, since we have the ability here
  // we can simply return a nullptr, which is also valid from the API when this stream is not valid.
  if (!is_valid_ || !db_)
    return nullptr;
  return std::make_shared<io::RocksDbStream>(claim.getContentFullPath(), gsl::make_not_null<minifi::internal::RocksDatabase*>(db_.get()), false);
}

bool DatabaseContentRepository::exists(const minifi::ResourceClaim &streamId) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  std::string value;
  rocksdb::Status status;
  status = opendb->Get(rocksdb::ReadOptions(), streamId.getContentFullPath(), &value);
  if (status.ok()) {
    logger_->log_debug("%s exists", streamId.getContentFullPath());
    return true;
  } else {
    logger_->log_debug("%s does not exist", streamId.getContentFullPath());
    return false;
  }
}

bool DatabaseContentRepository::remove(const minifi::ResourceClaim &claim) {
  if (!is_valid_ || !db_)
    return false;
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  rocksdb::Status status;
  status = opendb->Delete(rocksdb::WriteOptions(), claim.getContentFullPath());
  if (status.ok()) {
    logger_->log_debug("Deleting resource %s", claim.getContentFullPath());
    return true;
  } else {
    logger_->log_debug("Attempted, but could not delete %s", claim.getContentFullPath());
    return false;
  }
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::write(const minifi::ResourceClaim& claim, bool /*append*/, rocksdb::WriteBatch* batch) {
  // the traditional approach with these has been to return -1 from the stream; however, since we have the ability here
  // we can simply return a nullptr, which is also valid from the API when this stream is not valid.
  if (!is_valid_ || !db_)
    return nullptr;
  // append is already supported in all modes
  return std::make_shared<io::RocksDbStream>(claim.getContentFullPath(), gsl::make_not_null<minifi::internal::RocksDatabase*>(db_.get()), true, batch);
}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
