// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>

#include "io/OutputStream.h"
#include "io/InputStream.h"
#include "core/Core.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::io {

struct EntryInfo {
  std::string filename;
  size_t size;
};

class WriteArchiveStream : public virtual OutputStream {
 public:
  virtual bool newEntry(const EntryInfo& info) = 0;
  virtual bool finish() = 0;
};

class ReadArchiveStream : public virtual InputStream {
 public:
  virtual std::optional<EntryInfo> nextEntry() = 0;
};

class ArchiveStreamProvider : public virtual core::CoreComponent {
 public:
  using CoreComponent::CoreComponent;
  virtual std::unique_ptr<WriteArchiveStream> createWriteStream(int compress_level, const std::string& compress_format,
                                                      std::shared_ptr<OutputStream> sink, std::shared_ptr<core::logging::Logger> logger) = 0;
  virtual std::unique_ptr<ReadArchiveStream> createReadStream(std::shared_ptr<InputStream> archive_stream) = 0;
};

}  // namespace org::apache::nifi::minifi::io
