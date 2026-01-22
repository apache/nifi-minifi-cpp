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
#include <string_view>

#include "core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::test::utils {

template<typename T>
std::unique_ptr<core::controller::ControllerService> make_controller_service(std::string_view name, std::optional<minifi::utils::Identifier> uuid = std::nullopt) {
  if (!uuid) {
    uuid = minifi::utils::IdGenerator::getIdGenerator()->generate();
  }
  auto processor_impl = std::make_unique<T>(core::controller::ControllerServiceMetadata{
      .uuid = uuid.value(),
      .name = std::string{name},
      .logger = minifi::core::logging::LoggerFactory<T>::getLogger(uuid.value())
  });
  return std::make_unique<core::controller::ControllerService>(name, uuid.value(), std::move(processor_impl));
}

}  // namespace org::apache::nifi::minifi::test::utils
