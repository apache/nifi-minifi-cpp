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

#ifdef OPENSSL_SUPPORT

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "HashContent.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Property HashContent::HashAttribute("Hash Attribute", "Attribute to store checksum to", "Checksum");
const core::Property HashContent::HashAlgorithm("Hash Algorithm", "Name of the algorithm used to generate checksum", "SHA256");
const core::Property HashContent::FailOnEmpty("Fail on empty", "Route to failure relationship in case of empty content", "false");

const core::Relationship HashContent::Success("success", "success operational on the flow record");
const core::Relationship HashContent::Failure("failure", "failure operational on the flow record");

void HashContent::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void HashContent::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  std::string value;

  attrKey_ = (context->getProperty(HashAttribute.getName(), value)) ? value : "Checksum";
  algoName_ = (context->getProperty(HashAlgorithm.getName(), value)) ? value : "SHA256";

  if (context->getProperty(FailOnEmpty.getName(), value)) {
    failOnEmpty_ = utils::StringUtils::toBool(value).value_or(false);
  } else {
    failOnEmpty_ = false;
  }

  std::transform(algoName_.begin(), algoName_.end(), algoName_.begin(), ::toupper);

  // Erase '-' to make sha-256 and sha-1 work, too
  algoName_.erase(std::remove(algoName_.begin(), algoName_.end(), '-'), algoName_.end());
}

void HashContent::onTrigger(core::ProcessContext *, core::ProcessSession *session) {
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    logger_->log_trace("No flow file");
    return;
  }

  if (failOnEmpty_ && flowFile->getSize() == 0) {
    logger_->log_debug("Failure as flow file is empty");
    session->transfer(flowFile, Failure);
    return;
  }

  logger_->log_trace("attempting read");
  session->read(flowFile, [&flowFile, this](const std::shared_ptr<io::InputStream>& stream) {
    // This throws in case algo is not found, but that's fine
    logger_->log_trace("Searching for %s", algoName_);
    auto algo = HashAlgos.at(algoName_);

    const auto& ret_val = algo(stream);

    flowFile->setAttribute(attrKey_, ret_val.first);

    return ret_val.second;
  });
  session->transfer(flowFile, Success);
}

REGISTER_RESOURCE(HashContent, Processor);

}  // namespace org::apache::nifi::minifi::processors

#endif  // OPENSSL_SUPPORT
