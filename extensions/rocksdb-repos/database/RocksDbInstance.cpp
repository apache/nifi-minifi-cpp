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

#include "RocksDbInstance.h"
#include <vector>
#include "logging/LoggerConfiguration.h"
#include "rocksdb/utilities/options_util.h"
#include "OpenRocksDb.h"
#include "ColumnHandle.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

std::shared_ptr<core::logging::Logger> RocksDbInstance::logger_ = core::logging::LoggerFactory<RocksDbInstance>::getLogger();

RocksDbInstance::RocksDbInstance(const std::string& path, RocksDbMode mode) : db_name_(path), mode_(mode) {}

void RocksDbInstance::invalidate() {
  std::lock_guard<std::mutex> db_guard{mtx_};
  // discard our own instance
  columns_.clear();
  impl_.reset();
}

utils::optional<OpenRocksDb> RocksDbInstance::open(const std::string& column, const DBOptionsPatch& db_options_patch, const ColumnFamilyOptionsPatch& cf_options_patch) {
  std::lock_guard<std::mutex> db_guard{mtx_};
  if (!impl_) {
    gsl_Expects(columns_.empty());
    // database needs to be (re)opened
    rocksdb::DB* db_instance = nullptr;
    rocksdb::Status result;

    rocksdb::ConfigOptions conf_options = [&] {
      // we have to extract the encryptor environment otherwise
      // we won't be able to read the options file
      rocksdb::ConfigOptions result;
      if (db_options_patch) {
        rocksdb::DBOptions dummy_opts;
        Writable<rocksdb::DBOptions> db_options_writer(dummy_opts);
        db_options_patch(db_options_writer);
        if (dummy_opts.env) {
          result.env = dummy_opts.env;
        }
      }
      return result;
    }();
    db_options_ = rocksdb::DBOptions{};
    std::vector<rocksdb::ColumnFamilyDescriptor> cf_descriptors;
    rocksdb::Status option_status = rocksdb::LoadLatestOptions(conf_options, db_name_, &db_options_, &cf_descriptors);
    Writable<rocksdb::DBOptions> db_options_writer(db_options_);
    if (db_options_patch) {
      db_options_patch(db_options_writer);
    }
    if (option_status.ok()) {
      logger_->log_trace("Found existing database '%s', checking compatibility", db_name_);
      rocksdb::Status compat_status = rocksdb::CheckOptionsCompatibility(conf_options, db_name_, db_options_, cf_descriptors);
      if (!compat_status.ok()) {
        logger_->log_error("Incompatible database options: %s", compat_status.ToString());
        return utils::nullopt;
      }
    } else if (option_status.IsNotFound()) {
      logger_->log_trace("Database at '%s' not found, creating", db_name_);
      if (column == "default") {
        rocksdb::ColumnFamilyOptions default_cf_options;
        Writable<rocksdb::ColumnFamilyOptions> cf_writer(default_cf_options);
        if (cf_options_patch) {
          cf_options_patch(cf_writer);
        }
        cf_descriptors.emplace_back("default", default_cf_options);
      } else {
        // we must create the "default" column, using default options
        cf_descriptors.emplace_back("default", rocksdb::ColumnFamilyOptions{});
      }
    } else if (!option_status.ok()) {
      logger_->log_error("Couldn't query database '%s' for options: '%s'", db_name_, option_status.ToString());
      return utils::nullopt;
    }
    std::vector<rocksdb::ColumnFamilyHandle*> column_handles;
    switch (mode_) {
      case RocksDbMode::ReadWrite:
        result = rocksdb::DB::Open(db_options_, db_name_, cf_descriptors, &column_handles, &db_instance);
        if (!result.ok()) {
          logger_->log_error("Cannot open writable rocksdb database '%s', error: '%s'", db_name_, result.ToString());
        }
        break;
      case RocksDbMode::ReadOnly:
        result = rocksdb::DB::OpenForReadOnly(db_options_, db_name_, cf_descriptors, &column_handles, &db_instance);
        if (!result.ok()) {
          logger_->log_error("Cannot open read-only rocksdb database '%s', error: '%s'", db_name_, result.ToString());
        }
        break;
    }
    if (!result.ok()) {
      // we failed to open the database
      return utils::nullopt;
    }
    gsl_Expects(db_instance);
    // the patcher could have internal resources the we need to keep alive
    // as long as the database is open (e.g. custom environment)
    db_options_patch_ = db_options_patch;
    impl_.reset(db_instance);
    for (size_t cf_idx{0}; cf_idx < column_handles.size(); ++cf_idx) {
      columns_[column_handles[cf_idx]->GetName()]
          = std::make_shared<ColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(column_handles[cf_idx]), cf_descriptors[cf_idx].options);
    }
  } else {
    logger_->log_trace("Checking if the already open database is compatible with the requested options");
    if (db_options_patch) {
      rocksdb::DBOptions db_options_copy = db_options_;
      Writable<rocksdb::DBOptions> writer(db_options_copy);
      db_options_patch(writer);
      if (writer.isModified()) {
        logger_->log_error("Database '%s' has already been opened using a different configuration", db_name_);
        return utils::nullopt;
      }
    }
  }
  std::shared_ptr<ColumnHandle> column_handle = getOrCreateColumnFamily(column, cf_options_patch, db_guard);
  if (!column_handle) {
    // error is already logged by the method
    return utils::nullopt;
  }
  return OpenRocksDb(
      *this,
      gsl::make_not_null<std::shared_ptr<rocksdb::DB>>(impl_),
      gsl::make_not_null<std::shared_ptr<ColumnHandle>>(column_handle));
}

std::shared_ptr<ColumnHandle> RocksDbInstance::getOrCreateColumnFamily(const std::string& column, const ColumnFamilyOptionsPatch& cf_options_patch, const std::lock_guard<std::mutex>& /*guard*/) {
  gsl_Expects(impl_);
  auto it = columns_.find(column);
  if (it != columns_.end()) {
    logger_->log_trace("Column '%s' already exists in database '%s'", column, impl_->GetName());
    Writable<rocksdb::ColumnFamilyOptions> writer(it->second->options);
    if (cf_options_patch) {
      cf_options_patch(writer);
    }
    if (writer.isModified()) {
      logger_->log_error("Requested column '%s' has already been opened using a different configuration", column);
      return nullptr;
    }
    return it->second;
  }
  if (mode_ == RocksDbMode::ReadOnly) {
    logger_->log_error("Read-only database cannot dynamically create new columns");
    return nullptr;
  }
  rocksdb::ColumnFamilyHandle* raw_handle{nullptr};
  rocksdb::ColumnFamilyOptions cf_options;
  Writable<rocksdb::ColumnFamilyOptions> writer(cf_options);
  if (cf_options_patch) {
    cf_options_patch(writer);
  }
  auto status = impl_->CreateColumnFamily(cf_options, column, &raw_handle);
  if (!status.ok()) {
    logger_->log_error("Failed to create column '%s' in database '%s'", column, impl_->GetName());
    return nullptr;
  }
  logger_->log_trace("Successfully created column '%s' in database '%s'", column, impl_->GetName());
  columns_[column] = std::make_shared<ColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(raw_handle), cf_options);
  return columns_[column];
}

}  // namespace internal
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
