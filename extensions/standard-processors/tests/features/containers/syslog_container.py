#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

from minifi_behave.containers.container_linux import LinuxContainer


class SyslogContainer(LinuxContainer):
    def __init__(self, protocol, context):
        super().__init__("ubuntu:24.04", f"syslog-{protocol}-{context.scenario_id}", context.network)
        self.command = f'/bin/bash -c "echo Syslog {protocol} client started; while true; do logger --{protocol} -n minifi-primary-{context.scenario_id} -P 514 sample_log; sleep 1; done"'
