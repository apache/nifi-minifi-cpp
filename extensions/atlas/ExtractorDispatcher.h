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

#include <memory>
#include <vector>

#include "DatasetExtractor.h"

namespace org::apache::nifi::minifi::extensions::atlas {

// ExtractorDispatcher routes a provenance event to the first extractor that
// matches by componentType regex, transit-URI regex, or event type — in that
// order. First hit wins, matching NiFi's SimpleFlowPathLineage dispatch. Owns
// its extractors by unique_ptr so the reporting task can keep a single instance
// alive across trigger cycles.
class ExtractorDispatcher {
 public:
  // Constructs a dispatcher with the built-in extractor set.
  ExtractorDispatcher();

  DatasetReferences dispatch(const provenance::ProvenanceEventRecord& event) const;

 private:
  std::vector<std::unique_ptr<DatasetExtractor>> extractors_;
};

}  // namespace org::apache::nifi::minifi::extensions::atlas
