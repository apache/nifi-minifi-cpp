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
    ExtractText(std::string name, uuid_t uuid = NULL)
    : Processor(name, uuid)
    {
        logger_ = logging::LoggerFactory<ExtractText>::getLogger();
    }
    //! Destructor
    virtual ~ExtractText()
    {
    }
    //! Processor Name
    static constexpr char const* ProcessorName = "MergeContent";
    //! Supported Properties
    static core::Property Attribute;
    //! Supported Relationships
    static core::Relationship Success;

    //! OnTrigger method, implemented by NiFi ExtractText
    virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
    //! Initialize, over write by NiFi ExtractText
    virtual void initialize(void);

    class ReadCallback : public InputStreamCallback {
    public:
        ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ct);
        ~ReadCallback() { delete[] _buffer; }
        int64_t process(std::shared_ptr<io::BaseStream> stream);

    private:
        std::shared_ptr<logging::Logger> logger_;
        std::shared_ptr<core::FlowFile> _flowFile;
        core::ProcessContext *_ctx;
        uint8_t *_buffer;
        int64_t _max_read;
    };

protected:

private:
    //! Logger
    std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(ExtractText);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
