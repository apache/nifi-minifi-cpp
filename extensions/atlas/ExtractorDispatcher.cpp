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
#include "ExtractorDispatcher.h"

#include "extractors/KafkaTopicExtractor.h"
#include "extractors/AwsS3Extractor.h"
#include "extractors/FilePathExtractor.h"
#include "extractors/InvokeHttpExtractor.h"
#include "extractors/JdbcExtractor.h"
#include "extractors/SiteToSitePortExtractor.h"

namespace org::apache::nifi::minifi::extensions::atlas {

using extractors::KafkaTopicExtractor;
using extractors::AwsS3Extractor;
using extractors::FilePathExtractor;
using extractors::InvokeHttpExtractor;
using extractors::JdbcExtractor;
using extractors::SiteToSitePortExtractor;

ExtractorDispatcher::ExtractorDispatcher() {
  // Registration order sets tie-break priority; we register the more specific
  // (componentType-matched) extractors first, then the URI-matched ones. Order
  // matters only when both a componentType and a URI regex would fire — but the
  // dispatch walks by-componentType FIRST regardless.
  extractors_.push_back(std::make_unique<KafkaTopicExtractor>());
  extractors_.push_back(std::make_unique<SiteToSitePortExtractor>());
  extractors_.push_back(std::make_unique<InvokeHttpExtractor>());
  extractors_.push_back(std::make_unique<JdbcExtractor>());
  extractors_.push_back(std::make_unique<AwsS3Extractor>());
  extractors_.push_back(std::make_unique<FilePathExtractor>());
}

DatasetReferences ExtractorDispatcher::dispatch(const provenance::ProvenanceEventRecord& event) const {
  const auto component_type = event.getComponentType();
  const auto transit_uri = const_cast<provenance::ProvenanceEventRecord&>(event).getTransitUri();
  const auto event_type = event.getEventType();

  // Pass 1: componentType regex. Most specific — a Kafka processor's provenance
  // event should never fall through to a generic URI matcher.
  for (const auto& extractor : extractors_) {
    if (const auto pattern = extractor->componentTypePattern()) {
      if (std::regex_match(component_type, *pattern)) {
        return extractor->extract(event);
      }
    }
  }
  // Pass 2: transit URI regex — for events whose componentType we don't recognize
  // but whose URI scheme identifies the external system (e.g. s3://, file:/).
  if (!transit_uri.empty()) {
    for (const auto& extractor : extractors_) {
      if (const auto pattern = extractor->transitUriPattern()) {
        if (std::regex_match(transit_uri, *pattern)) {
          return extractor->extract(event);
        }
      }
    }
  }
  // Pass 3: event type. Last-resort catch-all for events with a transit URI but
  // no scheme we recognize. Reserved for a future generic RemoteInvocation
  // extractor.
  for (const auto& extractor : extractors_) {
    if (const auto et = extractor->eventType()) {
      if (*et == event_type) {
        return extractor->extract(event);
      }
    }
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::extensions::atlas
