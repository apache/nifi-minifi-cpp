/**
 * @file BlobStorage.h
 * BlobStorage class declaration
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

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace storage {

struct UploadBlobResult {
  std::string primary_uri;
  std::string etag;
  std::size_t length;
  std::string timestamp;
};

class BlobStorage {
 public:
  BlobStorage(std::string connection_string, std::string container_name)
    : connection_string_(std::move(connection_string))
    , container_name_(std::move(container_name)) {
  }

  virtual void createContainer() = 0;
  virtual void resetClientIfNeeded(const std::string &connection_string, const std::string &container_name) = 0;
  virtual std::optional<UploadBlobResult> uploadBlob(const std::string &blob_name, const uint8_t* buffer, std::size_t buffer_size) = 0;
  virtual ~BlobStorage() = default;

 protected:
  std::string connection_string_;
  std::string container_name_;
};

}  // namespace storage
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
