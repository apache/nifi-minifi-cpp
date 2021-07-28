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
#include <set>
#include <string>

#include "HashContent.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property HashContent::HashAttribute("Hash Attribute", "Attribute to store checksum to", "Checksum");
core::Property HashContent::HashAlgorithm("Hash Algorithm", "Name of the algorithm used to generate checksum", "SHA256");
core::Property HashContent::FailOnEmpty("Fail on empty", "Route to failure relationship in case of empty content", "false");
core::Relationship HashContent::Success("success", "success operational on the flow record");
core::Relationship HashContent::Failure("failure", "failure operational on the flow record");

void HashContent::initialize() {
  //! Set the supported properties
  std::set<core::Property> properties;
  properties.insert(HashAttribute);
  properties.insert(HashAlgorithm);
  properties.insert(FailOnEmpty);
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
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
  ReadCallback cb(flowFile, *this);
  session->read(flowFile, &cb);
  session->transfer(flowFile, Success);
}

int64_t HashContent::ReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  // This throws in case algo is not found, but that's fine
  parent_.logger_->log_trace("Searching for %s", parent_.algoName_);
  auto algo = HashAlgos.at(parent_.algoName_);

  const auto& ret_val = algo(stream);

  flowFile_->setAttribute(parent_.attrKey_, ret_val.first);

  return ret_val.second;
}

HashContent::ReadCallback::ReadCallback(std::shared_ptr<core::FlowFile> flowFile, const HashContent& parent)
  : flowFile_(flowFile),
    parent_(parent)
  {}

REGISTER_RESOURCE(HashContent,"HashContent calculates the checksum of the content of the flowfile and adds it as an attribute. Configuration options exist to select hashing algorithm and set the name of the attribute."); // NOLINT

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // OPENSSL_SUPPORT
