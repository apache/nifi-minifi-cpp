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

#include "utils/file/ArchiveUtils.h"
#include "core/ClassLoader.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::utils::archive {

nonstd::expected<std::shared_ptr<io::BufferStream>, std::string> createArchive(std::map<std::string, std::unique_ptr<io::InputStream>>& files,
    const std::shared_ptr<core::logging::Logger>& logger) {
  auto stream_provider = core::ClassLoader::getDefaultClassLoader().instantiate<io::ArchiveStreamProvider>(
      "ArchiveStreamProvider", "ArchiveStreamProvider");
  if (!stream_provider) {
    return nonstd::make_unexpected("Couldn't instantiate archiver provider");
  }
  auto bundle = std::make_shared<io::BufferStream>();
  auto archiver = stream_provider->createWriteStream(9, "gzip", bundle, logger);
  if (!archiver) {
    return nonstd::make_unexpected("Couldn't instantiate archiver");
  }
  for (auto& [filename, stream] : files) {
    size_t file_size = stream->size();
    if (!archiver->newEntry({filename, file_size})) {
      return nonstd::make_unexpected("Couldn't initialize archive entry for '" + filename + "'");
    }
    if (gsl::narrow<int64_t>(file_size) != minifi::internal::pipe(*stream, *archiver)) {
      // we have touched the input streams, they cannot be reused
      return nonstd::make_unexpected("Error while writing file '" + filename + "' into the debug bundle");
    }
  }
  if (!archiver->finish()) {
    return nonstd::make_unexpected("Failed to complete debug bundle archive");
  }
  return bundle;
}

}  // namespace org::apache::nifi::minifi::utils::archive
