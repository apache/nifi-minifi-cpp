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

#include <map>
#include <memory>
#include "ResourceClaim.h"
#include "io/BaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class ContentRepository;

class ContentSession {
 public:
  enum class WriteMode {
    OVERWRITE,
    APPEND
  };

  explicit ContentSession(std::shared_ptr<ContentRepository> repository);

  std::shared_ptr<ResourceClaim> create();

  std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resourceId, WriteMode mode = WriteMode::OVERWRITE);

  std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resourceId);

  virtual void commit();

  void rollback();

  virtual ~ContentSession() = default;

 protected:
  std::map<std::shared_ptr<ResourceClaim>, std::shared_ptr<io::BaseStream>> managedResources_;
  std::map<std::shared_ptr<ResourceClaim>, std::shared_ptr<io::BaseStream>> extendedResources_;
  std::shared_ptr<ContentRepository> repository_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

