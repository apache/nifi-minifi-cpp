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

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
