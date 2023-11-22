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
#include <string_view>
#include "Catch.h"
#include "AttributeRollingWindow.h"
#include "SingleProcessorTestController.h"
#include "range/v3/view/zip.hpp"
#include "range/v3/algorithm/contains.hpp"

namespace org::apache::nifi::minifi::test {
using AttributeRollingWindow = processors::AttributeRollingWindow;

TEST_CASE("AttributeRollingWindow", "[attributerollingwindow]") {
  const auto proc = std::make_shared<AttributeRollingWindow>("AttributeRollingWindow");
  proc->setProperty(AttributeRollingWindow::ValueToTrack, "${value}");
  proc->setProperty(AttributeRollingWindow::WindowLength, "3");
  SingleProcessorTestController controller{proc};
}

}  // namespace org::apache::nifi::minifi::test
