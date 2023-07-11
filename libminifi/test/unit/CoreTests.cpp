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

#include "../Catch.h"
#include "core/Core.h"

namespace org::apache::nifi::minifi::test {

struct DummyStruct {};
class DummyClass {};
template<typename T> struct DummyStructTemplate {};
template<typename T> class DummyClassTemplate {};

TEST_CASE("getClassName() works correctly") {
  CHECK(core::getClassName<DummyStruct>() == "org::apache::nifi::minifi::test::DummyStruct");
  CHECK(core::getClassName<DummyClass>() == "org::apache::nifi::minifi::test::DummyClass");
  CHECK(core::getClassName<DummyStructTemplate<int>>() == "org::apache::nifi::minifi::test::DummyStructTemplate<int>");
  CHECK(core::getClassName<DummyClassTemplate<int>>() == "org::apache::nifi::minifi::test::DummyClassTemplate<int>");
}

TEST_CASE("className() works correctly and is constexpr") {
  static constexpr auto struct_name = core::className<DummyStruct>();
  static_assert(struct_name == "org::apache::nifi::minifi::test::DummyStruct");

  static constexpr auto class_name = core::className<DummyClass>();
  static_assert(class_name == "org::apache::nifi::minifi::test::DummyClass");

  static constexpr auto struct_template_name = core::className<DummyStructTemplate<int>>();
  static_assert(struct_template_name == "org::apache::nifi::minifi::test::DummyStructTemplate<int>");

  static constexpr auto class_template_name = core::className<DummyClassTemplate<int>>();
  static_assert(class_template_name == "org::apache::nifi::minifi::test::DummyClassTemplate<int>");
}

}  // namespace org::apache::nifi::minifi::test
