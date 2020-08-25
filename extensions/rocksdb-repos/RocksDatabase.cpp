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

#include "RocksDatabase.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

OpenRocksDB::OpenRocksDB(RocksDatabase& db, gsl::not_null<std::shared_ptr<rocksdb::DB>> impl) : db_(&db), impl_(std::move(impl)) {}

rocksdb::Status OpenRocksDB::Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Put(options, key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value) {
  rocksdb::Status result = impl_->Get(options, key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

std::vector<rocksdb::Status> OpenRocksDB::MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values) {
  std::vector<rocksdb::Status> results = impl_->MultiGet(options, keys, values);
  for (const auto& result : results) {
    if (result == rocksdb::Status::NoSpace()) {
      db_->invalidate();
      break;
    }
  }
  return results;
}

rocksdb::Status OpenRocksDB::Write(const rocksdb::WriteOptions& options, rocksdb::WriteBatch* updates) {
  rocksdb::Status result = impl_->Write(options, updates);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key) {
  rocksdb::Status result = impl_->Delete(options, key);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Merge(options, key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

bool OpenRocksDB::GetProperty(const rocksdb::Slice& property, std::string* value) {
  return impl_->GetProperty(property, value);
}

std::unique_ptr<rocksdb::Iterator> OpenRocksDB::NewIterator(const rocksdb::ReadOptions& options) {
  return std::unique_ptr<rocksdb::Iterator>{impl_->NewIterator(options)};
}

rocksdb::Status OpenRocksDB::NewCheckpoint(rocksdb::Checkpoint **checkpoint) {
  return rocksdb::Checkpoint::Create(impl_.get(), checkpoint);
}

rocksdb::Status OpenRocksDB::FlushWAL(bool sync) {
  rocksdb::Status result = impl_->FlushWAL(sync);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::DB* OpenRocksDB::get() {
  return impl_.get();
}

std::shared_ptr<core::logging::Logger> RocksDatabase::logger_ = core::logging::LoggerFactory<RocksDatabase>::getLogger();

RocksDatabase::RocksDatabase(const rocksdb::Options& options, const std::string& name, Mode mode) : open_options_(options), db_name_(name), mode_(mode) {}

void RocksDatabase::invalidate() {
  std::lock_guard<std::mutex> db_guard{ mtx_ };
  // discard our own instance
  impl_.reset();
}

utils::optional<OpenRocksDB> RocksDatabase::open() {
  std::lock_guard<std::mutex> db_guard{ mtx_ };
  if (!impl_) {
    // database is not opened yet
    rocksdb::DB* db_instance = nullptr;
    rocksdb::Status result;
    switch (mode_) {
      case Mode::ReadWrite:
        result = rocksdb::DB::Open(open_options_, db_name_, &db_instance);
        if (!result.ok()) {
          logger_->log_error("Cannot open writable rocksdb database %s, error: %s", db_name_, result.ToString());
        }
        break;
      case Mode::ReadOnly:
        result = rocksdb::DB::OpenForReadOnly(open_options_, db_name_, &db_instance);
        if (!result.ok()) {
          logger_->log_error("Cannot open read-only rocksdb database %s, error: %s", db_name_, result.ToString());
        }
        break;
    }
    if (db_instance != nullptr && result.ok()) {
      impl_.reset(db_instance);
    } else {
      // we failed to open the database
      return utils::nullopt;
    }
  }
  return OpenRocksDB(*this, gsl::make_not_null<std::shared_ptr<rocksdb::DB>>(impl_));
}

} /* namespace internal */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
