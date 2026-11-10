// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>

#include "core/Resource.h"
#include "WriteArchiveStream.h"
#include "ReadArchiveStream.h"

namespace org::apache::nifi::minifi::io {

class ArchiveStreamProviderImpl : public core::CoreComponentImpl, public virtual ArchiveStreamProvider {
 public:
  using CoreComponentImpl::CoreComponentImpl;

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  std::unique_ptr<WriteArchiveStream> createWriteStream(int compress_level, const std::string& compress_format,
                                                        std::shared_ptr<OutputStream> sink, std::shared_ptr<core::logging::Logger> logger) override {
    auto format = magic_enum::enum_cast<CompressionFormat>(compress_format);
    if (!format) {
      if (logger) {
        logger->log_error("Unrecognized compression format '{}'", compress_format);
      }
      return nullptr;
    }
    return std::make_unique<WriteArchiveStreamImpl>(compress_level, *format, std::move(sink));
  }

  std::unique_ptr<ReadArchiveStream> createReadStream(std::shared_ptr<InputStream> archive_stream) override {
    return std::make_unique<ReadArchiveStreamImpl>(std::move(archive_stream));
  }
};

REGISTER_RESOURCE_IMPLEMENTATION(ArchiveStreamProviderImpl, "ArchiveStreamProvider", InternalResource);

}  // namespace org::apache::nifi::minifi::io
