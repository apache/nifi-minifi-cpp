# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import logging

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.containers.directory import Directory
from minifi_behave.core.helpers import retry_check, wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext

logger = logging.getLogger(__name__)


class FakeGcsServerContainer(LinuxContainer):
    IMAGE = "fsouza/fake-gcs-server:1.45.1"

    def __init__(self, test_context: MinifiTestContext):
        super().__init__(
            FakeGcsServerContainer.IMAGE,
            f"fake-gcs-server-{test_context.scenario_id}",
            test_context.network,
            command=f"-scheme http -host fake-gcs-server-{test_context.scenario_id}",
        )
        self.dirs.append(Directory(path="/data/test-bucket", files={"test-file": "preloaded data\n"}))

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        finished_str = "server started at http"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=context,
        )

    @retry_check()
    def check_google_cloud_storage(self, content):
        (code, output) = self.exec_run(["grep", "-r", content, "/storage"])
        logger.info(f"GCS storage contents matching '{content}': {output}")
        return code == 0

    @retry_check()
    def is_gcs_bucket_empty(self):
        (code, output) = self.exec_run(["ls", "/storage/test-bucket"])
        logger.info(f"GCS bucket contents: {output}")
        return code == 0 and output == ""
