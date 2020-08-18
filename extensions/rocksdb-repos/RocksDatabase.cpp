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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

OpenRocksDB::OpenRocksDB(RocksDatabase &db, gsl::not_null<rocksdb::DB*> impl) : db_(db), impl_(impl) {}

OpenRocksDB::OpenRocksDB(OpenRocksDB &&other) : db_(other.db_), impl_(other.impl_) {
  other.impl_ = nullptr;
}

OpenRocksDB& OpenRocksDB::operator=(OpenRocksDB &&other) {
  throw std::logic_error("Should not be called, only here for optional lazy init");
}

rocksdb::Status OpenRocksDB::Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Put(options, key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_.invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value) {
  rocksdb::Status result = impl_->Get(options, key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_.invalidate();
  }
  return result;
}

std::vector<rocksdb::Status> OpenRocksDB::MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values) {
  std::vector<rocksdb::Status> results = impl_->MultiGet(options, keys, values);
  for (const auto& result : results) {
    if (result == rocksdb::Status::NoSpace()) {
      db_.invalidate();
    }
  }
  return results;
}

rocksdb::Status OpenRocksDB::Write(const rocksdb::WriteOptions& options, rocksdb::WriteBatch* updates) {
  rocksdb::Status result = impl_->Write(options, updates);
  if (result == rocksdb::Status::NoSpace()) {
    db_.invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key) {
  rocksdb::Status result = impl_->Delete(options, key);
  if (result == rocksdb::Status::NoSpace()) {
    db_.invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Merge(options, key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_.invalidate();
  }
  return result;
}

bool OpenRocksDB::GetProperty(const rocksdb::Slice& property, std::string* value) {
  return impl_->GetProperty(property, value);
}

rocksdb::Iterator* OpenRocksDB::NewIterator(const rocksdb::ReadOptions& options) {
  return impl_->NewIterator(options);
}

rocksdb::Status OpenRocksDB::NewCheckpoint(rocksdb::Checkpoint **checkpoint) {
  return rocksdb::Checkpoint::Create(impl_, checkpoint);
}

rocksdb::Status OpenRocksDB::FlushWAL(bool sync) {
  rocksdb::Status result = impl_->FlushWAL(sync);
  if (result == rocksdb::Status::NoSpace()) {
    db_.invalidate();
  }
  return result;
}

rocksdb::DB* OpenRocksDB::get() {
  return impl_;
}

OpenRocksDB::~OpenRocksDB() {
  if (impl_ == nullptr) {
    // this instance has been moved from
    return;
  }
  if (--db_.reference_count_ == 0) {
    // it's on us to delete the database
    delete impl_;
  }
}

RocksDatabase::RocksDatabase(const rocksdb::Options& options, const std::string& name) : open_options_(options), db_name_(name) {}

void RocksDatabase::invalidate() {
  rocksdb::DB* current_db = impl_.load();
  impl_ = nullptr;
  if (current_db != nullptr && --reference_count_ == 0) {
    // it's on us to delete the database
    delete current_db;
  }
}

utils::optional<OpenRocksDB> RocksDatabase::open() {
  // to make sure that we have a valid reference in this method
  // also we increment on behalf of the OpenRocksDB instance to be created
  ++reference_count_;
  rocksdb::DB* db_instance = impl_.load();
  if (db_instance == nullptr) {
    // database is not opened yet
    rocksdb::Status result = rocksdb::DB::Open(open_options_, db_name_, &db_instance);
    if (result.ok()) {
      // since we could open the db, any previous db instance has surely been destroyed
      reference_count_ = 2;
      rocksdb::DB* current_db = nullptr;
      // we managed to open the db, we surely hold a single reference to it
      if (!impl_.compare_exchange_strong(current_db, db_instance)) {
        throw std::logic_error("Nobody else should have been able to open the database and set the pointer");
      }
    } else {
      // we failed to open the database either because of some real error
      // or because an other call this ::open was faster than the current one
      return utils::nullopt;
    }
  }
  return OpenRocksDB(*this, gsl::make_not_null<rocksdb::DB*>(db_instance));
}

RocksDatabase::~RocksDatabase() {
  delete impl_.load();
}

} /* namespace internal */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
