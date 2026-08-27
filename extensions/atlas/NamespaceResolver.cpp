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
#include "NamespaceResolver.h"

#include <cctype>
#include <utility>

namespace org::apache::nifi::minifi::extensions::atlas {

namespace {

std::vector<std::string> splitOnWhitespace(std::string_view input) {
  std::vector<std::string> parts;
  std::string current;
  for (char c : input) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) {
        parts.push_back(std::move(current));
        current.clear();
      }
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) {
    parts.push_back(std::move(current));
  }
  return parts;
}

}  // namespace

void NamespaceResolver::addRule(std::string namespace_name, std::vector<std::regex> patterns) {
  if (patterns.empty() || namespace_name.empty()) {
    return;
  }
  rules_.push_back({std::move(namespace_name), std::move(patterns)});
}

void NamespaceResolver::addRuleFromWhitespaceList(std::string namespace_name, std::string_view whitespace_separated_regexes) {
  std::vector<std::regex> patterns;
  for (const auto& raw : splitOnWhitespace(whitespace_separated_regexes)) {
    // A malformed regex would throw at construction; that is a configuration bug and
    // should surface at onSchedule time when the caller iterates dynamic properties.
    patterns.emplace_back(raw);
  }
  addRule(std::move(namespace_name), std::move(patterns));
}

std::string NamespaceResolver::resolve(std::string_view host) const {
  if (host.empty()) {
    return default_namespace_;
  }
  const std::string host_str{host};
  for (const auto& rule : rules_) {
    for (const auto& pattern : rule.patterns) {
      if (std::regex_match(host_str, pattern)) {
        return rule.namespace_name;
      }
    }
  }
  return default_namespace_;
}

}  // namespace org::apache::nifi::minifi::extensions::atlas
