import uuid


class Funnel:
    def __init__(self, funnel_name: str):
        self.name = funnel_name
        self.id: str = str(uuid.uuid4())

    def __repr__(self):
        return f"({self.name} {self.id})"

    def to_yaml_dict(self) -> dict:
        # Funnels have a simpler representation in the MiNiFi YAML
        return {'id': self.id, 'name': self.name, }
