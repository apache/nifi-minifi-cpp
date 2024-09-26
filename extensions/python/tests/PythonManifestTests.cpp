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

#include <fstream>

#include "agent/agent_version.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestControllerWithFlow.h"
#include "unit/EmptyFlow.h"
#include "unit/TestUtils.h"
#include "c2/C2MetricsPublisher.h"
#include "utils/gsl.h"

using minifi::state::response::SerializedResponseNode;
using minifi::state::response::ValueNode;

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
  FAIL(fmt::format("Node {} was not found", name));
  gsl_FailFast();
}

ValueNode getNthAllowableValue(const SerializedResponseNode& node, size_t n) {
  return getNode(node.children[n].children, "value").value;
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
    "  proc.addProperty('Prop1', 'A great property', 'banana', True, False, False, None, ['apple', 'orange', 'banana', 'durian'], None)\n";

  const auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
#ifdef WIN32
  std::filesystem::create_symlink(executable_dir / "minifi-python-script-extension.dll", python_dir / "minifi_native.pyd");
#endif
  std::filesystem::copy(executable_dir / "resources" / "minifi-python" / "nifiapi", python_dir / "nifiapi", std::filesystem::copy_options::recursive);
  utils::file::create_dir(python_dir / "nifi_python_processors");
  std::ofstream{python_dir / "nifi_python_processors" / "MyPyProc3.py"} << R"(
from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult
from nifiapi.properties import ExpressionLanguageScope, PropertyDescriptor, StandardValidators

class MyPyProc3(FlowFileTransform):

    class Java:
        implements = ['org.apache.nifi.python.processor.FlowFileTransform']

    class ProcessorDetails:
        version = '1.2.3'
        description = "Test processor number three."
        dependencies = []

    COLOR = PropertyDescriptor(
        name="Color",
        description="Symbolic name for the combination of frequencies of electromagnetic radiation reflected by the processor.",
        allowable_values=['red', 'blue', 'green', 'purple'],
        default_value='red',
        required=True,
        expression_language_scope=ExpressionLanguageScope.NONE
    )

    MOOD = PropertyDescriptor(
        name="Mood",
        description="The mental or emotional state of the processor.",
        required=False,
        validators=[StandardValidators.NON_EMPTY_VALIDATOR],
        expression_language_scope=ExpressionLanguageScope.FLOWFILE_ATTRIBUTES
    )

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return [self.COLOR, self.MOOD]

    def transform(self, context, flow_file):
        color = context.getProperty(self.COLOR).getValue()
        mood = context.getProperty(self.MOOD).evaluateAttributeExpressions(flowfile).getValue() or "OK"
        user = flow_file.getContentsAsBytes().decode('utf-8')
        response = f"Hello {user}! I am a {color} processor. I am feeling {mood}."
        return FlowFileTransformResult('success', contents=response.encode('utf-8'))
)";

  std::ofstream{python_dir / "nifi_python_processors" / "MyPyProc4.py"} << R"(
from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult

class MyPyProc4(FlowFileTransform):

    class Java:
        implements = ['org.apache.nifi.python.processor.FlowFileTransform']

    class ProcessorDetails:
        description = "Test processor number four. Does not define a version."
        dependencies = []

    def __init__(self, **kwargs):
        pass

    def transform(self, context, flow_file):
        return FlowFileTransformResult('success')
)";

  std::ofstream{python_dir / "nifi_python_processors" / "MyPyProc5.py"} << R"(
from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult

class MyPyProc5(FlowFileTransform):

    class Java:
        implements = ['org.apache.nifi.python.processor.FlowFileTransform']

    class ProcessorDetails:
        version = ''
        description = "Test processor number five. Defines a version, but it is blank."
        dependencies = []

    def __init__(self, **kwargs):
        pass

    def transform(self, context, flow_file):
        return FlowFileTransformResult('success')
)";

  controller.configuration_->set(minifi::Configuration::nifi_python_processor_dir, python_dir.string());
  controller.configuration_->set(minifi::Configuration::nifi_extension_path, "*minifi-python-script*");

  core::extension::ExtensionManagerImpl::get().initialize(controller.configuration_);

  controller.setupFlow();

  auto c2_metrics_publisher = std::dynamic_pointer_cast<minifi::c2::C2MetricsPublisher>(controller.metrics_publisher_store_->getMetricsPublisher(minifi::c2::C2_METRICS_PUBLISHER).lock());

  auto agent_info = c2_metrics_publisher->getAgentManifest();

  auto& manifest = getNode(agent_info.serialized_nodes, "agentManifest");

  auto findPythonBundle = [&](const std::string& name) {
    // each python file gets its own bundle
    auto* python_bundle = findNode(manifest.children, [&](const auto& child) {
      return child.name == "bundles" && findNode(child.children, [&](const auto& bundle_child) {
        return bundle_child.name == "artifact" && bundle_child.value == name + ".py";
      });
    });
    REQUIRE(python_bundle);
    return gsl::make_not_null(python_bundle);
  };

  auto getProcessorNode = [&] (gsl::not_null<const SerializedResponseNode*> bundle) {
    auto& processors = getNode(getNode(bundle->children, "componentManifest").children, "processors");
    REQUIRE(processors.children.size() == 1);
    auto& only_child = processors.children[0];
    return gsl::make_not_null(&only_child);
  };

  {
    auto python_bundle = findPythonBundle("MyPyProc");
    auto MyPyProc = getProcessorNode(python_bundle);

    CHECK(getNode(MyPyProc->children, "inputRequirement").value == "INPUT_ALLOWED");
    CHECK(getNode(MyPyProc->children, "isSingleThreaded").value == true);
    CHECK(getNode(MyPyProc->children, "typeDescription").value == "An amazing processor");
    CHECK(getNode(MyPyProc->children, "supportsDynamicRelationships").value == false);
    CHECK(getNode(MyPyProc->children, "supportsDynamicProperties").value == false);
    CHECK(getNode(MyPyProc->children, "type").value == "org.apache.nifi.minifi.processors.MyPyProc");

    auto& rels = getNode(MyPyProc->children, "supportedRelationships").children;
    REQUIRE(rels.size() == 3);

    auto* success = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "success";});
    REQUIRE(success);
    CHECK(getNode(success->children, "description").value == "Script succeeds");

    auto* failure = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "failure";});
    REQUIRE(failure);
    REQUIRE(getNode(failure->children, "description").value == "Script fails");
  }

  {
    auto python_bundle = findPythonBundle("MyPyProc2");
    auto MyPyProc2 = getProcessorNode(python_bundle);

    CHECK(getNode(MyPyProc2->children, "inputRequirement").value == "INPUT_ALLOWED");
    CHECK(getNode(MyPyProc2->children, "isSingleThreaded").value == true);
    CHECK(getNode(MyPyProc2->children, "typeDescription").value == "Another amazing processor");
    CHECK(getNode(MyPyProc2->children, "supportsDynamicRelationships").value == false);
    CHECK(getNode(MyPyProc2->children, "supportsDynamicProperties").value == true);
    CHECK(getNode(MyPyProc2->children, "type").value == "org.apache.nifi.minifi.processors.MyPyProc2");

    auto& properties = getNode(MyPyProc2->children, "propertyDescriptors").children;
    REQUIRE(properties.size() == 1);
    CHECK(properties[0].name == "Prop1");
    CHECK(getNode(properties[0].children, "name").value == "Prop1");
    CHECK(getNode(properties[0].children, "required").value == true);
    CHECK(getNode(properties[0].children, "expressionLanguageScope").value == "NONE");
    CHECK(getNode(properties[0].children, "defaultValue").value == "banana");
    auto& allowable_values = getNode(properties[0].children, "allowableValues");
    REQUIRE(allowable_values.children.size() == 4);
    CHECK(getNthAllowableValue(allowable_values, 0) == "apple");
    CHECK(getNthAllowableValue(allowable_values, 1) == "orange");
    CHECK(getNthAllowableValue(allowable_values, 2) == "banana");
    CHECK(getNthAllowableValue(allowable_values, 3) == "durian");

    auto& rels = getNode(MyPyProc2->children, "supportedRelationships").children;
    REQUIRE(rels.size() == 3);

    auto* success = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "success";});
    REQUIRE(success);
    CHECK(getNode(success->children, "description").value == "Script succeeds");

    auto* failure = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "failure";});
    REQUIRE(failure);
    CHECK(getNode(failure->children, "description").value == "Script fails");
  }

  {
    auto python_bundle = findPythonBundle("MyPyProc3");
    auto MyPyProc3 = getProcessorNode(python_bundle);

    CHECK(getNode(python_bundle->children, "version").value == "1.2.3");

    CHECK(getNode(MyPyProc3->children, "inputRequirement").value == "INPUT_ALLOWED");
    CHECK(getNode(MyPyProc3->children, "isSingleThreaded").value == true);
    CHECK(getNode(MyPyProc3->children, "typeDescription").value == "Test processor number three.");
    CHECK(getNode(MyPyProc3->children, "supportsDynamicRelationships").value == false);
    CHECK(getNode(MyPyProc3->children, "supportsDynamicProperties").value == true);
    CHECK(getNode(MyPyProc3->children, "type").value == "org.apache.nifi.minifi.processors.nifi_python_processors.MyPyProc3");

    auto& properties = getNode(MyPyProc3->children, "propertyDescriptors").children;
    REQUIRE(properties.size() == 2);
    CHECK(properties[0].name == "Color");
    CHECK(getNode(properties[0].children, "name").value == "Color");
    CHECK(getNode(properties[0].children, "required").value == true);
    CHECK(getNode(properties[0].children, "expressionLanguageScope").value == "NONE");
    CHECK(getNode(properties[0].children, "defaultValue").value == "red");
    CHECK(properties[1].name == "Mood");
    CHECK(getNode(properties[1].children, "name").value == "Mood");
    CHECK(getNode(properties[1].children, "required").value == false);
    CHECK(getNode(properties[1].children, "expressionLanguageScope").value == "FLOWFILE_ATTRIBUTES");

    auto& rels = getNode(MyPyProc3->children, "supportedRelationships").children;
    REQUIRE(rels.size() == 3);

    auto* success = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "success";});
    REQUIRE(success);
    CHECK(getNode(success->children, "description").value == "Script succeeds");

    auto* failure = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "failure";});
    REQUIRE(failure);
    CHECK(getNode(failure->children, "description").value == "Script fails");

    auto* original = findNode(rels, [] (auto& rel) {return getNode(rel.children, "name").value == "original";});
    REQUIRE(original);
    CHECK(getNode(original->children, "description").value == "Original flow file");
  }

  {
    auto python_bundle = findPythonBundle("MyPyProc4");
    CHECK(getNode(python_bundle->children, "version").value == minifi::AgentBuild::VERSION);
  }

  {
    auto python_bundle = findPythonBundle("MyPyProc5");
    CHECK(getNode(python_bundle->children, "version").value == minifi::AgentBuild::VERSION);
  }
}
