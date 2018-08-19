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

#include <iostream>
#include <sstream>

#include "processors/ExtractText.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define MAX_BUFFER_SIZE 4096

core::Property ExtractText::Attribute(core::PropertyBuilder::createProperty("Attribute")->withDescription("Attribute to set from content")->build());

// despite there being a size value, ExtractText was initially built with a numeric for this property
core::Property ExtractText::SizeLimit(
    core::PropertyBuilder::createProperty("Size Limit")->withDescription("Maximum number of bytes to read into the attribute. 0 for no limit. Default is 2MB.")->withDefaultValue<uint32_t>(
        DEFAULT_SIZE_LIMIT)->build());

core::Relationship ExtractText::Success("success", "success operational on the flow record");

void ExtractText::initialize() {
  //! Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Attribute);
  properties.insert(SizeLimit);
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

  ReadCallback cb(flowFile, context);
  session->read(flowFile, &cb);
  session->transfer(flowFile, Success);
}

int64_t ExtractText::ReadCallback::process(std::shared_ptr<io::BaseStream> stream) {
  int64_t ret = 0;
  uint64_t read_size = 0;
  uint64_t size_limit = flowFile_->getSize();

  std::string attrKey, sizeLimitStr;
  ctx_->getProperty(Attribute.getName(), attrKey);
  ctx_->getProperty(SizeLimit.getName(), sizeLimitStr);

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

  flowFile_->setAttribute(attrKey, contentStream.str());
  return read_size;
}

ExtractText::ReadCallback::ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ctx)
    : flowFile_(flowFile),
      ctx_(ctx) {
  buffer_.reserve(std::min<uint64_t>(flowFile->getSize(), MAX_BUFFER_SIZE));
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
