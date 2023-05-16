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
import re

from package_manager import PackageManager


class CMakeCacheValue:
    def __init__(self, description: str, name: str, value_type: str, value: str):
        self.description = description
        self.name = name
        self.value_type = value_type
        self.value = value
        self.possible_values = None

    def __str__(self):
        return f"CMakeCacheVariable, description:{self.description} name:{self.name}, type:{self.value_type}, value:{self.value}"

    def __repr__(self):
        return self.__str__()

    def create_cmake_option_str(self):
        return f"-D{self.name}={self.value}"


def create_cmake_cache(cmake_path: str, cmake_options: str, directory: str, package_manager: PackageManager):
    cmake_lists_path = os.path.join(directory, 'CMakeLists.txt')

    with open(cmake_lists_path, 'w') as cmake_lists_file:
        cmake_lists_file.write('cmake_minimum_required(VERSION 3.5)\n')
        cmake_lists_file.write(f'include("{cmake_path}")\n')

    if cmake_options is None:
        assert package_manager.run_cmd(f'cmake -G Ninja -Wno-dev --log-level=ERROR {directory} -B {directory}')
    else:
        assert package_manager.run_cmd(
            f'cmake -G Ninja -Wno-dev --no-warn-unused-cli --log-level=ERROR {cmake_options} {directory} -B {directory}')
    return os.path.join(directory, 'CMakeCache.txt')


def parse_cmake_cache_values(path: str):
    parsed_variables = {}
    with open(path, 'r') as file:
        contents = file.read()
        pattern = r'\/\/(?P<description>[\s\S]*?)\n(?P<variable>.+?):(?P<type>\w+?)=(?P<value>.+)\n'
        matches = re.findall(pattern, contents)

        for match in matches:
            cmake_cache_value = CMakeCacheValue(description=match[0].replace("//", "").replace("\n", " "),
                                                name=match[1],
                                                value_type=match[2], value=match[3])
            if cmake_cache_value.name.endswith("-STRINGS"):
                possible_values_of = cmake_cache_value.name[:-len("-STRINGS")]
                if possible_values_of not in parsed_variables:
                    raise ValueError(f"Did not parse {possible_values_of} yet")
                parsed_variables[possible_values_of].possible_values = cmake_cache_value.value.split(";")
                continue

            parsed_variables[cmake_cache_value.name] = cmake_cache_value
    return parsed_variables
