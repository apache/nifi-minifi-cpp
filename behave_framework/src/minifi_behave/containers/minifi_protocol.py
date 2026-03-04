from typing import Protocol


class MinifiProtocol(Protocol):
    def set_property(self, key: str, value: str):
        ...

    def set_log_property(self, key: str, value: str):
        ...
