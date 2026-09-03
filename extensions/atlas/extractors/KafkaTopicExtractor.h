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

// Kafka: matched by componentType (^(Consume|Publish)Kafka.*$). We prefer the
// kafka.topic attribute set by MiNiFi's Kafka processors, and fall back to
// parsing kafka://broker[:port]/<topic> from the transit URI. The host (broker)
// is retained so NamespaceResolver can map it to a cluster namespace.
class KafkaTopicExtractor : public DatasetExtractor {
 public:
  std::optional<std::regex> componentTypePattern() const override {
    static const std::regex pattern{"^(Consume|Publish)Kafka.*$"};
    return pattern;
  }

  DatasetReferences extract(const provenance::ProvenanceEventRecord& event) const override {
    DatasetReferences refs;
    std::string topic;
    std::string host;
    const auto attributes = event.getAttributes();
    if (auto it = attributes.find("kafka.topic"); it != attributes.end()) {
      topic = it->second;
    }
    if (topic.empty()) {
      // Parse kafka://host[:port]/topic
      const auto transit = event.getTransitUri();
      constexpr std::string_view scheme = "kafka://";
      if (transit.starts_with(scheme)) {
        const auto rest = transit.substr(scheme.size());
        const auto slash = rest.find('/');
        if (slash != std::string::npos) {
          host = rest.substr(0, slash);
          const auto colon = host.find(':');
          if (colon != std::string::npos) host = host.substr(0, colon);
          topic = rest.substr(slash + 1);
        }
      }
    }
    if (topic.empty()) {
      return refs;
    }
    Dataset ds{.system = "kafka", .identifier = topic, .host = host.empty() ? std::nullopt : std::optional{host}, .attributes = {}};
    const auto component_type = event.getComponentType();
    if (component_type.starts_with("Consume")) {
      refs.inputs.push_back(std::move(ds));
    } else {
      refs.outputs.push_back(std::move(ds));
    }
    return refs;
  }
};

}  // namespace org::apache::nifi::minifi::extensions::atlas::extractors
