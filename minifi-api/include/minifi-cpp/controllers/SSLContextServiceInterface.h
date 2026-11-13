// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/core/ControllerServiceTypeDefinition.h"
#include "minifi-cpp/agent/agent_version.h"
#include "minifi-cpp/core/controller/ControllerServiceHandle.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * SSLContextService provides a configurable controller service from
 * which we can provide an SSL Context or component parts that go
 * into creating one.
 *
 * Justification: Abstracts SSL support out of processors into a
 * configurable controller service.
 */
class SSLContextServiceInterface : public core::controller::ControllerServiceHandle {
 public:
  static constexpr auto ProvidesApi = core::ControllerServiceTypeDefinition{
    .artifact = "minifi-system",
    .group = "org.apache.nifi.minifi",
    .type = "org.apache.nifi.minifi.controllers.SSLContextServiceInterface",
    .version = MINIFI_VERSION_STR
  };

  virtual const std::filesystem::path& getCertificateFile() const = 0;
  virtual const std::string& getPassphrase() const = 0;
  virtual const std::filesystem::path& getPrivateKeyFile() const = 0;
  virtual const std::filesystem::path& getCACertificate() const = 0;


  virtual void setMinTlsVersion(long min_version) = 0;  // NOLINT(runtime/int) long due to SSL lib API

  virtual void setMaxTlsVersion(long max_version) = 0;  // NOLINT(runtime/int) long due to SSL lib API
  virtual bool configure_ssl_context(void* ssl_ctx) = 0;
};


}  // namespace org::apache::nifi::minifi::controllers
