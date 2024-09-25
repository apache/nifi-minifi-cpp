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
#include "utils/HTTPUtils.h"

#include <array>
#include <filesystem>

namespace org::apache::nifi::minifi::utils {

std::optional<std::string_view> getDefaultCAFile() {
#ifndef WIN32
  static constexpr std::array<std::string_view, 5> possible_ca_paths = {
      "/usr/local/share/certs/ca-root-nss.crt",
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/usr/share/ssl/certs/ca-bundle.crt",
      "/etc/ssl/cert.pem"
  };

  for (const auto& possible_ca_path : possible_ca_paths) {
    if (std::filesystem::exists(possible_ca_path)) {
      return possible_ca_path;
    }
  }
#endif
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils
