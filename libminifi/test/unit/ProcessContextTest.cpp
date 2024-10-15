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
#include <memory>
#include "unit/Catch.h"
#include "core/ProcessContext.h"
#include "core/FlowFile.h"
#include "utils/meta/detected.h"

namespace org::apache::nifi::minifi {

template<typename... Args>
using getProperty_compiles = decltype(std::declval<core::ProcessContext>().getProperty(std::declval<Args>()...));

TEST_CASE("ProcessContextTest") {
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, std::string, const core::FlowFile&>);
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, std::string, const std::shared_ptr<core::FlowFile>&>);
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, std::string, core::FlowFile&>);
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, std::string, std::shared_ptr<core::FlowFile>>);
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, const char*, const core::FlowFile&>);
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, const char*, const std::shared_ptr<core::FlowFile>&>);
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, const char*, core::FlowFile&>);
  STATIC_REQUIRE_FALSE(utils::meta::is_detected_v<getProperty_compiles, const char*, std::shared_ptr<core::FlowFile>>);
}


}  // namespace org::apache::nifi::minifi
