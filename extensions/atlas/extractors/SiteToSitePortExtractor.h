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

// Site-to-Site: matched on componentType (RemoteProcessGroupPort).
//
// The dataset is keyed on the remote port's UUID, stamped onto the event by SiteToSiteClient
// as the "s2s.port.id" attribute. That id equals the receiving instance's own port component
// id, so the resulting <port-uuid>@<namespace> qualifiedName merges in Atlas with the entity
// the receiver advertises for the same port — this is what makes cross-instance lineage work.
// (The transit URI can't be used for this: it embeds a per-transfer flow-file UUID, so it
// changes every transfer and never matches the receiver. We keep it only for the peer host,
// which the reporting task resolves to the port's namespace, and as a last-resort identifier.)
//
// The entity type follows the remote port's real kind, which both sides agree on: a SEND
// targets a remote input port (nifi_input_port); a RECEIVE pulls from a remote output port
// (nifi_output_port). The reporting task maps the direction-specific system names accordingly.
class SiteToSitePortExtractor : public DatasetExtractor {
 public:
  // NiFi's SiteToSiteAttributes.S2S_PORT_ID; set by SiteToSiteClient on the SEND/RECEIVE event.
  static constexpr std::string_view S2S_PORT_ID_ATTRIBUTE = "s2s.port.id";

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

    // Prefer the remote port UUID; fall back to the transit URI only if the attribute is
    // absent (e.g. an older MiNiFi that didn't stamp it), which keeps some lineage rather
    // than dropping the event, at the cost of not being correlatable.
    std::string identifier{transit};
    const auto attributes = event.getAttributes();
    if (const auto it = attributes.find(std::string{S2S_PORT_ID_ATTRIBUTE}); it != attributes.end() && !it->second.empty()) {
      identifier = it->second;
    }

    const bool is_send = event.getEventType() == provenance::ProvenanceEventRecord::SEND;
    Dataset ds{
        .system = is_send ? "site-to-site-input-port" : "site-to-site-output-port",
        .identifier = std::move(identifier),
        .host = host.empty() ? std::nullopt : std::optional{host},
        .attributes = {}};
    if (is_send) {
      refs.outputs.push_back(std::move(ds));
    } else {
      refs.inputs.push_back(std::move(ds));
    }
    return refs;
  }
};

}  // namespace org::apache::nifi::minifi::extensions::atlas::extractors
