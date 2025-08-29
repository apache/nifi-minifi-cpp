import uuid
from typing import List

from minifi_test_framework.minifi.parameter import Parameter


class ParameterContext:
    def __init__(self, name: str):
        self.name = name
        self.id: str = str(uuid.uuid4())
        self.parameters: List[Parameter] = []

    def to_yaml_dict(self) -> dict:
        return {
            'id': self.id,
            'name': self.name,
            'Parameters': [p.to_yaml_dict() for p in self.parameters],
        }
