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
#include <map>
#include "io/BaseStream.h"
#include "minifi-cpp/core/ContentSession.h"
#include "ContentRepository.h"

namespace org::apache::nifi::minifi::core {

class StreamAppendLock;
class ContentRepository;

class ContentSessionImpl : public virtual ContentSession {
  struct AppendState {
    std::shared_ptr<io::BaseStream> stream;
    size_t base_size;
    std::unique_ptr<StreamAppendLock> lock;
  };

 public:
  explicit ContentSessionImpl(std::shared_ptr<ContentRepository> repository): repository_(std::move(repository)) {}

  std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id, size_t offset, const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) override;

 protected:
  virtual std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id) = 0;

  // contains aux data on resources that have been appended to
  std::map<std::shared_ptr<ResourceClaim>, AppendState> append_state_;
  std::shared_ptr<ContentRepository> repository_;
};

}  // namespace org::apache::nifi::minifi::core

