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
#include <utility>
#include "core/logging/LoggerFactory.h"
#include "rocksdb/utilities/options_util.h"
#include "OpenRocksDb.h"
#include "ColumnHandle.h"
#include "DbHandle.h"

namespace org::apache::nifi::minifi::internal {

std::shared_ptr<core::logging::Logger> RocksDbInstance::logger_ = core::logging::LoggerFactory<RocksDbInstance>::getLogger();

RocksDbInstance::RocksDbInstance(std::string path, RocksDbMode mode) : db_name_(std::move(path)), mode_(mode) {}

void RocksDbInstance::invalidate() {
  std::lock_guard<std::mutex> db_guard{mtx_};
  invalidate(db_guard);
}

void RocksDbInstance::invalidate(const std::lock_guard<std::mutex>&) {
  // discard our own instance
  columns_.clear();
  impl_.reset();
}

void RocksDbInstance::registerColumnConfig(const std::string& column, const DBOptionsPatch& db_options_patch, const ColumnFamilyOptionsPatch& cf_options_patch,
    const std::unordered_map<std::string, std::string>& db_config_override) {
  std::lock_guard<std::mutex> db_guard{mtx_};
  logger_->log_trace("Registering column '{}' in database '{}'", column, db_name_);
  db_config_override_ = db_config_override;
  auto [_, inserted] = column_configs_.insert({column, ColumnConfig{.dbo_patch = db_options_patch, .cfo_patch = cf_options_patch}});
  if (!inserted) {
    throw std::runtime_error("Configuration is already registered for column '" + column + "'");
  }

  bool need_reopen = [&] {
    if (!impl_) {
      logger_->log_trace("Database is already scheduled to be reopened");
      return false;
    }
    {
      rocksdb::DBOptions db_opts_copy = db_options_;
      Writable<rocksdb::DBOptions> db_opts_writer(db_opts_copy);
      if (db_options_patch) {
        db_options_patch(db_opts_writer);
        rocksdb::ConfigOptions conf_options;
        conf_options.sanity_level = rocksdb::ConfigOptions::kSanityLevelLooselyCompatible;
        auto db_config_override_status = rocksdb::GetDBOptionsFromMap(conf_options, db_opts_copy, db_config_override_, &db_opts_copy);
        if (!db_config_override_status.ok()) {
          throw std::runtime_error("Failed to override RocksDB options from minifi.properties file: " + db_config_override_status.ToString());
        }
        if (db_opts_copy != db_options_) {
          logger_->log_trace("Requested a different DBOptions than the one that was used to open the database");
          return true;
        }
      }
    }
    if (!columns_.contains(column)) {
      logger_->log_trace("Previously unspecified column, will dynamically create the column");
      return false;
    }
    if (!cf_options_patch) {
      logger_->log_trace("No explicit ColumnFamilyOptions was requested");
      return false;
    }
    logger_->log_trace("Could not determine if we definitely need to reopen or we are definitely safe, requesting reopen");
    return true;
  }();
  if (need_reopen) {
    // reset impl_, for the database to be reopened on the next RocksDbInstance::open call
    invalidate(db_guard);
  }
}

void RocksDbInstance::unregisterColumnConfig(const std::string& column) {
  std::lock_guard<std::mutex> db_guard{mtx_};
  if (column_configs_.erase(column) == 0) {
    throw std::runtime_error("Could not find column configuration for column '" + column + "'");
  }
}

std::optional<OpenRocksDb> RocksDbInstance::open(const std::string& column) {
  std::lock_guard<std::mutex> db_guard{mtx_};
  if (!impl_) {
    gsl_Expects(columns_.empty());
    // database needs to be (re)opened
    rocksdb::DB* db_instance = nullptr;
    rocksdb::Status result;

    std::vector<DBOptionsPatch> dbo_patches;
    rocksdb::ConfigOptions conf_options;
    conf_options.sanity_level = rocksdb::ConfigOptions::kSanityLevelLooselyCompatible;
    {
      // we have to extract the encryptor environment otherwise
      // we won't be able to read the options file
      rocksdb::DBOptions dummy_opts;
      dummy_opts.env = nullptr;  // manually clear it, for the patcher to explicitly set it
      for (auto& [col_name, config] : column_configs_) {
        if (auto& dbo_patch = config.dbo_patch) {
          dbo_patches.push_back(dbo_patch);
          Writable<rocksdb::DBOptions> db_options_writer(dummy_opts);
          dbo_patch(db_options_writer);
          if (dummy_opts.env) {
            conf_options.env = dummy_opts.env;
          }
        }
      }
      // we need to reapply the DBOptions changes to check for conflicts
      for (auto& [col_name, config] : column_configs_) {
        if (auto& dbo_patch = config.dbo_patch) {
          Writable<rocksdb::DBOptions> db_options_writer(dummy_opts);
          dbo_patch(db_options_writer);
          if (db_options_writer.isModified()) {
            logger_->log_error("Conflicting database options requested for '{}'", db_name_);
            return std::nullopt;
          }
        }
      }
    }
    db_options_ = rocksdb::DBOptions{};
    std::vector<rocksdb::ColumnFamilyDescriptor> cf_descriptors;
    rocksdb::Status latest_option_status = rocksdb::LoadLatestOptions(conf_options, db_name_, &db_options_, &cf_descriptors);
    {
      // apply the database options patchers
      Writable<rocksdb::DBOptions> db_options_writer(db_options_);
      for (auto& [col_name, config] : column_configs_) {
        if (auto& dbo_patch = config.dbo_patch) {
          dbo_patch(db_options_writer);
        }
      }
    }
    // apply requested ColumnFamilyOptions for each already existing ColumnFamily
    for (auto& cf_descr : cf_descriptors) {
      if (auto it = column_configs_.find(cf_descr.name); it != column_configs_.end()) {
        if (auto& cfo_patch = it->second.cfo_patch) {
          cfo_patch(cf_descr.options);
        }
      }
    }
    auto db_config_override_status = rocksdb::GetDBOptionsFromMap(conf_options, db_options_, db_config_override_, &db_options_);
    if (!db_config_override_status.ok()) {
      logger_->log_error("Failed to override RocksDB options from minifi.properties file: {}", db_config_override_status.ToString());
      return std::nullopt;
    }
    if (latest_option_status.ok()) {
      logger_->log_trace("Found existing database '{}', checking compatibility", db_name_);
      rocksdb::Status compat_status = rocksdb::CheckOptionsCompatibility(conf_options, db_name_, db_options_, cf_descriptors);
      if (!compat_status.ok()) {
        logger_->log_error("Incompatible database options: {}", compat_status.ToString());
        return std::nullopt;
      }
    } else if (latest_option_status.IsNotFound()) {
      logger_->log_trace("Database at '{}' not found, creating", db_name_);
      rocksdb::ColumnFamilyOptions default_cf_options;
      if (auto it = column_configs_.find("default"); it != column_configs_.end()) {
        if (auto& cfo_patch = it->second.cfo_patch) {
          cfo_patch(default_cf_options);
        }
      }
      cf_descriptors.emplace_back("default", default_cf_options);
    } else if (!latest_option_status.ok()) {
      logger_->log_error("Couldn't query database '{}' for options: '{}'", db_name_, latest_option_status.ToString());
      return std::nullopt;
    }
    std::vector<rocksdb::ColumnFamilyHandle*> column_handles;
    switch (mode_) {
      case RocksDbMode::ReadWrite:
        result = rocksdb::DB::Open(db_options_, db_name_, cf_descriptors, &column_handles, &db_instance);
        if (!result.ok()) {
          logger_->log_error("Cannot open writable rocksdb database '{}', error: '{}'", db_name_, result.ToString());
        }
        break;
      case RocksDbMode::ReadOnly:
        result = rocksdb::DB::OpenForReadOnly(db_options_, db_name_, cf_descriptors, &column_handles, &db_instance);
        if (!result.ok()) {
          logger_->log_error("Cannot open read-only rocksdb database '{}', error: '{}'", db_name_, result.ToString());
        }
        break;
    }
    if (!result.ok()) {
      // we failed to open the database
      return std::nullopt;
    }
    gsl_Expects(db_instance);
    // the patches could have internal resources that we need to keep alive
    // as long as the database is open (e.g. custom environment)
    impl_ = std::make_shared<DbHandle>(std::unique_ptr<rocksdb::DB>(db_instance), std::move(dbo_patches));
    for (size_t cf_idx{0}; cf_idx < column_handles.size(); ++cf_idx) {
      ColumnFamilyOptionsPatch cfo_patch;
      if (auto it = column_configs_.find(column_handles[cf_idx]->GetName()); it != column_configs_.end()) {
        cfo_patch = it->second.cfo_patch;
      }
      columns_[column_handles[cf_idx]->GetName()]
          = std::make_shared<ColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(column_handles[cf_idx]), cfo_patch);
    }
  }
  std::shared_ptr<ColumnHandle> column_handle = getOrCreateColumnFamily(column, db_guard);
  if (!column_handle) {
    // error is already logged by the method
    return std::nullopt;
  }
  return OpenRocksDb(
      *this,
      gsl::make_not_null<std::shared_ptr<rocksdb::DB>>(std::shared_ptr<rocksdb::DB>(impl_, impl_->handle.get())),
      gsl::make_not_null<std::shared_ptr<ColumnHandle>>(column_handle));
}

std::shared_ptr<ColumnHandle> RocksDbInstance::getOrCreateColumnFamily(const std::string& column, const std::lock_guard<std::mutex>& /*guard*/) {
  gsl_Expects(impl_);
  if (!column_configs_.contains(column)) {
    logger_->log_error("Trying to access column '{}' in database '{}' without configuration", column, impl_->handle->GetName());
    return nullptr;
  }
  if (auto it = columns_.find(column); it != columns_.end()) {
    logger_->log_trace("Column '{}' already exists in database '{}'", column, impl_->handle->GetName());
    return it->second;
  }
  if (mode_ == RocksDbMode::ReadOnly) {
    logger_->log_error("Read-only database cannot dynamically create new columns");
    return nullptr;
  }
  rocksdb::ColumnFamilyHandle* raw_handle{nullptr};
  rocksdb::ColumnFamilyOptions cf_options;
  ColumnFamilyOptionsPatch cfo_patch;
  if (auto it = column_configs_.find(column); it != column_configs_.end()) {
    cfo_patch = it->second.cfo_patch;
    if (cfo_patch) {
      cfo_patch(cf_options);
    }
  }
  auto status = impl_->handle->CreateColumnFamily(cf_options, column, &raw_handle);
  if (!status.ok()) {
    logger_->log_error("Failed to create column '{}' in database '{}'", column, impl_->handle->GetName());
    return nullptr;
  }
  logger_->log_trace("Successfully created column '{}' in database '{}'", column, impl_->handle->GetName());
  columns_[column] = std::make_shared<ColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(raw_handle), cfo_patch);
  return columns_[column];
}

}  // namespace org::apache::nifi::minifi::internal
