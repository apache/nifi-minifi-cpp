# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
