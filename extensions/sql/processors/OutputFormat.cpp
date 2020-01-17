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

#include "OutputFormat.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const char* const OutputFormat::s_outputFormatJSON;
const char* const OutputFormat::s_outputFormatJSONPretty;

const core::Property& OutputFormat::outputFormat() {
  static const core::Property s_outputFormat =
      core::PropertyBuilder::createProperty("Output Format")->
          isRequired(true)->
          withDefaultValue(s_outputFormatJSONPretty)->
          withAllowableValues<std::string>({ s_outputFormatJSON, s_outputFormatJSONPretty })->
          withDescription("Set the output format type.")->
          build();

  return s_outputFormat;
}

bool OutputFormat::isJSONFormat() const {
  return outputFormat_ == s_outputFormatJSON || outputFormat_ == s_outputFormatJSONPretty;
}

bool OutputFormat::isJSONPretty() const {
  return outputFormat_ == s_outputFormatJSONPretty;
}

void OutputFormat::initOutputFormat(const std::shared_ptr<core::ProcessContext>& context) {
  context->getProperty(outputFormat().getName(), outputFormat_);
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
