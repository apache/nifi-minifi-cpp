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

# Recipe based on https://github.com/conan-io/conan-center-index/blob/master/recipes/couchbase_cxx_client/all/conanfile.py
import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy, rm, rmdir, replace_in_file, export_conandata_patches, apply_conandata_patches

required_conan_version = ">=2.0.9"

class CouchbaseCxxClientConan(ConanFile):
    name = "couchbase_cxx_client"
    description = "Couchbase C++ SDK"
    license = "Apache-2.0"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/couchbase/couchbase-cxx-client"
    topics = ("couchbase", "database", "nosql", "sdk")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.settings.os == "Windows":
            # Only static on Windows
            del self.options.shared
            self.package_type = "static-library"
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")

    def requirements(self):
        # these should match https://github.com/couchbase/couchbase-cxx-client/blob/main/couchbase-sdk-cxx-black-duck-manifest.yaml
        # as best as possible
        self.requires("spdlog/[>=1.15 <2]")
        self.requires("fmt/[*]")
        self.requires("ms-gsl/4.0.0")
        self.requires("snappy/[~1.2.1]")
        self.requires("asio/1.38.0")
        self.requires("hdrhistogram-c/0.11.8")
        self.requires("taocpp-json/1.0.0-beta.14")
        self.requires("llhttp/[>=9.1.3 <10]")
        self.requires("openssl/[>=1.1 <4]")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.19.0]")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def validate(self):
        check_min_cppstd(self, 17)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        # Use only the ConanThirdPartyDependencies.cmake module
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"),
                        "include(cmake/ThirdPartyDependencies.cmake)",
                        "include(cmake/ConanThirdPartyDependencies.cmake)")
        apply_conandata_patches(self)

    def export_sources(self):
        export_conandata_patches(self)
        copy(self, "ConanThirdPartyDependencies.cmake", self.recipe_folder,
             os.path.join(self.export_sources_folder, "src", "cmake"))

    def generate(self):
        deps = CMakeDeps(self)
        deps.set_property("hdrhistogram-c", "cmake_target_name", "hdr_histogram_static")
        deps.set_property("snappy", "cmake_target_name", "snappy")
        deps.set_property("asio", "cmake_target_name", "asio")
        deps.generate()

        tc = CMakeToolchain(self)
        tc.cache_variables["COUCHBASE_CXX_CLIENT_COLUMNAR"] = "OFF"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_TESTS"] = "OFF"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_EXAMPLES"] =  "OFF"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_TOOLS"] = "OFF"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_DOCS"] = "OFF"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_INSTALL"] = "ON"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_CLANG_TIDY"] = False
        if self.options.get_safe("shared"):
            tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_SHARED"] = "ON"
            tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_STATIC"] = "OFF"
        else:
            tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_SHARED"] = "OFF"
            tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_STATIC"] = "ON"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_STATIC_BORINGSSL"] = "OFF"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_POST_LINKED_OPENSSL"] = "OFF"
        tc.cache_variables["COUCHBASE_CXX_CLIENT_USE_HOMEBREW_TO_DETECT_OPENSSL"] = False
        tc.cache_variables["COUCHBASE_CXX_CLIENT_USE_SCOOP_TO_DETECT_OPENSSL"] = False
        tc.cache_variables["COUCHBASE_CXX_CLIENT_EMBED_MOZILLA_CA_BUNDLE"] = False
        tc.cache_variables["COUCHBASE_CXX_CLIENT_STATIC_OPENSSL"] = not bool(self.dependencies["openssl"].options.shared)
        tc.cache_variables["OPENSSL_USABLE"] = True
        # Force try_compile checks to use the current single-config build type to avoid looking for missing *_DEBUG imported targets
        tc.cache_variables["CMAKE_TRY_COMPILE_CONFIGURATION"] = str(self.settings.build_type)
        tc.cache_variables["COUCHBASE_CXX_CLIENT_BUILD_OPENTELEMETRY"] = "OFF"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        for pattern in ("*.hxx", "*.ixx", "*.h"):
            copy(self, pattern, src=os.path.join(self.source_folder, "core"),
                 dst=os.path.join(self.package_folder, "include", "core"), keep_path=True)
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rm(self, "*.pdb", self.package_folder, recursive=True)

    def package_info(self):
        # Provide canonical CMake target name for consumers
        self.cpp_info.set_property("cmake_file_name", "couchbase_cxx_client")
        self.cpp_info.set_property("cmake_target_name", "couchbase_cxx_client::couchbase_cxx_client")
        self.cpp_info.set_property("pkg_config_name", "couchbase_cxx_client")

        # couchbase has different library names for shared vs static builds
        self.cpp_info.libs = ["couchbase_cxx_client" if self.options.get_safe("shared")
                              else "couchbase_cxx_client_static"]
