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

#ifndef NIFI_MINIFI_CPP_CACHEDVALUEVALIDATOR_H
#define NIFI_MINIFI_CPP_CACHEDVALUEVALIDATOR_H

#include "PropertyValidation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class CachedValueValidator{
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
    validator_ = other.validator_;
    validation_result_ = Result::RECOMPUTE;
    return *this;
  }
  CachedValueValidator& operator=(CachedValueValidator&& other) {
    validator_ = std::move(other.validator_);
    validation_result_ = Result::RECOMPUTE;
    return *this;
  }

  CachedValueValidator(const std::shared_ptr<PropertyValidator>& other) : validator_(other) {}
  CachedValueValidator(std::shared_ptr<PropertyValidator>&& other) : validator_(std::move(other)) {}
  CachedValueValidator& operator=(const std::shared_ptr<PropertyValidator>& new_validator) {
    validator_ = new_validator;
    validation_result_ = Result::RECOMPUTE;
    return *this;
  }
  CachedValueValidator& operator=(std::shared_ptr<PropertyValidator>&& new_validator) {
    validator_ = std::move(new_validator);
    validation_result_ = Result::RECOMPUTE;
    return *this;
  }

  const std::shared_ptr<PropertyValidator>& operator->() const {
    return validator_;
  }

  operator bool() const {
    return static_cast<bool>(validator_);
  }

  const std::shared_ptr<PropertyValidator>& operator*() const {
    return validator_;
  }

  void setValidationResult(bool success) const {
    validation_result_ = success ? Result::SUCCESS : Result::FAILURE;
  }

  void clearValidationResult() const {
    validation_result_ = Result::RECOMPUTE;
  }

  Result isValid() const {
    if(!validator_){
      return Result::SUCCESS;
    }
    return validation_result_;
  }

 private:
  std::shared_ptr<PropertyValidator> validator_;
  mutable Result validation_result_{Result::RECOMPUTE};
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_CACHEDVALUEVALIDATOR_H
