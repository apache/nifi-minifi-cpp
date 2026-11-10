// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "api/core/ControllerServiceContext.h"
#include "api/utils/minifi-c-utils.h"

namespace org::apache::nifi::minifi::api::core {

std::expected<std::string, std::error_code> ControllerServiceContext::getProperty(const std::string_view name) const {
  std::optional<std::string> value = std::nullopt;
  const minifi_status status = minifi_controller_service_context_get_property(impl_, utils::minifiStringView(name),
    [] (void* data, const minifi_string_view result) {
      (*static_cast<std::optional<std::string>*>(data)) = std::string(result.data, result.length);
    }, &value);

  if (status != MINIFI_STATUS_SUCCESS) {
    return std::unexpected{utils::make_error_code(status)};
  }

  if (!value) {
    return std::unexpected{utils::make_error_code(MINIFI_STATUS_UNKNOWN_ERROR)};
  }
  return value.value();
}

}  // namespace org::apache::nifi::minifi::api::core
