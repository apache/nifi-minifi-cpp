/**
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

#include <memory>
#include <utility>
#include <string>

#include "io/ArchiveStream.h"
#include "archive_entry.h"
#include "archive.h"
#include "utils/Enum.h"
#include "core/Core.h"
#include "logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::io {

SMART_ENUM(CompressionFormat,
  (GZIP, "gzip"),
  (LZMA, "lzma"),
  (XZ_LZMA2, "xz-lzma2"),
  (BZIP2, "bzip2")
)

class WriteArchiveStreamImpl: public WriteArchiveStream {
  struct archive_ptr : public std::unique_ptr<struct archive, int(*)(struct archive*)> {
    using Base = std::unique_ptr<struct archive, int(*)(struct archive*)>;
    archive_ptr(): Base(nullptr, archive_write_free) {}
    archive_ptr(std::nullptr_t): Base(nullptr, archive_write_free) {}
    archive_ptr(struct archive* arch): Base(arch, archive_write_free) {}
  };
  struct archive_entry_ptr : public std::unique_ptr<struct archive_entry, void(*)(struct archive_entry*)> {
    using Base = std::unique_ptr<struct archive_entry, void(*)(struct archive_entry*)>;
    archive_entry_ptr(): Base(nullptr, archive_entry_free) {}
    archive_entry_ptr(std::nullptr_t): Base(nullptr, archive_entry_free) {}
    archive_entry_ptr(struct archive_entry* arch_entry): Base(arch_entry, archive_entry_free) {}
  };

  archive_ptr createWriteArchive();

 public:
  WriteArchiveStreamImpl(int compress_level, CompressionFormat compress_format, std::shared_ptr<OutputStream> sink)
    : compress_level_(compress_level),
      compress_format_(compress_format),
      sink_(std::move(sink)) {
    arch_ = createWriteArchive();
  }

  int compress_level_;
  CompressionFormat compress_format_;
  std::shared_ptr<io::OutputStream> sink_;
  int status_{0};
  archive_ptr arch_;
  archive_entry_ptr arch_entry_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<WriteArchiveStreamImpl>::getLogger();

  static la_ssize_t archive_write(struct archive* /*arch*/, void *context, const void *buff, size_t size) {
    auto* const output = static_cast<OutputStream*>(context);
    const auto ret = output->write(reinterpret_cast<const uint8_t*>(buff), size);
    return io::isError(ret) ? -1 : gsl::narrow<la_ssize_t>(ret);
  }

  using OutputStream::write;

  bool newEntry(const EntryInfo& info) override;

  size_t write(const uint8_t* data, size_t len) override;
};

}  // namespace org::apache::nifi::minifi::io
