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
#include "RocksDbUtils.h"

#include <string>

#include "minifi-cpp/Exception.h"
#include "minifi-cpp/utils/Literals.h"

namespace org::apache::nifi::minifi::internal {

std::optional<rocksdb::CompressionType> readConfiguredCompressionType(const std::shared_ptr<Configure> &configuration, const std::string& config_key) {
  std::string value;
  if (!configuration->get(config_key, value) || value.empty()) {
    return std::nullopt;
  }
#ifdef WIN32
  if (value == "auto" || value == "xpress") {
    return rocksdb::CompressionType::kXpressCompression;
  } else {
    throw Exception(REPOSITORY_EXCEPTION, "RocksDB compression type not supported: " + value);
  }
#else
  if (value == "zlib") {
    return rocksdb::CompressionType::kZlibCompression;
  } else if (value == "bzip2") {
    return rocksdb::CompressionType::kBZip2Compression;
  } else if (value == "auto" || value == "zstd") {
    return rocksdb::CompressionType::kZSTD;
  } else if (value == "lz4") {
    return rocksdb::CompressionType::kLZ4Compression;
  } else if (value == "lz4hc") {
    return rocksdb::CompressionType::kLZ4HCCompression;
  } else {
    throw Exception(REPOSITORY_EXCEPTION, "RocksDB compression type not supported: " + value);
  }
#endif
}


void setCommonRocksDbOptions(Writable<rocksdb::DBOptions>& db_opts) {
  db_opts.set(&rocksdb::DBOptions::create_if_missing, true);
  db_opts.set(&rocksdb::DBOptions::use_direct_io_for_flush_and_compaction, true);
  db_opts.set(&rocksdb::DBOptions::use_direct_reads, true);
  db_opts.set(&rocksdb::DBOptions::keep_log_file_num, 1);
  db_opts.set(&rocksdb::DBOptions::max_log_file_size, 1_MiB);
}

std::unordered_map<std::string, std::string> getRocksDbOptionsToOverride(const std::shared_ptr<Configure> &configuration, std::string_view custom_db_prefix) {
  std::unordered_map<std::string, std::string> options;
  const auto addOverrideOptions = [&configuration, &options](std::string_view prefix) {
    if (prefix.empty()) {
      return;
    }
    for (const auto& [key, value] : configuration->getProperties()) {
      if (key.starts_with(prefix)) {
        options[key.substr(prefix.size())] = value;
      }
    }
  };
  addOverrideOptions(Configuration::nifi_global_rocksdb_options);
  addOverrideOptions(custom_db_prefix);
  return options;
}

}  // namespace org::apache::nifi::minifi::internal
