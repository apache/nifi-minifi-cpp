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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_EXTRACTTEXT_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_EXTRACTTEXT_H_

#include <memory>
#include <string>
#include <vector>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "FlowFileRecord.h"
#include "utils/Export.h"

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
    explicit ExtractText(const std::string& name,  const utils::Identifier& uuid = {})
    : Processor(name, uuid) {
        logger_ = logging::LoggerFactory<ExtractText>::getLogger();
    }
    //! Processor Name
    EXTENSIONAPI static constexpr char const* ProcessorName = "ExtractText";
    //! Supported Properties
    EXTENSIONAPI static core::Property Attribute;
    EXTENSIONAPI static core::Property SizeLimit;

    EXTENSIONAPI static core::Property RegexMode;
    EXTENSIONAPI static core::Property IgnoreCaptureGroupZero;
    EXTENSIONAPI static core::Property InsensitiveMatch;
    EXTENSIONAPI static core::Property MaxCaptureGroupLen;
    EXTENSIONAPI static core::Property EnableRepeatingCaptureGroup;

    //! Supported Relationships
    EXTENSIONAPI static core::Relationship Success;
    //! Default maximum bytes to read into an attribute
    EXTENSIONAPI static constexpr int DEFAULT_SIZE_LIMIT = 2 * 1024 * 1024;

    //! OnTrigger method, implemented by NiFi ExtractText
    void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
    //! Initialize, over write by NiFi ExtractText
    void initialize(void) override;

    bool supportsDynamicProperties() override {
      return true;
    }

    class ReadCallback : public InputStreamCallback {
     public:
        ReadCallback(std::shared_ptr<core::FlowFile> flowFile, core::ProcessContext *ct, std::shared_ptr<logging::Logger> lgr);
        ~ReadCallback() = default;
        int64_t process(const std::shared_ptr<io::BaseStream>& stream);

     private:
        std::shared_ptr<core::FlowFile> flowFile_;
        core::ProcessContext *ctx_;
        std::vector<uint8_t> buffer_;
        std::shared_ptr<logging::Logger> logger_;
    };

 private:
    core::annotation::Input getInputRequirement() const override {
      return core::annotation::Input::INPUT_REQUIRED;
    }

    //! Logger
    std::shared_ptr<logging::Logger> logger_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_EXTRACTTEXT_H_
