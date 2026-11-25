// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <expected>
#include <map>
#include <optional>
#include <string>
#include <system_error>

#include "api/utils/minifi-c-utils.h"
#include "minifi-api.h"
#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/core/PropertyDefinition.h"

namespace org::apache::nifi::minifi::api::core::detail {

using PropertyCallback = void (*)(void*, minifi_string_view);
using DynamicPropertiesCallback = void (*)(void*, minifi_string_view, minifi_string_view);

template<typename CCall>
std::expected<std::string, std::error_code> readProperty(const CCall& c_call) {
  std::optional<std::string> value;
  const minifi_status status = c_call(
      [](void* data, const minifi_string_view result) { (*static_cast<std::optional<std::string>*>(data)) = std::string(result.data, result.length); },
      &value);
  if (!value) { return std::unexpected{utils::make_error_code(status)}; }
  return value.value();
}

template<typename CCall>
std::map<std::string, std::string> readDynamicProperties(const CCall& c_call) {
  std::map<std::string, std::string> result;
  c_call(
      [](void* user_ctx, const minifi_string_view key, const minifi_string_view value) {
        static_cast<std::map<std::string, std::string>*>(user_ctx)->emplace(utils::toString(key), utils::toString(value));
      },
      &result);
  return result;
}

template<typename CCall>
std::expected<minifi_controller_service*, std::error_code> readControllerService(const minifi::core::PropertyReference& prop, const CCall& c_call) {
  minifi_controller_service* controller_service = nullptr;
  gsl_Assert(prop.allowed_types.size() == 1);
  const minifi_status status = c_call(utils::minifiStringView(prop.name), utils::minifiStringView(prop.allowed_types[0]), &controller_service);
  if (status == MINIFI_STATUS_PROPERTY_NOT_SET) {
    return nullptr;
  }
  if (status != MINIFI_STATUS_SUCCESS) {
    return std::unexpected{utils::make_error_code(status)};
  }
  return controller_service;
}

}  // namespace org::apache::nifi::minifi::api::core::detail
