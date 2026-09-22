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

"""Wrapper around an Apache Atlas container.

Each scenario provisions its own fresh `atlas-<scenario_id>` container from
`adamdebreceni/atlas:latest` (see extensions/atlas/tests/features/README.md for how the
image is built), publishing the REST API on a Docker-assigned (ephemeral) host port so
that features running in parallel don't collide on a fixed host port. `deploy()` starts
the container via the shared `LinuxContainer` base, resolves the host port Docker mapped,
then polls `/api/atlas/admin/status` until ACTIVE (up to `COLD_START_TIMEOUT_SECONDS`).
MiNiFi itself never uses the host port - it reaches Atlas over the scenario's Docker
network via `in_network_url` (`atlas-<scenario_id>:21000`); the host port is only used by
the host-side REST probes/assertions in this class. Teardown is handled by the generic
`common_after_scenario` -> `LinuxContainer.clean_up()` path, which removes the container
along with every other per-scenario container.
"""

from __future__ import annotations

import logging
import urllib.parse

import requests
from requests.auth import HTTPBasicAuth

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


class AtlasServerContainer(LinuxContainer):
    """Per-scenario Apache Atlas container. Publishes the REST API on a Docker-assigned host port."""

    IMAGE = "adamdebreceni/atlas:latest"
    REST_PORT = 21000
    USERNAME = "admin"
    PASSWORD = "admin"
    # Cold-start budget. Atlas boots HBase + Solr + Kafka + its own server before REST comes up,
    # so 5 min is a safe upper bound on modest hardware; runs that miss it should investigate.
    COLD_START_TIMEOUT_SECONDS = 300

    def __init__(self, test_context: MinifiTestContext):
        super().__init__(AtlasServerContainer.IMAGE,
                         f"atlas-{test_context.scenario_id}",
                         test_context.network)
        # Publish the REST API on a Docker-assigned host port (value None) so multiple Atlas
        # containers from parallel features can bind concurrently without colliding on a fixed
        # host port. The actual mapped port is resolved in deploy(). We intentionally don't
        # publish the embedded Kafka (9092) to the host: nothing host-side uses it (MiNiFi's
        # reporting task talks to Atlas over the Docker network via REST only), and a fixed
        # host publish would reintroduce the very collision we're removing.
        self.ports = {f"{AtlasServerContainer.REST_PORT}/tcp": None}
        # Resolved from the Docker-assigned host port once the container is running.
        self._base_url: str | None = None
        self._auth = HTTPBasicAuth(AtlasServerContainer.USERNAME, AtlasServerContainer.PASSWORD)

    # --- Lifecycle -------------------------------------------------------------------------------

    def deploy(self, context: MinifiTestContext | None) -> bool:
        if not super().deploy(context):
            return False
        # Resolve the host port Docker assigned before polling: _is_active() (the wait
        # condition below) issues host-side REST calls against self._base_url.
        self._base_url = f"http://localhost:{self._published_rest_port(context)}"
        return wait_for_condition(
            condition=self._is_active,
            timeout_seconds=AtlasServerContainer.COLD_START_TIMEOUT_SECONDS,
            bail_condition=lambda: self.exited,
            context=context)

    def _published_rest_port(self, context: MinifiTestContext | None) -> int:
        """Return the host port Docker assigned to the container's REST port (21000).

        Docker allocates the ephemeral host port at start time, but the binding can take a
        moment to surface in the container's attrs - notably when several containers start
        concurrently (parallel features), where the first reload usually returns an empty
        binding list. Poll (reloading each time) until it appears.
        """
        port: int | None = None

        def _resolved() -> bool:
            nonlocal port
            self.container.reload()
            bindings = self.container.ports.get(f"{AtlasServerContainer.REST_PORT}/tcp")
            if bindings:
                port = int(bindings[0]["HostPort"])
            return port is not None

        if not wait_for_condition(condition=_resolved, timeout_seconds=30,
                                  bail_condition=lambda: self.exited, context=context):
            raise RuntimeError(
                f"Atlas container '{self.container_name}' never published a host port for "
                f"{AtlasServerContainer.REST_PORT}/tcp; ports={self.container.ports}")
        return port

    # --- Addressing ------------------------------------------------------------------------------

    @property
    def alias(self) -> str:
        """Hostname MiNiFi (running on the scenario network) should use to reach Atlas."""
        return self.container_name

    @property
    def in_network_url(self) -> str:
        """Full URL for the `Atlas URLs` reporting-task property."""
        return f"http://{self.container_name}:{AtlasServerContainer.REST_PORT}"

    # --- Assertion helpers -----------------------------------------------------------------------

    def _is_active(self) -> bool:
        try:
            response = requests.get(f"{self._base_url}/api/atlas/admin/status", auth=self._auth, timeout=5)
            if response.status_code != 200:
                return False
            return response.json().get("Status") == "ACTIVE"
        except (requests.RequestException, ValueError):
            return False

    def search_entities(self, type_name: str, limit: int = 200) -> list[dict]:
        """Return the raw basic-search hits (each is a summary, NOT the full entity)."""
        try:
            response = requests.get(f"{self._base_url}/api/atlas/v2/search/basic",
                                    params={"typeName": type_name, "excludeDeletedEntities": "true", "limit": limit},
                                    auth=self._auth, timeout=10)
        except requests.RequestException as exc:
            logging.warning(f"Atlas basic-search request failed: {exc}")
            return []
        if response.status_code != 200:
            return []
        return response.json().get("entities", [])

    def find_entity_by_qn(self, type_name: str, qualified_name: str) -> dict | None:
        """Return the first entity of `type_name` whose qualifiedName matches, or None."""
        for entity in self.search_entities(type_name):
            if entity.get("attributes", {}).get("qualifiedName") == qualified_name:
                return self.get_entity(entity["guid"])
        return None

    def get_entity(self, guid: str) -> dict | None:
        try:
            response = requests.get(f"{self._base_url}/api/atlas/v2/entity/guid/{urllib.parse.quote(guid, safe='')}",
                                    auth=self._auth, timeout=10)
        except requests.RequestException as exc:
            logging.warning(f"Atlas get-entity request failed: {exc}")
            return None
        if response.status_code != 200:
            return None
        return response.json().get("entity")

    def wait_for_entity(self, type_name: str, qualified_name: str, timeout_seconds: float,
                        context: MinifiTestContext | None = None) -> dict | None:
        """Poll find_entity_by_qn until it returns non-None or the timeout elapses.

        Returns the entity on success, None on timeout. Passing `context` lets the underlying
        wait_for_condition dump every container's stdout on failure - critical for diagnosing
        why Atlas didn't see an expected entity (it's almost always a MiNiFi-side issue we can
        only see in the minifi container's logs, not in Atlas' REST responses).
        """
        found: dict | None = None

        def _found() -> bool:
            nonlocal found
            found = self.find_entity_by_qn(type_name, qualified_name)
            return found is not None

        wait_for_condition(condition=_found, timeout_seconds=timeout_seconds,
                           bail_condition=lambda: False, context=context)
        return found
