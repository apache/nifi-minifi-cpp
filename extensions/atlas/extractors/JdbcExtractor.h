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

// SQL processors (ExecuteSQL / PutSQL / QueryDatabaseTable) emit provenance
// events whose transit URI is the JDBC connection string (e.g.
// "jdbc:postgresql://db.example.com:5432/warehouse"). Atlas represents a JDBC
// database as an rdbms_instance whose qualifiedName is the JDBC URL; we surface
// that. Table-level lineage (rdbms_table) would need SQL parsing, which the
// NiFi hook also punts on for MiNiFi's usage.
class JdbcExtractor : public DatasetExtractor {
 public:
  std::optional<std::regex> componentTypePattern() const override {
    static const std::regex pattern{"^(Execute|Put)SQL$|^QueryDatabaseTable$"};
    return pattern;
  }

  DatasetReferences extract(const provenance::ProvenanceEventRecord& event) const override {
    DatasetReferences refs;
    const auto transit = const_cast<provenance::ProvenanceEventRecord&>(event).getTransitUri();
    if (transit.empty() || !transit.starts_with("jdbc:")) {
      return refs;
    }

    // jdbc:<subprotocol>://<host>[:port]/<db>[?...]
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

    Dataset ds{.system = "jdbc", .identifier = transit, .host = host.empty() ? std::nullopt : std::optional{host}, .attributes = {}};
    // ExecuteSQL / QueryDatabaseTable read from the DB (RECEIVE/FETCH → input).
    // PutSQL writes to the DB (SEND → output). Fall back to component type.
    const auto component_type = event.getComponentType();
    if (component_type == "PutSQL" || event.getEventType() == provenance::ProvenanceEventRecord::SEND) {
      refs.outputs.push_back(std::move(ds));
    } else {
      refs.inputs.push_back(std::move(ds));
    }
    return refs;
  }
};

}  // namespace org::apache::nifi::minifi::extensions::atlas::extractors
