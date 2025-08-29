from typing import List

from behave.runner import Context
from docker.models.networks import Network

from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.minifi_container import MinifiContainer


class MinifiTestContext(Context):
    minifi_container: MinifiContainer
    containers: List[Container]
    scenario_id: str
    network: Network
