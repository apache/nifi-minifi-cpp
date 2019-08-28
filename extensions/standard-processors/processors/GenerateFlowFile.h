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
#ifndef __GENERATE_FLOW_FILE_H__
#define __GENERATE_FLOW_FILE_H__

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"

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
  GenerateFlowFile(std::string name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid), logger_(logging::LoggerFactory<GenerateFlowFile>::getLogger()) {
    batchSize_ = 1;
    uniqueFlowFile_ = true;
    fileSize_ = 1024;
    textData_ = false;
  }
  // Destructor
  virtual ~GenerateFlowFile() = default;
  // Processor Name
  static constexpr char const* ProcessorName = "GenerateFlowFile";
  // Supported Properties
  static core::Property FileSize;
  static core::Property BatchSize;
  static core::Property DataFormat;
  static core::Property UniqueFlowFiles;
  static const char *DATA_FORMAT_TEXT;
  // Supported Relationships
  static core::Relationship Success;
  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(std::vector<char> && data) : data_(std::move(data)) {
    }
    WriteCallback(const std::vector<char>& data) : data_(data) {
    }
    std::vector<char> data_;
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      int64_t ret = 0;
      if(data_.size() > 0)
        ret = stream->write(reinterpret_cast<uint8_t*>(&data_[0]), data_.size());
      return ret;
    }
  };

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  // OnTrigger method, implemented by NiFi GenerateFlowFile
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  // Initialize, over write by NiFi GenerateFlowFile
  virtual void initialize(void) override;

 protected:
  std::vector<char> data_;

  uint64_t batchSize_;
  bool uniqueFlowFile_;
  uint64_t fileSize_;
  bool textData_;

 private:

  // logger instance
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(GenerateFlowFile, "This processor creates FlowFiles with random data or custom content. GenerateFlowFile is useful for load testing, configuration, and simulation.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
