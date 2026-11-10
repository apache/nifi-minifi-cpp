// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "c2/C2Utils.h"

#include "core/ClassLoader.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::c2 {

bool isC2Enabled(const std::shared_ptr<Configure>& configuration) {
  std::string c2_enable_str;
  configuration->get(minifi::Configuration::nifi_c2_enable, "c2.enable", c2_enable_str);
  return utils::string::toBool(c2_enable_str).value_or(false);
}

bool isControllerSocketEnabled(const std::shared_ptr<Configure>& configuration) {
  std::string controller_socket_enable_str;
  configuration->get(minifi::Configuration::controller_socket_enable, controller_socket_enable_str);
  return utils::string::toBool(controller_socket_enable_str).value_or(false);
}

std::expected<std::shared_ptr<io::BufferStream>, std::string> createDebugBundleArchive(const std::map<std::string, std::unique_ptr<io::InputStream>>& files) {
  auto stream_provider = core::ClassLoader::getDefaultClassLoader().instantiate<io::ArchiveStreamProvider>(
      "ArchiveStreamProvider", "ArchiveStreamProvider");
  if (!stream_provider) {
    return std::unexpected{"Couldn't instantiate archiver provider"};
  }
  auto bundle = std::make_shared<io::BufferStream>();
  auto archiver = stream_provider->createWriteStream(9, "gzip", bundle, nullptr);
  if (!archiver) {
    return std::unexpected{"Couldn't instantiate archiver"};
  }
  for (const auto& [filename, stream] : files) {
    size_t file_size = stream->size();
    if (!archiver->newEntry({filename, file_size})) {
      return std::unexpected{"Couldn't initialize archive entry for '" + filename + "'"};
    }
    if (gsl::narrow<int64_t>(file_size) != minifi::internal::pipe(*stream, *archiver).toI64()) {
      // we have touched the input streams, they cannot be reused
      return std::unexpected{"Error while writing file '" + filename + "' into the debug bundle"};
    }
  }
  if (!archiver->finish()) {
    return std::unexpected{"Failed to complete debug bundle archive"};
  }
  return bundle;
}

}  // namespace org::apache::nifi::minifi::c2
