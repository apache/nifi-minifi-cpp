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

// Site-to-Site: matched on componentType (RemoteProcessGroupPort). The transit
// URI is the remote NiFi endpoint URL. Direction is unambiguous from event type
// (SEND → the remote port is a downstream sink; RECEIVE/FETCH → upstream source).
// The dataset system name "site-to-site-port" is mapped by the reporting task to
// nifi_input_port / nifi_output_port depending on direction (an input to us is
// an output_port on the remote NiFi, and vice versa).
class SiteToSitePortExtractor : public DatasetExtractor {
 public:
  std::optional<std::regex> componentTypePattern() const override {
    static const std::regex pattern{"^RemoteProcessGroupPort$"};
    return pattern;
  }

  DatasetReferences extract(const provenance::ProvenanceEventRecord& event) const override {
    DatasetReferences refs;
    const auto transit = event.getTransitUri();
    if (transit.empty()) return refs;

    std::string host;
    const auto scheme_end = transit.find("://");
    if (scheme_end != std::string::npos) {
      const auto rest = transit.substr(scheme_end + 3);
      const auto slash = rest.find('/');
      host = (slash == std::string::npos) ? rest : rest.substr(0, slash);
      if (const auto colon = host.find(':'); colon != std::string::npos) {
        host = host.substr(0, colon);
      }
    }

    Dataset ds{.system = "site-to-site-port", .identifier = transit, .host = host.empty() ? std::nullopt : std::optional{host}, .attributes = {}};
    if (event.getEventType() == provenance::ProvenanceEventRecord::SEND) {
      refs.outputs.push_back(std::move(ds));
    } else {
      refs.inputs.push_back(std::move(ds));
    }
    return refs;
  }
};

}  // namespace org::apache::nifi::minifi::extensions::atlas::extractors
