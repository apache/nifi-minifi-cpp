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

#include "ProcessContextExpr.h"

#include <memory>
#include <string>

#include "asio/detail/mutex.hpp"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::core {

nonstd::expected<std::string, std::error_code> ProcessContextExpr::getProperty(const std::string_view name, const FlowFile* flow_file) const {
  const auto property = getProcessor().getSupportedProperty(name);
  if (!property) {
    return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty);
  }

  if (!property->supportsExpressionLanguage()) {
    return ProcessContextImpl::getProperty(name, flow_file);
  }
  if (!cached_expressions_.contains(name)) {
    auto expression_str = ProcessContextImpl::getProperty(name, flow_file);
    if (!expression_str) { return expression_str; }
    cached_expressions_.emplace(std::string{name}, expression::compile(*expression_str));
  }
  expression::Parameters p(this, flow_file);
  auto result = cached_expressions_[std::string{name}](p).asString();
  if (!property->getValidator().validate(result)) {
    return nonstd::make_unexpected(PropertyErrorCode::ValidationFailed);
  }
  return result;
}

nonstd::expected<std::string, std::error_code> ProcessContextExpr::getDynamicProperty(const std::string_view name, const FlowFile* flow_file) const {
  // all dynamic properties support EL
  if (!cached_dynamic_expressions_.contains(name)) {
    auto expression_str = ProcessContextImpl::getDynamicProperty(name, flow_file);
    if (!expression_str) { return expression_str; }
    cached_dynamic_expressions_.emplace(std::string{name}, expression::compile(*expression_str));
  }
  const expression::Parameters p(this, flow_file);
  return cached_dynamic_expressions_[std::string{name}](p).asString();
}

nonstd::expected<void, std::error_code> ProcessContextExpr::setProperty(const std::string_view name, std::string value) {
  cached_expressions_.erase(std::string{name});
  return ProcessContextImpl::setProperty(name, std::move(value));
}

nonstd::expected<void, std::error_code> ProcessContextExpr::setDynamicProperty(std::string name, std::string value) {
  cached_dynamic_expressions_.erase(name);
  return ProcessContextImpl::setDynamicProperty(std::move(name), std::move(value));
}

std::map<std::string, std::string> ProcessContextExpr::getDynamicProperties(const FlowFile* flow_file) const {
  auto dynamic_props = ProcessContextImpl::getDynamicProperties(flow_file);
  for (auto& [dynamic_property_name, dynamic_property_value] : dynamic_props) {
    if (!cached_dynamic_expressions_.contains(dynamic_property_name)) {
      auto expression = expression::compile(dynamic_property_value);
      expression::Parameters p(this, flow_file);
      dynamic_property_value = expression(p).asString();
      cached_dynamic_expressions_.emplace(std::string{dynamic_property_name}, std::move(expression));
    }
  }
  return dynamic_props;
}

}  // namespace org::apache::nifi::minifi::core
