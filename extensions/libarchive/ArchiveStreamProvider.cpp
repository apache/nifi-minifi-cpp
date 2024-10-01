/**
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
