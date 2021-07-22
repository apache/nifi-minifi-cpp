/**
 * @file GetGPS.h
 * GetGPS class declaration
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

#include <memory>
#include <string>

#include "../FlowFileRecord.h"
#include "../core/Processor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class GetGPS : public core::Processor {
 public:
  explicit GetGPS(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid), logger_(logging::LoggerFactory<GetGPS>::getLogger()) {
    gpsdHost_ = "localhost";
    gpsdPort_ = "2947";
    gpsdWaitTime_ = 50000000;
  }
  virtual ~GetGPS() = default;
  static const std::string ProcessorName;
  // Supported Properties
  static core::Property GPSDHost;
  static core::Property GPSDPort;
  static core::Property GPSDWaitTime;

  // Supported Relationships
  static core::Relationship Success;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  //! OnTrigger method, implemented by NiFi GetGPS
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  //! Initialize, over write by NiFi GetGPS
  void initialize(void) override;

 private:
  std::string gpsdHost_;
  std::string gpsdPort_;
  int64_t gpsdWaitTime_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
