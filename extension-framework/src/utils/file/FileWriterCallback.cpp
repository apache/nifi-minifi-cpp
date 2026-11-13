// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/file/FileWriterCallback.h"

#include <fstream>

#include "utils/Id.h"

namespace org::apache::nifi::minifi::utils {

FileWriterCallback::FileWriterCallback(std::filesystem::path dest_path)
    : dest_path_(std::move(dest_path)) {
  auto new_filename = std::filesystem::path("." + dest_path_.filename().string() + "." +  utils::IdGenerator::getIdGenerator()->generate().to_string());
  temp_path_ = dest_path_.parent_path() / new_filename;
}

FileWriterCallback::~FileWriterCallback() {
  std::error_code remove_error;
  std::filesystem::remove(temp_path_, remove_error);
}

io::IoResult FileWriterCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  write_succeeded_ = false;
  size_t size = 0;
  std::array<std::byte, 1024> buffer{};

  std::ofstream tmp_file_os(temp_path_, std::ios::out | std::ios::binary);

  do {
    const auto read = stream->read(buffer);
    if (io::isError(read)) return io::IoResult::error();
    if (read == 0) break;
    tmp_file_os.write(reinterpret_cast<char *>(buffer.data()), gsl::narrow<std::streamsize>(read));
    size += read;
  } while (size < stream->size());

  tmp_file_os.close();

  if (tmp_file_os) {
    write_succeeded_ = true;
  }

  return io::IoResult::from(size);
}

bool FileWriterCallback::commit() {
  if (!write_succeeded_)
    return false;

  std::error_code rename_error;
  std::filesystem::rename(temp_path_, dest_path_, rename_error);
  return !rename_error;
}
}  // namespace org::apache::nifi::minifi::utils
