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

#include "state/Value.h"
#include "ValidationResult.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core {

class PropertyValue;
class PropertyValidator;

template<typename T, typename> struct Converter;

namespace internal {

class CachedValueValidator {
  friend class core::PropertyValue;
  template<typename T, typename> friend struct core::Converter;

 public:
  enum class Result {
    FAILURE,
    SUCCESS,
    RECOMPUTE
  };

  CachedValueValidator();

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

  ValidationResult validate(const std::string& subject, const std::shared_ptr<state::response::Value>& value) const;

  void invalidateCachedResult() {
    validation_result_ = Result::RECOMPUTE;
  }

  gsl::not_null<const PropertyValidator*> validator_;
  mutable Result validation_result_{Result::RECOMPUTE};
};

}  // namespace internal
}  // namespace org::apache::nifi::minifi::core
