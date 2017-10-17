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

#include "processors/ExtractText.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ExtractText::Attribute("Attribute", "Attribute to set from content (TEMPORARY)", "");
core::Relationship ExtractText::Success("success", "success operational on the flow record");

void ExtractText::initialize() {
    //! Set the supported properties
    std::set<core::Property> properties;
    properties.insert(Attribute);
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

    std::string attrKey;
    _ctx->getProperty(Attribute.getName(), attrKey);
    std::stringstream contentStream(std::stringstream::out | std::stringstream::in);
    std::string contentStr;

    while (read_size < _flowFile->getSize()) {
        ret = stream->read(_buffer, _max_read);
        if (ret < 0) {
            return -1;
        }

        if (ret > 0) {
            contentStream.write(reinterpret_cast<const char*>(_buffer), ret);
            if (contentStream.fail()) {
                return -1;
            }
            read_size += ret;
        } else {
            break;
        }
    }

    contentStr = contentStream.str();
    _flowFile->setAttribute(attrKey, contentStr);
    return read_size;
}

ExtractText::ReadCallback::ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ctx) {
    _max_read = getpagesize() * sizeof(uint8_t);
    _buffer = new uint8_t[_max_read];
    logger_ = logging::LoggerFactory<ReadCallback>::getLogger();
    _flowFile = flowFile;
    _ctx = ctx;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
