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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/controller/ControllerServiceBase.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ControllerServiceNode.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/ProcessorImpl.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/Enum.h"
#include "utils/Id.h"
#include "unit/TestUtils.h"
#include "core/ProcessContextImpl.h"
#include "unit/ControllerServiceUtils.h"

namespace org::apache::nifi::minifi::core {
namespace {

class TestProcessor : public ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  void initialize() override {
    on_initialize_(*this);
  }

  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  std::function<void(ProcessorImpl&)> on_initialize_;
};

enum class TestEnum {
  A,
  B
};
}  // namespace

TEST_CASE("Parse enum property") {
  static constexpr auto prop = PropertyDefinitionBuilder<magic_enum::enum_count<TestEnum>()>::createProperty("prop")
      .withAllowedValues(magic_enum::enum_names<TestEnum>())
      .build();
  auto proc = minifi::test::utils::make_processor<TestProcessor>("test-proc");
  proc->getImpl<TestProcessor>().on_initialize_ = [&] (auto& self) {
    self.setSupportedProperties(std::to_array<core::PropertyReference>({prop}));
  };
  proc->initialize();
  core::ProcessContextImpl context(*proc, nullptr, nullptr, nullptr, nullptr, nullptr);
  SECTION("Valid") {
    REQUIRE(proc->setProperty(prop.name, "B"));
    const auto val = utils::parseEnumProperty<TestEnum>(context, prop);
    REQUIRE(val == TestEnum::B);
  }
  SECTION("Invalid") {
    CHECK_FALSE(proc->setProperty(prop.name, "C"));  // Cant set it anymore
  }
  SECTION("Missing") {
    REQUIRE_THROWS(utils::parseEnumProperty<TestEnum>(context, prop));
  }
  SECTION("Optional enum property valid") {
    REQUIRE(proc->setProperty(prop.name, "B"));
    const auto val = utils::parseOptionalEnumProperty<TestEnum>(context, prop);
    REQUIRE(*val == TestEnum::B);
  }
  SECTION("Optional enum property invalid") {
    CHECK_FALSE(proc->setProperty(prop.name, "C"));  // Cant set it anymore
  }
  SECTION("Optional enum property missing") {
    const auto val = utils::parseOptionalEnumProperty<TestEnum>(context, prop);
    REQUIRE(val == std::nullopt);
  }
}

namespace {
class TestControllerService : public controller::ControllerServiceBase {
 public:
  using ControllerServiceBase::ControllerServiceBase;
};

const std::shared_ptr test_controller_service = []() {
  auto service = minifi::test::utils::make_controller_service<TestControllerService>("test-controller-service", utils::IdGenerator::getIdGenerator()->generate());
  service->initialize();
  service->onEnable();
  service->setState(minifi::core::controller::ControllerServiceState::ENABLED);
  return service;
}();

class WrongTestControllerService : public TestControllerService {};

class TestControllerServiceProvider : public controller::ControllerServiceProvider {
 public:
  using ControllerServiceProvider::ControllerServiceProvider;
  std::shared_ptr<controller::ControllerServiceNode> createControllerService(const std::string&, const std::string&) override { return nullptr; }
  void clearControllerServices() override {}
  void enableAllControllerServices() override {}
  void disableAllControllerServices() override {}

  std::shared_ptr<controller::ControllerService> getControllerService(const std::string& name, const utils::Identifier&) const override {
    if (name == "TestControllerService") { return test_controller_service; }
    return nullptr;
  }
};

TestControllerServiceProvider test_controller_service_provider("test-provider");
}  // namespace

TEST_CASE("Parse controller service property") {
  static constexpr auto property = PropertyDefinitionBuilder<>::createProperty("Controller Service")
      .withAllowedTypes<TestControllerService>()
      .build();
  auto processor = minifi::test::utils::make_processor<TestProcessor>("test-processor");
  processor->getImpl<TestProcessor>().on_initialize_ = [&] (auto& self) {
    self.setSupportedProperties(std::to_array<core::PropertyReference>({property}));
  };
  processor->initialize();
  auto configuration = minifi::Configure::create();
  core::ProcessContextImpl context(*processor, &test_controller_service_provider, nullptr, nullptr, configuration, nullptr);

  SECTION("Required controller service property") {
    SECTION("... is valid") {
      REQUIRE(processor->setProperty(property.name, "TestControllerService"));
      const auto value = utils::parseControllerService<TestControllerService>(context, property, processor->getUUID());
      CHECK(value.get() == test_controller_service->getImplementation<TestControllerService>());
    }
    SECTION("... is missing") {
      CHECK_THROWS(utils::parseControllerService<TestControllerService>(context, property, processor->getUUID()));
    }
    SECTION("... is blank") {
      REQUIRE(processor->setProperty(property.name, ""));
      CHECK_THROWS(utils::parseControllerService<TestControllerService>(context, property, processor->getUUID()));
    }
    SECTION("... is invalid") {
      REQUIRE(processor->setProperty(property.name, "NonExistentControllerService"));
      CHECK_THROWS(utils::parseControllerService<TestControllerService>(context, property, processor->getUUID()));
    }
    SECTION("... is not the correct class") {
      REQUIRE(processor->setProperty(property.name, "TestControllerService"));
      CHECK_THROWS(utils::parseControllerService<WrongTestControllerService>(context, property, processor->getUUID()));
    }
  }

  SECTION("Optional controller service property") {
    SECTION("... is valid") {
      REQUIRE(processor->setProperty(property.name, "TestControllerService"));
      const auto value = utils::parseOptionalControllerService<TestControllerService>(context, property, processor->getUUID());;
      CHECK(value.get() == test_controller_service->getImplementation<TestControllerService>());
    }
    SECTION("... is missing") {
      const auto value = utils::parseOptionalControllerService<TestControllerService>(context, property, processor->getUUID());;
      CHECK(value == nullptr);
    }
    SECTION("... is blank") {
      REQUIRE(processor->setProperty(property.name, ""));
      const auto value = utils::parseOptionalControllerService<TestControllerService>(context, property, processor->getUUID());;
      CHECK(value == nullptr);
    }
    SECTION("... is invalid") {
      REQUIRE(processor->setProperty(property.name, "NonExistentControllerService"));
      CHECK_THROWS(utils::parseOptionalControllerService<TestControllerService>(context, property, processor->getUUID()));
    }
    SECTION("... is not the correct class") {
      REQUIRE(processor->setProperty(property.name, "TestControllerService"));
      CHECK_THROWS(utils::parseOptionalControllerService<WrongTestControllerService>(context, property, processor->getUUID()));
    }
  }
}
}  // namespace org::apache::nifi::minifi::core
