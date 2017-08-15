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
    directory_ = "dbcontentrepository";
  }
  rocksdb::Options options;
  options.create_if_missing = true;
  options.use_direct_io_for_flush_and_compaction = true;
  options.use_direct_reads = true;
  rocksdb::Status status = rocksdb::DB::Open(options, directory_.c_str(), &db_);
  if (status.ok()) {
    logger_->log_info("NiFi Content DB Repository database open %s success", directory_.c_str());
  } else {
    logger_->log_error("NiFi Content DB Repository database open %s fail", directory_.c_str());
    return false;
  }
  return true;
}
void DatabaseContentRepository::stop() {
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::write(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  return std::make_shared<io::RocksDbStream>(claim->getContentFullPath(), db_, true);
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::read(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  return std::make_shared<io::RocksDbStream>(claim->getContentFullPath(), db_, false);
}

bool DatabaseContentRepository::remove(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  rocksdb::Status status;
  status = db_->Delete(rocksdb::WriteOptions(), claim->getContentFullPath());
  if (status.ok()) {
    logger_->log_debug("Deleted %s", claim->getContentFullPath());
    return true;
  } else {
    logger_->log_debug("Attempted, but could not delete %s", claim->getContentFullPath());
    return false;
  }
}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
