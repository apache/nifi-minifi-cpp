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

#include "io/OutputStream.h"
#include "io/ArchiveStream.h"
#include "io/InputStream.h"
#include "core/logging/LoggerFactory.h"

#include "archive_entry.h"
#include "archive.h"

namespace org::apache::nifi::minifi::io {

class ReadArchiveStreamImpl : public InputStreamImpl, public ReadArchiveStream {
  struct archive_read_deleter {
    int operator()(struct archive* ptr) const {
      return archive_read_free(ptr);
    }
  };
  using archive_ptr = std::unique_ptr<struct archive, archive_read_deleter>;

  class BufferedReader {
   public:
    explicit BufferedReader(std::shared_ptr<InputStream> input) : input_(std::move(input)) {}

    std::optional<std::span<const std::byte>> readChunk() {
      size_t result = input_->read(buffer_);
      if (io::isError(result)) {
        return std::nullopt;
      }
      return gsl::make_span(buffer_).subspan(0, result);
    }

   private:
    std::shared_ptr<InputStream> input_;
    std::array<std::byte, 4096> buffer_{};
  };

  archive_ptr createReadArchive();

 public:
  explicit ReadArchiveStreamImpl(std::shared_ptr<InputStream> input) : reader_(std::move(input)) {
    arch_ = createReadArchive();
  }

  std::optional<EntryInfo> nextEntry() override;

  using InputStream::read;

  size_t read(std::span<std::byte> out_buffer) override;

 private:
  static la_ssize_t archive_read(struct archive* archive, void *context, const void **buff) {
    auto* const input = reinterpret_cast<BufferedReader*>(context);
    auto opt_buffer = input->readChunk();
    if (!opt_buffer) {
      archive_set_error(archive, EIO, "Error reading archive");
      return -1;
    }
    *buff = opt_buffer->data();
    return gsl::narrow<la_ssize_t>(opt_buffer->size());
  }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ReadArchiveStream>::getLogger();
  BufferedReader reader_;
  archive_ptr arch_;
  std::optional<size_t> entry_size_;
};

}  // namespace org::apache::nifi::minifi::io
