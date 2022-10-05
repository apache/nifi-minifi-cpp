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

namespace org::apache::nifi::minifi::core {

class ContentRepository;

class ContentSession {
 public:
  enum class WriteMode {
    OVERWRITE,
    APPEND
  };

  explicit ContentSession(std::shared_ptr<ContentRepository> repository);

  virtual std::shared_ptr<ResourceClaim> create();

  virtual std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resourceId, WriteMode mode = WriteMode::OVERWRITE);

  virtual std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resourceId);

  virtual void commit();

  virtual void rollback();

  virtual ~ContentSession() = default;

 protected:
  std::shared_ptr<ContentRepository> repository_;
};

class BufferedContentSession : public ContentSession {
 public:
  explicit BufferedContentSession(std::shared_ptr<ContentRepository> repository);

  std::shared_ptr<ResourceClaim> create() override;

  std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resourceId, WriteMode mode = WriteMode::OVERWRITE) override;

  std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resourceId) override;

  void commit() override;

  void rollback() override;

 protected:
  std::map<std::shared_ptr<ResourceClaim>, std::shared_ptr<io::BufferStream>> managedResources_;
  std::map<std::shared_ptr<ResourceClaim>, std::shared_ptr<io::BufferStream>> extendedResources_;
};

}  // namespace org::apache::nifi::minifi::core

