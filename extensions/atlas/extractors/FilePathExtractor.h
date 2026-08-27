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

// Local filesystem: matched on transit URI (^file:/.*). Whether the identifier is
// the file or its containing directory is controlled by the reporting task's
// "Filesystem Path Level" property (dispatcher passes it via ExtractorContext).
// This extractor emits the file path; the reporting task collapses it to a
// directory at serialize time if configured to.
class FilePathExtractor : public DatasetExtractor {
 public:
  std::optional<std::regex> transitUriPattern() const override {
    static const std::regex pattern{"^file:/.*"};
    return pattern;
  }

  DatasetReferences extract(const provenance::ProvenanceEventRecord& event) const override {
    DatasetReferences refs;
    const auto uri = const_cast<provenance::ProvenanceEventRecord&>(event).getTransitUri();
    // Accept both "file:///abs/path" and "file:/abs/path"; extract everything after
    // the scheme's leading "file:".
    constexpr std::string_view scheme = "file:";
    if (!uri.starts_with(scheme)) return refs;
    auto path = std::string_view{uri}.substr(scheme.size());
    // Strip leading //authority if present (RFC 3986). We accept authority-less
    // file URIs since MiNiFi's PutFile emits "file:///path".
    while (!path.empty() && path.front() == '/') {
      if (path.size() >= 2 && path[1] == '/') {
        path = path.substr(1);
        continue;
      }
      break;
    }
    if (path.empty()) return refs;

    Dataset ds{.system = "file", .identifier = std::string{path}, .host = std::string{"localhost"}, .attributes = {}};
    if (event.getEventType() == provenance::ProvenanceEventRecord::SEND) {
      refs.outputs.push_back(std::move(ds));
    } else {
      refs.inputs.push_back(std::move(ds));
    }
    return refs;
  }
};

}  // namespace org::apache::nifi::minifi::extensions::atlas::extractors
