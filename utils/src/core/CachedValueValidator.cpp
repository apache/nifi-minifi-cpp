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

#include "minifi-cpp/core/CachedValueValidator.h"
#include "core/PropertyType.h"

namespace org::apache::nifi::minifi::core::internal {

CachedValueValidator::CachedValueValidator() : validator_{&StandardPropertyTypes::VALID_TYPE} {}

ValidationResult CachedValueValidator::validate(const std::string& subject, const std::shared_ptr<state::response::Value>& value) const {
  if (validation_result_ == CachedValueValidator::Result::SUCCESS) {
    return ValidationResult{.valid = true, .subject = {}, .input = {}};
  }
  if (validation_result_ == CachedValueValidator::Result::FAILURE) {
    return ValidationResult{.valid = false, .subject = subject, .input = value->getStringValue()};
  }
  auto result = validator_->validate(subject, value);
  if (result.valid) {
    validation_result_ = Result::SUCCESS;
  } else {
    validation_result_ = Result::FAILURE;
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::core::internal
