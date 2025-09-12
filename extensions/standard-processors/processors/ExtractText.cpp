/**
 * @file ExtractText.cpp
 * ExtractText class implementation
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
#include "ExtractText.h"

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ConfigurationUtils.h"
#include "utils/RegexUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void ExtractText::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ExtractText::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  std::shared_ptr<core::FlowFile> flowFile = session.get();

  if (!flowFile) {
    return;
  }

  session.read(flowFile, ReadCallback{flowFile, context, logger_});
  session.transfer(flowFile, Success);
}

int64_t ExtractText::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) const {
  const auto flow_file_size = gsl::narrow<size_t>(flowFile_->getSize());
  const auto default_buffer_size = utils::configuration::getBufferSize(*ctx_->getConfiguration());
  std::vector<std::byte> buffer((std::min)(flow_file_size, default_buffer_size));

  std::string attrKey = utils::parseOptionalProperty(*ctx_, Attribute).value_or("");
  bool regex_mode = utils::parseBoolProperty(*ctx_, RegexMode);
  auto size_limit = gsl::narrow<size_t>(utils::parseU64Property(*ctx_, SizeLimit));
  if (size_limit == 0) {
    size_limit = flow_file_size;
  }

  std::ostringstream contentStream;

  size_t read_size = 0;
  while (read_size < size_limit) {
    // Don't read more than config limit or the size of the buffer
    const auto length = (std::min)(size_limit - read_size, buffer.size());
    const auto ret = stream->read(std::span(buffer).subspan(0, length));

    if (io::isError(ret)) {
      return -1;  // Stream error
    } else if (ret == 0) {
      break;  // End of stream, no more data
    }

    contentStream.write(reinterpret_cast<const char*>(buffer.data()), gsl::narrow<std::streamsize>(ret));
    read_size += ret;
    if (contentStream.fail()) {
      return -1;
    }
  }

  if (regex_mode) {
    const bool insensitive = utils::parseBoolProperty(*ctx_, InsensitiveMatch);
    const bool include_capture_group_zero = utils::parseBoolProperty(*ctx_, IncludeCaptureGroupZero);
    const bool repeating_capture = utils::parseBoolProperty(*ctx_, EnableRepeatingCaptureGroup);
    const auto max_capture_size = gsl::narrow<size_t>(utils::parseU64Property(*ctx_, MaxCaptureGroupLen));
    std::vector<utils::Regex::Mode> regex_flags;
    if (insensitive) {
      regex_flags.push_back(utils::Regex::Mode::ICASE);
    }

    std::string contentStr = contentStream.str();

    std::map<std::string, std::string> regexAttributes;

    for (const auto& [dynamic_property_key, dynamic_property_value] : ctx_->getDynamicProperties()) {
      std::string workStr = contentStr;

      int matchcount = 0;

      try {
        utils::Regex rgx(dynamic_property_value, regex_flags);
        utils::SMatch matches;
        while (utils::regexSearch(workStr, matches, rgx)) {
          for (std::size_t i = (include_capture_group_zero ? 0 : 1); i < matches.size(); ++i, ++matchcount) {
            std::string attributeValue = matches[i];
            if (attributeValue.length() > max_capture_size) {
              attributeValue = attributeValue.substr(0, max_capture_size);
            }
            if (matchcount == 0) {
              regexAttributes[dynamic_property_key] = attributeValue;
            }
            regexAttributes[dynamic_property_key + '.' + std::to_string(matchcount)] = attributeValue;
          }
          if (!repeating_capture) {
            break;
          }
          workStr = matches.suffix();
        }
      } catch (const Exception &e) {
        logger_->log_error("{} error encountered when trying to construct regular expression from property (key: {}) value: {}",
                           e.what(), dynamic_property_key, dynamic_property_value);
        continue;
      }
    }

    for (const auto& kv : regexAttributes) {
      flowFile_->setAttribute(kv.first, kv.second);
    }
  } else {
    flowFile_->setAttribute(attrKey, contentStream.str());
  }
  return gsl::narrow<int64_t>(read_size);
}

ExtractText::ReadCallback::ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext& ctx,  std::shared_ptr<core::logging::Logger> lgr)
    : flowFile_(std::move(flowFile)),
      ctx_(gsl::make_not_null(&ctx)),
      logger_(std::move(lgr)) {
}

REGISTER_RESOURCE(ExtractText, Processor);

}  // namespace org::apache::nifi::minifi::processors
