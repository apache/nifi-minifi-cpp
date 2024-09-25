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

namespace org::apache::nifi::minifi::core {

bool ProcessContextExpr::getProperty(bool supports_expression_language, std::string_view property_name, std::string& value, const FlowFile* const flow_file) {
  if (!supports_expression_language) {
    return ProcessContextImpl::getProperty(property_name, value);
  }
  std::string name{property_name};
  if (expressions_.find(name) == expressions_.end()) {
    std::string expression_str;
    if (!ProcessContextImpl::getProperty(name, expression_str)) {
      return false;
    }
    logger_->log_debug("Compiling expression for {}/{}: {}", getProcessorNode()->getName(), name, expression_str);
    expressions_.emplace(name, expression::compile(expression_str));
    expression_strs_.insert_or_assign(name, expression_str);
  }

  minifi::expression::Parameters p(shared_from_this(), flow_file);
  value = expressions_[name](p).asString();
  logger_->log_debug(R"(expression "{}" of property "{}" evaluated to: {})", expression_strs_[name], name, value);
  return true;
}

bool ProcessContextExpr::getProperty(const Property& property, std::string& value, const FlowFile* const flow_file) {
  return getProperty(property.supportsExpressionLanguage(), property.getName(), value, flow_file);
}

bool ProcessContextExpr::getProperty(const PropertyReference& property, std::string& value, const FlowFile* const flow_file) {
  return getProperty(property.supports_expression_language, property.name, value, flow_file);
}

bool ProcessContextExpr::getDynamicProperty(const Property &property, std::string &value, const FlowFile* const flow_file) {
  if (!property.supportsExpressionLanguage()) {
    return ProcessContextImpl::getDynamicProperty(property.getName(), value);
  }
  auto name = property.getName();
  if (dynamic_property_expressions_.find(name) == dynamic_property_expressions_.end()) {
    std::string expression_str;
    ProcessContextImpl::getDynamicProperty(name, expression_str);
    logger_->log_debug("Compiling expression for {}/{}: {}", getProcessorNode()->getName(), name, expression_str);
    dynamic_property_expressions_.emplace(name, expression::compile(expression_str));
    expression_strs_.insert_or_assign(name, expression_str);
  }
  minifi::expression::Parameters p(shared_from_this(), flow_file);
  value = dynamic_property_expressions_[name](p).asString();
  logger_->log_debug(R"(expression "{}" of dynamic property "{}" evaluated to: {})", expression_strs_[name], name, value);
  return true;
}


bool ProcessContextExpr::setProperty(const std::string& property, std::string value) {
  expressions_.erase(property);
  return ProcessContextImpl::setProperty(property, value);
}

bool ProcessContextExpr::setDynamicProperty(const std::string& property, std::string value) {
  dynamic_property_expressions_.erase(property);
  return ProcessContextImpl::setDynamicProperty(property, value);
}

}  // namespace org::apache::nifi::minifi::core
