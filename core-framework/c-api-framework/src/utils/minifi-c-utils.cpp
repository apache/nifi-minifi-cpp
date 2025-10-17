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

#include "api/utils/minifi-c-utils.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::api::utils {

std::error_code make_error_code(MinifiStatus status) {
  switch (status) {
    case MINIFI_NOT_SUPPORTED_PROPERTY: return make_error_code(minifi::core::PropertyErrorCode::NotSupportedProperty);
    case MINIFI_DYNAMIC_PROPERTIES_NOT_SUPPORTED: return make_error_code(minifi::core::PropertyErrorCode::DynamicPropertiesNotSupported);
    case MINIFI_PROPERTY_NOT_SET: return make_error_code(minifi::core::PropertyErrorCode::PropertyNotSet);
    case MINIFI_VALIDATION_FAILED: return make_error_code(minifi::core::PropertyErrorCode::ValidationFailed);
    default: return std::error_code{};
  }
}

MinifiStandardPropertyValidator toStandardPropertyValidator(const minifi::core::PropertyValidator* validator) {
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_ALWAYS_VALID_VALIDATOR;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::NON_BLANK_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_NON_BLANK_VALIDATOR;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_TIME_PERIOD_VALIDATOR;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_BOOLEAN_VALIDATOR;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::INTEGER_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_INTEGER_VALIDATOR;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_UNSIGNED_INTEGER_VALIDATOR;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::DATA_SIZE_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_DATA_SIZE_VALIDATOR;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::PORT_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_PORT_VALIDATOR;
  }
  gsl_FailFast();
}

}  // namespace org::apache::nifi::minifi::api::utils
