/**
 * @file GenerateFlowFile.h
 * GenerateFlowFile class declaration
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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_GENERATEFLOWFILE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_GENERATEFLOWFILE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
// GenerateFlowFile Class
class GenerateFlowFile : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  GenerateFlowFile(const std::string& name, const utils::Identifier& uuid = {}) // NOLINT
      : Processor(name, uuid), logger_(logging::LoggerFactory<GenerateFlowFile>::getLogger()) {
    batchSize_ = 1;
    uniqueFlowFile_ = true;
    fileSize_ = 1024;
    textData_ = false;
  }
  // Destructor
  ~GenerateFlowFile() override = default;
  // Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "GenerateFlowFile";
  // Supported Properties
  EXTENSIONAPI static core::Property FileSize;
  EXTENSIONAPI static core::Property BatchSize;
  EXTENSIONAPI static core::Property DataFormat;
  EXTENSIONAPI static core::Property UniqueFlowFiles;
  EXTENSIONAPI static core::Property CustomText;
  EXTENSIONAPI static const char *DATA_FORMAT_TEXT;
  // Supported Relationships
  EXTENSIONAPI static core::Relationship Success;
  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(const std::vector<char>& data)
        :data_(&data)
    { }
    gsl::not_null<const std::vector<char>*> data_;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      if (data_->empty()) return 0;
      const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(data_->data()), data_->size());
      return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
    }
  };

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  // OnTrigger method, implemented by NiFi GenerateFlowFile
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  // Initialize, over write by NiFi GenerateFlowFile
  void initialize() override;

 protected:
  std::vector<char> data_;

  uint64_t batchSize_;
  bool uniqueFlowFile_;
  uint64_t fileSize_;
  bool textData_;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  // logger instance
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_GENERATEFLOWFILE_H_
