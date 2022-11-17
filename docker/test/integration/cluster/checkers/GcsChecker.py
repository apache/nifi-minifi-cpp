from utils import retry_check


class GcsChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    @retry_check()
    def check_google_cloud_storage(self, gcs_container_name, content):
        (code, _) = self.container_communicator.execute_command(gcs_container_name, ["grep", "-r", content, "/storage"])
        return code == 0

    @retry_check()
    def is_gcs_bucket_empty(self, container_name):
        (code, output) = self.container_communicator.execute_command(container_name, ["ls", "/storage/test-bucket"])
        return code == 0 and output == ""
