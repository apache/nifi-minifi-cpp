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

#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>

#include "DatasetExtractor.h"

namespace org::apache::nifi::minifi::extensions::atlas::extractors {

// InvokeHTTP: matched on componentType. The dataset identifier is method+URL so
// that GET https://api.example.com/orders and POST to the same URL are distinct
// lineage endpoints in Atlas. The host portion of the URL is used for namespace
// resolution.
class InvokeHttpExtractor : public DatasetExtractor {
 public:
  std::optional<std::regex> componentTypePattern() const override {
    static const std::regex pattern{"^InvokeHTTP$"};
    return pattern;
  }

  DatasetReferences extract(const provenance::ProvenanceEventRecord& event) const override {
    DatasetReferences refs;
    const auto uri = const_cast<provenance::ProvenanceEventRecord&>(event).getTransitUri();
    if (uri.empty()) return refs;

    const auto attributes = const_cast<provenance::ProvenanceEventRecord&>(event).getAttributes();
    std::string method = "GET";
    if (auto it = attributes.find("invokehttp.request.method"); it != attributes.end()) {
      method = it->second;
    }

    // Extract the host from https?://host[:port]/... for namespace resolution.
    std::string host;
    const auto scheme_end = uri.find("://");
    if (scheme_end != std::string::npos) {
      const auto rest = uri.substr(scheme_end + 3);
      const auto slash = rest.find('/');
      host = (slash == std::string::npos) ? rest : rest.substr(0, slash);
      if (const auto colon = host.find(':'); colon != std::string::npos) {
        host = host.substr(0, colon);
      }
    }

    Dataset ds{
        .system = "http",
        .identifier = method + " " + uri,
        .host = host.empty() ? std::nullopt : std::optional{host},
        .attributes = {{"method", method}, {"url", uri}},
    };
    if (event.getEventType() == provenance::ProvenanceEventRecord::SEND) {
      refs.outputs.push_back(std::move(ds));
    } else {
      refs.inputs.push_back(std::move(ds));
    }
    return refs;
  }
};

}  // namespace org::apache::nifi::minifi::extensions::atlas::extractors
