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
#include <unordered_map>
#include <utility>
#include <vector>

#include "minifi-cpp/provenance/Provenance.h"

namespace org::apache::nifi::minifi::extensions::atlas {

// Dataset is the Atlas-neutral value type an extractor emits when it recognizes a
// provenance event. `system` names the class of external system ("kafka", "s3",
// "file", "http", "jdbc", "site-to-site-port"); the reporting task maps `system` to
// an Atlas entity type at emit time (kafka -> kafka_topic, s3 -> aws_s3_v2_directory,
// etc.). `identifier` is the natural-key portion of the qualifiedName BEFORE the
// namespace suffix; `host` is used by the namespace resolver.
struct Dataset {
  std::string system;
  std::string identifier;
  std::optional<std::string> host;
  std::unordered_map<std::string, std::string> attributes;
};

// DatasetReferences captures the extraction result for one provenance event:
// a list of upstream datasets (inputs, for RECEIVE/FETCH-shaped events) and/or
// a list of downstream datasets (outputs, for SEND-shaped events). Most events
// populate exactly one side.
struct DatasetReferences {
  std::vector<Dataset> inputs;
  std::vector<Dataset> outputs;

  bool empty() const { return inputs.empty() && outputs.empty(); }
};

// DatasetExtractor is the plug-in point that turns a provenance event into a
// dataset reference. Each concrete extractor targets one external system by
// declaring one or more of:
//   - a regex on componentType (most specific)
//   - a regex on transitUri
//   - an exact eventType match
// The dispatcher probes those three fields in that order and calls the first
// extractor that matches. First-hit-wins mirrors NiFi's SimpleFlowPathLineage.
class DatasetExtractor {
 public:
  virtual ~DatasetExtractor() = default;
  [[nodiscard]] virtual std::optional<std::regex> componentTypePattern() const { return std::nullopt; }
  [[nodiscard]] virtual std::optional<std::regex> transitUriPattern() const { return std::nullopt; }
  [[nodiscard]] virtual std::optional<provenance::ProvenanceEventRecord::ProvenanceEventType> eventType() const { return std::nullopt; }
  [[nodiscard]] virtual DatasetReferences extract(const provenance::ProvenanceEventRecord& event) const = 0;
};

}  // namespace org::apache::nifi::minifi::extensions::atlas
