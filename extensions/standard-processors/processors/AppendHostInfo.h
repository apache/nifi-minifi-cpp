/**
 * @file AppendHostInfo.h
 * AppendHostInfo class declaration
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
#ifndef __APPEND_HOSTINFO_H__
#define __APPEND_HOSTINFO_H__

#include "core/Property.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// AppendHostInfo Class
class AppendHostInfo : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  AppendHostInfo(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<AppendHostInfo>::getLogger()) {
  }
  // Destructor
  virtual ~AppendHostInfo() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "AppendHostInfo";
  // Supported Properties
  static core::Property InterfaceName;
  static core::Property HostAttribute;
  static core::Property IPAttribute;

  // Supported Relationships
  static core::Relationship Success;

 public:
  // OnTrigger method, implemented by NiFi AppendHostInfo
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  // Initialize, over write by NiFi AppendHostInfo
  virtual void initialize(void);

 protected:

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(AppendHostInfo, "Appends host information such as IP address and hostname as an attribute to incoming flowfiles.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
