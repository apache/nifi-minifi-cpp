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

#include "api/core/ProcessContext.h"

#include "api/core/FlowFile.h"
#include "api/utils/minifi-c-utils.h"

namespace org::apache::nifi::minifi::api::core {

std::expected<std::string, std::error_code> CffiProcessContext::getProperty(const minifi::core::PropertyReference& property_reference,
    const FlowFile* flow_file) const {
  return getProperty(property_reference.name, flow_file);
}

std::expected<std::string, std::error_code> CffiProcessContext::getProperty(std::string_view name, const FlowFile* flow_file) const {
  std::optional<std::string> value;
  const MinifiStatus status = MinifiProcessContextGetProperty(
      impl_,
      utils::minifiStringView(name),
      flow_file ? flow_file->get() : MINIFI_NULL,
      [](void* data, const MinifiStringView result) { (*static_cast<std::optional<std::string>*>(data)) = std::string(result.data, result.length); },
      &value);

  if (!value) { return std::unexpected{utils::make_error_code(status)}; }
  return value.value();
}

bool CffiProcessContext::hasNonEmptyProperty(std::string_view name) const {
  return MinifiProcessContextHasNonEmptyProperty(impl_, utils::minifiStringView(name));
}

std::expected<MinifiControllerService*, std::error_code> CffiProcessContext::getControllerService(const minifi::core::PropertyReference& prop) const {
  MinifiControllerService* controller_service = nullptr;
  gsl_Assert(prop.allowed_types.size() == 1);
  const MinifiStatus status = MinifiProcessContextGetControllerServiceFromProperty(impl_,
          utils::minifiStringView(prop.name),
          utils::minifiStringView(prop.allowed_types[0]),
          &controller_service);
  if (status == MINIFI_STATUS_PROPERTY_NOT_SET) {
    return nullptr;
  }
  if (status != MINIFI_STATUS_SUCCESS) {
    return std::unexpected{utils::make_error_code(status)};
  }
  return controller_service;
}

std::map<std::string, std::string> CffiProcessContext::getDynamicProperties(const FlowFile* flow_file) const {
  std::map<std::string, std::string> result;
  MinifiProcessContextGetDynamicProperties(
      impl_,
      flow_file ? flow_file->get() : MINIFI_NULL,
      [](void* user_ctx, const MinifiStringView key, const MinifiStringView value) {
        static_cast<std::map<std::string, std::string>*>(user_ctx)->emplace(utils::toString(key), utils::toString(value));
      },
      &result);
  return result;
}

std::expected<std::optional<utils::net::SslData>, std::error_code> CffiProcessContext::getSslData(const minifi::core::PropertyReference& prop) const {
  const auto controller_name = getProperty(prop, nullptr);
  if (!controller_name) { return std::nullopt; }

  auto ssl_data = utils::net::SslData{};

  if (const auto status = MinifiProcessContextGetSslDataFromProperty(impl_, utils::minifiStringView(prop.name), [](void* data, const MinifiSslData* minifi_ssl_data) {
      auto* my_ssl_data = static_cast<utils::net::SslData*>(data);
      my_ssl_data->ca_loc = utils::toString(minifi_ssl_data->ca_certificate_file);
      my_ssl_data->cert_loc = utils::toString(minifi_ssl_data->certificate_file);
      my_ssl_data->key_loc = utils::toString(minifi_ssl_data->private_key_file);
      my_ssl_data->key_pw = utils::toString(minifi_ssl_data->passphrase);
  }, &ssl_data);
      status != MINIFI_STATUS_SUCCESS) {
    return std::unexpected{utils::make_error_code(status)};
  }

  return ssl_data;
}

std::expected<std::optional<utils::ProxyData>, std::error_code> CffiProcessContext::getProxyData(const minifi::core::PropertyReference& prop) const {
  auto proxy_data = utils::ProxyData{};
  const auto status = MinifiProcessContextGetProxyDataFromProperty(
      impl_,
      utils::minifiStringView(prop.name),
      [](void* data, const MinifiProxyData* minifi_proxy_data) {
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

}  // namespace org::apache::nifi::minifi::api::core
