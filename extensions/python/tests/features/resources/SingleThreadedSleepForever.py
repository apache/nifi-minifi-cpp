# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import time
from typing import ClassVar

from nifiapi.decorators import trigger_serially
from nifiapi.flowfilesource import FlowFileSource


@trigger_serially
class SingleThreadedSleepForever(FlowFileSource):
    class Java:
        implements: ClassVar[list] = ["org.apache.nifi.python.processor.FlowFileSource"]

    class ProcessorDetails:
        version = "2.0.0-snapshot"
        description = """NiFi-style sleep forever python processor (single-threaded)."""
        tags: ClassVar[list] = ["test", "python", "sleep"]

    def __init__(self, **kwargs):
        pass

    def create(self, context):
        self.logger.info("Sleeping forever")
        while True:
            time.sleep(1)
