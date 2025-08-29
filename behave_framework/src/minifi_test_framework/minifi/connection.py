import uuid


class Connection:
    def __init__(self, source_name: str, source_relationship: str, target_name: str):
        self.id: str = str(uuid.uuid4())
        self.source_name: str = source_name
        self.source_relationship: str = source_relationship
        self.target_name: str = target_name

    def __repr__(self):
        return f"({self.source_name}:{self.source_relationship} --> {self.target_name})"
