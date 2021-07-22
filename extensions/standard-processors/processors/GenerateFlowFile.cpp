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

#include "GenerateFlowFile.h"

#include <time.h>

#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "utils/gsl.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyValidation.h"
#include "core/TypedValues.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
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

core::Property GenerateFlowFile::CustomText(
    core::PropertyBuilder::createProperty("Custom Text")
      ->withDescription("If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. "
                        "Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles")
      ->supportsExpressionLanguage(true)
      ->build());

core::Relationship GenerateFlowFile::Success("success", "success operational on the flow record");

constexpr const char * TEXT_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*()-_=+/?.,';:\"?<>\n\t ";

void GenerateFlowFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(FileSize);
  properties.insert(BatchSize);
  properties.insert(DataFormat);
  properties.insert(UniqueFlowFiles);
  properties.insert(CustomText);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void generateData(std::vector<char>& data, bool textData = false) {
  std::random_device rd;
  std::mt19937 eng(rd());
  if (textData) {
    const int index_of_last_char = gsl::narrow<int>(strlen(TEXT_CHARS)) - 1;
    std::uniform_int_distribution<> distr(0, index_of_last_char);
    std::generate_n(data.begin(), data.size(), [&] { return TEXT_CHARS[static_cast<uint8_t>(distr(eng))]; });
  } else {
    std::uniform_int_distribution<> distr(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
    auto rand = std::bind(distr, eng);
    std::generate_n(data.begin(), data.size(), rand);
  }
}

void GenerateFlowFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  if (context->getProperty(FileSize.getName(), fileSize_)) {
    logger_->log_trace("File size is configured to be %d", fileSize_);
  }

  if (context->getProperty(BatchSize.getName(), batchSize_)) {
    logger_->log_trace("Batch size is configured to be %d", batchSize_);
  }

  std::string value;
  if (context->getProperty(DataFormat.getName(), value)) {
    textData_ = (value == GenerateFlowFile::DATA_FORMAT_TEXT);
  }
  if (context->getProperty(UniqueFlowFiles.getName(), uniqueFlowFile_)) {
    logger_->log_trace("Unique Flow files is configured to be %i", uniqueFlowFile_);
  }

  std::string custom_text;
  context->getProperty(CustomText, custom_text, nullptr);
  if (!custom_text.empty()) {
    if (textData_ && !uniqueFlowFile_) {
      data_.assign(custom_text.begin(), custom_text.end());
      return;
    } else {
      logger_->log_warn("Custom Text property is set, but not used!");
    }
  }

  if (!uniqueFlowFile_) {
    data_.resize(gsl::narrow<size_t>(fileSize_));
    generateData(data_, textData_);
  }
}

void GenerateFlowFile::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession *session) {
  for (uint64_t i = 0; i < batchSize_; i++) {
    // For each batch
    std::shared_ptr<core::FlowFile> flowFile = session->create();
    if (!flowFile) {
      logger_->log_error("Failed to create flowfile!");
      return;
    }
    if (uniqueFlowFile_) {
      std::vector<char> data(gsl::narrow<size_t>(fileSize_));
      if (fileSize_ > 0) {
        generateData(data, textData_);
      }
      GenerateFlowFile::WriteCallback callback(data);
      session->write(flowFile, &callback);
    } else {
      GenerateFlowFile::WriteCallback callback(data_);
      session->write(flowFile, &callback);
    }
    session->transfer(flowFile, Success);
  }
}

REGISTER_RESOURCE(GenerateFlowFile, "This processor creates FlowFiles with random data or custom content. GenerateFlowFile is useful for load testing, configuration, and simulation.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
