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
    _data = NULL;
    _dataSize = 0;
  }
  // Destructor
  virtual ~GenerateFlowFile() {
    if (_data)
      delete[] _data;
  }
  // Processor Name
  static constexpr char const* ProcessorName = "GenerateFlowFile";
  // Supported Properties
  static core::Property FileSize;
  static core::Property BatchSize;
  static core::Property DataFormat;
  static core::Property UniqueFlowFiles;
  static const char *DATA_FORMAT_BINARY;
  static const char *DATA_FORMAT_TEXT;
  // Supported Relationships
  static core::Relationship Success;
  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(char *data, uint64_t size)
        : _data(data),
          _dataSize(size) {
    }
    char *_data;
    uint64_t _dataSize;
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      int64_t ret = 0;
      if (_data && _dataSize > 0)
        ret = stream->write(reinterpret_cast<uint8_t*>(_data), _dataSize);
      return ret;
    }
  };

 public:
  // OnTrigger method, implemented by NiFi GenerateFlowFile
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  // Initialize, over write by NiFi GenerateFlowFile
  virtual void initialize(void);

 protected:

 private:
  // Generated data
  char * _data;
  // Size of the generated data
  uint64_t _dataSize;
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
