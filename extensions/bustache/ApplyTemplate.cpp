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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "core/Resource.h"
#include "bustache/model.hpp"

namespace org::apache::nifi::minifi::processors {

const core::Property ApplyTemplate::Template("Template", "Path to the input mustache template file", "");
const core::Relationship ApplyTemplate::Success("success", "success operational on the flow record");

void ApplyTemplate::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ApplyTemplate::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) {
  auto flow_file = session->get();
  if (!flow_file) {
    return;
  }

  std::string template_file;
  context->getProperty(Template, template_file, flow_file);
  session->write(flow_file, [&template_file, &flow_file, this](const auto& output_stream) {
    logger_->log_info("ApplyTemplate reading template file from %s", template_file);
    // TODO(szaszm): we might want to return to memory-mapped input files when the next todo is done. Until then, the agents stores the whole result in memory anyway, so no point in not doing the same
    // with the template file itself
    const auto template_file_contents = [&] {
      std::ifstream ifs{template_file};
      return std::string{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
    }();

    bustache::format format(template_file_contents);
    bustache::object data;

    for (const auto &attr : flow_file->getAttributes()) {
      data[attr.first] = attr.second;
    }

    // TODO(calebj) write ostream reciever for format() to prevent excessive copying
    std::string ostring = to_string(format(data));
    output_stream->write(gsl::make_span(ostring).as_span<const std::byte>());
    return gsl::narrow<int64_t>(ostring.length());
  });
  session->transfer(flow_file, Success);
}

REGISTER_RESOURCE(ApplyTemplate, Processor);

}  // namespace org::apache::nifi::minifi::processors
