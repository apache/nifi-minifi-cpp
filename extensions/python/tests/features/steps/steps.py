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
import os
import docker
from behave import given

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder


class PythonWithDependenciesOptions:
    REQUIREMENTS_FILE = "apacheminificpp-python-requirements-file:latest"
    SYSTEM_INSTALLED_PACKAGES = "apacheminificpp-python-system-installed-packages:latest"
    INLINE_DEFINED_PACKAGES = "apacheminificpp-python-inline-defined-packages:latest"


def build_minifi_cpp_image_with_nifi_python_processors_using_dependencies(context, python_option):
    minifi_tag_prefix = os.environ['MINIFI_TAG_PREFIX'] if 'MINIFI_TAG_PREFIX' in os.environ else ""
    client: docker.DockerClient = docker.from_env()
    is_fhs = 'MINIFI_INSTALLATION_TYPE=FHS' in str(client.images.get(context.minifi_container_image).history())
    minifi_python_dir_path = '/var/lib/nifi-minifi-cpp/minifi-python' if is_fhs else '/opt/minifi/minifi-current/minifi-python'
    minifi_python_venv_parent = '/var/lib/nifi-minifi-cpp' if is_fhs else '/opt/minifi/minifi-current'
    parse_document_url = "https://raw.githubusercontent.com/apache/nifi-python-extensions/refs/heads/main/src/extensions/chunking/ParseDocument.py"
    chunk_document_url = "https://raw.githubusercontent.com/apache/nifi-python-extensions/refs/heads/main/src/extensions/chunking/ChunkDocument.py"
    pip3_install_command = ""
    requirements_install_command = ""
    additional_cmd = ""
    # The following sed command is used to remove the existing dependencies from the ParseDocument and ChunkDocument processors
    # /class ProcessorDetails:/,/^$/: Do the following between 'class ProcessorDetails:' and the first empty line (so we don't modify other PropertyDescriptor blocks below)
    # /^\s*dependencies\s*=/,/\]\s*$/: Do the following between 'dependencies =' at the start of a line, and ']' at the end of a line
    # d: Delete line
    parse_document_sed_cmd = 'sed -i "/class ProcessorDetails:/,/^$/{{/^\\s*dependencies\\s*=/,/\\]\\s*$/d}}" {minifi_python_dir}/nifi_python_processors/ParseDocument.py && \\'.format(minifi_python_dir=minifi_python_dir_path)
    chunk_document_sed_cmd = 'sed -i "/class ProcessorDetails:/,/^$/{{/^\\s*dependencies\\s*=/,/\\]\\s*$/d}}" {minifi_python_dir}/nifi_python_processors/ChunkDocument.py && \\'.format(minifi_python_dir=minifi_python_dir_path)
    if python_option == PythonWithDependenciesOptions.SYSTEM_INSTALLED_PACKAGES:
        if not minifi_tag_prefix or "bookworm" in minifi_tag_prefix or "noble" in minifi_tag_prefix or "trixie" in minifi_tag_prefix:
            additional_cmd = "RUN pip3 install --break-system-packages 'langchain<=0.17.0'"
        else:
            additional_cmd = "RUN pip3 install 'langchain<=0.17.0'"
    elif python_option == PythonWithDependenciesOptions.REQUIREMENTS_FILE:
        requirements_install_command = "echo 'langchain<=0.17.0' > {minifi_python_dir}/nifi_python_processors/requirements.txt && \\".format(minifi_python_dir=minifi_python_dir_path)
    elif python_option == PythonWithDependenciesOptions.INLINE_DEFINED_PACKAGES:
        parse_document_sed_cmd = parse_document_sed_cmd[:-2] + ' sed -i "s/langchain==[0-9.]\\+/langchain<=0.17.0/" {minifi_python_dir}/nifi_python_processors/ParseDocument.py && \\'.format(minifi_python_dir=minifi_python_dir_path)
        chunk_document_sed_cmd = 'sed -i "s/\\[\\\'langchain\\\'\\]/\\[\\\'langchain<=0.17.0\\\'\\]/" {minifi_python_dir}/nifi_python_processors/ChunkDocument.py && \\'.format(minifi_python_dir=minifi_python_dir_path)
    if not minifi_tag_prefix:
        pip3_install_command = "RUN apk --update --no-cache add py3-pip"
    dockerfile = """\
FROM {base_image}
USER root
{pip3_install_command}
{additional_cmd}
USER minificpp
RUN wget {parse_document_url} --directory-prefix={minifi_python_dir}/nifi_python_processors && \\
    wget {chunk_document_url} --directory-prefix={minifi_python_dir}/nifi_python_processors && \\
    echo 'langchain<=0.17.0' > {minifi_python_dir}/nifi_python_processors/requirements.txt && \\
    {requirements_install_command}
    {parse_document_sed_cmd}
    {chunk_document_sed_cmd}
    python3 -m venv {minifi_python_venv_parent}/venv && \\
    python3 -m venv {minifi_python_venv_parent}/venv-with-langchain && \\
    . {minifi_python_venv_parent}/venv-with-langchain/bin/activate && python3 -m pip install --no-cache-dir "langchain<=0.17.0" && \\
    deactivate
            """.format(base_image=context.minifi_container_image,
                       pip3_install_command=pip3_install_command,
                       parse_document_url=parse_document_url,
                       chunk_document_url=chunk_document_url,
                       additional_cmd=additional_cmd,
                       requirements_install_command=requirements_install_command,
                       parse_document_sed_cmd=parse_document_sed_cmd,
                       chunk_document_sed_cmd=chunk_document_sed_cmd,
                       minifi_python_dir=minifi_python_dir_path,
                       minifi_python_venv_parent=minifi_python_venv_parent)
    builder = DockerImageBuilder(
        image_tag=python_option,
        dockerfile_content=dockerfile,
    )
    builder.build()
    context.get_or_create_default_minifi_container().image_name = python_option


@given("the example MiNiFi python processors are present")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().add_example_python_processors()


@given("python with langchain is installed on the MiNiFi agent {install_mode}")
def step_impl(context, install_mode):
    client: docker.DockerClient = docker.from_env()
    is_fhs = 'MINIFI_INSTALLATION_TYPE=FHS' in str(client.images.get(context.minifi_container_image).history())
    minifi_python_venv_parent = '/var/lib/nifi-minifi-cpp' if is_fhs else '/opt/minifi/minifi-current'
    if install_mode == "with required python packages":
        build_minifi_cpp_image_with_nifi_python_processors_using_dependencies(context, PythonWithDependenciesOptions.SYSTEM_INSTALLED_PACKAGES)
        context.get_or_create_default_minifi_container().set_property("nifi.python.install.packages.automatically", "false")
    elif install_mode == "with a pre-created virtualenv":
        build_minifi_cpp_image_with_nifi_python_processors_using_dependencies(context, PythonWithDependenciesOptions.REQUIREMENTS_FILE)
        context.get_or_create_default_minifi_container().set_property("nifi.python.virtualenv.directory", f"{minifi_python_venv_parent}/venv")
        context.get_or_create_default_minifi_container().set_property("nifi.python.install.packages.automatically", "true")
    elif install_mode == "with a pre-created virtualenv containing the required python packages":
        build_minifi_cpp_image_with_nifi_python_processors_using_dependencies(context, PythonWithDependenciesOptions.REQUIREMENTS_FILE)
        context.get_or_create_default_minifi_container().set_property("nifi.python.virtualenv.directory", f"{minifi_python_venv_parent}/venv-with-langchain")
        context.get_or_create_default_minifi_container().set_property("nifi.python.install.packages.automatically", "false")
    elif install_mode == "using inline defined Python dependencies to install packages":
        build_minifi_cpp_image_with_nifi_python_processors_using_dependencies(context, PythonWithDependenciesOptions.INLINE_DEFINED_PACKAGES)
        context.get_or_create_default_minifi_container().set_property("nifi.python.virtualenv.directory", f"{minifi_python_venv_parent}/venv")
        context.get_or_create_default_minifi_container().set_property("nifi.python.install.packages.automatically", "true")
    else:
        raise Exception("Unknown python install mode.")
