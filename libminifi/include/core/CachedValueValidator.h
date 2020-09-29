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

#ifndef LIBMINIFI_INCLUDE_CORE_CACHEDVALUEVALIDATOR_H_
#define LIBMINIFI_INCLUDE_CORE_CACHEDVALUEVALIDATOR_H_

#include <utility>
#include <memory>
#include <string>
#include "PropertyValidation.h"
#include "state/Value.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

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

  CachedValueValidator(CachedValueValidator&& other) noexcept : validator_(std::move(other.validator_)) {}

  CachedValueValidator& operator=(const CachedValueValidator& other) {
    if (this == &other) {
      return *this;
    }
    setValidator(other.validator_);
    return *this;
  }

  CachedValueValidator& operator=(CachedValueValidator&& other) {
    if (this == &other) {
      return *this;
    }
    setValidator(std::move(other.validator_));
    return *this;
  }

  explicit CachedValueValidator(const std::shared_ptr<PropertyValidator>& other) : validator_(other) {}

  explicit CachedValueValidator(std::shared_ptr<PropertyValidator>&& other) : validator_(std::move(other)) {}

  CachedValueValidator& operator=(const gsl::not_null<std::shared_ptr<PropertyValidator>>& new_validator) {
    setValidator(new_validator);
    return *this;
  }

  CachedValueValidator& operator=(gsl::not_null<std::shared_ptr<PropertyValidator>>&& new_validator) {
    setValidator(std::move(new_validator));
    return *this;
  }

  const gsl::not_null<std::shared_ptr<PropertyValidator>>& operator*() const {
    return validator_;
  }

 private:
  template<typename T>
  void setValidator(T&& newValidator) {
    invalidateCachedResult();
    validator_ = std::forward<T>(newValidator);
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

  gsl::not_null<std::shared_ptr<PropertyValidator>> validator_{StandardValidators::get().VALID_VALIDATOR};
  mutable Result validation_result_{Result::RECOMPUTE};
};

} /* namespace internal */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_CORE_CACHEDVALUEVALIDATOR_H_
