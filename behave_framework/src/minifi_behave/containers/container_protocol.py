from typing import Protocol


class ContainerProtocol(Protocol):
    def deploy(self, context) -> bool:
        ...

    def clean_up(self):
        ...

    def exec_run(self, command):
        ...

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        ...

    def directory_contains_file_with_regex(self, directory_path: str, regex_str: str) -> bool:
        ...

    def path_with_content_exists(self, path: str, content: str) -> bool:
        ...

    def get_logs(self) -> str:
        ...

    @property
    def exited(self) -> bool:
        ...

    def get_number_of_files(self, directory_path: str) -> int:
        ...

    def verify_file_contents(self, directory_path: str, expected_contents: list[str]) -> bool:
        ...

    def log_app_output(self) -> bool:
        ...
