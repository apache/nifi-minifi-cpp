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

#include "core/PropertyType.h"
#include "archive_entry.h"
#include "archive.h"

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/logging/LoggerFactory.h"
#include "io/ZlibStream.h"
#include "utils/Enum.h"
#include "utils/gsl.h"
#include "utils/Export.h"
#include "WriteArchiveStream.h"
#include "ReadArchiveStream.h"

namespace org::apache::nifi::minifi::processors::compress_content {
enum class CompressionMode {
  compress,
  decompress
};

enum class ExtendedCompressionFormat {
  GZIP,
  LZMA,
  XZ_LZMA2,
  BZIP2,
  USE_MIME_TYPE
};

}  // namespace org::apache::nifi::minifi::processors::compress_content

namespace magic_enum::customize {
using ExtendedCompressionFormat = org::apache::nifi::minifi::processors::compress_content::ExtendedCompressionFormat;

template <>
constexpr customize_t enum_name<ExtendedCompressionFormat>(ExtendedCompressionFormat value) noexcept {
  switch (value) {
    case ExtendedCompressionFormat::GZIP:
      return "gzip";
    case ExtendedCompressionFormat::LZMA:
      return "lzma";
    case ExtendedCompressionFormat::XZ_LZMA2:
      return "xz-lzma2";
    case ExtendedCompressionFormat::BZIP2:
      return "bzip2";
    case ExtendedCompressionFormat::USE_MIME_TYPE:
      return "use mime.type attribute";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class CompressContent : public core::ProcessorImpl {
 public:
  explicit CompressContent(std::string_view name, const utils::Identifier& uuid = {})
    : core::ProcessorImpl(name, uuid) {
  }
  ~CompressContent() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Compresses or decompresses the contents of FlowFiles using a user-specified compression algorithm "
      "and updates the mime.type attribute as appropriate";

  EXTENSIONAPI static constexpr auto CompressMode = core::PropertyDefinitionBuilder<magic_enum::enum_count<compress_content::CompressionMode>()>::createProperty("Mode")
      .withDescription("Indicates whether the processor should compress content or decompress content.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(compress_content::CompressionMode::compress))
      .withAllowedValues(magic_enum::enum_names<compress_content::CompressionMode>())
      .build();
  EXTENSIONAPI static constexpr auto CompressLevel = core::PropertyDefinitionBuilder<>::createProperty("Compression Level")
      .withDescription("The compression level to use; this is valid only when using GZIP compression.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto CompressFormat = core::PropertyDefinitionBuilder<magic_enum::enum_count<compress_content::ExtendedCompressionFormat>()>::createProperty("Compression Format")
      .withDescription("The compression format to use.")
      .isRequired(false)
      .withDefaultValue(magic_enum::enum_name(compress_content::ExtendedCompressionFormat::USE_MIME_TYPE))
      .withAllowedValues(magic_enum::enum_names<compress_content::ExtendedCompressionFormat>())
      .build();
  EXTENSIONAPI static constexpr auto UpdateFileName = core::PropertyDefinitionBuilder<>::createProperty("Update Filename")
      .withDescription("Determines if filename extension need to be updated")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto EncapsulateInTar = core::PropertyDefinitionBuilder<>::createProperty("Encapsulate in TAR")
      .withDescription("If true, on compression the FlowFile is added to a TAR archive and then compressed, "
          "and on decompression a compressed, TAR-encapsulated FlowFile is expected.\n"
          "If false, on compression the content of the FlowFile simply gets compressed, and on decompression a simple compressed content is expected.\n"
          "true is the behaviour compatible with older MiNiFi C++ versions, false is the behaviour compatible with NiFi.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto BatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
      .withDescription("Maximum number of FlowFiles processed in a single session")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      CompressMode,
      CompressLevel,
      CompressFormat,
      UpdateFileName,
      EncapsulateInTar,
      BatchSize
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles will be transferred to the failure relationship if they fail to compress/decompress"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  static const std::string TAR_EXT;

  class GzipWriteCallback {
   public:
    GzipWriteCallback(compress_content::CompressionMode compress_mode, int compress_level, std::shared_ptr<core::FlowFile> flow, core::ProcessSession& session)
      : compress_mode_(compress_mode)
      , compress_level_(compress_level)
      , flow_(std::move(flow))
      , session_(session) {
    }

    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CompressContent>::getLogger();
    compress_content::CompressionMode compress_mode_;
    int compress_level_;
    std::shared_ptr<core::FlowFile> flow_;
    core::ProcessSession& session_;
    bool success_{false};

    int64_t operator()(const std::shared_ptr<io::OutputStream>& output_stream) {
      std::shared_ptr<io::ZlibBaseStream> filterStream;
      if (compress_mode_ == compress_content::CompressionMode::compress) {
        filterStream = std::make_shared<io::ZlibCompressStream>(gsl::make_not_null(output_stream.get()), io::ZlibCompressionFormat::GZIP, compress_level_);
      } else {
        filterStream = std::make_shared<io::ZlibDecompressStream>(gsl::make_not_null(output_stream.get()), io::ZlibCompressionFormat::GZIP);
      }
      session_.read(flow_, [this, &filterStream](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
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

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void initialize() override;

 private:
  static std::string toMimeType(io::CompressionFormat format);

  void processFlowFile(const std::shared_ptr<core::FlowFile>& flowFile, core::ProcessSession& session);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CompressContent>::getLogger(uuid_);
  int compressLevel_{};
  compress_content::CompressionMode compressMode_;
  compress_content::ExtendedCompressionFormat compressFormat_;
  bool updateFileName_ = false;
  bool encapsulateInTar_ = false;
  uint32_t batchSize_{1};
  static const std::map<std::string, io::CompressionFormat> compressionFormatMimeTypeMap_;
  static const std::map<io::CompressionFormat, std::string> fileExtension_;
};

}  // namespace org::apache::nifi::minifi::processors
