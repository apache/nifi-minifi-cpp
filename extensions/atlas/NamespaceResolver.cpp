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
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::extensions::atlas {

void NamespaceResolver::addRule(std::string namespace_name, std::vector<std::regex> patterns) {
  if (patterns.empty() || namespace_name.empty()) {
    return;
  }
  rules_.push_back({std::move(namespace_name), std::move(patterns)});
}

void NamespaceResolver::addRuleFromWhitespaceList(std::string namespace_name, std::string_view whitespace_separated_regexes) {
  std::vector<std::regex> patterns;
  for (const auto& raw : utils::string::splitAndTrimRemovingEmpty(whitespace_separated_regexes, " ")) {
    // A malformed regex throws std::regex_error at construction. That is a configuration
    // bug, and ReportLineageToAtlas::onSchedule catches it to report which dynamic
    // property is at fault before failing the schedule.
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
