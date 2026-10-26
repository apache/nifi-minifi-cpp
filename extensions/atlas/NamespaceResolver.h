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
#pragma once

#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace org::apache::nifi::minifi::extensions::atlas {

// Maps a hostname (extracted from a transit URI) to an Atlas metadata namespace
// string. Mirrors NiFi's RegexNamespaceResolver: for each dynamic property named
// hostnamePattern.<namespaceName>, the value is a whitespace-separated list of
// regexes. If any regex matches the host, the resolver returns <namespaceName>.
// Otherwise it returns the configured default namespace.
//
// The '@' delimiter that appears in every Atlas qualifiedName ("<id>@<ns>") is
// applied by callers; the resolver returns the raw namespace string.
class NamespaceResolver {
 public:
  // Registers a namespace rule. Every pattern is a full-match regex against the host.
  // If patterns is empty, the rule matches nothing (used only to preserve caller
  // ordering — an empty pattern list should be filtered by the caller).
  void addRule(std::string namespace_name, std::vector<std::regex> patterns);

  // Convenience: parse "  a\.example\.com  b\.example\.com " into two regexes.
  void addRuleFromWhitespaceList(std::string namespace_name, std::string_view whitespace_separated_regexes);

  void setDefaultNamespace(std::string default_ns) { default_namespace_ = std::move(default_ns); }
  const std::string& defaultNamespace() const { return default_namespace_; }

  // Resolves the namespace for a host. If host is empty, returns the default.
  std::string resolve(std::string_view host) const;

 private:
  struct Rule {
    std::string namespace_name;
    std::vector<std::regex> patterns;
  };
  std::vector<Rule> rules_;
  std::string default_namespace_;
};

}  // namespace org::apache::nifi::minifi::extensions::atlas
