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
#include "core/logging/LoggerFactory.h"
#include "io/Stream.h"

namespace org::apache::nifi::minifi::io {

enum class CompressionFormat {
  GZIP,
  LZMA,
  XZ_LZMA2,
  BZIP2
};

}  // namespace org::apache::nifi::minifi::io

namespace magic_enum::customize {
using CompressionFormat = org::apache::nifi::minifi::io::CompressionFormat;

template <>
constexpr customize_t enum_name<CompressionFormat>(CompressionFormat value) noexcept {
  switch (value) {
    case CompressionFormat::GZIP:
      return "gzip";
    case CompressionFormat::LZMA:
      return "lzma";
    case CompressionFormat::XZ_LZMA2:
      return "xz-lzma2";
    case CompressionFormat::BZIP2:
      return "bzip2";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::io {

class WriteArchiveStreamImpl: public StreamImpl, public WriteArchiveStream {
  struct archive_write_deleter {
    int operator()(struct archive* ptr) const {
      return archive_write_free(ptr);
    }
  };
  using archive_ptr = std::unique_ptr<struct archive, archive_write_deleter>;
  struct archive_entry_deleter {
    void operator()(struct archive_entry* ptr) const {
      archive_entry_free(ptr);
    }
  };
  using archive_entry_ptr = std::unique_ptr<struct archive_entry, archive_entry_deleter>;

  archive_ptr createWriteArchive();

 public:
  WriteArchiveStreamImpl(int compress_level, CompressionFormat compress_format, std::shared_ptr<OutputStream> sink)
    : compress_level_(compress_level),
      compress_format_(compress_format),
      sink_(std::move(sink)) {
    arch_ = createWriteArchive();
  }

  using OutputStream::write;

  bool newEntry(const EntryInfo& info) override;

  size_t write(const uint8_t* data, size_t len) override;

  // TODO(adebreceni):
  //    Stream::close does not make it possible to signal failure (short of throwing)
  //    investigate if it is worth making Stream::close capable of this
  bool finish() override;

  ~WriteArchiveStreamImpl() override;

 private:
  static la_ssize_t archive_write(struct archive* /*arch*/, void *context, const void *buff, size_t size) {
    auto* const output = static_cast<OutputStream*>(context);
    const auto ret = output->write(reinterpret_cast<const uint8_t*>(buff), size);
    return io::isError(ret) ? -1 : gsl::narrow<la_ssize_t>(ret);
  }

  int compress_level_;
  CompressionFormat compress_format_;
  std::shared_ptr<io::OutputStream> sink_;
  archive_ptr arch_;
  archive_entry_ptr arch_entry_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<WriteArchiveStreamImpl>::getLogger();
};

}  // namespace org::apache::nifi::minifi::io
