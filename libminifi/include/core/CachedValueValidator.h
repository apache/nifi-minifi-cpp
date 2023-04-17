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

#pragma once

#include <utility>
#include <memory>
#include <string>
#include "PropertyValidation.h"
#include "state/Value.h"

namespace org::apache::nifi::minifi::core {

class PropertyValue;

namespace internal {

class CachedValueValidator {
  friend class core::PropertyValue;

 public:
  enum class Result {
    FAILURE,
    SUCCESS,
    RECOMPUTE
  };

  CachedValueValidator() = default;

  CachedValueValidator(const CachedValueValidator& other) : validator_(other.validator_) {}

  CachedValueValidator& operator=(const CachedValueValidator& other) {
    if (this == &other) {
      return *this;
    }
    setValidator(*other.validator_);
    return *this;
  }

  CachedValueValidator& operator=(const PropertyValidator& new_validator) {
    setValidator(new_validator);
    return *this;
  }

 private:
  void setValidator(const PropertyValidator& new_validator) {
    invalidateCachedResult();
    validator_ = gsl::make_not_null(&new_validator);
  }

  ValidationResult validate(const std::string& subject, const std::shared_ptr<state::response::Value>& value) const {
    if (validation_result_ == CachedValueValidator::Result::SUCCESS) {
      return ValidationResult::Builder::createBuilder().isValid(true).build();
    }
    if (validation_result_ == CachedValueValidator::Result::FAILURE) {
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(value->getStringValue()).isValid(false).build();
    }
    auto result = validator_->validate(subject, value);
    if (result.valid()) {
      validation_result_ = Result::SUCCESS;
    } else {
      validation_result_ = Result::FAILURE;
    }
    return result;
  }

  void invalidateCachedResult() {
    validation_result_ = Result::RECOMPUTE;
  }

  gsl::not_null<const PropertyValidator*> validator_{&StandardValidators::VALID_VALIDATOR};
  mutable Result validation_result_{Result::RECOMPUTE};
};

}  // namespace internal
}  // namespace org::apache::nifi::minifi::core
