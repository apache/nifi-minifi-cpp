# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import docker

FHS_ENV_MARKER = "MINIFI_INSTALLATION_TYPE=FHS"


class NormalDeployment:
    def __init__(self):
        self.conf_path = "/opt/minifi/minifi-current/conf"
        self.bin_path = "/opt/minifi/minifi-current/bin"
        self.extension_dir = "/opt/minifi/minifi-current/extensions"
        self.extension_pattern = "../extensions/*"
        self.minifi_python_path = "/opt/minifi/minifi-current/minifi-python"
        self.minifi_python_examples_path = "/opt/minifi/minifi-current/minifi-python-examples"


class FHSDeployment:
    def __init__(self):
        self.conf_path = "/etc/nifi-minifi-cpp/"
        self.bin_path = "/usr/bin"
        self.extension_dir = "/usr/lib64/nifi-minifi-cpp/extensions"
        self.extension_pattern = f"{self.extension_dir}/*"
        self.minifi_python_path = "/var/lib/nifi-minifi-cpp/minifi-python"
        self.minifi_python_examples_path = "/usr/share/doc/nifi-minifi-cpp/pythonprocessor-examples"


def is_fhs_image(image: str) -> bool:
    return FHS_ENV_MARKER in str(docker.from_env().images.get(image).history())


def get_deployment_for_image(image: str) -> NormalDeployment | FHSDeployment:
    return FHSDeployment() if is_fhs_image(image) else NormalDeployment()
