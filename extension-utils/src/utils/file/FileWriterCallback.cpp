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

#include "utils/file/FileWriterCallback.h"
#include <fstream>

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

int64_t FileWriterCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  write_succeeded_ = false;
  size_t size = 0;
  std::array<std::byte, 1024> buffer{};

  std::ofstream tmp_file_os(temp_path_, std::ios::out | std::ios::binary);

  do {
    const auto read = stream->read(buffer);
    if (io::isError(read)) return -1;
    if (read == 0) break;
    tmp_file_os.write(reinterpret_cast<char *>(buffer.data()), gsl::narrow<std::streamsize>(read));
    size += read;
  } while (size < stream->size());

  tmp_file_os.close();

  if (tmp_file_os) {
    write_succeeded_ = true;
  }

  return gsl::narrow<int64_t>(size);
}

bool FileWriterCallback::commit() {
  if (!write_succeeded_)
    return false;

  std::error_code rename_error;
  std::filesystem::rename(temp_path_, dest_path_, rename_error);
  return !rename_error;
}
}  // namespace org::apache::nifi::minifi::utils
