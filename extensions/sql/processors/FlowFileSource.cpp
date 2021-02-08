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

#include "FlowFileSource.h"

#include "FlowFile.h"
#include "data/JSONSQLWriter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const core::Property FlowFileSource::OutputFormat(
  core::PropertyBuilder::createProperty("Output Format")
  ->isRequired(true)
  ->supportsExpressionLanguage(true)
  ->withDefaultValue(toString(OutputType::JSONPretty))
  ->withAllowableValues<std::string>(OutputType::values())
  ->withDescription("Set the output format type.")->build());

const core::Property FlowFileSource::MaxRowsPerFlowFile(
  core::PropertyBuilder::createProperty("Max Rows Per Flow File")
  ->isRequired(true)
  ->supportsExpressionLanguage(true)
  ->withDefaultValue<uint64_t>(0)
  ->withDescription(
      "The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. "
      "If the value specified is zero, then all rows are returned in a single FlowFile.")->build());

const std::string FlowFileSource::FRAGMENT_IDENTIFIER = "fragment.identifier";
const std::string FlowFileSource::FRAGMENT_COUNT = "fragment.count";
const std::string FlowFileSource::FRAGMENT_INDEX = "fragment.index";

void FlowFileSource::FlowFileGenerator::endProcessBatch(State state) {
  if (state == State::DONE) {
    // annotate the flow files with the fragment.count
    std::string fragment_count = std::to_string(flow_files_.size());
    for (const auto& flow_file : flow_files_) {
      flow_file->addAttribute(FRAGMENT_COUNT, fragment_count);
    }
    return;
  }

  OutputStreamPipe writer{std::make_shared<io::BufferStream>(json_writer_.toString())};
  auto new_flow = session_.create();
  new_flow->addAttribute(FRAGMENT_INDEX, std::to_string(flow_files_.size()));
  new_flow->addAttribute(FRAGMENT_IDENTIFIER, batch_id_.to_string());
  session_.write(new_flow, &writer);
  flow_files_.push_back(std::move(new_flow));
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
