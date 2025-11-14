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
from textwrap import dedent

from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder
from minifi_test_framework.core.hooks import common_before_scenario
from minifi_test_framework.core.hooks import common_after_scenario
from minifi_test_framework.core.hooks import get_minifi_container_image
from minifi_test_framework.core.minifi_test_context import MinifiTestContext

# These hooks are executed by behave before and after each scenario
# The common_before_scenario and common_after_scenario must be called for proper setup and tear down


def before_all(context: MinifiTestContext):
    minifi_container_image = get_minifi_container_image()

    dockerfile = dedent("""\
                FROM {base_image}
                RUN mkdir {models_path} && wget https://huggingface.co/bartowski/Qwen2-0.5B-Instruct-GGUF/resolve/main/Qwen2-0.5B-Instruct-IQ3_M.gguf --directory-prefix={models_path}
        """.format(base_image=minifi_container_image, models_path='/tmp/models'))

    builder = DockerImageBuilder(
        image_tag="apacheminificpp:llama",
        dockerfile_content=dockerfile
    )
    builder.build()


def before_scenario(context: MinifiTestContext, scenario):
    context.minifi_container_image = "apacheminificpp:llama"
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
