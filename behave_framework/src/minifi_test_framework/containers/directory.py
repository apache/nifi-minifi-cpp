from typing import Dict


class Directory:
    def __init__(self, path):
        self.path = path
        self.files: Dict[str, str] = {}
        self.mode = "rw"

    def add_file(self, file_name: str, content: str):
        self.files[file_name] = content
