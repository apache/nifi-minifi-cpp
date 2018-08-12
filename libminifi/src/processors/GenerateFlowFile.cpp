/**
 * @file GenerateFlowFile.cpp
 * GenerateFlowFile class implementation
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
#include "processors/GenerateFlowFile.h"
#include <time.h>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <chrono>
#include <thread>
#include <random>
#ifdef WIN32
#define srandom srand
#define random rand
#endif
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyValidation.h"
#include "core/TypedValues.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
const char *GenerateFlowFile::DATA_FORMAT_BINARY = "Binary";
const char *GenerateFlowFile::DATA_FORMAT_TEXT = "Text";
core::Property GenerateFlowFile::FileSize(
    core::PropertyBuilder::createProperty("File Size")->withDescription("The size of the file that will be used")->isRequired(false)->withDefaultValue<core::DataSizeValue>("1 kB")->build());

core::Property GenerateFlowFile::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("The number of FlowFiles to be transferred in each invocation")->isRequired(false)->withDefaultValue<int>(1)->build());

core::Property GenerateFlowFile::DataFormat(
    core::PropertyBuilder::createProperty("Data Format")->withDescription("Specifies whether the data should be Text or Binary")->isRequired(false)->withAllowableValue<std::string>("Text")
        ->withAllowableValue("Binary")->withDefaultValue("Binary")->build());

core::Property GenerateFlowFile::UniqueFlowFiles(
    core::PropertyBuilder::createProperty("Unique FlowFiles")->withDescription("If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());

core::Relationship GenerateFlowFile::Success("success", "success operational on the flow record");
const unsigned int TEXT_LEN = 90;
static const char TEXT_CHARS[TEXT_LEN + 1] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*()-_=+/?.,';:\"?<>\n\t ";

void GenerateFlowFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(FileSize);
  properties.insert(BatchSize);
  properties.insert(DataFormat);
  properties.insert(UniqueFlowFiles);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void GenerateFlowFile::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  uint64_t batchSize = 1;
  bool uniqueFlowFile = true;
  uint64_t fileSize = 1024;
  bool textData = false;

  std::string value;
  if (context->getProperty(FileSize.getName(), fileSize)) {
    logger_->log_trace("File size is configured to be %d", fileSize);
  }

  if (context->getProperty(BatchSize.getName(), batchSize)) {
    logger_->log_trace("Batch size is configured to be %d", batchSize);
  }
  if (context->getProperty(DataFormat.getName(), value)) {
    textData = (value == GenerateFlowFile::DATA_FORMAT_TEXT);
  }
  if (context->getProperty(UniqueFlowFiles.getName(), uniqueFlowFile)) {
    logger_->log_trace("Unique Flow files is configured to be %i", uniqueFlowFile);
  }

  if (uniqueFlowFile) {
    char *data;
    data = new char[fileSize];
    if (!data)
      return;
    uint64_t dataSize = fileSize;
    GenerateFlowFile::WriteCallback callback(data, dataSize);
    char *current = data;
    if (textData) {
      for (uint64_t i = 0; i < fileSize; i++) {
        int randValue = random();
        data[i] = TEXT_CHARS[randValue % TEXT_LEN];
      }
    } else {
      for (uint64_t i = 0; i < fileSize; i += sizeof(int)) {
        int randValue = random();
        *(reinterpret_cast<int*>(current)) = randValue;
        current += sizeof(int);
      }
    }
    for (uint64_t i = 0; i < batchSize; i++) {
      // For each batch
      std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
      if (!flowFile)
        return;
      if (fileSize > 0)
        session->write(flowFile, &callback);
      session->transfer(flowFile, Success);
    }
    delete[] data;
  } else {
    if (!_data) {
      // We have not created the data yet
      _data = new char[fileSize];
      _dataSize = fileSize;
      char *current = _data;
      if (textData) {
        for (uint64_t i = 0; i < fileSize; i++) {
          int randValue = random();
          _data[i] = TEXT_CHARS[randValue % TEXT_LEN];
        }
      } else {
        for (uint64_t i = 0; i < fileSize; i += sizeof(int)) {
          int randValue = random();
          *(reinterpret_cast<int*>(current)) = randValue;
          current += sizeof(int);
        }
      }
    }
    GenerateFlowFile::WriteCallback callback(_data, _dataSize);
    for (uint64_t i = 0; i < batchSize; i++) {
      // For each batch
      std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
      if (!flowFile)
        return;
      if (fileSize > 0)
        session->write(flowFile, &callback);
      session->transfer(flowFile, Success);
    }
  }
}
} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
