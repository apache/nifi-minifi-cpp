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

#include <string>

#include "minifi-cpp/utils/gsl.h"

struct apiClient_t;
struct list_t;
struct sslConfig_t;

namespace org::apache::nifi::minifi::kubernetes {

class ApiClient {
 public:
  ApiClient();
  ~ApiClient() noexcept;

  ApiClient(ApiClient&&) = delete;
  ApiClient(const ApiClient&) = delete;
  ApiClient& operator=(ApiClient&&) = delete;
  ApiClient& operator=(const ApiClient&) = delete;

  [[nodiscard]] gsl::not_null<apiClient_t*> getClient() const noexcept { return api_client_; }

 private:
  gsl::owner<char*> base_path_ = nullptr;
  gsl::owner<sslConfig_t*> ssl_config_ = nullptr;
  gsl::owner<list_t*> api_keys_ = nullptr;
  gsl::not_null<gsl::owner<apiClient_t*>> api_client_;  // must be declared after base_path_, ssl_config_ and api_keys_
};

}  // namespace org::apache::nifi::minifi::kubernetes
