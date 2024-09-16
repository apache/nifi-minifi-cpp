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
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace tls {

class DistinguishedName {
 public:
  explicit DistinguishedName(const std::vector<std::string>& components);
  static DistinguishedName fromCommaSeparated(const std::string& comma_separated_components);
  static DistinguishedName fromSlashSeparated(const std::string& slash_separated_components);

  [[nodiscard]] std::optional<std::string> getCN() const;
  [[nodiscard]] std::string toString() const;

  friend bool operator==(const DistinguishedName& left, const DistinguishedName& right) { return left.components_ == right.components_; }
  friend bool operator!=(const DistinguishedName& left, const DistinguishedName& right) { return !(left == right); }

 private:
  std::vector<std::string> components_;
};

}  // namespace tls
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
