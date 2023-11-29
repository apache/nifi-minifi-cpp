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

#define CUSTOM_EXTENSION_INIT

#undef NDEBUG
#include <fstream>

#include "TestBase.h"
#include "Catch.h"
#include "flow-tests/TestControllerWithFlow.h"
#include "EmptyFlow.h"
#include "c2/C2MetricsPublisher.h"
#include "utils/gsl.h"

using minifi::state::response::SerializedResponseNode;

template<typename F>
const SerializedResponseNode* findNode(const std::vector<SerializedResponseNode>& nodes, F&& filter) {
  for (auto& node : nodes) {
    if (filter(node)) return &node;
  }
  return nullptr;
}

const SerializedResponseNode& getNode(const std::vector<SerializedResponseNode>& nodes, std::string_view name) {
  for (auto& node : nodes) {
    if (node.name == name) return node;
  }
  gsl_FailFast();
}

TEST_CASE("Python processor's description is part of the manifest") {
  TestControllerWithFlow controller(empty_flow, false /* DEFER FLOW SETUP */);

  auto python_dir = controller.configuration_->getHome() / "minifi-python";
  utils::file::create_dir(python_dir);
  std::ofstream{python_dir / "MyPyProc.py"} <<
    "def describe(proc):\n"
    "  proc.setDescription('An amazing processor')\n";

  std::ofstream{python_dir / "MyPyProc2.py"} <<
    "def describe(proc):\n"
    "  proc.setDescription('Another amazing processor')\n"
    "  proc.setSupportsDynamicProperties()\n"
    "  proc.addProperty('Prop1', 'A great property', 'banana', True, False)\n";

  controller.configuration_->set(minifi::Configuration::nifi_python_processor_dir, python_dir.string());
  controller.configuration_->set(minifi::Configuration::nifi_extension_path, "*minifi-python-script*,*minifi-http-curl*");

  core::extension::ExtensionManager::get().initialize(controller.configuration_);

  controller.setupFlow();

  auto c2_metrics_publisher = std::static_pointer_cast<minifi::c2::C2MetricsPublisher>(controller.metrics_publisher_store_->getMetricsPublisher(minifi::c2::C2_METRICS_PUBLISHER).lock());

  auto agent_info = c2_metrics_publisher->getAgentManifest();

  auto& manifest = getNode(agent_info.serialized_nodes, "agentManifest");

  auto findPythonProcessor = [&] (const std::string& name) {
    // each python file gets its own bundle
    auto* python_bundle = findNode(manifest.children, [&] (auto& child) {
      return child.name == "bundles" && findNode(child.children, [&] (auto& bundle_child) {
        return bundle_child.name == "artifact" && bundle_child.value == name + ".py";
      });
    });
    REQUIRE(python_bundle);

    auto& py_processors = getNode(getNode(python_bundle->children, "componentManifest").children, "processors");

    // single processor in each bundle
    REQUIRE(py_processors.children.size() == 1);

    return findNode(py_processors.children, [&] (auto& child) {return utils::string::endsWith(child.name, name);});
  };

  {
    auto* MyPyProc = findPythonProcessor("MyPyProc");
    REQUIRE(MyPyProc);

    REQUIRE(getNode(MyPyProc->children, "inputRequirement").value == "INPUT_ALLOWED");
    REQUIRE(getNode(MyPyProc->children, "isSingleThreaded").value == true);
    REQUIRE(getNode(MyPyProc->children, "typeDescription").value == "An amazing processor");
    REQUIRE(getNode(MyPyProc->children, "supportsDynamicRelationships").value == false);
    REQUIRE(getNode(MyPyProc->children, "supportsDynamicProperties").value == false);
    REQUIRE(getNode(MyPyProc->children, "type").value == "org.apache.nifi.minifi.processors.MyPyProc");

    auto& rels = getNode(MyPyProc->children, "supportedRelationships").children;
    REQUIRE(rels.size() == 3);

    auto* success = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "success";});
    REQUIRE(success);
    REQUIRE(getNode(success->children, "description").value == "Script succeeds");

    auto* failure = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "failure";});
    REQUIRE(failure);
    REQUIRE(getNode(failure->children, "description").value == "Script fails");
  }


  {
    auto* MyPyProc2 = findPythonProcessor("MyPyProc2");
    REQUIRE(MyPyProc2);

    REQUIRE(getNode(MyPyProc2->children, "inputRequirement").value == "INPUT_ALLOWED");
    REQUIRE(getNode(MyPyProc2->children, "isSingleThreaded").value == true);
    REQUIRE(getNode(MyPyProc2->children, "typeDescription").value == "Another amazing processor");
    REQUIRE(getNode(MyPyProc2->children, "supportsDynamicRelationships").value == false);
    REQUIRE(getNode(MyPyProc2->children, "supportsDynamicProperties").value == true);
    REQUIRE(getNode(MyPyProc2->children, "type").value == "org.apache.nifi.minifi.processors.MyPyProc2");

    auto& properties = getNode(MyPyProc2->children, "propertyDescriptors").children;
    REQUIRE(properties.size() == 1);
    REQUIRE(properties[0].name == "Prop1");
    REQUIRE(getNode(properties[0].children, "name").value == "Prop1");
    REQUIRE(getNode(properties[0].children, "required").value == true);
    REQUIRE(getNode(properties[0].children, "expressionLanguageScope").value == "NONE");
    REQUIRE(getNode(properties[0].children, "defaultValue").value == "banana");

    auto& rels = getNode(MyPyProc2->children, "supportedRelationships").children;
    REQUIRE(rels.size() == 3);

    auto* success = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "success";});
    REQUIRE(success);
    REQUIRE(getNode(success->children, "description").value == "Script succeeds");

    auto* failure = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "failure";});
    REQUIRE(failure);
    REQUIRE(getNode(failure->children, "description").value == "Script fails");
  }
}
