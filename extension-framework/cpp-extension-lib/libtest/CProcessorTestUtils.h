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

#include "core/Processor.h"
#include "minifi-cpp/core/ProcessorMetadata.h"
#include "api/core/Resource.h"
#include "utils/CProcessor.h"

namespace org::apache::nifi::minifi::test::utils {

template<typename T, typename ...Args>
std::unique_ptr<minifi::core::Processor> make_custom_c_processor(minifi::core::ProcessorMetadata metadata, Args&&... args) {  // NOLINT(cppcoreguidelines-missing-std-forward)
  std::unique_ptr<minifi::core::ProcessorApi> processor_impl;
  minifi::api::core::useProcessorClassDescription<T>([&] (const MinifiProcessorClassDefinition& description) {
    minifi::utils::useCProcessorClassDescription(description, [&] (const auto&, auto c_description) {
      processor_impl = std::make_unique<minifi::utils::CProcessor>(std::move(c_description), metadata, new T(metadata, std::forward<Args>(args)...));
    });
  });
  return std::make_unique<minifi::core::Processor>(metadata.name, metadata.uuid, std::move(processor_impl));
}

}  // namespace org::apache::nifi::minifi::test::utils
