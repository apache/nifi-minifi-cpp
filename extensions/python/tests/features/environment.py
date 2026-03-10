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
import os
from pathlib import Path
from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder
from minifi_test_framework.core.hooks import common_before_scenario
from minifi_test_framework.core.hooks import common_after_scenario


def get_minifi_container_image():
    if 'MINIFI_TAG_PREFIX' in os.environ and 'MINIFI_VERSION' in os.environ:
        minifi_tag_prefix = os.environ['MINIFI_TAG_PREFIX']
        minifi_version = os.environ['MINIFI_VERSION']
        return 'apacheminificpp:' + minifi_tag_prefix + minifi_version
    return "apacheminificpp:behave"


def before_all(context):
    context.minifi_container_image = get_minifi_container_image()
    client: docker.DockerClient = docker.from_env()
    is_fhs = 'MINIFI_INSTALLATION_TYPE=FHS' in str(client.images.get(context.minifi_container_image).history())
    pip3_install_command = ""
    minifi_tag_prefix = os.environ['MINIFI_TAG_PREFIX'] if 'MINIFI_TAG_PREFIX' in os.environ else ""
    if not minifi_tag_prefix:
        pip3_install_command = "RUN apk --update --no-cache add py3-pip"
    minifi_python_dir_path = '/var/lib/nifi-minifi-cpp/minifi-python' if is_fhs else '/opt/minifi/minifi-current/minifi-python'
    minifi_python_venv_parent = '/var/lib/nifi-minifi-cpp' if is_fhs else '/opt/minifi/minifi-current'
    dockerfile = """
FROM {base_image}
USER root
{pip3_install_command}
USER minificpp
COPY RotatingForwarder.py {minifi_python_dir}/nifi_python_processors/RotatingForwarder.py
COPY SpecialPropertyTypeChecker.py {minifi_python_dir}/nifi_python_processors/SpecialPropertyTypeChecker.py
COPY ProcessContextInterfaceChecker.py {minifi_python_dir}/nifi_python_processors/ProcessContextInterfaceChecker.py
COPY CreateFlowFile.py {minifi_python_dir}/nifi_python_processors/CreateFlowFile.py
COPY FailureWithAttributes.py {minifi_python_dir}/nifi_python_processors/FailureWithAttributes.py
COPY subtractutils.py {minifi_python_dir}/nifi_python_processors/compute/subtractutils.py
COPY RelativeImporterProcessor.py {minifi_python_dir}/nifi_python_processors/compute/processors/RelativeImporterProcessor.py
COPY multiplierutils.py {minifi_python_dir}/nifi_python_processors/compute/processors/multiplierutils.py
COPY CreateNothing.py {minifi_python_dir}/nifi_python_processors/CreateNothing.py
COPY FailureWithContent.py {minifi_python_dir}/nifi_python_processors/FailureWithContent.py
COPY TransferToOriginal.py {minifi_python_dir}/nifi_python_processors/TransferToOriginal.py
COPY SetRecordField.py {minifi_python_dir}/nifi_python_processors/SetRecordField.py
COPY TestStateManager.py {minifi_python_dir}/nifi_python_processors/TestStateManager.py
COPY NifiStyleLogDynamicProperties.py {minifi_python_dir}/nifi_python_processors/NifiStyleLogDynamicProperties.py
COPY LogDynamicProperties.py {minifi_python_dir}/LogDynamicProperties.py
COPY ExpressionLanguagePropertyWithValidator.py {minifi_python_dir}/nifi_python_processors/ExpressionLanguagePropertyWithValidator.py
COPY EvaluateExpressionLanguageChecker.py {minifi_python_dir}/nifi_python_processors/EvaluateExpressionLanguageChecker.py
RUN python3 -m venv {minifi_python_venv_parent}/venv
    """.format(base_image=context.minifi_container_image,
               pip3_install_command=pip3_install_command,
               minifi_python_dir=minifi_python_dir_path,
               minifi_python_venv_parent=minifi_python_venv_parent)

    build_context_path = str(Path(__file__).resolve().parent / "resources")
    files_on_context = {}
    for filename in os.listdir(build_context_path):
        file_path = os.path.join(build_context_path, filename)
        with open(file_path, "rb") as f:
            files_on_context[filename] = f.read()

    builder = DockerImageBuilder(
        image_tag="apacheminificpp-python:latest",
        dockerfile_content=dockerfile,
        files_on_context=files_on_context
    )
    builder.build()


def is_conda_available_in_minifi_image(context):
    client: docker.DockerClient = docker.from_env()
    container = client.containers.create(
        image=context.minifi_container_image,
        command=['conda', '--version'],
    )
    try:
        container.start()
        result = container.logs()
        container.remove(force=True)
    except docker.errors.APIError:
        container.remove(force=True)
        return False

    return result.decode('utf-8').startswith('conda ')


def get_minifi_image_python_version(context):
    client: docker.DockerClient = docker.from_env()
    result = client.containers.run(
        image=context.minifi_container_image,
        command=['python3', '-c', 'import platform; print(platform.python_version())'],
        remove=True
    )

    python_ver_str = result.decode('utf-8')
    return tuple(map(int, python_ver_str.split('.')))


def before_scenario(context, scenario):
    if "USE_NIFI_PYTHON_PROCESSORS_WITH_LANGCHAIN" in scenario.effective_tags:
        if not is_conda_available_in_minifi_image(context) and get_minifi_image_python_version(context) < (3, 8, 1):
            scenario.skip("NiFi Python processor tests use langchain library which requires Python 3.8.1 or later.")
            return

    common_before_scenario(context, scenario)
    context.minifi_container_image = "apacheminificpp-python:latest"


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
