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

#include "ReportLineageToAtlas.h"
#include "core/Processor.h"
#include "unit/Catch.h"
#include "unit/ReportingTaskUtils.h"

namespace org::apache::nifi::minifi::extensions::atlas::test {

TEST_CASE("ReportLineageToAtlas registers its properties", "[atlas]") {
  auto task = minifi::test::utils::make_reporting_task<ReportLineageToAtlas>("test-report-lineage-to-atlas");
  REQUIRE(task);
  REQUIRE(task->getName() == "test-report-lineage-to-atlas");
  task->initialize();

  // Sanity-check that each declared property is present in the supported set.
  const auto supported = task->getSupportedProperties();
  REQUIRE(supported.contains(ReportLineageToAtlas::AtlasUrls.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::NiFiUrl.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::DefaultNamespace.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::AwsS3ModelVersion.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::FilesystemPathLevel.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::ProvenanceBatchSize.name));
}

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
