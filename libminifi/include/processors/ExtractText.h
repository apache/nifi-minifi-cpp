/**
 * @file ExtractText.h
 * ExtractText class declaration
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
#ifndef __EXTRACT_TEXT_H__
#define __EXTRACT_TEXT_H__

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

//! ExtractText Class
class ExtractText : public core::Processor {
public:
    //! Constructor
    /*!
     * Create a new processor
     */
    explicit ExtractText(std::string name,  utils::Identifier uuid = utils::Identifier())
    : Processor(name, uuid)
    {
        logger_ = logging::LoggerFactory<ExtractText>::getLogger();
    }
    //! Processor Name
    static constexpr char const* ProcessorName = "ExtractText";
    //! Supported Properties
    static core::Property Attribute;
    static core::Property SizeLimit;
    //! Supported Relationships
    static core::Relationship Success;
    //! Default maximum bytes to read into an attribute
    static constexpr int DEFAULT_SIZE_LIMIT = 2 * 1024 * 1024;

    //! OnTrigger method, implemented by NiFi ExtractText
    void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
    //! Initialize, over write by NiFi ExtractText
    void initialize(void) override;

    class ReadCallback : public InputStreamCallback {
    public:
        ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ct);
        ~ReadCallback() {}
        int64_t process(std::shared_ptr<io::BaseStream> stream);

    private:
        std::shared_ptr<core::FlowFile> flowFile_;
        core::ProcessContext *ctx_;
        std::vector<uint8_t> buffer_;
    };

protected:

private:
    //! Logger
    std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(ExtractText,"Extracts the content of a FlowFile and places it into an attribute.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
