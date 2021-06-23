/**
 * OPCBase class declaration
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
#include <memory>
#include <vector>
#include <set>

#include "opc.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class BaseOPCProcessor : public core::Processor {
 public:
  static core::Property OPCServerEndPoint;

  static core::Property ApplicationURI;
  static core::Property Username;
  static core::Property Password;
  static core::Property CertificatePath;
  static core::Property KeyPath;
  static core::Property TrustedPath;

  explicit BaseOPCProcessor(const std::string& name, const utils::Identifier& uuid = {})
  : Processor(name, uuid) {
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;

 protected:
  virtual bool reconnect();

  std::shared_ptr<logging::Logger> logger_;

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

  virtual std::set<core::Property> getSupportedProperties() const {return {OPCServerEndPoint, ApplicationURI, Username, Password, CertificatePath, KeyPath, TrustedPath};}
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
