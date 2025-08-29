class Parameter:
    def __init__(self, name: str, value: str, is_sensitive: bool):
        self.name: str = name
        self.value: str = value
        self.description: str = name
        self.is_sensitive: bool = is_sensitive

    def to_yaml_dict(self):
        data = {'name': self.name, 'value': self.value, 'description': self.description,
                'sensitive': self.is_sensitive, }
        return data
