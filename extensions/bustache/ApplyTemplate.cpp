/**
 * @file ApplyTemplate.cpp
 * ApplyTemplate class implementation
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
#include "ApplyTemplate.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <set>
#include <string>

#include <boost/iostreams/device/mapped_file.hpp>

#include <bustache/model.hpp>

#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ApplyTemplate::Template("Template", "Path to the input mustache template file", "");
core::Relationship ApplyTemplate::Success("success", "success operational on the flow record");

void ApplyTemplate::initialize() {
  //! Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Template);
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void ApplyTemplate::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) {
  auto flow_file = session->get();

  if (!flow_file) {
    return;
  }

  std::string template_file;
  context->getProperty(Template, template_file, flow_file);
  WriteCallback cb(template_file, flow_file);
  session->write(flow_file, &cb);
  session->transfer(flow_file, Success);
}

ApplyTemplate::WriteCallback::WriteCallback(const std::string &path, const std::shared_ptr<core::FlowFile> &flow_file) {
  logger_ = logging::LoggerFactory<ApplyTemplate::WriteCallback>::getLogger();
  template_file_ = path;
  flow_file_ = flow_file;
}

int64_t ApplyTemplate::WriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  logger_->log_info("ApplyTemplate reading template file from %s", template_file_);
  boost::iostreams::mapped_file_source file(template_file_);

  bustache::format format(file);
  bustache::object data;

  for (const auto &attr : flow_file_->getAttributes()) {
    data[attr.first] = attr.second;
  }

  // TODO(calebj) write ostream reciever for format() to prevent excessive copying
  std::string ostring = to_string(format(data));
  stream->write(reinterpret_cast<const uint8_t *>(ostring.c_str()), ostring.length());

  return ostring.length();
}

REGISTER_RESOURCE(ApplyTemplate, "Applies the mustache template specified by the \"Template\" property and writes the output to the flow file content. "
    "FlowFile attributes are used as template parameters.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
