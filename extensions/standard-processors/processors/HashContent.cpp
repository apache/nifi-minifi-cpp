/**
 * @file HashContent.cpp
 * HashContent class implementation
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
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "HashContent.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/FlowFile.h"
#include "core/Resource.h"

#include "range/v3/view.hpp"

namespace org::apache::nifi::minifi::processors {

void HashContent::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void HashContent::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  attrKey_ = context.getProperty(HashAttribute) | utils::orThrow("Missing HashContent::HashAttribute despite default value");
  failOnEmpty_ = context.getProperty(FailOnEmpty) | utils::andThen(parsing::parseBool) | utils::orThrow("Missing HashContent::FailOnEmpty despite default value");

  {
    std::string algo_name = context.getProperty(HashAlgorithm) | utils::orThrow("HashContent::HashAlgorithm is required property");
    std::transform(algo_name.begin(), algo_name.end(), algo_name.begin(), ::toupper);
    std::erase(algo_name, '-');
    if (!HashAlgos.contains(algo_name)) {
      const auto supported_algorithms = ranges::views::keys(HashAlgos) | ranges::views::join(std::string_view(", ")) | ranges::to<std::string>();
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, algo_name + " is not supported, supported algorithms are: " + supported_algorithms);
    }
    algorithm_ = HashAlgos.at(algo_name);
  }
}

void HashContent::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  std::shared_ptr<core::FlowFile> flowFile = session.get();

  if (!flowFile) {
    logger_->log_trace("No flow file");
    return;
  }

  if (failOnEmpty_ && flowFile->getSize() == 0) {
    logger_->log_debug("Failure as flow file is empty");
    session.transfer(flowFile, Failure);
    return;
  }

  logger_->log_trace("attempting read");
  session.read(flowFile, [&flowFile, this](const std::shared_ptr<io::InputStream>& stream) {
    const auto& ret_val = algorithm_(stream);

    flowFile->setAttribute(attrKey_, ret_val.first);

    return ret_val.second;
  });
  session.transfer(flowFile, Success);
}

REGISTER_RESOURCE(HashContent, Processor);

}  // namespace org::apache::nifi::minifi::processors
