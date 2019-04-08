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
#include <iterator>
#include <string>
#include <memory>
#include <set>
#include <regex>

#include <iostream>
#include <sstream>

#include "processors/ExtractText.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"

#if !defined(_WIN32)
#if __cplusplus <= 201103L
#include <regex.h>
#endif
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define MAX_BUFFER_SIZE 4096
#define MAX_CAPTURE_GROUP_SIZE 1024

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

core::Property ExtractText::IgnoreCaptureGroupZero(
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
  //! Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Attribute);
  properties.insert(SizeLimit);
  properties.insert(RegexMode);
  properties.insert(IgnoreCaptureGroupZero);
  properties.insert(MaxCaptureGroupLen);
  properties.insert(EnableRepeatingCaptureGroup);
  properties.insert(InsensitiveMatch);
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void ExtractText::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  ReadCallback cb(flowFile, context, logger_);
  session->read(flowFile, &cb);
  session->transfer(flowFile, Success);
}

int64_t ExtractText::ReadCallback::process(std::shared_ptr<io::BaseStream> stream) {
  int64_t ret = 0;
  uint64_t read_size = 0;
  bool regex_mode;
  uint64_t size_limit = flowFile_->getSize();

  std::string attrKey, sizeLimitStr;
  ctx_->getProperty(Attribute.getName(), attrKey);
  ctx_->getProperty(SizeLimit.getName(), sizeLimitStr);
  ctx_->getProperty(RegexMode.getName(), regex_mode);

  if (sizeLimitStr == "")
    size_limit = DEFAULT_SIZE_LIMIT;
  else if (sizeLimitStr != "0")
    size_limit = std::stoi(sizeLimitStr);

  std::ostringstream contentStream;

  while (read_size < size_limit) {
    // Don't read more than config limit or the size of the buffer
    ret = stream->readData(buffer_, std::min<uint64_t>((size_limit - read_size), buffer_.capacity()));

    if (ret < 0) {
      return -1;  // Stream error
    } else if (ret == 0) {
      break;  // End of stream, no more data
    }

    contentStream.write(reinterpret_cast<const char*>(&buffer_[0]), ret);
    read_size += ret;
    if (contentStream.fail()) {
      return -1;
    }
  }

  if (regex_mode) {
    std::regex_constants::syntax_option_type regex_mode = std::regex_constants::ECMAScript;

    bool insensitive;
    if (ctx_->getProperty(InsensitiveMatch.getName(), insensitive) && insensitive) {
      regex_mode |= std::regex_constants::icase;
    }

    bool ignoregroupzero;
    ctx_->getProperty(IgnoreCaptureGroupZero.getName(), ignoregroupzero);

    bool repeatingcapture;
    ctx_->getProperty(EnableRepeatingCaptureGroup.getName(), repeatingcapture);

    int maxCaptureSize;
    ctx_->getProperty(MaxCaptureGroupLen.getName(), maxCaptureSize);

    std::string contentStr = contentStream.str();

    std::map<std::string, std::string> regexAttributes;

    for (const auto& k : ctx_->getDynamicPropertyKeys()) {
      std::string value;
      ctx_->getDynamicProperty(k, value);

      std::string workStr = contentStr;

      int matchcount = 0;

#if (__cplusplus > 201103L) || defined(_WIN32)

      std::regex rgx;

      try {
        rgx = std::regex(value, regex_mode);
      } catch(const std::regex_error& e) {
        logger_->log_error("%s error encountered when trying to construct regular expression from property (key: %s) value: %s",
            e.what(), k, value);
        continue;
      }

      std::smatch matches;

      while (std::regex_search(workStr, matches, rgx)) {
        size_t i = ignoregroupzero ? 1 : 0;

        for (; i < matches.size(); ++i, ++matchcount) {
          std::string attributeValue = matches[i].str();
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
#else

      size_t maxGroups = std::count(value.begin(), value.end(), '(') + 1;

      regex_t regexCompiled;
      std::vector<regmatch_t> groups;
      groups.reserve(maxGroups);

      if (regcomp(&regexCompiled, value.c_str(), REG_EXTENDED | (insensitive ? REG_ICASE : 0))) {
        logger_->log_error("error encountered when trying to construct regular expression from property (key: %s) value: %s",
                            k, value);
        continue;
      }

      while (regexec(&regexCompiled, workStr.c_str(), groups.capacity(), groups.data(), 0) == 0) {
        size_t g = 0;
        size_t match_len = 0;
        for (g = 0; g < maxGroups; g++) {
          if (groups[g].rm_so == -1) {
            break;  // No more groups
          }

          if (g == 0) {
            match_len = groups[g].rm_eo;
            if (ignoregroupzero) {
              continue;
            }
          }

          std::string attributeValue(workStr.begin() + groups[g].rm_so, workStr.begin() + groups[g].rm_eo);
          if (attributeValue.length() > maxCaptureSize) {
            attributeValue = attributeValue.substr(0, maxCaptureSize);
          }

          if (matchcount == 0) {
            regexAttributes[k] = attributeValue;
          }
          regexAttributes[k + '.' + std::to_string(matchcount)] = attributeValue;
          matchcount++;
        }
        if (!repeatingcapture || (match_len >= workStr.length())) {
          break;
        }
        workStr = workStr.substr(match_len + 1);
      }
#endif
    }

    for (const auto& kv : regexAttributes) {
      flowFile_->setAttribute(kv.first, kv.second);
    }
  } else {
    flowFile_->setAttribute(attrKey, contentStream.str());
  }
  return read_size;
}

ExtractText::ReadCallback::ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ctx,  std::shared_ptr<logging::Logger> lgr)
    : flowFile_(flowFile),
      ctx_(ctx),
      logger_(lgr) {
  buffer_.reserve(std::min<uint64_t>(flowFile->getSize(), MAX_BUFFER_SIZE));
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
