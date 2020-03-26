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
core::Property CompressContent::EncapsulateInTar("Encapsulate in TAR", "If true, on compression the FlowFile is added to a TAR archive and then compressed, "
                                                                       "and on decompression a compressed, TAR-encapsulated FlowFile is expected.\n"
                                                                       "If false, on compression the content of the FlowFile simply gets compressed, and on decompression a simple compressed content is expected.\n"
                                                                       "true is the behaviour compatible with older MiNiFi C++ versions, false is the behaviour compatible with NiFi.",
                                                                       "true");
core::Relationship CompressContent::Success("success", "FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed");
core::Relationship CompressContent::Failure("failure", "FlowFiles will be transferred to the failure relationship if they fail to compress/decompress");

void CompressContent::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(CompressLevel);
  properties.insert(CompressMode);
  properties.insert(CompressFormat);
  properties.insert(UpdateFileName);
  properties.insert(EncapsulateInTar);
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
  value = "";
  if (context->getProperty(EncapsulateInTar.getName(), value) && !value.empty()) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, encapsulateInTar_);
  }
  logger_->log_info("Compress Content: Mode [%s] Format [%s] Level [%d] UpdateFileName [%d] EncapsulateInTar [%d]",
      compressMode_, compressFormat_, compressLevel_, updateFileName_, encapsulateInTar_);

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

  // Validate
  if (!encapsulateInTar_ && compressFormat != COMPRESSION_FORMAT_GZIP) {
    logger_->log_error("non-TAR encapsulated format only supports GZIP compression");
    session->transfer(flowFile, Failure);
    return;
  }
  if (compressFormat == COMPRESSION_FORMAT_BZIP2 && archive_bzlib_version() == nullptr) {
    logger_->log_error("%s compression format is requested, but the agent was compiled without BZip2 support", compressFormat);
    session->transfer(flowFile, Failure);
    return;
  }
  if ((compressFormat == COMPRESSION_FORMAT_LZMA || compressFormat == COMPRESSION_FORMAT_XZ_LZMA2) && archive_liblzma_version() == nullptr) {
    logger_->log_error("%s compression format is requested, but the agent was compiled without LZMA support ", compressFormat);
    session->transfer(flowFile, Failure);
    return;
  }

  std::string fileExtension;
  auto search = fileExtension_.find(compressFormat);
  if (search != fileExtension_.end()) {
    fileExtension = search->second;
  }
  std::shared_ptr<core::FlowFile> processFlowFile = session->create(flowFile);
  bool success = false;
  if (encapsulateInTar_) {
    CompressContent::WriteCallback callback(compressMode_, compressLevel_, compressFormat, flowFile, session);
    session->write(processFlowFile, &callback);
    success = callback.status_ >= 0;
  } else {
    CompressContent::GzipWriteCallback callback(compressMode_, compressLevel_, flowFile, session);
    session->write(processFlowFile, &callback);
    success = callback.success_;
  }

  if (!success) {
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
