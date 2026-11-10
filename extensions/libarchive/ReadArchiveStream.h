// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <utility>

#include "minifi-cpp/io/ArchiveStream.h"
#include "io/InputStream.h"
#include "core/logging/LoggerFactory.h"
#include "archive_entry.h"
#include "archive.h"
#include "SmartArchivePtrs.h"
#include "utils/ConfigurationUtils.h"

namespace org::apache::nifi::minifi::io {

class ReadArchiveStreamImpl final : public InputStreamImpl, public ReadArchiveStream {
  class BufferedReader {
   public:
    explicit BufferedReader(std::shared_ptr<InputStream> input) : input_(std::move(input)) {}

    std::optional<std::span<const std::byte>> readChunk() {
      const size_t result = input_->read(buffer_);
      if (io::isError(result)) {
        return std::nullopt;
      }
      return std::span(buffer_).subspan(0, result);
    }

   private:
    std::shared_ptr<InputStream> input_;
    std::array<std::byte, utils::configuration::DEFAULT_BUFFER_SIZE> buffer_{};
  };

  processors::archive_read_unique_ptr createReadArchive();

 public:
  explicit ReadArchiveStreamImpl(std::shared_ptr<InputStream> input) : reader_(std::move(input)) {
    arch_ = createReadArchive();
  }

  std::optional<EntryInfo> nextEntry() override;

  using InputStream::read;

  size_t read(std::span<std::byte> out_buffer) override;

 private:
  static la_ssize_t archive_read(struct archive* archive, void *context, const void **buff) {
    auto* const input = static_cast<BufferedReader*>(context);
    const auto opt_buffer = input->readChunk();
    if (!opt_buffer) {
      archive_set_error(archive, EIO, "Error reading archive");
      return -1;
    }
    *buff = opt_buffer->data();
    return gsl::narrow<la_ssize_t>(opt_buffer->size());
  }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ReadArchiveStream>::getLogger();
  BufferedReader reader_;
  processors::archive_read_unique_ptr arch_;
  std::optional<size_t> entry_size_;
};

}  // namespace org::apache::nifi::minifi::io
