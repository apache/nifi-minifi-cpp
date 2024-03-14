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

from typing import Dict

import pathlib

import cmake_parser
from cmake_parser import CMakeCacheValue
from package_manager import PackageManager


class MinifiOptions:
    def __init__(self, cache_values: Dict[str, CMakeCacheValue]):
        self.cmake_override = ""
        self.build_type = CMakeCacheValue("Specifies the build type on single-configuration generators",
                                          "CMAKE_BUILD_TYPE", "STRING", "Release")
        self.build_type.possible_values = ["Release", "Debug", "RelWithDebInfo", "MinSizeRel"]
        self.bool_options = {name: cache_value for name, cache_value in cache_values.items() if
                             cache_value.value_type == "BOOL" and ("ENABLE" in name or "MINIFI" in name)}
        self.multi_choice_options = [cache_value for name, cache_value in cache_values.items() if
                                     cache_value.value_type == "STRING" and cache_value.possible_values is not None]
        self.build_dir = pathlib.Path(__file__).parent.parent.resolve() / "build"
        self.source_dir = pathlib.Path(__file__).parent.parent.resolve()
        self.no_confirm = False

    def create_cmake_options_str(self) -> str:
        cmake_options = [bool_option.create_cmake_option_str() for name, bool_option in self.bool_options.items()]
        if self.cmake_override:
            cmake_options.append(self.cmake_override)
        cmake_options.append(f'-DCMAKE_BUILD_TYPE={self.build_type.value}')
        cmake_options_str = " ".join(filter(None, cmake_options))
        return cmake_options_str

    def is_enabled(self, option_name: str) -> bool:
        if option_name not in self.bool_options:
            raise ValueError(f"Expected {option_name} to be a minifi option")
        if "ENABLE_ALL" in self.bool_options and self.bool_options["ENABLE_ALL"].value == "ON":
            return True
        return self.bool_options[option_name].value == "ON"

    def set_cmake_override(self, cmake_override: str):
        self.cmake_override = cmake_override


def parse_minifi_options(path: str, cmake_options: str, package_manager: PackageManager, cmake_cache_dir: str):
    cmake_cache_path = cmake_parser.create_cmake_cache(path, cmake_options, cmake_cache_dir, package_manager)
    return MinifiOptions(cmake_parser.parse_cmake_cache_values(cmake_cache_path))
