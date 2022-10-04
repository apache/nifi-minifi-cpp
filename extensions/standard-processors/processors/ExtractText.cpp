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
#include <algorithm>
#include <iterator>
#include <string>
#include <memory>
#include <map>
#include <sstream>
#include <utility>

#include "ExtractText.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "core/FlowFile.h"
#include "utils/gsl.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

constexpr size_t MAX_BUFFER_SIZE = 4096;
constexpr int MAX_CAPTURE_GROUP_SIZE = 1024;

core::Property ExtractText::Attribute(core::PropertyBuilder::createProperty("Attribute")->withDescription("Attribute to set from content")->build());

// despite there being a size value, ExtractText was initially built with a numeric for this property
core::Property ExtractText::SizeLimit(
    core::PropertyBuilder::createProperty("Size Limit")
    ->withDescription("Maximum number of bytes to read into the attribute. 0 for no limit. Default is 2MB.")
    ->withDefaultValue<uint32_t>(DEFAULT_SIZE_LIMIT)->build());

core::Property ExtractText::RegexMode(
    core::PropertyBuilder::createProperty("Regex Mode")
    ->withDescription("Set this to extract parts of flowfile content using regular experssions in dynamic properties")
    ->withDefaultValue<bool>(false)->build());

core::Property ExtractText::IncludeCaptureGroupZero(
    core::PropertyBuilder::createProperty("Include Capture Group 0")
    ->withDescription("Indicates that Capture Group 0 should be included as an attribute. "
                      "Capture Group 0 represents the entirety of the regular expression match, is typically not used, and could have considerable length.")
    ->withDefaultValue<bool>(true)->build());

core::Property ExtractText::InsensitiveMatch(
    core::PropertyBuilder::createProperty("Enable Case-insensitive Matching")
    ->withDescription("Indicates that two characters match even if they are in a different case. ")
    ->withDefaultValue<bool>(false)->build());

core::Property ExtractText::MaxCaptureGroupLen(
    core::PropertyBuilder::createProperty("Maximum Capture Group Length")
    ->withDescription("Specifies the maximum number of characters a given capture group value can have. "
                      "Any characters beyond the max will be truncated.")
    ->withDefaultValue<int>(MAX_CAPTURE_GROUP_SIZE)->build());


core::Property ExtractText::EnableRepeatingCaptureGroup(
    core::PropertyBuilder::createProperty("Enable repeating capture group")
    ->withDescription("f set to true, every string matching the capture groups will be extracted. "
                      "Otherwise, if the Regular Expression matches more than once, only the first match will be extracted.")
    ->withDefaultValue<bool>(false)->build());

core::Relationship ExtractText::Success("success", "success operational on the flow record");

void ExtractText::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ExtractText::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  session->read(flowFile, ReadCallback{flowFile, context, logger_});
  session->transfer(flowFile, Success);
}

int64_t ExtractText::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) const {
  size_t read_size = 0;
  bool regex_mode;
  size_t size_limit = flowFile_->getSize();
  std::vector<std::byte> buffer;
  buffer.resize(std::min(gsl::narrow<size_t>(flowFile_->getSize()), MAX_BUFFER_SIZE));

  std::string attrKey;
  std::string sizeLimitStr;
  ctx_->getProperty(Attribute.getName(), attrKey);
  ctx_->getProperty(SizeLimit.getName(), sizeLimitStr);
  ctx_->getProperty(RegexMode.getName(), regex_mode);

  if (sizeLimitStr.empty())
    size_limit = DEFAULT_SIZE_LIMIT;
  else if (sizeLimitStr != "0")
    size_limit = static_cast<size_t>(std::stoi(sizeLimitStr));

  std::ostringstream contentStream;

  while (read_size < size_limit) {
    // Don't read more than config limit or the size of the buffer
    const auto length = std::min(size_limit - read_size, buffer.size());
    const auto ret = stream->read(gsl::make_span(buffer).subspan(0, length));

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
    bool insensitive;
    std::vector<utils::Regex::Mode> regex_flags;
    if (ctx_->getProperty(InsensitiveMatch.getName(), insensitive) && insensitive) {
      regex_flags.push_back(utils::Regex::Mode::ICASE);
    }

    const bool include_capture_group_zero = ctx_->getProperty<bool>(IncludeCaptureGroupZero).value_or(true);

    bool repeatingcapture;
    ctx_->getProperty(EnableRepeatingCaptureGroup.getName(), repeatingcapture);

    const size_t maxCaptureSize = [this] {
      uint64_t val = 0;
      ctx_->getProperty(MaxCaptureGroupLen.getName(), val);
      return gsl::narrow<size_t>(val);
    }();

    std::string contentStr = contentStream.str();

    std::map<std::string, std::string> regexAttributes;

    for (const auto& k : ctx_->getDynamicPropertyKeys()) {
      std::string value;
      ctx_->getDynamicProperty(k, value);

      std::string workStr = contentStr;

      int matchcount = 0;

      try {
        utils::Regex rgx(value, regex_flags);
        utils::SMatch matches;
        while (utils::regexSearch(workStr, matches, rgx)) {
          for (std::size_t i = (include_capture_group_zero ? 0 : 1); i < matches.size(); ++i, ++matchcount) {
            std::string attributeValue = matches[i];
            if (attributeValue.length() > maxCaptureSize) {
              attributeValue = attributeValue.substr(0, maxCaptureSize);
            }
            if (matchcount == 0) {
              regexAttributes[k] = attributeValue;
            }
            regexAttributes[k + '.' + std::to_string(matchcount)] = attributeValue;
          }
          if (!repeatingcapture) {
            break;
          }
          workStr = matches.suffix();
        }
      } catch (const Exception &e) {
        logger_->log_error("%s error encountered when trying to construct regular expression from property (key: %s) value: %s",
                           e.what(), k, value);
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

ExtractText::ReadCallback::ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ctx,  std::shared_ptr<core::logging::Logger> lgr)
    : flowFile_(std::move(flowFile)),
      ctx_(ctx),
      logger_(std::move(lgr)) {
}

REGISTER_RESOURCE(ExtractText, Processor);

}  // namespace org::apache::nifi::minifi::processors
