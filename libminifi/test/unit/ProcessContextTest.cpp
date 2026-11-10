// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <memory>
#include "unit/Catch.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/FlowFile.h"
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
