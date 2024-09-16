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
#include <string>

#include "OutputStream.h"
#include "InputStream.h"
#include "core/Core.h"
#include "core/logging/Logger.h"

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

class ArchiveStreamProvider : public core::CoreComponent {
 public:
  using CoreComponent::CoreComponent;
  virtual std::unique_ptr<WriteArchiveStream> createWriteStream(int compress_level, const std::string& compress_format,
                                                      std::shared_ptr<OutputStream> sink, std::shared_ptr<core::logging::Logger> logger) = 0;
  virtual std::unique_ptr<ReadArchiveStream> createReadStream(std::shared_ptr<InputStream> archive_stream) = 0;
};

}  // namespace org::apache::nifi::minifi::io
