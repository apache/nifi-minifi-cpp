import json
from utils import retry_check


class AwsChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    @retry_check()
    def check_s3_server_object_data(self, container_name, test_data):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/tmp/", "-type", "d", "-name", "s3mock*"])
        if code != 0:
            return False
        s3_mock_dir = output.strip()
        (code, file_data) = self.container_communicator.execute_command(container_name, ["cat", s3_mock_dir + "/test_bucket/test_object_key/fileData"])
        return code == 0 and file_data == test_data

    @retry_check()
    def check_s3_server_object_metadata(self, container_name, content_type="application/octet-stream", metadata=dict()):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/tmp/", "-type", "d", "-name", "s3mock*"])
        if code != 0:
            return False
        s3_mock_dir = output.strip()
        (code, output) = self.container_communicator.execute_command(container_name, ["cat", s3_mock_dir + "/test_bucket/test_object_key/metadata"])
        server_metadata = json.loads(output)
        return code == 0 and server_metadata["contentType"] == content_type and metadata == server_metadata["userMetadata"]

    @retry_check()
    def is_s3_bucket_empty(self, container_name):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/tmp/", "-type", "d", "-name", "s3mock*"])
        if code != 0:
            return False
        s3_mock_dir = output.strip()
        (code, ls_result) = self.container_communicator.execute_command(container_name, ["ls", s3_mock_dir + "/test_bucket/"])
        return code == 0 and not ls_result
