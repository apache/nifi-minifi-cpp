import uuid
from typing import Dict


class ControllerService:
    def __init__(self, class_name: str, service_name: str):
        self.class_name = class_name
        self.id: str = str(uuid.uuid4())
        self.name = service_name

        self.properties: Dict[str, str] = {}

    def add_property(self, property_name: str, property_value: str):
        self.properties[property_name] = property_value

    def to_yaml_dict(self) -> dict:
        data = {'name': self.name, 'id': self.id, 'class': self.class_name, 'Properties': self.properties}

        return data
