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

core::Property CompressContent::CompressLevel(
    core::PropertyBuilder::createProperty("Compression Level")->withDescription("The compression level to use; this is valid only when using GZIP compression.")
        ->isRequired(false)->withDefaultValue<int>(1)->build());
core::Property CompressContent::CompressMode(
    core::PropertyBuilder::createProperty("Mode")->withDescription("Indicates whether the processor should compress content or decompress content.")
        ->isRequired(false)->withAllowableValues<std::string>({MODE_COMPRESS, MODE_DECOMPRESS})->withDefaultValue(MODE_COMPRESS)->build());
core::Property CompressContent::CompressFormat(
    core::PropertyBuilder::createProperty("Compression Format")->withDescription("The compression format to use.")
        ->isRequired(false)
        ->withAllowableValues<std::string>({
          toString(ExtendedFormat::USE_MIME_TYPE),
          toString(Format::GZIP),
          toString(Format::BZIP2),
          toString(Format::XZ_LZMA2),
          toString(Format::LZMA)})->withDefaultValue(toString(ExtendedFormat::USE_MIME_TYPE))->build());
core::Property CompressContent::UpdateFileName(
    core::PropertyBuilder::createProperty("Update Filename")->withDescription("Determines if filename extension need to be updated")
        ->isRequired(false)->withDefaultValue<bool>(false)->build());
core::Property CompressContent::EncapsulateInTar(
    core::PropertyBuilder::createProperty("Encapsulate in TAR")
        ->withDescription("If true, on compression the FlowFile is added to a TAR archive and then compressed, "
                          "and on decompression a compressed, TAR-encapsulated FlowFile is expected.\n"
                          "If false, on compression the content of the FlowFile simply gets compressed, and on decompression a simple compressed content is expected.\n"
                          "true is the behaviour compatible with older MiNiFi C++ versions, false is the behaviour compatible with NiFi.")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());
core::Property CompressContent::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")
    ->withDescription("Maximum number of FlowFiles processed in a single session")
    ->withDefaultValue<uint32_t>(1)->build());

core::Relationship CompressContent::Success("success", "FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed");
core::Relationship CompressContent::Failure("failure", "FlowFiles will be transferred to the failure relationship if they fail to compress/decompress");

std::map<std::string, CompressContent::Format::Type> CompressContent::compressionFormatMimeTypeMap_{
  {"application/gzip", Format::GZIP},
  {"application/bzip2", Format::BZIP2},
  {"application/x-bzip2", Format::BZIP2},
  {"application/x-lzma", Format::LZMA},
  {"application/x-xz", Format::XZ_LZMA2}
};

std::map<CompressContent::Format::Type, std::string> CompressContent::fileExtension_{
  {Format::GZIP, ".gz"},
  {Format::LZMA, ".lzma"},
  {Format::BZIP2, ".bz2"},
  {Format::XZ_LZMA2, ".xz"}
};

void CompressContent::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(CompressLevel);
  properties.insert(CompressMode);
  properties.insert(CompressFormat);
  properties.insert(UpdateFileName);
  properties.insert(EncapsulateInTar);
  properties.insert(BatchSize);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void CompressContent::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  context->getProperty(CompressLevel.getName(), compressLevel_);
  context->getProperty(CompressMode.getName(), compressMode_);

  {
    std::string compressFormatStr;
    context->getProperty(CompressFormat.getName(), compressFormatStr);
    std::transform(compressFormatStr.begin(), compressFormatStr.end(), compressFormatStr.begin(), ::tolower);
    compressFormat_ = ExtendedFormat::parse(compressFormatStr.c_str());
  }

  context->getProperty(UpdateFileName.getName(), updateFileName_);
  context->getProperty(EncapsulateInTar.getName(), encapsulateInTar_);
  context->getProperty(BatchSize.getName(), batchSize_);

  logger_->log_info("Compress Content: Mode [%s] Format [%s] Level [%d] UpdateFileName [%d] EncapsulateInTar [%d]",
      compressMode_, toString(compressFormat_), compressLevel_, updateFileName_, encapsulateInTar_);
}

void CompressContent::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  for (size_t i = 0; i < batchSize_; ++i) {
    if (onTriggerImpl(context, session) != TriggerResult::CONTINUE) {
      break;
    }
  }
}

CompressContent::TriggerResult CompressContent::onTriggerImpl(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return TriggerResult::BREAK;
  }

  session->remove(flowFile);

  Format::Type compressFormat;
  if (compressFormat_ == ExtendedFormat::USE_MIME_TYPE) {
    std::string attr;
    flowFile->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, attr);
    if (attr.empty()) {
      logger_->log_error("No %s attribute existed for the flow, route to failure", core::SpecialFlowAttribute::MIME_TYPE);
      session->transfer(flowFile, Failure);
      return TriggerResult::CONTINUE;
    }
    auto search = compressionFormatMimeTypeMap_.find(attr);
    if (search != compressionFormatMimeTypeMap_.end()) {
      compressFormat = search->second;
    } else {
      logger_->log_info("Mime type of %s is not indicated a support format, route to success", attr);
      session->transfer(flowFile, Success);
      return TriggerResult::CONTINUE;
    }
  } else {
    compressFormat = ExtendedFormat::cast<Format>(compressFormat_);
  }
  std::string mimeType = to_mimeType(compressFormat);

  // Validate
  if (!encapsulateInTar_ && compressFormat != Format::GZIP) {
    logger_->log_error("non-TAR encapsulated format only supports GZIP compression");
    session->transfer(flowFile, Failure);
    return TriggerResult::CONTINUE;
  }
  if (compressFormat == Format::BZIP2 && archive_bzlib_version() == nullptr) {
    logger_->log_error("%s compression format is requested, but the agent was compiled without BZip2 support", compressFormat);
    session->transfer(flowFile, Failure);
    return TriggerResult::CONTINUE;
  }
  if ((compressFormat == Format::LZMA || compressFormat == Format::XZ_LZMA2) && archive_liblzma_version() == nullptr) {
    logger_->log_error("%s compression format is requested, but the agent was compiled without LZMA support ", compressFormat);
    session->transfer(flowFile, Failure);
    return TriggerResult::CONTINUE;
  }

  std::string fileExtension;
  auto search = fileExtension_.find(compressFormat);
  if (search != fileExtension_.end()) {
    fileExtension = search->second;
  }
  std::shared_ptr<core::FlowFile> processFlowFile = session->create(flowFile);
  bool success = false;
  if (encapsulateInTar_) {
    CompressContent::WriteCallback callback(compressMode_, compressLevel_, toString(compressFormat), flowFile, session);
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
    processFlowFile->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
    if (compressMode_ == MODE_COMPRESS) {
      session->putAttribute(processFlowFile, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
      if (updateFileName_) {
        fileName = fileName + fileExtension;
        session->putAttribute(processFlowFile, core::SpecialFlowAttribute::FILENAME, fileName);
      }
    } else {
      session->removeAttribute(processFlowFile, core::SpecialFlowAttribute::MIME_TYPE);
      if (updateFileName_) {
        if (fileName.size() >= fileExtension.size() && fileName.compare(fileName.size() - fileExtension.size(), fileExtension.size(), fileExtension) == 0) {
          fileName = fileName.substr(0, fileName.size() - fileExtension.size());
          session->putAttribute(processFlowFile, core::SpecialFlowAttribute::FILENAME, fileName);
        }
      }
    }
    logger_->log_debug("Compress Content processing success for the flow with UUID %s name %s", processFlowFile->getUUIDStr(), fileName);
    session->transfer(processFlowFile, Success);
  }
  return TriggerResult::CONTINUE;
}

std::string CompressContent::to_mimeType(Format::Type format) {
  switch (format) {
    case Format::GZIP: return "application/gzip";
    case Format::BZIP2: return "application/bzip2";
    case Format::LZMA: return "application/x-lzma";
    case Format::XZ_LZMA2: return "application/x-xz";
  }
  throw Exception(ExceptionType::GENERAL_EXCEPTION, "Unknown compression format: "
      + std::to_string(static_cast<int>(format)));
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
