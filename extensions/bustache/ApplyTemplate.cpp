// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "ApplyTemplate.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "bustache/model.hpp"
#include "bustache/render/string.hpp"
#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

void ApplyTemplate::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ApplyTemplate::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    return;
  }

  std::string template_file = context.getProperty(Template.name, flow_file.get()).value_or("");
  session.write(flow_file, [&template_file, &flow_file, this](const auto& output_stream) -> io::IoResult {
    logger_->log_info("ApplyTemplate reading template file from {}", template_file);
    // TODO(szaszm): we might want to return to memory-mapped input files when the next todo is done. Until then, the agents stores the whole result in memory anyway, so no point in not doing the same
    // with the template file itself
    const auto template_file_contents = [&] {
      std::ifstream ifs{template_file};
      return std::string{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
    }();

    bustache::format format(template_file_contents);
    std::unordered_map<std::string, std::string> data;

    for (const auto &attr : flow_file->getAttributes()) {
      data[attr.first] = attr.second;
    }

    // TODO(calebj) write ostream reciever for format() to prevent excessive copying
    std::string ostring = bustache::to_string(format(data));
    output_stream->write(std::as_bytes(std::span(ostring)));
    return io::IoResult::from(ostring.length());
  });
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(ApplyTemplate, Processor);

}  // namespace org::apache::nifi::minifi::processors
