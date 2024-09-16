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

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * SSLContextService provides a configurable controller service from
 * which we can provide an SSL Context or component parts that go
 * into creating one.
 *
 * Justification: Abstracts SSL support out of processors into a
 * configurable controller service.
 */
class SSLContextService : public virtual core::controller::ControllerService {
 public:
  virtual const std::filesystem::path& getCertificateFile() const = 0;
  virtual const std::string& getPassphrase() const = 0;
  virtual const std::filesystem::path& getPrivateKeyFile() const = 0;
  virtual const std::filesystem::path& getCACertificate() const = 0;


  virtual void setMinTlsVersion(long min_version) = 0;  // NOLINT(runtime/int) long due to SSL lib API

  virtual void setMaxTlsVersion(long max_version) = 0;  // NOLINT(runtime/int) long due to SSL lib API
  virtual bool configure_ssl_context(void* ssl_ctx) = 0;
};

}  // namespace org::apache::nifi::minifi::controllers
