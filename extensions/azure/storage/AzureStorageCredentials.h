/**
 * @file AzureStorageCredentials.h
 * AzureStorageCredentials class declaration
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
#pragma once

#include <string>

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

struct AzureStorageCredentials {
  std::string storage_account_name;
  std::string storage_account_key;
  std::string sas_token;
  std::string endpoint_suffix;
  std::string connection_string;

  std::string getConnectionString() const {
    if (!connection_string.empty()) {
      return connection_string;
    }

    if (storage_account_name.empty() || (storage_account_key.empty() && sas_token.empty())) {
      return "";
    }

    std::string credentials;
    credentials += "AccountName=" + storage_account_name;

    if (!storage_account_key.empty()) {
      credentials += ";AccountKey=" + storage_account_key;
    }

    if (!sas_token.empty()) {
      credentials += ";SharedAccessSignature=" + (sas_token[0] == '?' ? sas_token.substr(1) : sas_token);
    }

    if (!endpoint_suffix.empty()) {
      credentials += ";EndpointSuffix=" + endpoint_suffix;
    }

    return credentials;
  }
};

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
