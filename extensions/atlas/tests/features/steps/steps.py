# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import importlib.util
import sys
from pathlib import Path

from behave import given, then  # noqa: F401 - `step` is unused here but the shared step modules use it

# Register shared step modules. Same convention every extension uses.
from minifi_behave.steps import checking_steps        # noqa: F401
from minifi_behave.steps import configuration_steps   # noqa: F401
from minifi_behave.steps import core_steps            # noqa: F401
from minifi_behave.steps import flow_building_steps   # noqa: F401
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext

from containers.atlas_server_container import AtlasServerContainer


def _load_module_from_path(name: str, path: Path):
    """Load a Python file directly by path so we don't have to fight behave's implicit
    `sys.path` layout (which only adds the current feature's dir, not siblings')."""
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"cannot load {name} from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


# The kafka feature (kafka-topic-extractor.feature) uses steps + a container class defined under
# extensions/kafka/tests/features. Behave only auto-loads the current feature dir, so we import
# them here by absolute path. The `containers.kafka_server_container` alias mirrors what
# extensions/kafka/tests/features/steps/steps.py itself imports - registering under that alias
# lets that module's `from containers.kafka_server_container import KafkaServer` resolve.
_KAFKA_FEATURES = Path(__file__).resolve().parent.parent.parent.parent.parent / "kafka" / "tests" / "features"
if _KAFKA_FEATURES.is_dir():
    _load_module_from_path("containers.kafka_server_container",
                           _KAFKA_FEATURES / "containers" / "kafka_server_container.py")
    _load_module_from_path("_kafka_steps",  # side-effect: registers @step decorators
                           _KAFKA_FEATURES / "steps" / "steps.py")


ATLAS_CONTAINER_KEY = "atlas"


def _atlas(context: MinifiTestContext) -> AtlasServerContainer:
    container = context.containers.get(ATLAS_CONTAINER_KEY)
    if not isinstance(container, AtlasServerContainer):
        raise AssertionError("Atlas server has not been set up in this scenario; missing 'Given an Atlas server is available'.")
    return container


# --- Setup ----------------------------------------------------------------------------------------


@given("an Atlas server is available")
def atlas_server_is_available(context: MinifiTestContext):
    atlas = AtlasServerContainer(context)
    # Register the container before deploy so that if deploy fails/times out, common_after_scenario's
    # generic log_due_to_failure loop still finds it and dumps its logs.
    context.containers[ATLAS_CONTAINER_KEY] = atlas
    assert atlas.deploy(context) or atlas.log_app_output(), "Atlas server is not reachable - see extensions/atlas/tests/features/README.md."


@given("the \"{reporting_task_name}\" reporting task is configured for the Atlas server")
def configure_atlas_reporting_task(context: MinifiTestContext, reporting_task_name: str):
    """Sugar step that fills in the Atlas RT properties for the current scenario.

    Uses `context.scenario_id` as the metadata namespace so every scenario ends up with its own
    slice of the Atlas graph (`@<scenario_id>` suffix on every qualifiedName)."""
    atlas = _atlas(context)
    flow_definition = context.get_or_create_default_minifi_container().flow_definition
    reporting_task = flow_definition.get_reporting_task(reporting_task_name)
    if reporting_task is None:
        raise AssertionError(f"ReportingTask '{reporting_task_name}' not found in the MiNiFi flow.")
    reporting_task.add_property("Atlas URLs", atlas.in_network_url)
    reporting_task.add_property("Atlas Username", AtlasServerContainer.USERNAME)
    reporting_task.add_property("Atlas Password", AtlasServerContainer.PASSWORD)
    reporting_task.add_property("NiFi URL for Atlas", f"http://minifi-{context.scenario_id}:8080/nifi")
    reporting_task.add_property("Default Metadata Namespace", context.scenario_id)
    reporting_task.add_property("Filesystem Path Level", "FILE")
    reporting_task.add_property("AWS S3 Model Version", "v2")


# --- Assertions -----------------------------------------------------------------------------------


@then("a \"{type_name}\" entity with qualified name \"{qualified_name}\" exists in Atlas within {timeout_seconds:d} seconds")
def entity_exists(context: MinifiTestContext, type_name: str, qualified_name: str, timeout_seconds: int):
    atlas = _atlas(context)
    entity = atlas.wait_for_entity(type_name, qualified_name, float(timeout_seconds), context=context)
    assert entity is not None, f"No {type_name} with qualifiedName '{qualified_name}' appeared within {timeout_seconds}s."


@then("a \"{type_name}\" entity with qualified name ending in \"{qn_suffix}\" exists in Atlas within {timeout_seconds:d} seconds")
def entity_with_qn_suffix_exists(context: MinifiTestContext, type_name: str, qn_suffix: str, timeout_seconds: int):
    """Existence check when the full qualifiedName isn't predictable (e.g. contains an auto-generated UUID)."""
    atlas = _atlas(context)

    def _found() -> bool:
        for entity in atlas.search_entities(type_name):
            if entity.get("attributes", {}).get("qualifiedName", "").endswith(qn_suffix):
                return True
        return False

    ok = wait_for_condition(condition=_found, timeout_seconds=float(timeout_seconds),
                            bail_condition=lambda: False, context=context)
    assert ok, f"No {type_name} with qualifiedName ending in '{qn_suffix}' appeared within {timeout_seconds}s."


@then("the \"{type_name}\" entity with qualified name \"{qualified_name}\" has attribute \"{attr_name}\" equal to \"{attr_value}\"")
def entity_attribute_equals(context: MinifiTestContext, type_name: str, qualified_name: str, attr_name: str, attr_value: str):
    atlas = _atlas(context)
    entity = atlas.find_entity_by_qn(type_name, qualified_name)
    assert entity is not None, f"No {type_name} with qualifiedName '{qualified_name}' present in Atlas."
    actual = entity.get("attributes", {}).get(attr_name)
    assert str(actual) == attr_value, f"Attribute '{attr_name}' on {type_name}('{qualified_name}'): expected '{attr_value}', got '{actual}'."


def _entity_ref_list_contains(refs, ref_type: str, ref_qn: str) -> bool:
    for ref in refs or []:
        if ref.get("typeName") != ref_type:
            continue
        if ref.get("uniqueAttributes", {}).get("qualifiedName") == ref_qn:
            return True
    return False


@then("the \"{type_name}\" entity with qualified name \"{qualified_name}\" has an input of type \"{ref_type}\" with qualified name \"{ref_qn}\"")
def entity_has_input(context: MinifiTestContext, type_name: str, qualified_name: str, ref_type: str, ref_qn: str):
    atlas = _atlas(context)
    entity = atlas.find_entity_by_qn(type_name, qualified_name)
    assert entity is not None, f"No {type_name} with qualifiedName '{qualified_name}' present in Atlas."
    inputs = entity.get("attributes", {}).get("inputs")
    assert _entity_ref_list_contains(inputs, ref_type, ref_qn), (
        f"{type_name}('{qualified_name}').inputs does not contain a {ref_type}('{ref_qn}'). Actual: {inputs}")


@then("the \"{type_name}\" entity with qualified name \"{qualified_name}\" has an output of type \"{ref_type}\" with qualified name \"{ref_qn}\"")
def entity_has_output(context: MinifiTestContext, type_name: str, qualified_name: str, ref_type: str, ref_qn: str):
    atlas = _atlas(context)
    entity = atlas.find_entity_by_qn(type_name, qualified_name)
    assert entity is not None, f"No {type_name} with qualifiedName '{qualified_name}' present in Atlas."
    outputs = entity.get("attributes", {}).get("outputs")
    assert _entity_ref_list_contains(outputs, ref_type, ref_qn), (
        f"{type_name}('{qualified_name}').outputs does not contain a {ref_type}('{ref_qn}'). Actual: {outputs}")
