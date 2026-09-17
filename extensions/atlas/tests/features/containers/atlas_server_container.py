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
image is built), publishing REST on host port 21000 and Kafka on 9092. `deploy()` starts
the container via the shared `LinuxContainer` base and polls `/api/atlas/admin/status`
until ACTIVE (up to `COLD_START_TIMEOUT_SECONDS`). Teardown is handled by the generic
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
    """Per-scenario Apache Atlas container. Publishes REST on host port 21000 and Kafka on 9092."""

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
        # Publish REST + embedded Kafka on the loopback interface so host-side probes and the
        # framework's assertion helpers can hit them directly.
        self.ports = {f"{AtlasServerContainer.REST_PORT}/tcp": AtlasServerContainer.REST_PORT,
                      "9092/tcp": 9092}
        self._base_url = f"http://localhost:{AtlasServerContainer.REST_PORT}"
        self._auth = HTTPBasicAuth(AtlasServerContainer.USERNAME, AtlasServerContainer.PASSWORD)

    # --- Lifecycle -------------------------------------------------------------------------------

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        return wait_for_condition(
            condition=self._is_active,
            timeout_seconds=AtlasServerContainer.COLD_START_TIMEOUT_SECONDS,
            bail_condition=lambda: self.exited,
            context=context)

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
