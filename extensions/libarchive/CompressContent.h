/**
 * @file CompressContent.h
 * CompressContent class declaration
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
#pragma once

#include <cinttypes>
#include <vector>
#include <utility>
#include <memory>
#include <map>
#include <string>

#include "archive_entry.h"
#include "archive.h"

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "io/ZlibStream.h"
#include "utils/Enum.h"
#include "utils/gsl.h"
#include "utils/Export.h"
#include "WriteArchiveStream.h"
#include "ReadArchiveStream.h"

namespace org::apache::nifi::minifi::processors {

class CompressContent : public core::Processor {
 public:
  explicit CompressContent(std::string name, const utils::Identifier& uuid = {})
    : core::Processor(std::move(name), uuid)
    , updateFileName_(false)
    , encapsulateInTar_(false) {
  }
  ~CompressContent() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Compresses or decompresses the contents of FlowFiles using a user-specified compression algorithm "
      "and updates the mime.type attribute as appropriate";

  EXTENSIONAPI static const core::Property CompressMode;
  EXTENSIONAPI static const core::Property CompressLevel;
  EXTENSIONAPI static const core::Property CompressFormat;
  EXTENSIONAPI static const core::Property UpdateFileName;
  EXTENSIONAPI static const core::Property EncapsulateInTar;
  EXTENSIONAPI static const core::Property BatchSize;
  static auto properties() {
    return std::array{
      CompressMode,
      CompressLevel,
      CompressFormat,
      UpdateFileName,
      EncapsulateInTar,
      BatchSize
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  static const std::string TAR_EXT;

  SMART_ENUM(CompressionMode,
    (Compress, "compress"),
    (Decompress, "decompress")
  )

  SMART_ENUM_EXTEND(ExtendedCompressionFormat, io::CompressionFormat, (GZIP, LZMA, XZ_LZMA2, BZIP2),
    (USE_MIME_TYPE, "use mime.type attribute")
  )

 public:
  class GzipWriteCallback {
   public:
    GzipWriteCallback(CompressionMode compress_mode, int compress_level, std::shared_ptr<core::FlowFile> flow, std::shared_ptr<core::ProcessSession> session)
      : compress_mode_(std::move(compress_mode))
      , compress_level_(compress_level)
      , flow_(std::move(flow))
      , session_(std::move(session)) {
    }

    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CompressContent>::getLogger();
    CompressionMode compress_mode_;
    int compress_level_;
    std::shared_ptr<core::FlowFile> flow_;
    std::shared_ptr<core::ProcessSession> session_;
    bool success_{false};

    int64_t operator()(const std::shared_ptr<io::OutputStream>& output_stream) {
      std::shared_ptr<io::ZlibBaseStream> filterStream;
      if (compress_mode_ == CompressionMode::Compress) {
        filterStream = std::make_shared<io::ZlibCompressStream>(gsl::make_not_null(output_stream.get()), io::ZlibCompressionFormat::GZIP, compress_level_);
      } else {
        filterStream = std::make_shared<io::ZlibDecompressStream>(gsl::make_not_null(output_stream.get()), io::ZlibCompressionFormat::GZIP);
      }
      session_->read(flow_, [this, &filterStream](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
        std::vector<std::byte> buffer(16 * 1024U);
        size_t read_size = 0;
        while (read_size < flow_->getSize()) {
          const auto ret = input_stream->read(buffer);
          if (io::isError(ret)) {
            return -1;
          } else if (ret == 0) {
            break;
          } else {
            const auto writeret = filterStream->write(gsl::make_span(buffer).subspan(0, ret));
            if (io::isError(writeret) || gsl::narrow<size_t>(writeret) != ret) {
              return -1;
            }
            read_size += ret;
          }
        }
        filterStream->close();
        return gsl::narrow<int64_t>(read_size);
      });

      success_ = filterStream->isFinished();

      return gsl::narrow<int64_t>(flow_->getSize());
    }
  };

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  // OnTrigger method, implemented by NiFi CompressContent
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
  }
  // OnTrigger method, implemented by NiFi CompressContent
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  // Initialize, over write by NiFi CompressContent
  void initialize() override;

 private:
  static std::string toMimeType(io::CompressionFormat format);

  void processFlowFile(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<core::ProcessSession>& session);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CompressContent>::getLogger();
  int compressLevel_{};
  CompressionMode compressMode_;
  ExtendedCompressionFormat compressFormat_;
  bool updateFileName_;
  bool encapsulateInTar_;
  uint32_t batchSize_{1};
  static const std::map<std::string, io::CompressionFormat> compressionFormatMimeTypeMap_;
  static const std::map<io::CompressionFormat, std::string> fileExtension_;
};

}  // namespace org::apache::nifi::minifi::processors
