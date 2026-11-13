// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "api/utils/minifi-c-utils.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::api::utils {

std::error_code make_error_code(minifi_status status) {
  switch (status) {
    case MINIFI_STATUS_NOT_SUPPORTED_PROPERTY: return make_error_code(minifi::core::PropertyErrorCode::NotSupportedProperty);
    case MINIFI_STATUS_DYNAMIC_PROPERTIES_NOT_SUPPORTED: return make_error_code(minifi::core::PropertyErrorCode::DynamicPropertiesNotSupported);
    case MINIFI_STATUS_PROPERTY_NOT_SET: return make_error_code(minifi::core::PropertyErrorCode::PropertyNotSet);
    case MINIFI_STATUS_VALIDATION_FAILED: return make_error_code(minifi::core::PropertyErrorCode::ValidationFailed);
    default: return std::error_code{};
  }
}

minifi_validator toStandardPropertyValidator(const minifi::core::PropertyValidator* validator) {
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_ALWAYS_VALID;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::NON_BLANK_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_NON_BLANK;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_TIME_PERIOD;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_BOOLEAN;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::INTEGER_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_INTEGER;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_UNSIGNED_INTEGER;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::DATA_SIZE_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_DATA_SIZE;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::PORT_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_PORT;
  }
  if (validator->getEquivalentNifiStandardValidatorName() == minifi::core::StandardPropertyValidators::NUMBER_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return MINIFI_VALIDATOR_NUMBER;
  }
  gsl_FailFast();
}

}  // namespace org::apache::nifi::minifi::api::utils
