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
#include <memory>
#include <utility>
#include <vector>

#include "opc.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"

namespace org::apache::nifi::minifi::processors {

class BaseOPCProcessor : public core::Processor {
 public:
  EXTENSIONAPI static const core::Property OPCServerEndPoint;
  EXTENSIONAPI static const core::Property ApplicationURI;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;
  EXTENSIONAPI static const core::Property CertificatePath;
  EXTENSIONAPI static const core::Property KeyPath;
  EXTENSIONAPI static const core::Property TrustedPath;
  static auto properties() {
    return std::array{
      OPCServerEndPoint,
      ApplicationURI,
      Username,
      Password,
      CertificatePath,
      KeyPath,
      TrustedPath
    };
  }

  explicit BaseOPCProcessor(std::string name, const utils::Identifier& uuid = {})
  : Processor(std::move(name), uuid) {
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;

 protected:
  virtual bool reconnect();

  std::shared_ptr<core::logging::Logger> logger_;

  opc::ClientPtr connection_;

  std::string endPointURL_;

  std::string applicationURI_;
  std::string username_;
  std::string password_;
  std::string certpath_;
  std::string keypath_;
  std::string trustpath_;

  std::vector<char> certBuffer_;
  std::vector<char> keyBuffer_;
  std::vector<std::vector<char>> trustBuffers_;
};

}  // namespace org::apache::nifi::minifi::processors
