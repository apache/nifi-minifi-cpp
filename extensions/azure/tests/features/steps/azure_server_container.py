#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

import logging

from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from azure.storage.blob import BlobServiceClient
from azure.core.exceptions import ResourceExistsError


class AzureServerContainer(Container):
    AZURE_CONNECTION_STRING = \
        ("DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;"
         "BlobEndpoint=http://127.0.0.1:10000/devstoreaccount1;QueueEndpoint=http://127.0.0.1:10001/devstoreaccount1;")

    def __init__(self, test_context: MinifiTestContext):
        super().__init__("mcr.microsoft.com/azure-storage/azurite:3.35.0", f"azure-storage-server-{test_context.scenario_id}", test_context.network)
        self.blob_service_client = BlobServiceClient.from_connection_string(AzureServerContainer.AZURE_CONNECTION_STRING)
        self.ports = {'10000/tcp': 10000, '10001/tcp': 10001}

    def deploy(self):
        super().deploy()
        finished_str = "Azurite Queue service is successfully listening at"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=15,
            bail_condition=lambda: self.exited,
            context=None)

    def check_azure_storage_server_data(self, test_data):
        (code, output) = self.exec_run(["find", "/data/__blobstorage__", "-type", "f"])
        if code != 0:
            return False
        data_file = output.strip()
        (code, file_data) = self.exec_run(["cat", data_file])
        return code == 0 and test_data in file_data

    def add_test_blob(self, blob_name, content="", with_snapshot=False) -> bool:
        try:
            self.blob_service_client.create_container("test-container")
        except ResourceExistsError:
            logging.debug('test-container already exists')

        blob_client = self.blob_service_client.get_blob_client(container="test-container", blob=blob_name)
        blob_client.upload_blob(content)

        if with_snapshot:
            blob_client.create_snapshot()
        return True

    def __get_blob_and_snapshot_count(self):
        container_client = self.blob_service_client.get_container_client("test-container")
        return len(list(container_client.list_blobs(include=['deleted'])))

    def check_azure_blob_and_snapshot_count(self, blob_and_snapshot_count):
        return self.__get_blob_and_snapshot_count() == blob_and_snapshot_count

    def check_azure_blob_storage_is_empty(self):
        return self.__get_blob_and_snapshot_count() == 0
