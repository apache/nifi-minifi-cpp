/**
 * @file GenerateFlowFile.h
 * GenerateFlowFile class declaration
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

#include <string>
#include <memory>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"

#pragma once

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
// GenerateFlowFile Class
class KamikazeProcessor : public core::Processor {
 public:
  static const std::string OnScheduleExceptionStr;
  static const std::string OnTriggerExceptionStr;
  static const std::string OnScheduleLogStr;
  static const std::string OnTriggerLogStr;
  static const std::string OnUnScheduleLogStr;

  explicit KamikazeProcessor(const std::string& name, const utils::Identifier& uuid = utils::Identifier())
  : Processor(name, uuid), logger_(logging::LoggerFactory<KamikazeProcessor>::getLogger()) {
    _throwInOnTrigger = false;
  }

  // Processor Name
  static constexpr char const* ProcessorName = "KamikazeProcessor";
  // Supported Properties
  static core::Property ThrowInOnSchedule;
  static core::Property ThrowInOnTrigger;

 public:
  virtual void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  virtual void initialize();
  virtual void onUnSchedule();

 private:
  bool _throwInOnTrigger;
  // logger instance
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
