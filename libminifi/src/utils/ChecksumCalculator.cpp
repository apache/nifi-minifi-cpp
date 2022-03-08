/**
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

#include "utils/ChecksumCalculator.h"

#include <array>
#include <fstream>

#include "sodium/crypto_hash_sha256.h"
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"
#include "properties/Configuration.h"

namespace {

const std::string AGENT_IDENTIFIER_KEY = std::string(org::apache::nifi::minifi::Configuration::nifi_c2_agent_identifier) + "=";

}

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

void ChecksumCalculator::setFileLocation(const std::string& file_location) {
  file_location_ = file_location;
  file_name_ = utils::file::get_child_path(file_location);
  invalidateChecksum();
}

std::string ChecksumCalculator::getFileName() const {
  gsl_Expects(file_name_);
  return *file_name_;
}

std::string ChecksumCalculator::getChecksum() {
  gsl_Expects(file_location_);
  if (!checksum_) {
    checksum_ = computeChecksum(*file_location_);
  }
  return *checksum_;
}

std::string ChecksumCalculator::computeChecksum(const std::string& file_location) {
  using org::apache::nifi::minifi::utils::StringUtils;

  std::ifstream input_file{file_location, std::ios::in | std::ios::binary};
  if (!input_file.is_open()) {
    throw std::runtime_error(StringUtils::join_pack("Could not open config file '", file_location, "' to compute the checksum: ", std::strerror(errno)));
  }

  crypto_hash_sha256_state state;
  crypto_hash_sha256_init(&state);

  std::string line;
  while (std::getline(input_file, line)) {
    // skip lines containing the agent identifier, so agents in the same class will have the same checksum
    if (StringUtils::startsWith(line, AGENT_IDENTIFIER_KEY)) {
      continue;
    }
    if (!input_file.eof()) {  // eof() means we have just read the last line, which was not terminated by a newline
      line.append("\n");
    }
    crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(line.data()), line.size());
  }
  if (input_file.bad()) {
    throw std::runtime_error(StringUtils::join_pack("Error reading config file '", file_location, "' while computing the checksum: ", std::strerror(errno)));
  }

  std::array<unsigned char, LENGTH_OF_HASH_IN_BYTES> hash{};
  crypto_hash_sha256_final(&state, hash.data());

  return StringUtils::to_hex(gsl::make_span(hash).as_span<std::byte>());
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
