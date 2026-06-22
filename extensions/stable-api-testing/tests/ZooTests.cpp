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

#include "../AnimalControllerServices.h"
#include "CProcessorTestUtils.h"
#include "ZooProcessor.h"
#include "api/core/Resource.h"
#include "minifi-cpp/core/FlowFile.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "utils/CProcessor.h"

namespace org::apache::nifi::minifi::api_testing::test {
TEST_CASE("ZooTest") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<ZooProcessor>(
      core::ProcessorMetadata{utils::Identifier{}, "ZooProcessor", logging::LoggerFactory<ZooProcessor>::getLogger()}));
  const auto dog_with_jetpack = minifi::test::utils::make_custom_c_controller_service<DogController>(core::ControllerServiceMetadata{
      utils::Identifier{},
      "DogController",
      logging::LoggerFactory<DogController>::getLogger()});
  auto dog_with_jetpack_node = controller.plan->addController("dog_with_jetpack", dog_with_jetpack);
  CHECK(dog_with_jetpack->setProperty(DogController::HasJetpack.name, "true"));

  const auto duck = minifi::test::utils::make_custom_c_controller_service<DuckController>(core::ControllerServiceMetadata{utils::Identifier{},
      "DuckController",
      logging::LoggerFactory<DuckController>::getLogger()});
  auto duck_node = controller.plan->addController("duck", duck);

  {
    CHECK(controller.getProcessor()->setProperty(ZooProcessor::CanFlyService.name, "dog_with_jetpack"));
    CHECK(controller.getProcessor()->setProperty(ZooProcessor::NumberOfLegsService.name, "duck"));
    controller.trigger();
    CHECK(LogTestController::getInstance()
            .contains("[org::apache::nifi::minifi::api_testing::ZooProcessor] [critical] Can dog_with_jetpack fly? true"));
    CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::api_testing::ZooProcessor] [critical] duck has 2 legs"));
  }
  {
    LogTestController::getInstance().clear();
    CHECK(controller.getProcessor()->setProperty(ZooProcessor::CanFlyService.name, "duck"));
    CHECK(controller.getProcessor()->setProperty(ZooProcessor::NumberOfLegsService.name, "dog_with_jetpack"));
    controller.trigger();
    CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::api_testing::ZooProcessor] [critical] Can duck fly? true"));
    CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::api_testing::ZooProcessor] [critical] dog_with_jetpack has 4 legs"));
  }
  {
    LogTestController::getInstance().clear();
    CHECK(controller.getProcessor()->setProperty(ZooProcessor::CanFlyService.name, "duck"));
    CHECK(controller.getProcessor()->setProperty(ZooProcessor::NumberOfLegsService.name, "invalid"));
    controller.trigger();
    CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::api_testing::ZooProcessor] [critical] Can duck fly? true"));
  }
}

}  // namespace org::apache::nifi::minifi::api_testing::test
