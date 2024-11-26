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
import json
import platform
import os

import cmake_parser
from cmake_parser import CMakeCacheValue
from package_manager import PackageManager


class MinifiOptions:
    def __init__(self, cache_values: Dict[str, CMakeCacheValue]):
        self.cmake_override = ""
        self.build_type = CMakeCacheValue("Specifies the build type on single-configuration generators",
                                          "CMAKE_BUILD_TYPE", "STRING", "Release")
        self.build_type.possible_values = ["Release", "Debug", "RelWithDebInfo", "MinSizeRel"]
        additional_build_options = ["DOCKER_BUILD_ONLY", "DOCKER_SKIP_TESTS", "SKIP_TESTS", "PORTABLE"]
        self.use_ninja = CMakeCacheValue("Specifies if CMake should use the Ninja generator or the system default", "USE_NINJA", "BOOL", "ON")
        self.bool_options = {name: cache_value for name, cache_value in cache_values.items() if
                             cache_value.value_type == "BOOL" and ("ENABLE" in name or "MINIFI" in name or name in additional_build_options)}
        self.build_options = {name: cache_value for name, cache_value in self.bool_options.items() if "MINIFI" in name or name in additional_build_options}
        self.build_options["USE_NINJA"] = self.use_ninja
        self.extension_options = {name: cache_value for name, cache_value in self.bool_options.items() if "ENABLE" in name}
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

    def create_cmake_generator_str(self) -> str:
        return "-G Ninja" if self.use_ninja.value == "ON" else ""

    def create_cmake_build_flags_str(self) -> str:
        additional_flags = ""
        if self.use_ninja.value != "ON":
            additional_flags = f"-j {os.cpu_count()}"
            if platform.system() == "Windows":
                additional_flags += f" --config {self.build_type.value}"
        return additional_flags

    def is_enabled(self, option_name: str) -> bool:
        if option_name not in self.bool_options:
            raise ValueError(f"Expected {option_name} to be a minifi option")
        if "ENABLE_ALL" in self.bool_options and self.bool_options["ENABLE_ALL"].value == "ON":
            return True
        return self.bool_options[option_name].value == "ON"

    def set_cmake_override(self, cmake_override: str):
        self.cmake_override = cmake_override

    def save_option_state(self):
        options_dict = dict()
        for option_name in self.bool_options:
            options_dict[option_name] = self.bool_options[option_name].value
        options_dict[self.use_ninja.name] = self.use_ninja.value
        options_dict[self.build_type.name] = self.build_type.value
        options_dict["build_dir"] = str(self.build_dir)

        with open(pathlib.Path(__file__).parent / "option_state.json", "w") as f:
            json.dump(options_dict, f)

    def load_option_state(self):
        state_path = pathlib.Path(__file__).parent / "option_state.json"
        if not pathlib.Path.exists(state_path):
            return
        with open(state_path, "r") as f:
            options_dict = json.load(f)
            for option_name in options_dict:
                if option_name not in self.bool_options:
                    continue
                self.bool_options[option_name].value = options_dict[option_name]
            if self.use_ninja.name in options_dict:
                self.use_ninja.value = options_dict[self.use_ninja.name]
            if self.build_type.name in options_dict:
                self.build_type.value = options_dict[self.build_type.name]
            if "build_dir" in options_dict:
                self.build_dir = pathlib.Path(options_dict["build_dir"])


def parse_minifi_options(path: str, cmake_options: str, package_manager: PackageManager, cmake_cache_dir: str):
    cmake_cache_path = cmake_parser.create_cmake_cache(path, cmake_options, cmake_cache_dir, package_manager)
    return MinifiOptions(cmake_parser.parse_cmake_cache_values(cmake_cache_path))
