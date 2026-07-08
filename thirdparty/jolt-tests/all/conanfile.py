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

from conan import ConanFile
from conan.tools.files import copy, get, save

required_conan_version = ">=2.0"


class JoltTestsConan(ConanFile):
    name = "jolt-tests"
    description = "Jolt JSON transformation test resources used by the MiNiFi C++ unit tests"
    license = "Apache-2.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/bazaarvoice/jolt"
    topics = ("jolt", "json", "transform", "test-data")
    package_type = "build-scripts"
    # No settings: this package ships only test data, so it is configuration independent.

    # Test resources the MiNiFi unit tests consume, relative to the extracted source root.
    _res_subdir = "jolt-core/src/test/resources/json/shiftr"

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*", src=os.path.join(self.source_folder, self._res_subdir),
             dst=os.path.join(self.package_folder, "res", self._res_subdir))
        # Expose jolt_tests_SOURCE_DIR the same way the bundled FetchContent build does, so the
        # consumer can build "${jolt_tests_SOURCE_DIR}/jolt-core/src/test/resources/json/shiftr".
        save(self, os.path.join(self.package_folder, "res", "conan-jolt-tests-variables.cmake"),
             'set(jolt_tests_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")\n')

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "jolt-tests")
        self.cpp_info.set_property("cmake_build_modules", [os.path.join("res", "conan-jolt-tests-variables.cmake")])
        self.cpp_info.builddirs = ["res"]
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.bindirs = []
