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
#include "core/ProcessGroup.h"
#include "core/Repository.h"
#include "core/ContentRepository.h"
#include "core/FlowConfiguration.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class Flow {
 protected:
  Flow(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo,
       std::shared_ptr<core::ContentRepository> content_repo, std::unique_ptr<core::FlowConfiguration> flow_configuration)
      : provenance_repo_(std::move(provenance_repo)),
        flow_file_repo_(std::move(flow_file_repo)),
        content_repo_(std::move(content_repo)),
        flow_configuration_(std::move(flow_configuration)) {}

  virtual ~Flow() = default;

  virtual utils::Identifier getControllerUUID() const = 0;

  std::unique_ptr<core::ProcessGroup> root_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::unique_ptr<core::FlowConfiguration> flow_configuration_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
