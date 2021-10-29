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

#include "SplunkHECProcessor.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::splunk {

class PutSplunkHTTP final : public SplunkHECProcessor {
 public:
  explicit PutSplunkHTTP(const std::string& name, const utils::Identifier& uuid = {})
      : SplunkHECProcessor(name, uuid) {
  }
  PutSplunkHTTP(const PutSplunkHTTP&) = delete;
  PutSplunkHTTP(PutSplunkHTTP&&) = delete;
  PutSplunkHTTP& operator=(const PutSplunkHTTP&) = delete;
  PutSplunkHTTP& operator=(PutSplunkHTTP&&) = delete;
  ~PutSplunkHTTP() override = default;

  EXTENSIONAPI static const core::Property Source;
  EXTENSIONAPI static const core::Property SourceType;
  EXTENSIONAPI static const core::Property Host;
  EXTENSIONAPI static const core::Property Index;
  EXTENSIONAPI static const core::Property ContentType;

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
};

}  // namespace org::apache::nifi::minifi::extensions::splunk

