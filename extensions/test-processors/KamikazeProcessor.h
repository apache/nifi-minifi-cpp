/**
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

namespace org::apache::nifi::minifi::processors {

class KamikazeProcessor : public core::Processor {
 public:
  EXTENSIONAPI static const std::string OnScheduleExceptionStr;
  EXTENSIONAPI static const std::string OnTriggerExceptionStr;
  EXTENSIONAPI static const std::string OnScheduleLogStr;
  EXTENSIONAPI static const std::string OnTriggerLogStr;
  EXTENSIONAPI static const std::string OnUnScheduleLogStr;

  explicit KamikazeProcessor(const std::string& name, const utils::Identifier& uuid = utils::Identifier())
      : Processor(name, uuid) {
  }

  // Processor Name
  static constexpr char const* ProcessorName = "KamikazeProcessor";
  // Supported Properties
  EXTENSIONAPI static core::Property ThrowInOnSchedule;
  EXTENSIONAPI static core::Property ThrowInOnTrigger;

 public:
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;
  void onUnSchedule() override;

 private:
  bool _throwInOnTrigger = false;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<KamikazeProcessor>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
