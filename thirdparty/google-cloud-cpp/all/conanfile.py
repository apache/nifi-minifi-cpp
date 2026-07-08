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

# Recipe based on https://github.com/conan-io/conan-center-index/blob/master/recipes/google-cloud-cpp/2.x/conanfile.py only building storage component
import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd, cross_building
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualBuildEnv, VirtualRunEnv
from conan.tools.files import apply_conandata_patches, copy, export_conandata_patches, get, rmdir
from conan.tools.microsoft import check_min_vs, is_msvc
from conan.tools.scm import Version
from conan.errors import ConanInvalidConfiguration

required_conan_version = ">=2.0.0"


class GoogleCloudCppConan(ConanFile):
    name = "google-cloud-cpp"
    description = "C++ Client Libraries for Google Cloud Services (storage only)"
    license = "Apache-2.0"
    topics = (
        "google",
        "cloud",
        "google-cloud-storage",
        "google-cloud-platform",
    )
    homepage = "https://github.com/googleapis/google-cloud-cpp"
    url = "https://github.com/conan-io/conan-center-index"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False], "with_mocks": [True, False]}
    default_options = {"shared": False, "fPIC": True, "with_mocks": False}

    short_paths = True

    @property
    def _is_legacy_one_profile(self):
        return not hasattr(self, "settings_build")

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def validate(self):
        # As-of 2022-03, google-cloud-cpp only supports "Visual Studio >= 2019",
        # and Visual Studio < 2019 is out of mainline support.
        # The wikipedia page says this maps to 192* for the MSVC version:
        #   https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B
        check_min_vs(self, "192")
        if is_msvc(self) and self.info.options.shared:
            raise ConanInvalidConfiguration(f"{self.ref} shared not supported by Visual Studio")

        if hasattr(self, "settings_build") and cross_building(self):
            raise ConanInvalidConfiguration(
                "Recipe not prepared for cross-building (yet)"
            )

        if (self.settings.compiler == "clang" and Version(self.settings.compiler.version) < "6.0"):
            raise ConanInvalidConfiguration("Clang version must be at least 6.0.")

        if self.settings.compiler.cppstd:
            check_min_cppstd(self, 14)

        if (self.settings.compiler == "gcc" and Version(self.settings.compiler.version) < "5.4"):
            raise ConanInvalidConfiguration("Building requires GCC >= 5.4")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], destination=self.source_folder, strip_root=True)

    def requirements(self):
        self.requires("abseil/20260526.0", transitive_headers=True)
        self.requires("nlohmann_json/3.12.0")
        self.requires("crc32c/1.1.2")
        self.requires("libcurl/8.20.0")
        self.requires("openssl/3.6.2")
        self.requires("zlib/1.3.2")
        if self.options.with_mocks:
            self.requires("gtest/1.17.0", transitive_headers=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = False
        tc.variables["GOOGLE_CLOUD_CPP_WITH_MOCKS"] = self.options.with_mocks
        tc.variables["GOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK"] = False
        tc.variables["GOOGLE_CLOUD_CPP_ENABLE_WERROR"] = False
        tc.variables["GOOGLE_CLOUD_CPP_ENABLE"] = "storage"
        tc.generate()
        VirtualBuildEnv(self).generate()
        if self._is_legacy_one_profile:
            VirtualRunEnv(self).generate(scope="build")
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        apply_conandata_patches(self)
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, path=os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, path=os.path.join(self.package_folder, "lib", "pkgconfig"))
        if self.options.with_mocks:
            copy(self, "canonical_errors.h",
                 src=os.path.join(self.source_folder, "google", "cloud", "storage", "testing"),
                 dst=os.path.join(self.package_folder, "include", "google", "cloud", "storage", "testing"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "GoogleCloudCpp")

        self.cpp_info.components["common"].requires = ["abseil::absl_any", "abseil::absl_flat_hash_map", "abseil::absl_memory", "abseil::absl_optional", "abseil::absl_time"]
        self.cpp_info.components["common"].libs = ["google_cloud_cpp_common"]
        self.cpp_info.components["common"].names["pkg_config"] = "google_cloud_cpp_common"

        self.cpp_info.components["rest_internal"].requires = ["common", "libcurl::libcurl", "openssl::ssl", "openssl::crypto", "zlib::zlib"]
        self.cpp_info.components["rest_internal"].libs = ["google_cloud_cpp_rest_internal"]
        self.cpp_info.components["rest_internal"].names["pkg_config"] = "google_cloud_cpp_rest_internal"

        self.cpp_info.components["storage"].requires = ["rest_internal", "common", "nlohmann_json::nlohmann_json", "abseil::absl_memory", "abseil::absl_strings", "abseil::absl_str_format", "abseil::absl_time", "abseil::absl_variant", "crc32c::crc32c", "libcurl::libcurl", "openssl::ssl", "openssl::crypto", "zlib::zlib"]
        self.cpp_info.components["storage"].libs = ["google_cloud_cpp_storage"]
        self.cpp_info.components["storage"].names["pkg_config"] = "google_cloud_cpp_storage"
        if self.options.with_mocks:
            self.cpp_info.components["storage"].requires.append("gtest::gmock")
