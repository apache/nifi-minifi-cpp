/**
 *
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
#include "core/repository/VolatileProvenanceRepository.h"

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "io/BufferStream.h"
#include "utils/ParsingUtils.h"

namespace org::apache::nifi::minifi::core::repository {

namespace {
class EventCursor : public provenance::ProvenanceRepository::Cursor {
 public:
  explicit EventCursor(uint64_t last_event_ordinal): last_event_ordinal_(last_event_ordinal) {}

  // the ordinal of the last event observed, 0 if no event has been read yet
  [[nodiscard]]
  std::string toString() const override {
    return std::to_string(last_event_ordinal_);
  }

  uint64_t last_event_ordinal_;
};
}  // namespace

std::unique_ptr<provenance::ProvenanceRepository::Cursor> VolatileProvenanceRepository::cursorFromString(std::string_view cursor_str) {
  if (cursor_str.empty()) {
    return std::make_unique<EventCursor>(0);
  }
  if (auto last_event_ordinal = parsing::parseIntegral<uint64_t>(cursor_str)) {
    return std::make_unique<EventCursor>(*last_event_ordinal);
  }
  logger_->log_warn("Could not interpret provenance cursor '{}', reading from the first event", cursor_str);
  return std::make_unique<EventCursor>(0);
}

std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> VolatileProvenanceRepository::getEvents(size_t max_size, Cursor* cursor) {
  auto* event_cursor = dynamic_cast<EventCursor*>(cursor);
  if (cursor && !event_cursor) {
    return std::unexpected{"Invalid cursor"};
  }
  if (max_size == 0) {
    return {};
  }
  const uint64_t first_event_ordinal = event_cursor ? event_cursor->last_event_ordinal_ + 1 : 0;

  // The entries are stored in no particular order, but each of them is keyed by the ordinal of
  // the event it holds, so the events to be returned can be selected without deserializing all
  // of them.
  std::vector<std::pair<uint64_t, AtomicEntry<std::string>*>> candidates;
  const auto event_ordinal_of = [] (const std::pair<uint64_t, AtomicEntry<std::string>*>& candidate) { return candidate.first; };
  for (auto* entry : repo_data_.value_vector) {
    entry->visitValue([&] (const std::string& key, std::span<const std::byte> /*buffer*/) {
      if (auto event_ordinal = parsing::parseIntegral<uint64_t>(key); event_ordinal && *event_ordinal >= first_event_ordinal) {
        candidates.emplace_back(*event_ordinal, entry);
      }
    });
  }
  std::ranges::sort(candidates, std::less<>{}, event_ordinal_of);
  if (candidates.size() > max_size) {
    candidates.resize(max_size);
  }

  std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> records;
  records.reserve(candidates.size());
  for (const auto& [event_ordinal, entry] : candidates) {
    auto event = provenance::ProvenanceEventRecord::create();
    bool deserialized = false;
    entry->visitValue([&] (const std::string& key, std::span<const std::byte> buffer) {
      if (key != std::to_string(event_ordinal)) {
        // the entry has been overwritten since it was selected
        return;
      }
      io::BufferStream stream{buffer};
      deserialized = event->deserialize(stream);
    });
    if (deserialized) {
      records.push_back(std::move(event));
    } else {
      logger_->log_warn("Could not read provenance event '{}'", event_ordinal);
    }
  }
  if (event_cursor && !candidates.empty()) {
    // advanced even for the events we failed to read, otherwise they would stall the iteration
    event_cursor->last_event_ordinal_ = candidates.back().first;
  }

  return records;
}

}  // namespace org::apache::nifi::minifi::core::repository
