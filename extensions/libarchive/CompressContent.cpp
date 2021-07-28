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
#include <memory>
#include <string>
#include <map>
#include <set>
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/StringUtils.h"
#include "core/Resource.h"

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
        ->isRequired(false)->withAllowableValues(CompressionMode::values())
        ->withDefaultValue(toString(CompressionMode::Compress))->build());
core::Property CompressContent::CompressFormat(
    core::PropertyBuilder::createProperty("Compression Format")->withDescription("The compression format to use.")
        ->isRequired(false)
        ->withAllowableValues(ExtendedCompressionFormat::values())
        ->withDefaultValue(toString(ExtendedCompressionFormat::USE_MIME_TYPE))->build());
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

const std::string CompressContent::TAR_EXT = ".tar";

const std::map<std::string, CompressContent::CompressionFormat> CompressContent::compressionFormatMimeTypeMap_{
  {"application/gzip", CompressionFormat::GZIP},
  {"application/bzip2", CompressionFormat::BZIP2},
  {"application/x-bzip2", CompressionFormat::BZIP2},
  {"application/x-lzma", CompressionFormat::LZMA},
  {"application/x-xz", CompressionFormat::XZ_LZMA2}
};

const std::map<CompressContent::CompressionFormat, std::string> CompressContent::fileExtension_{
  {CompressionFormat::GZIP, ".gz"},
  {CompressionFormat::LZMA, ".lzma"},
  {CompressionFormat::BZIP2, ".bz2"},
  {CompressionFormat::XZ_LZMA2, ".xz"}
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

void CompressContent::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  context->getProperty(CompressLevel.getName(), compressLevel_);
  context->getProperty(CompressMode.getName(), compressMode_);
  context->getProperty(CompressFormat.getName(), compressFormat_);
  context->getProperty(UpdateFileName.getName(), updateFileName_);
  context->getProperty(EncapsulateInTar.getName(), encapsulateInTar_);
  context->getProperty(BatchSize.getName(), batchSize_);

  logger_->log_info("Compress Content: Mode [%s] Format [%s] Level [%d] UpdateFileName [%d] EncapsulateInTar [%d]",
      compressMode_.toString(), compressFormat_.toString(), compressLevel_, updateFileName_, encapsulateInTar_);
}

void CompressContent::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  size_t processedFlowFileCount = 0;
  for (; processedFlowFileCount < batchSize_; ++processedFlowFileCount) {
    std::shared_ptr<core::FlowFile> flowFile = session->get();
    if (!flowFile) {
      break;
    }
    processFlowFile(flowFile, session);
  }
  if (processedFlowFileCount == 0) {
    // we got no flowFiles
    context->yield();
    return;
  }
}

void CompressContent::processFlowFile(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<core::ProcessSession>& session) {
  session->remove(flowFile);

  CompressionFormat compressFormat;
  if (compressFormat_ == ExtendedCompressionFormat::USE_MIME_TYPE) {
    std::string attr;
    flowFile->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, attr);
    if (attr.empty()) {
      logger_->log_error("No %s attribute existed for the flow, route to failure", core::SpecialFlowAttribute::MIME_TYPE);
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
  } else {
    compressFormat = compressFormat_.cast<CompressionFormat>();
  }
  std::string mimeType = toMimeType(compressFormat);

  // Validate
  if (!encapsulateInTar_ && compressFormat != CompressionFormat::GZIP) {
    logger_->log_error("non-TAR encapsulated format only supports GZIP compression");
    session->transfer(flowFile, Failure);
    return;
  }
  if (compressFormat == CompressionFormat::BZIP2 && archive_bzlib_version() == nullptr) {
    logger_->log_error("%s compression format is requested, but the agent was compiled without BZip2 support", compressFormat.toString());
    session->transfer(flowFile, Failure);
    return;
  }
  if ((compressFormat == CompressionFormat::LZMA || compressFormat == CompressionFormat::XZ_LZMA2) && archive_liblzma_version() == nullptr) {
    logger_->log_error("%s compression format is requested, but the agent was compiled without LZMA support ", compressFormat.toString());
    session->transfer(flowFile, Failure);
    return;
  }

  std::string fileExtension;
  auto search = fileExtension_.find(compressFormat);
  if (search != fileExtension_.end()) {
    fileExtension = search->second;
  }
  std::shared_ptr<core::FlowFile> result = session->create(flowFile);
  bool success = false;
  if (encapsulateInTar_) {
    CompressContent::WriteCallback callback(compressMode_, compressLevel_, compressFormat, flowFile, session);
    session->write(result, &callback);
    success = callback.status_ >= 0;
  } else {
    CompressContent::GzipWriteCallback callback(compressMode_, compressLevel_, flowFile, session);
    session->write(result, &callback);
    success = callback.success_;
  }

  if (!success) {
    logger_->log_error("Compress Content processing fail for the flow with UUID %s", flowFile->getUUIDStr());
    session->transfer(flowFile, Failure);
    session->remove(result);
  } else {
    std::string fileName;
    result->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
    if (compressMode_ == CompressionMode::Compress) {
      session->putAttribute(result, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
      if (updateFileName_) {
        if (encapsulateInTar_) {
          fileName = fileName + TAR_EXT;
        }
        fileName = fileName + fileExtension;
        session->putAttribute(result, core::SpecialFlowAttribute::FILENAME, fileName);
      }
    } else {
      session->removeAttribute(result, core::SpecialFlowAttribute::MIME_TYPE);
      if (updateFileName_) {
        if (utils::StringUtils::endsWith(fileName, fileExtension)) {
          fileName = fileName.substr(0, fileName.size() - fileExtension.size());
          if (encapsulateInTar_ && utils::StringUtils::endsWith(fileName, TAR_EXT)) {
            fileName = fileName.substr(0, fileName.size() - TAR_EXT.size());
          }
          session->putAttribute(result, core::SpecialFlowAttribute::FILENAME, fileName);
        }
      }
    }
    logger_->log_debug("Compress Content processing success for the flow with UUID %s name %s", result->getUUIDStr(), fileName);
    session->transfer(result, Success);
  }
}

std::string CompressContent::toMimeType(CompressionFormat format) {
  switch (format.value()) {
    case CompressionFormat::GZIP: return "application/gzip";
    case CompressionFormat::BZIP2: return "application/bzip2";
    case CompressionFormat::LZMA: return "application/x-lzma";
    case CompressionFormat::XZ_LZMA2: return "application/x-xz";
  }
  throw Exception(GENERAL_EXCEPTION, "Invalid compression format");
}

REGISTER_RESOURCE(CompressContent, "Compresses or decompresses the contents of FlowFiles using a user-specified compression algorithm and updates the mime.type attribute as appropriate");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
