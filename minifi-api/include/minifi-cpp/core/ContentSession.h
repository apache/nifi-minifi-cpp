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
#include "minifi-cpp/ResourceClaim.h"
#include "io/BaseStream.h"

namespace org::apache::nifi::minifi::core {

class StreamAppendLock;
class ContentRepository;

class ContentSession {
 public:
  virtual std::shared_ptr<ResourceClaim> create() = 0;

  virtual std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resource_id) = 0;

  virtual std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id, size_t offset, const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) = 0;

  virtual std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resource_id) = 0;

  virtual void commit() = 0;

  virtual void rollback() = 0;

  virtual ~ContentSession() = default;
};

}  // namespace org::apache::nifi::minifi::core

