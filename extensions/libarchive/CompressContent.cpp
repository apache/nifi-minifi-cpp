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
#include <cstdio>
#include <memory>
#include <string>
#include <map>
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/StringUtils.h"
#include "core/Resource.h"
#include "io/StreamPipe.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const std::string CompressContent::TAR_EXT = ".tar";

const std::map<std::string, io::CompressionFormat> CompressContent::compressionFormatMimeTypeMap_{
  {"application/gzip", io::CompressionFormat::GZIP},
  {"application/bzip2", io::CompressionFormat::BZIP2},
  {"application/x-bzip2", io::CompressionFormat::BZIP2},
  {"application/x-lzma", io::CompressionFormat::LZMA},
  {"application/x-xz", io::CompressionFormat::XZ_LZMA2}
};

const std::map<io::CompressionFormat, std::string> CompressContent::fileExtension_{
  {io::CompressionFormat::GZIP, ".gz"},
  {io::CompressionFormat::LZMA, ".lzma"},
  {io::CompressionFormat::BZIP2, ".bz2"},
  {io::CompressionFormat::XZ_LZMA2, ".xz"}
};

void CompressContent::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void CompressContent::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  compressLevel_ = gsl::narrow<int>(utils::parseI64Property(context, CompressLevel));
  compressMode_ = utils::parseEnumProperty<compress_content::CompressionMode>(context, CompressMode);
  compressFormat_ = utils::parseEnumProperty<compress_content::ExtendedCompressionFormat>(context, CompressFormat);
  updateFileName_ = utils::parseBoolProperty(context, UpdateFileName);
  encapsulateInTar_ = utils::parseBoolProperty(context, EncapsulateInTar);
  batchSize_ = utils::parseU64Property(context, BatchSize);

  logger_->log_info("Compress Content: Mode [{}] Format [{}] Level [{}] UpdateFileName [{}] EncapsulateInTar [{}]",
      magic_enum::enum_name(compressMode_), magic_enum::enum_name(compressFormat_), compressLevel_, updateFileName_, encapsulateInTar_);
}

void CompressContent::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  uint64_t processedFlowFileCount = 0;
  for (; processedFlowFileCount < batchSize_; ++processedFlowFileCount) {
    std::shared_ptr<core::FlowFile> flowFile = session.get();
    if (!flowFile) {
      break;
    }
    processFlowFile(flowFile, session);
  }
  if (processedFlowFileCount == 0) {
    // we got no flowFiles
    context.yield();
    return;
  }
}

void CompressContent::processFlowFile(const std::shared_ptr<core::FlowFile>& flowFile, core::ProcessSession& session) {
  session.remove(flowFile);

  io::CompressionFormat compressFormat{};
  if (compressFormat_ == compress_content::ExtendedCompressionFormat::USE_MIME_TYPE) {
    std::string attr;
    flowFile->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, attr);
    if (attr.empty()) {
      logger_->log_error("No {} attribute existed for the flow, route to failure", core::SpecialFlowAttribute::MIME_TYPE);
      session.transfer(flowFile, Failure);
      return;
    }
    auto search = compressionFormatMimeTypeMap_.find(attr);
    if (search != compressionFormatMimeTypeMap_.end()) {
      compressFormat = search->second;
    } else {
      logger_->log_info("Mime type of {} is not indicated a support format, route to success", attr);
      session.transfer(flowFile, Success);
      return;
    }
  } else {
    compressFormat = *magic_enum::enum_cast<io::CompressionFormat>(magic_enum::enum_name(compressFormat_));
  }
  std::string mimeType = toMimeType(compressFormat);

  // Validate
  if (!encapsulateInTar_ && compressFormat != io::CompressionFormat::GZIP) {
    logger_->log_error("non-TAR encapsulated format only supports GZIP compression");
    session.transfer(flowFile, Failure);
    return;
  }
  if (compressFormat == io::CompressionFormat::BZIP2 && archive_bzlib_version() == nullptr) {
    logger_->log_error("{} compression format is requested, but the agent was compiled without BZip2 support", magic_enum::enum_name(compressFormat));
    session.transfer(flowFile, Failure);
    return;
  }
  if ((compressFormat == io::CompressionFormat::LZMA || compressFormat == io::CompressionFormat::XZ_LZMA2) && archive_liblzma_version() == nullptr) {
    logger_->log_error("{} compression format is requested, but the agent was compiled without LZMA support ", magic_enum::enum_name(compressFormat));
    session.transfer(flowFile, Failure);
    return;
  }

  std::string fileExtension;
  auto search = fileExtension_.find(compressFormat);
  if (search != fileExtension_.end()) {
    fileExtension = search->second;
  }
  std::shared_ptr<core::FlowFile> result = session.create(flowFile.get());
  bool success = true;
  if (encapsulateInTar_) {
    std::function<int64_t(const std::shared_ptr<io::InputStream>&, const std::shared_ptr<io::OutputStream>&)> transformer;

    if (compressMode_ == compress_content::CompressionMode::compress) {
      std::string filename;
      flowFile->getAttribute(core::SpecialFlowAttribute::FILENAME, filename);
      transformer = [&, filename] (const std::shared_ptr<io::InputStream>& in, const std::shared_ptr<io::OutputStream>& out) -> int64_t {
        io::WriteArchiveStreamImpl compressor(compressLevel_, compressFormat, out);
        if (!compressor.newEntry({filename, in->size()})) {
          return -1;
        }
        return internal::pipe(*in, compressor);
      };
    } else {
      transformer = [&] (const std::shared_ptr<io::InputStream>& in, const std::shared_ptr<io::OutputStream>& out) -> int64_t {
        io::ReadArchiveStreamImpl decompressor(in);
        if (!decompressor.nextEntry()) {
          success = false;
          return 0;  // prevents a session rollback
        }
        auto ret = internal::pipe(decompressor, *out);
        if (ret < 0) {
          success = false;
          return 0;  // prevents a session rollback
        }
        return ret;
      };
    }
    session.write(result, [&] (const auto& out) {
      return session.read(flowFile, [&] (const auto& in) {
        return transformer(in, out);
      });
    });
  } else {
    CompressContent::GzipWriteCallback callback(compressMode_, compressLevel_, flowFile, session);
    session.write(result, std::ref(callback));
    success = callback.success_;
  }

  if (!success) {
    logger_->log_error("Compress Content processing fail for the flow with UUID {}", flowFile->getUUIDStr());
    session.transfer(flowFile, Failure);
    session.remove(result);
  } else {
    std::string fileName;
    result->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
    if (compressMode_ == compress_content::CompressionMode::compress) {
      session.putAttribute(*result, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
      if (updateFileName_) {
        if (encapsulateInTar_) {
          fileName = fileName + TAR_EXT;
        }
        fileName = fileName + fileExtension;
        session.putAttribute(*result, core::SpecialFlowAttribute::FILENAME, fileName);
      }
    } else {
      session.removeAttribute(*result, core::SpecialFlowAttribute::MIME_TYPE);
      if (updateFileName_) {
        if (utils::string::endsWith(fileName, fileExtension)) {
          fileName = fileName.substr(0, fileName.size() - fileExtension.size());
          if (encapsulateInTar_ && utils::string::endsWith(fileName, TAR_EXT)) {
            fileName = fileName.substr(0, fileName.size() - TAR_EXT.size());
          }
          session.putAttribute(*result, core::SpecialFlowAttribute::FILENAME, fileName);
        }
      }
    }
    logger_->log_debug("Compress Content processing success for the flow with UUID {} name {}", result->getUUIDStr(), fileName);
    session.transfer(result, Success);
  }
}

std::string CompressContent::toMimeType(io::CompressionFormat format) {
  switch (format) {
    case io::CompressionFormat::GZIP: return "application/gzip";
    case io::CompressionFormat::BZIP2: return "application/bzip2";
    case io::CompressionFormat::LZMA: return "application/x-lzma";
    case io::CompressionFormat::XZ_LZMA2: return "application/x-xz";
  }
  throw Exception(GENERAL_EXCEPTION, "Invalid compression format");
}

REGISTER_RESOURCE(CompressContent, Processor);

}  // namespace org::apache::nifi::minifi::processors
