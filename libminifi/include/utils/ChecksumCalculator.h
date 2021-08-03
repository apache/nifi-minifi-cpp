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

#pragma once

#include <optional>
#include <string>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class ChecksumCalculator {
 public:
  static constexpr const char* CHECKSUM_TYPE = "SHA256";
  static constexpr size_t LENGTH_OF_HASH_IN_BYTES = 32;

  void setFileLocation(const std::string& file_location);
  std::string getFileName() const;
  std::string getChecksum();
  void invalidateChecksum();

 private:
  static std::string computeChecksum(const std::string& file_location);

  std::optional<std::string> file_location_;
  std::optional<std::string> file_name_;
  std::optional<std::string> checksum_;
};

inline void ChecksumCalculator::invalidateChecksum() {
  checksum_.reset();
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
