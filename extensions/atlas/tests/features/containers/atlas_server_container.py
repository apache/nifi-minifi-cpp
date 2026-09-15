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

We self-provision a single Atlas container from the local `atlas:latest` image
(see extensions/atlas/tests/features/README.md for how the image is built).
Because Atlas' 2-3 min cold start would dominate every scenario's runtime, the
policy is:

  - If a container named `atlas` already exists AND its REST endpoint returns
    ACTIVE within a few seconds, we reuse it (the common case across scenarios
    in the same run).
  - Otherwise we (re)create it from `atlas:latest`, wait for ACTIVE, and use it.

Set `ATLAS_FORCE_RECREATE=1` to always recreate. Useful when the previous run
left Atlas in a wedged state (the local image is known to lock up its bulk-entity
endpoint after some load; a fresh container clears the corruption).

Scenarios are isolated on Atlas' side via a per-namespace convention: namespace
= scenario_id, appended to every qualifiedName as `@<scenario_id>`. Multiple
scenarios reusing the same Atlas container never collide because they write
under different namespaces.
"""

from __future__ import annotations

import logging
import os
import urllib.parse

import docker
import requests
from requests.auth import HTTPBasicAuth

from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


class AtlasServerContainer:
    """Self-provisions (or reuses a healthy) `atlas` container from `atlas:latest`.

    - `deploy()` ensures the container is running and healthy, then attaches it to
      the per-scenario docker network with alias `atlas-<scenario_id>`.
    - `clean_up()` only disconnects from the scenario network - the container keeps
      running so the next scenario can skip the 2-3 min cold start.
    - Assertion helpers hit the Atlas REST v2 API over the host port `21000`.
    """

    HOST_CONTAINER_NAME = "atlas"
    IMAGE = "atlas:latest"
    REST_PORT = 21000
    USERNAME = "admin"
    PASSWORD = "admin"
    # Cold-start budget. Atlas boots HBase + Solr + Kafka + its own server before REST comes up,
    # so 5 min is a safe upper bound on modest hardware; runs that miss it should investigate.
    COLD_START_TIMEOUT_SECONDS = 300
    # A running container should be answering the status endpoint in well under a second. If it
    # doesn't respond within a few seconds, treat it as wedged and recreate rather than wait out
    # the 5-min cold-start budget.
    HEALTHY_REUSE_TIMEOUT_SECONDS = 5

    def __init__(self, test_context: MinifiTestContext):
        self._test_context = test_context
        self._scenario_id = test_context.scenario_id
        self._alias = f"atlas-{self._scenario_id}"
        self._network = test_context.network
        self._client = docker.from_env()
        self._base_url = f"http://localhost:{AtlasServerContainer.REST_PORT}"
        self._auth = HTTPBasicAuth(AtlasServerContainer.USERNAME, AtlasServerContainer.PASSWORD)
        self._connected = False
        # _container is resolved during deploy() so a failure to reach the image doesn't fire
        # from inside the step-registration import chain.
        self._container = None

    # --- Lifecycle -------------------------------------------------------------------------------

    def deploy(self, context: MinifiTestContext | None) -> bool:
        self._container = self._ensure_healthy_container(context)
        if self._container is None:
            return False
        # Connect to the scenario network under the alias `atlas-<scenario_id>`.
        # Guard against a stale connection carried over from a prior run: docker keeps the
        # container-name endpoint registered against the container, so if the previous scenario's
        # after_scenario didn't disconnect (e.g. because the run was killed), the second attach
        # fails with "endpoint with name atlas already exists". Refresh the container state and
        # force-disconnect if we spot the network in its adjacency list, then connect fresh.
        try:
            self._container.reload()
            if self._network.name in self._container.attrs.get("NetworkSettings", {}).get("Networks", {}):
                logging.warning(f"'{AtlasServerContainer.HOST_CONTAINER_NAME}' is already attached to '{self._network.name}'; force-disconnecting before reattach.")
                self._network.disconnect(self._container, force=True)
        except docker.errors.APIError as exc:
            logging.warning(f"Pre-connect cleanup for '{AtlasServerContainer.HOST_CONTAINER_NAME}' on '{self._network.name}' failed (continuing): {exc}")

        try:
            self._network.connect(self._container, aliases=[self._alias])
            self._connected = True
        except docker.errors.APIError as exc:
            logging.error(f"Failed to attach '{AtlasServerContainer.HOST_CONTAINER_NAME}' to network '{self._network.name}': {exc}")
            return False
        return True

    def _ensure_healthy_container(self, context: MinifiTestContext | None):
        """Return a running, ACTIVE-reporting `atlas` container.

        Reuse policy: if a container by that name exists and its /admin/status returns
        ACTIVE within HEALTHY_REUSE_TIMEOUT_SECONDS, hand it back. Otherwise remove any
        existing container and create a fresh one from `atlas:latest`, then wait for
        cold start (up to COLD_START_TIMEOUT_SECONDS).

        `ATLAS_FORCE_RECREATE=1` skips the reuse check and always recreates. Useful
        after a run where Atlas got wedged (the atlas:latest image is known to hang
        its bulk-entity endpoint under load; a fresh container clears it).
        """
        force_recreate = os.environ.get("ATLAS_FORCE_RECREATE", "").lower() in ("1", "true", "yes")

        existing = self._get_existing_container()
        if existing is not None and not force_recreate:
            if self._probe_active(timeout_seconds=AtlasServerContainer.HEALTHY_REUSE_TIMEOUT_SECONDS):
                logging.info(f"Reusing existing healthy '{AtlasServerContainer.HOST_CONTAINER_NAME}' container.")
                return existing
            logging.warning(f"Existing '{AtlasServerContainer.HOST_CONTAINER_NAME}' container is not ACTIVE within {AtlasServerContainer.HEALTHY_REUSE_TIMEOUT_SECONDS}s; recreating.")

        if existing is not None:
            try:
                existing.remove(force=True)
            except docker.errors.APIError as exc:
                logging.error(f"Failed to remove stale '{AtlasServerContainer.HOST_CONTAINER_NAME}' container: {exc}")
                return None

        try:
            self._client.images.get(AtlasServerContainer.IMAGE)
        except docker.errors.ImageNotFound:
            logging.error(
                f"Image '{AtlasServerContainer.IMAGE}' not found locally. Apache Atlas has no official "
                "image on Docker Hub; build it yourself first. See "
                "extensions/atlas/tests/features/README.md.")
            return None

        logging.info(f"Starting fresh '{AtlasServerContainer.HOST_CONTAINER_NAME}' container from '{AtlasServerContainer.IMAGE}' - cold start is 2-3 min.")
        try:
            container = self._client.containers.run(
                image=AtlasServerContainer.IMAGE,
                name=AtlasServerContainer.HOST_CONTAINER_NAME,
                # Publish REST + embedded Kafka on the loopback interface so host-side probes
                # and the framework's assertion helpers can hit them directly.
                ports={f"{AtlasServerContainer.REST_PORT}/tcp": AtlasServerContainer.REST_PORT,
                       "9092/tcp": 9092},
                detach=True)
        except docker.errors.APIError as exc:
            logging.error(f"Failed to start '{AtlasServerContainer.HOST_CONTAINER_NAME}' from '{AtlasServerContainer.IMAGE}': {exc}")
            return None

        if not wait_for_condition(
                condition=self._is_active,
                timeout_seconds=AtlasServerContainer.COLD_START_TIMEOUT_SECONDS,
                bail_condition=lambda: False,
                context=context):
            logging.error(f"Atlas did not reach ACTIVE state within {AtlasServerContainer.COLD_START_TIMEOUT_SECONDS}s.")
            return None
        return container

    def _get_existing_container(self):
        try:
            return self._client.containers.get(AtlasServerContainer.HOST_CONTAINER_NAME)
        except docker.errors.NotFound:
            return None

    def _probe_active(self, timeout_seconds: float) -> bool:
        """Single short-timeout check for ACTIVE. Different from `_is_active` (used by
        cold-start polling): we want a quick verdict, not a loop."""
        try:
            response = requests.get(f"{self._base_url}/api/atlas/admin/status",
                                    auth=self._auth, timeout=timeout_seconds)
            return response.status_code == 200 and response.json().get("Status") == "ACTIVE"
        except (requests.RequestException, ValueError):
            return False

    def clean_up(self):
        if self._connected:
            try:
                self._network.disconnect(self._container, force=True)
            except Exception as exc:
                logging.warning(f"Failed to detach '{AtlasServerContainer.HOST_CONTAINER_NAME}' from '{self._network.name}': {exc}")
        self._connected = False

    def log_app_output(self) -> bool:
        """Dump the atlas container's tail on failure. Called by log_due_to_failure via
        common_after_scenario, so a diagnostic snapshot of what Atlas saw (or didn't) lands in
        the behavex log alongside the MiNiFi container's own tail."""
        try:
            logs = self._container.logs(tail=200).decode("utf-8", errors="replace")
        except Exception as exc:
            logging.warning(f"Could not fetch logs from '{AtlasServerContainer.HOST_CONTAINER_NAME}': {exc}")
            return False
        logging.info("Tail of Atlas container '%s' logs:", AtlasServerContainer.HOST_CONTAINER_NAME)
        for line in logs.splitlines():
            logging.info(line)
        return False

    # --- Addressing ------------------------------------------------------------------------------

    @property
    def alias(self) -> str:
        """Hostname MiNiFi (running on the scenario network) should use to reach Atlas."""
        return self._alias

    @property
    def in_network_url(self) -> str:
        """Full URL for the `Atlas URLs` reporting-task property."""
        return f"http://{self._alias}:{AtlasServerContainer.REST_PORT}"

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
