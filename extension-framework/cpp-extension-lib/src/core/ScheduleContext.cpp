// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "api/core/ScheduleContext.h"

#include "CffiPropertyReaders.h"
#include "api/utils/minifi-c-utils.h"

namespace org::apache::nifi::minifi::api::core {

using detail::DynamicPropertiesCallback;
using detail::PropertyCallback;
using detail::readControllerService;
using detail::readDynamicProperties;
using detail::readProperty;

std::expected<std::string, std::error_code> CffiScheduleContext::getProperty(const minifi::core::PropertyReference& prop) const {
  return readProperty([&](PropertyCallback cb, void* user_ctx) {
    return minifi_process_context_get_property(impl_, utils::minifiStringView(prop.name), nullptr, cb, user_ctx);
  });
}

std::map<std::string, std::string> CffiScheduleContext::getDynamicProperties() const {
  return readDynamicProperties([&](DynamicPropertiesCallback cb, void* user_ctx) {
    minifi_process_context_get_dynamic_properties(impl_, nullptr, cb, user_ctx);
  });
}

std::expected<minifi_controller_service*, std::error_code> CffiScheduleContext::getControllerService(const minifi::core::PropertyReference& prop) const {
  return readControllerService(prop, [&](auto name, auto type, auto out) {
    return minifi_process_context_get_controller_service_from_property(impl_, name, type, out);
  });
}

std::expected<std::optional<utils::net::SslData>, std::error_code> CffiScheduleContext::getSslData(const minifi::core::PropertyReference& prop) const {
  auto ssl_data = utils::net::SslData{};

  if (const auto status = minifi_process_context_get_ssl_data_from_property(
          impl_,
          utils::minifiStringView(prop.name),
          [](void* data, const minifi_ssl_data* minifi_ssl_data) {
            auto* my_ssl_data = static_cast<utils::net::SslData*>(data);
            my_ssl_data->ca_loc = utils::toString(minifi_ssl_data->ca_certificate_file);
            my_ssl_data->cert_loc = utils::toString(minifi_ssl_data->certificate_file);
            my_ssl_data->key_loc = utils::toString(minifi_ssl_data->private_key_file);
            my_ssl_data->key_pw = utils::toString(minifi_ssl_data->passphrase);
          },
          &ssl_data);
      status != MINIFI_STATUS_SUCCESS) {
    if (status == MINIFI_STATUS_PROPERTY_NOT_SET) {
      return std::nullopt;
    }
    return std::unexpected{utils::make_error_code(status)};
  }

  return ssl_data;
}

std::expected<std::optional<utils::ProxyData>, std::error_code> CffiScheduleContext::getProxyData(const minifi::core::PropertyReference& prop) const {
  auto proxy_data = utils::ProxyData{};
  const auto status = minifi_process_context_get_proxy_data_from_property(
      impl_,
      utils::minifiStringView(prop.name),
      [](void* data, const minifi_proxy_data* minifi_proxy_data) {
        auto* proxy = static_cast<utils::ProxyData*>(data);
        proxy->host = utils::toString(minifi_proxy_data->hostname);
        proxy->port = minifi_proxy_data->port;
        if (minifi_proxy_data->password && minifi_proxy_data->username) {
          proxy->proxy_credentials = utils::BasicAuthCredentials{.username = utils::toString(*minifi_proxy_data->username),
              .password = utils::toString(*minifi_proxy_data->password)};
        } else {
          proxy->proxy_credentials = std::nullopt;
        }
        if (minifi_proxy_data->proxy_type == MINIFI_PROXY_TYPE_HTTP) {
          proxy->proxy_type = utils::ProxyType::HTTP;
        } else {
          proxy->proxy_type = utils::ProxyType::DIRECT;
        }
      },
      &proxy_data);
  if (status == MINIFI_STATUS_PROPERTY_NOT_SET) { return std::nullopt; }
  if (status == MINIFI_STATUS_SUCCESS) { return proxy_data; }
  return std::unexpected{utils::make_error_code(status)};
}

std::expected<void, std::error_code> CffiScheduleContext::setTriggerWhenEmpty(bool trigger_when_empty) {
  const auto status = minifi_process_context_set_trigger_when_empty(impl_, trigger_when_empty);
  if (status != MINIFI_STATUS_SUCCESS) {
    return std::unexpected{utils::make_error_code(status)};
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::api::core
