/**
 * @file CompressContent.cpp
 * CompressContent class implementation
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
#include "CompressContent.h"
#include <stdio.h>
#include <algorithm>
#include <memory>
#include <string>
#include <map>
#include <set>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property CompressContent::CompressLevel("Compression Level", "The compression level to use; this is valid only when using GZIP compression.", "1");
core::Property CompressContent::CompressMode("Mode", "Indicates whether the processor should compress content or decompress content.", MODE_COMPRESS);
core::Property CompressContent::CompressFormat("Compression Format", "The compression format to use.", COMPRESSION_FORMAT_ATTRIBUTE);
core::Property CompressContent::UpdateFileName("Update Filename", "Determines if filename extension need to be updated", "false");
core::Relationship CompressContent::Success("success", "FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed");
core::Relationship CompressContent::Failure("failure", "FlowFiles will be transferred to the failure relationship if they fail to compress/decompress");

void CompressContent::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(CompressLevel);
  properties.insert(CompressMode);
  properties.insert(CompressFormat);
  properties.insert(UpdateFileName);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void CompressContent::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  std::string value;
  if (context->getProperty(CompressLevel.getName(), value) && !value.empty()) {
    core::Property::StringToInt(value, compressLevel_);
  }
  value = "";
  if (context->getProperty(CompressMode.getName(), value) && !value.empty()) {
    this->compressMode_ = value;
  }
  value = "";
  if (context->getProperty(CompressFormat.getName(), value) && !value.empty()) {
    this->compressFormat_ = value;
  }
  value = "";
  if (context->getProperty(UpdateFileName.getName(), value) && !value.empty()) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, updateFileName_);
  }
  logger_->log_info("Compress Content: Mode [%s] Format [%s] Level [%d] UpdateFileName [%d]", compressMode_, compressFormat_, compressLevel_, updateFileName_);
  // update the mimeTypeMap
  compressionFormatMimeTypeMap_["application/gzip"] = COMPRESSION_FORMAT_GZIP;
  compressionFormatMimeTypeMap_["application/bzip2"] = COMPRESSION_FORMAT_BZIP2;
  compressionFormatMimeTypeMap_["application/x-bzip2"] = COMPRESSION_FORMAT_BZIP2;
  compressionFormatMimeTypeMap_["application/x-lzma"] = COMPRESSION_FORMAT_LZMA;
  compressionFormatMimeTypeMap_["application/x-xz"] = COMPRESSION_FORMAT_XZ_LZMA2;
  fileExtension_[COMPRESSION_FORMAT_GZIP] = ".gz";
  fileExtension_[COMPRESSION_FORMAT_LZMA] = ".lzma";
  fileExtension_[COMPRESSION_FORMAT_BZIP2] = ".bz2";
  fileExtension_[COMPRESSION_FORMAT_XZ_LZMA2] = ".xz";
}

void CompressContent::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  std::string compressFormat = compressFormat_;
  if (compressFormat_ == COMPRESSION_FORMAT_ATTRIBUTE) {
    std::string attr;
    flowFile->getAttribute(FlowAttributeKey(MIME_TYPE), attr);
    if (attr.empty()) {
      logger_->log_error("No %s attribute existed for the flow, route to failure", FlowAttributeKey(MIME_TYPE));
      session->transfer(flowFile, Failure);
      return;
    }
    auto search = compressionFormatMimeTypeMap_.find(attr);
    if (search != compressionFormatMimeTypeMap_.end()) {
      compressFormat = search->second;
    } else {
      logger_->log_info("Mime type of %s is not indicated a support format, route to success", attr);
      session->transfer(flowFile, Success);
      return;
    }
  }
  std::transform(compressFormat.begin(), compressFormat.end(), compressFormat.begin(), ::tolower);
  std::string mimeType;
  if (compressFormat == COMPRESSION_FORMAT_GZIP) {
    mimeType = "application/gzip";
  } else if (compressFormat == COMPRESSION_FORMAT_BZIP2) {
    mimeType = "application/bzip2";
  } else if (compressFormat == COMPRESSION_FORMAT_LZMA) {
    mimeType = "application/x-lzma";
  } else if (compressFormat == COMPRESSION_FORMAT_XZ_LZMA2) {
    mimeType = "application/x-xz";
  } else {
    logger_->log_error("Compress format is invalid %s", compressFormat);
    session->transfer(flowFile, Failure);
    return;
  }

  std::string fileExtension;
  auto search = fileExtension_.find(compressFormat);
  if (search != fileExtension_.end()) {
    fileExtension = search->second;
  }
  std::shared_ptr<core::FlowFile> processFlowFile = session->create(flowFile);
  CompressContent::WriteCallback callback(compressMode_, compressLevel_, compressFormat, flowFile, session);
  session->write(processFlowFile, &callback);

  if (callback.status_ < 0) {
    logger_->log_error("Compress Content processing fail for the flow with UUID %s", flowFile->getUUIDStr());
    session->transfer(flowFile, Failure);
    session->remove(processFlowFile);
  } else {
    std::string fileName;
    processFlowFile->getAttribute(FlowAttributeKey(FILENAME), fileName);
    if (compressMode_ == MODE_COMPRESS) {
      session->putAttribute(processFlowFile, FlowAttributeKey(MIME_TYPE), mimeType);
      if (updateFileName_) {
        fileName = fileName + fileExtension;
        session->putAttribute(processFlowFile, FlowAttributeKey(FILENAME), fileName);
      }
    } else {
      session->removeAttribute(processFlowFile, FlowAttributeKey(MIME_TYPE));
      if (updateFileName_) {
        if (fileName.size() >= fileExtension.size() && fileName.compare(fileName.size() - fileExtension.size(), fileExtension.size(), fileExtension) == 0) {
          fileName = fileName.substr(0, fileName.size() - fileExtension.size());
          session->putAttribute(processFlowFile, FlowAttributeKey(FILENAME), fileName);
        }
      }
    }
    logger_->log_debug("Compress Content processing success for the flow with UUID %s name %s", processFlowFile->getUUIDStr(), fileName);
    session->transfer(processFlowFile, Success);
    session->remove(flowFile);
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
