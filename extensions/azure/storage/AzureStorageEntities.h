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
#include <utility>

namespace org::apache::nifi::minifi::azure::storage {

struct ManagedIdentityParameters {
  ManagedIdentityParameters(std::string account, std::string suffix)
    : storage_account(std::move(account)),
      endpoint_suffix(suffix.empty() ? "core.windows.net" : std::move(suffix)) {}
  std::string storage_account;
  std::string endpoint_suffix;
};

struct ConnectionString {
  std::string value;
};

}  // namespace org::apache::nifi::minifi::azure::storage
