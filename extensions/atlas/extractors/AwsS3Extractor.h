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

// AWS S3: matched on transit URI (^s3(a|n)?://.*). Strips the object key to the
// containing directory prefix, matching how Atlas's canonical S3 hooks emit
// aws_s3_v2_directory / aws_s3_pseudo_dir entities. Direction is SEND -> output,
// anything else -> input.
class AwsS3Extractor : public DatasetExtractor {
 public:
  std::optional<std::regex> transitUriPattern() const override {
    static const std::regex pattern{"^s3(a|n)?://.*"};
    return pattern;
  }

  DatasetReferences extract(const provenance::ProvenanceEventRecord& event) const override {
    DatasetReferences refs;
    const auto uri = const_cast<provenance::ProvenanceEventRecord&>(event).getTransitUri();
    // Locate the ':' after the scheme prefix and skip "//".
    const auto scheme_end = uri.find("://");
    if (scheme_end == std::string::npos) return refs;
    auto rest = std::string_view{uri}.substr(scheme_end + 3);  // bucket/prefix/key

    const auto slash = rest.find('/');
    if (slash == std::string::npos) return refs;

    const auto bucket = rest.substr(0, slash);
    auto path = rest.substr(slash);  // starts with '/'
    // Trim the object key: keep everything up to and including the last '/'.
    const auto last_slash = path.rfind('/');
    if (last_slash != std::string::npos) {
      path = path.substr(0, last_slash + 1);
    }

    Dataset ds{
        .system = "s3",
        .identifier = "s3://" + std::string{bucket} + std::string{path},
        .host = std::string{bucket},
        .attributes = {{"bucket", std::string{bucket}}},
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
